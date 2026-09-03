#include "brain/StickmonClawRuntime.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "sdkconfig.h"
#include "BrainBridge.h"
#include "StickmonCapability.h"
#include "app_capabilities.h"
#include "app_claw.h"
#if CONFIG_APP_CLAW_CAP_IM_WECHAT
#include "cap_im_wechat.h"
#endif
#if CONFIG_APP_CLAW_CAP_IM_TG
#include "cap_im_tg.h"
#endif
#include "cJSON.h"
#include "claw_paths.h"
#include "claw_memory.h"
#include "claw_core.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_vfs_fat.h"
#include "esp_wifi.h"
#include "freertos/idf_additions.h"
#include "wear_levelling.h"
#include "platform/api/PlatformServices.h"
#include "presentation/QrCodeGen.h"

namespace Stickmon {
namespace {

constexpr char TAG[] = "StickmonClaw";
constexpr char BRAINFS_BASE[] = "/brain";
constexpr char BRAINFS_PARTITION[] = "brainfs";
constexpr char NVS_WIFI_NAMESPACE[] = "wifi";
constexpr char NVS_CLAW_NAMESPACE[] = "claw";
// The initialization path peaks at about 12 KB of stack on AMOLED V1. Keep
// enough margin for filesystem and network setup while returning 12 KB of
// internal RAM to the root agent's internal-only worker stack.
constexpr uint32_t CLAW_INIT_TASK_STACK_SIZE = 20 * 1024;
constexpr uint32_t CLAW_WIFI_CONNECT_TIMEOUT_MS = 20000;

esp_netif_t* s_staNetif = nullptr;
esp_netif_t* s_apNetif = nullptr;
httpd_handle_t s_setupServer = nullptr;
bool s_wifiInitialized = false;
bool s_wifiEventsRegistered = false;
volatile bool s_wifiConnected = false;
portMUX_TYPE s_wifiMux = portMUX_INITIALIZER_UNLOCKED;
wl_handle_t s_brainWlHandle = WL_INVALID_HANDLE;

constexpr size_t SETUP_MAX_BODY = 4096;

bool wifiConnected();
bool startWifi(const char* ssid, const char* password, bool keepAp);

bool readText(const char* nameSpace, const char* key, char* output,
              size_t outputSize) {
    if (!output || outputSize == 0) return false;
    output[0] = '\0';
    size_t size = Platform::blobs().blobSize(nameSpace, key);
    if (size == 0 || size >= outputSize) return false;
    if (!Platform::blobs().readBlob(nameSpace, key, output, size)) return false;
    output[size] = '\0';
    return true;
}

void copyText(char* destination, size_t destinationSize, const char* source) {
    if (!destination || destinationSize == 0) return;
    std::snprintf(destination, destinationSize, "%s", source ? source : "");
}

struct SetupValues {
    char wifiSsid[65] = {};
    char wifiPassword[65] = {};
    char cozeToken[321] = {};
    char cozeBotId[65] = {};
    char cozeBaseUrl[321] = {};
    // The setup page selects one IM channel. Keep the list representation for
    // compatibility with older saves, but sanitize it to one entry.
    char imChannels[65] = {};
    char wechatToken[257] = {};
    char wechatBaseUrl[161] = {};
    char wechatCdnUrl[161] = {};
    char wechatAccountId[33] = {};
    char telegramToken[193] = {};
    char qqAppId[65] = {};
    char qqAppSecret[321] = {};
    char feishuAppId[65] = {};
    char feishuAppSecret[321] = {};
};

// id, Chinese label (used in log lines), NVS keys cleared on channel delete.
struct ImChannelDef {
    const char* id;
    const char* label;
    const char* keys[4];
};
constexpr ImChannelDef IM_CHANNEL_DEFS[] = {
    {"wechat", "微信",
     {"wechat_token", "wechat_base_url", "wechat_cdn_url", "wechat_acct_id"}},
    {"telegram", "Telegram", {"telegram_token", nullptr, nullptr, nullptr}},
    {"qq", "QQ", {"qq_app_id", "qq_app_secret", nullptr, nullptr}},
    {"feishu", "飞书",
     {"feishu_app_id", "feishu_secret", nullptr, nullptr}},
};

const ImChannelDef* findImChannelDef(const char* id, size_t length) {
    for (const ImChannelDef& def : IM_CHANNEL_DEFS) {
        if (std::strlen(def.id) == length &&
            std::strncmp(def.id, id, length) == 0) {
            return &def;
        }
    }
    return nullptr;
}

bool imChannelListed(const char* list, const char* id) {
    if (!list || !id) return false;
    const size_t idLength = std::strlen(id);
    const char* cursor = list;
    while (*cursor) {
        const char* end = std::strchr(cursor, ',');
        const size_t length =
            end ? static_cast<size_t>(end - cursor) : std::strlen(cursor);
        if (length == idLength && std::strncmp(cursor, id, length) == 0) {
            return true;
        }
        if (!end) break;
        cursor = end + 1;
    }
    return false;
}

void appendImChannelId(char* list, size_t listSize, const char* id) {
    if (!list || listSize == 0 || imChannelListed(list, id)) return;
    const size_t used = std::strlen(list);
    std::snprintf(list + used, listSize - used, used > 0 ? ",%s" : "%s", id);
}

// Keeps only supported channel ids and selects the first one. The list form
// remains on disk for compatibility with earlier multi-channel builds.
void sanitizeImChannels(const char* input, char* output, size_t outputSize) {
    if (!output || outputSize == 0) return;
    output[0] = '\0';
    const char* cursor = input;
    while (cursor && *cursor) {
        const char* end = std::strchr(cursor, ',');
        const size_t length =
            end ? static_cast<size_t>(end - cursor) : std::strlen(cursor);
        const ImChannelDef* def = findImChannelDef(cursor, length);
        if (def && std::strcmp(def->id, "telegram") != 0) {
            appendImChannelId(output, outputSize, def->id);
            break;
        }
        if (!end) break;
        cursor = end + 1;
    }
}

void loadSetupValues(SetupValues& values) {
    readText(NVS_WIFI_NAMESPACE, "ssid", values.wifiSsid,
             sizeof(values.wifiSsid));
    readText(NVS_WIFI_NAMESPACE, "password", values.wifiPassword,
             sizeof(values.wifiPassword));
    readText(NVS_CLAW_NAMESPACE, "coze_token", values.cozeToken,
             sizeof(values.cozeToken));
    readText(NVS_CLAW_NAMESPACE, "coze_bot_id", values.cozeBotId,
             sizeof(values.cozeBotId));
    readText(NVS_CLAW_NAMESPACE, "coze_base_url", values.cozeBaseUrl,
             sizeof(values.cozeBaseUrl));
    readText(NVS_CLAW_NAMESPACE, "wechat_token", values.wechatToken,
             sizeof(values.wechatToken));
    readText(NVS_CLAW_NAMESPACE, "wechat_base_url", values.wechatBaseUrl,
             sizeof(values.wechatBaseUrl));
    readText(NVS_CLAW_NAMESPACE, "wechat_cdn_url", values.wechatCdnUrl,
             sizeof(values.wechatCdnUrl));
    readText(NVS_CLAW_NAMESPACE, "wechat_acct_id", values.wechatAccountId,
             sizeof(values.wechatAccountId));
    readText(NVS_CLAW_NAMESPACE, "telegram_token", values.telegramToken,
             sizeof(values.telegramToken));
    readText(NVS_CLAW_NAMESPACE, "qq_app_id", values.qqAppId,
             sizeof(values.qqAppId));
    readText(NVS_CLAW_NAMESPACE, "qq_app_secret", values.qqAppSecret,
             sizeof(values.qqAppSecret));
    readText(NVS_CLAW_NAMESPACE, "feishu_app_id", values.feishuAppId,
             sizeof(values.feishuAppId));
    readText(NVS_CLAW_NAMESPACE, "feishu_secret", values.feishuAppSecret,
             sizeof(values.feishuAppSecret));
    if (!readText(NVS_CLAW_NAMESPACE, "im_channels", values.imChannels,
                  sizeof(values.imChannels))) {
        // Legacy saves have no channel list; derive it from stored tokens so
        // existing setups keep their channels enabled.
        if (values.wechatToken[0]) {
            appendImChannelId(values.imChannels, sizeof(values.imChannels),
                              "wechat");
        }
        if (values.feishuAppId[0] && values.feishuAppSecret[0]) {
            appendImChannelId(values.imChannels, sizeof(values.imChannels),
                              "feishu");
        }
    }

    // Normalize legacy multi-channel saves before they reach app_claw.
    char sanitizedChannels[sizeof(values.imChannels)] = {};
    sanitizeImChannels(values.imChannels, sanitizedChannels,
                       sizeof(sanitizedChannels));
    copyText(values.imChannels, sizeof(values.imChannels), sanitizedChannels);
}

int hexValue(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

bool decodeFormComponent(const char* input, size_t length, char* output,
                         size_t outputSize) {
    if (!input || !output || outputSize == 0) return false;
    size_t written = 0;
    for (size_t index = 0; index < length; ++index) {
        if (written + 1 >= outputSize) return false;
        char value = input[index];
        if (value == '+') {
            value = ' ';
        } else if (value == '%') {
            if (index + 2 >= length) return false;
            int high = hexValue(input[index + 1]);
            int low = hexValue(input[index + 2]);
            if (high < 0 || low < 0) return false;
            value = static_cast<char>((high << 4) | low);
            index += 2;
        }
        output[written++] = value;
    }
    output[written] = '\0';
    return true;
}

bool assignFormField(const char* key, const char* value, SetupValues& values) {
    if (std::strcmp(key, "wifi_ssid") == 0) {
        copyText(values.wifiSsid, sizeof(values.wifiSsid), value);
    } else if (std::strcmp(key, "wifi_password") == 0) {
        if (value[0]) copyText(values.wifiPassword, sizeof(values.wifiPassword), value);
    } else if (std::strcmp(key, "coze_token") == 0) {
        if (value[0]) copyText(values.cozeToken, sizeof(values.cozeToken), value);
    } else if (std::strcmp(key, "coze_bot_id") == 0) {
        if (value[0]) copyText(values.cozeBotId, sizeof(values.cozeBotId), value);
    } else if (std::strcmp(key, "coze_base_url") == 0) {
        if (value[0]) copyText(values.cozeBaseUrl, sizeof(values.cozeBaseUrl), value);
    } else if (std::strcmp(key, "wechat_token") == 0) {
        if (value[0]) copyText(values.wechatToken, sizeof(values.wechatToken), value);
    } else if (std::strcmp(key, "wechat_base_url") == 0) {
        if (value[0]) copyText(values.wechatBaseUrl, sizeof(values.wechatBaseUrl), value);
    } else if (std::strcmp(key, "wechat_cdn_url") == 0) {
        if (value[0]) copyText(values.wechatCdnUrl, sizeof(values.wechatCdnUrl), value);
    } else if (std::strcmp(key, "wechat_acct_id") == 0) {
        if (value[0]) copyText(values.wechatAccountId, sizeof(values.wechatAccountId), value);
    } else if (std::strcmp(key, "telegram_token") == 0) {
        if (value[0]) copyText(values.telegramToken, sizeof(values.telegramToken), value);
    } else if (std::strcmp(key, "im_channels") == 0) {
        sanitizeImChannels(value, values.imChannels, sizeof(values.imChannels));
    } else if (std::strcmp(key, "qq_app_id") == 0) {
        copyText(values.qqAppId, sizeof(values.qqAppId), value);
    } else if (std::strcmp(key, "qq_app_secret") == 0) {
        if (value[0]) copyText(values.qqAppSecret, sizeof(values.qqAppSecret), value);
    } else if (std::strcmp(key, "feishu_app_id") == 0) {
        copyText(values.feishuAppId, sizeof(values.feishuAppId), value);
    } else if (std::strcmp(key, "feishu_app_secret") == 0) {
        if (value[0]) copyText(values.feishuAppSecret, sizeof(values.feishuAppSecret), value);
    } else {
        return false;
    }
    return true;
}

bool parseSetupForm(const char* body, size_t length, SetupValues& values) {
    if (!body) return false;
    size_t offset = 0;
    while (offset < length) {
        size_t end = offset;
        while (end < length && body[end] != '&') ++end;
        size_t equals = offset;
        while (equals < end && body[equals] != '=') ++equals;
        if (equals == end) return false;
        char key[32] = {};
        char value[321] = {};
        if (!decodeFormComponent(body + offset, equals - offset, key,
                                 sizeof(key)) ||
            !decodeFormComponent(body + equals + 1, end - equals - 1, value,
                                 sizeof(value))) {
            return false;
        }
        assignFormField(key, value, values);
        offset = end < length ? end + 1 : end;
    }
    return true;
}

bool saveText(const char* nameSpace, const char* key, const char* value) {
    if (!value || !value[0]) {
        // Avoid an unnecessary NVS transaction when the key is already absent.
        if (Platform::blobs().blobSize(nameSpace, key) == 0) return true;
        return Platform::blobs().removeBlob(nameSpace, key);
    }
    const size_t length = std::strlen(value);
    // Re-saving the complete setup form used to rewrite every credential on
    // every click. NVS keeps old blob entries until garbage collection, so
    // avoid consuming another entry when the value is unchanged.
    if (Platform::blobs().blobSize(nameSpace, key) == length) {
        void* existing = std::malloc(length);
        if (existing) {
            const bool same = Platform::blobs().readBlob(
                nameSpace, key, existing, length) &&
                std::memcmp(existing, value, length) == 0;
            std::free(existing);
            if (same) return true;
        }
    }
    return Platform::blobs().writeBlob(nameSpace, key, value, length);
}

bool saveSetupField(const char* nameSpace, const char* key, const char* value,
                    const char** failedKey) {
    if (saveText(nameSpace, key, value)) return true;
    ESP_LOGE(TAG, "配置保存失败 namespace=%s key=%s value_len=%u",
             nameSpace, key,
             static_cast<unsigned>(value ? std::strlen(value) : 0));
    ClawRuntime::instance().logf(ClawStatusLog::Level::ERROR,
                                 "配置保存失败：%s（长度=%u）", key,
                                 static_cast<unsigned>(value ? std::strlen(value) : 0));
    if (failedKey && !*failedKey) *failedKey = key;
    return false;
}

bool saveSetupValues(const SetupValues& values, const char** failedKey = nullptr) {
    bool ok = true;
    auto save = [&](const char* nameSpace, const char* key, const char* value) {
        if (!saveSetupField(nameSpace, key, value, failedKey)) ok = false;
    };
    // Run every field even after a failure so one bad/fragmented key does not
    // silently prevent the remaining configuration from being saved.
    save(NVS_WIFI_NAMESPACE, "ssid", values.wifiSsid);
    save(NVS_WIFI_NAMESPACE, "password", values.wifiPassword);
    save(NVS_CLAW_NAMESPACE, "coze_token", values.cozeToken);
    save(NVS_CLAW_NAMESPACE, "coze_bot_id", values.cozeBotId);
    save(NVS_CLAW_NAMESPACE, "coze_base_url", values.cozeBaseUrl);
    save(NVS_CLAW_NAMESPACE, "im_channels", values.imChannels);
    save(NVS_CLAW_NAMESPACE, "wechat_token", values.wechatToken);
    save(NVS_CLAW_NAMESPACE, "wechat_base_url", values.wechatBaseUrl);
    save(NVS_CLAW_NAMESPACE, "wechat_cdn_url", values.wechatCdnUrl);
    save(NVS_CLAW_NAMESPACE, "wechat_acct_id", values.wechatAccountId);
    save(NVS_CLAW_NAMESPACE, "telegram_token", values.telegramToken);
    save(NVS_CLAW_NAMESPACE, "qq_app_id", values.qqAppId);
    save(NVS_CLAW_NAMESPACE, "qq_app_secret", values.qqAppSecret);
    save(NVS_CLAW_NAMESPACE, "feishu_app_id", values.feishuAppId);
    save(NVS_CLAW_NAMESPACE, "feishu_secret", values.feishuAppSecret);
    return ok;
}

// Channels that were in the stored list but are absent from the submitted
// list have been deleted on the page: wipe their NVS credentials so the
// local cache goes away with the tab.
void deleteRemovedImChannels(const char* previousChannels,
                             const char* currentChannels) {
    for (const ImChannelDef& def : IM_CHANNEL_DEFS) {
        if (!imChannelListed(previousChannels, def.id) ||
            imChannelListed(currentChannels, def.id)) {
            continue;
        }
        for (const char* key : def.keys) {
            if (key) Platform::blobs().removeBlob(NVS_CLAW_NAMESPACE, key);
        }
        ClawRuntime::instance().logf(ClawStatusLog::Level::INFO,
                                     "IM 渠道已删除：%s", def.label);
    }
}

bool hasRemovedImChannels(const char* previousChannels,
                          const char* currentChannels) {
    for (const ImChannelDef& def : IM_CHANNEL_DEFS) {
        if (imChannelListed(previousChannels, def.id) &&
            !imChannelListed(currentChannels, def.id)) {
            return true;
        }
    }
    return false;
}

bool readRequestBody(httpd_req_t* request, char** body, size_t* length) {
    if (!request || !body || !length || request->content_len == 0 ||
        request->content_len > SETUP_MAX_BODY) {
        return false;
    }
    char* buffer = static_cast<char*>(std::malloc(request->content_len + 1));
    if (!buffer) return false;
    size_t received = 0;
    while (received < request->content_len) {
        int result = httpd_req_recv(request, buffer + received,
                                    request->content_len - received);
        if (result <= 0) {
            std::free(buffer);
            return false;
        }
        received += static_cast<size_t>(result);
    }
    buffer[received] = '\0';
    *body = buffer;
    *length = received;
    return true;
}

constexpr char SETUP_PAGE[] =
    "<!doctype html><html lang=zh-CN><head><meta charset=utf-8>"
    "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
    "<title>StickMon ESP-Claw</title><style>"
    "body{font:16px system-ui,sans-serif;max-width:560px;margin:24px auto;"
    "padding:0 16px;color:#1e293b;background:#f8fafc}"
    "h1{font-size:24px}label{display:block;margin:14px 0 6px}"
    "input{box-sizing:border-box;width:100%;padding:10px;border:1px solid #94a3b8;"
    "border-radius:6px;font-size:16px}button{margin-top:20px;padding:11px 18px;"
    "border:0;border-radius:6px;background:#0f766e;color:white;font-size:16px}"
    "small{color:#64748b}section{margin-top:28px;padding-top:18px;border-top:1px solid #cbd5e1}"
    "#wechat-qr{display:block;max-width:256px;width:100%;margin:16px auto;background:#fff}"
    "#wechat-status{min-height:24px;color:#475569}button.secondary{margin-left:8px;background:#475569}"
    ".tabs{margin:14px 0 0}.tab{display:inline-block;padding:6px 12px;border:1px solid #94a3b8;"
    "border-bottom:0;border-radius:6px 6px 0 0;background:#e2e8f0;color:#334155;cursor:pointer;"
    "margin-right:4px;font-size:15px}"
    ".tab.active{background:#fff;color:#0f766e;font-weight:600}"
    ".tabpanel{border:1px solid #94a3b8;border-radius:0 6px 6px 6px;padding:2px 14px 14px;background:#fff}"
    "button.danger{background:#b91c1c}"
    "#im-add button{margin:8px 6px 0 0}"
    ".badge{display:inline-block;padding:2px 10px;border-radius:10px;background:#e2e8f0;"
    "color:#334155;font-size:14px;margin:2px 4px 2px 0}"
    ".badge.ok{background:#dcfce7;color:#166534}.badge.warn{background:#fef9c3;color:#854d0e}"
    ".badge.err{background:#fee2e2;color:#b91c1c}"
    "#log{background:#0f172a;color:#e2e8f0;font:12px/1.6 ui-monospace,monospace;padding:10px;"
    "border-radius:6px;height:220px;overflow-y:auto;white-space:pre-wrap;word-break:break-all;margin:8px 0 0}"
    "#log .lv-ok{color:#4ade80}#log .lv-warn{color:#facc15}#log .lv-error{color:#f87171}"
    "</style></head><body>"
    "<h1>StickMon ESP-Claw</h1>"
    "<section><h2>设备状态</h2><p>"
    "<span class=badge id=b-wifi>Wi-Fi：读取中</span>"
    "<span class=badge id=b-phone>手机：未加入热点</span>"
    "<span class=badge id=b-claw>ESP-Claw：读取中</span>"
    "<span class=badge id=b-wechat>微信：读取中</span></p>"
    "<p><button type=button class=danger id=claw-clear>清理 ESP-Claw 会话历史</button>"
    "<small id=claw-clear-status>仅清理聊天历史，不影响游戏存档</small></p>"
    "<pre id=log></pre></section>"
    "<form method=post action=/api/save>"
    "<section><h2>Wi-Fi 配置</h2>"
    "<label>Wi-Fi 名称</label><input id=wifi-ssid name=wifi_ssid maxlength=64 required>"
    "<label>Wi-Fi 密码</label><input id=wifi-password name=wifi_password maxlength=64 type=password>"
    "<small id=wifi-password-status>未读取配置</small></section>"
    "<section><h2>Coze 配置</h2>"
    "<label>Coze Access Token</label><input id=coze-token name=coze_token maxlength=320 type=password>"
    "<small id=coze-token-status>未读取配置</small>"
    "<label>Coze Bot ID</label><input id=coze-bot-id name=coze_bot_id maxlength=64 value=7679649015453597711 required>"
    "<label>Coze API 地址</label><input id=coze-base-url name=coze_base_url maxlength=320 value=https://api.coze.cn required></section>"
    "<section><h2>IM 渠道</h2>"
    "<p><small>请选择一个聊天渠道；删除渠道后点「保存配置」"
    "会同时清除本机保存的该渠道凭据。</small></p>"
    "<div class=tabs id=im-tabs></div>"
    "<div class=tabpanel>"
    "<div id=panel-wechat hidden>"
    "<p>生成二维码后，用微信扫码并确认；登录凭据只保存在本机。</p>"
    "<button type=button id=wechat-start>生成微信二维码</button>"
    "<button type=button class=secondary id=wechat-cancel>取消</button>"
    "<canvas id=wechat-qr aria-label=微信登录二维码 hidden></canvas>"
    "<p id=wechat-qr-fallback hidden><a id=wechat-qr-link target=_blank rel=noopener>打开微信二维码图片</a></p>"
    "<p id=wechat-status>状态：空闲</p>"
    "<p><button type=button id=wechat-save hidden>保存微信登录</button></p>"
    "<button type=button class=danger data-del=wechat>删除此渠道</button></div>"
    "<div id=panel-qq hidden>"
    "<label>App ID</label><input id=qq-app-id name=qq_app_id maxlength=64 disabled>"
    "<label>App Secret</label><input id=qq-app-secret name=qq_app_secret maxlength=320 type=password disabled>"
    "<small id=qq-secret-status>未读取配置</small>"
    "<p><button type=button class=danger data-del=qq>删除此渠道</button></p></div>"
    "<div id=panel-feishu hidden>"
    "<label>App ID</label><input id=feishu-app-id name=feishu_app_id maxlength=64 disabled>"
    "<label>App Secret</label><input id=feishu-app-secret name=feishu_app_secret maxlength=320 type=password disabled>"
    "<small id=feishu-secret-status>未读取配置</small>"
    "<p><button type=button class=danger data-del=feishu>删除此渠道</button></p></div>"
    "<p id=im-empty><small>尚未配置渠道。</small></p>"
    "</div>"
    "<p><button type=button class=secondary id=im-add-btn>+ 新建渠道</button>"
    "<span id=im-add hidden>"
    "<button type=button class=secondary data-add=wechat>微信</button>"
    "<button type=button class=secondary data-add=qq>QQ</button>"
    "<button type=button class=secondary data-add=feishu>飞书</button>"
    "</span></p>"
    "<input type=hidden id=im-channels name=im_channels>"
    "</section>"
    "<button type=submit>保存配置</button></form>"
    "<script>(function(){const q=document.getElementById('wechat-qr'),s=document.getElementById('wechat-status'),"
    "start=document.getElementById('wechat-start'),cancel=document.getElementById('wechat-cancel'),save=document.getElementById('wechat-save'),fallback=document.getElementById('wechat-qr-fallback'),link=document.getElementById('wechat-qr-link');"
    "let timer=0;async function requestJson(url,options){const r=await fetch(url,options);"
    "const text=await r.text();let d=null;try{d=text?JSON.parse(text):{};}catch(e){}"
    "if(!r.ok)throw new Error((d&&d.message)||text||('HTTP '+r.status));return d||{};}"
    "function drawQr(d){const n=Number(d.qr_size),bits=d.qr_modules||'';"
    "if(!n||bits.length<n*n){q.hidden=true;fallback.hidden=!d.qr_data_url;link.href=d.qr_data_url||'#';return;}"
    "fallback.hidden=true;const quiet=4,total=n+quiet*2,p=240,scale=Math.max(1,Math.floor(p/total)),side=total*scale;"
    "q.width=side;q.height=side;q.style.width=side+'px';q.style.height=side+'px';"
    "const c=q.getContext('2d');c.fillStyle='#fff';c.fillRect(0,0,side,side);c.fillStyle='#000';"
    "for(let y=0;y<n;y++)for(let x=0;x<n;x++)if(bits[y*n+x]==='1')c.fillRect((x+quiet)*scale,(y+quiet)*scale,scale,scale);q.hidden=false;}"
    "function show(d){s.textContent='状态：'+(d.message||d.status||'空闲');"
    "drawQr(d);"
    "save.hidden=!d.completed||d.persisted;if(d.active){clearTimeout(timer);timer=setTimeout(refresh,1200);}}"
    "async function refresh(){try{show(await requestJson('/api/wechat/login/status'));}"
    "catch(e){s.textContent='状态：'+(e.message||'请求失败');}}"
    "async function loadConfig(){try{const r=await fetch('/api/config');const d=await r.json();"
    "if(d.wifi_ssid)document.getElementById('wifi-ssid').value=d.wifi_ssid;"
    "if(d.coze_bot_id)document.getElementById('coze-bot-id').value=d.coze_bot_id;"
    "if(d.coze_base_url)document.getElementById('coze-base-url').value=d.coze_base_url;"
    "document.getElementById('wifi-password-status').textContent=d.wifi_password_configured?'已保存，留空保持不变':'尚未保存';"
    "document.getElementById('coze-token-status').textContent=d.coze_token_configured?'已保存，留空保持不变':'尚未保存';"
    "if(d.qq_app_id)document.getElementById('qq-app-id').value=d.qq_app_id;"
    "if(d.feishu_app_id)document.getElementById('feishu-app-id').value=d.feishu_app_id;"
    "document.getElementById('qq-secret-status').textContent=d.qq_app_secret_configured?'已保存，留空保持不变':'尚未保存';"
    "document.getElementById('feishu-secret-status').textContent=d.feishu_app_secret_configured?'已保存，留空保持不变':'尚未保存';"
    "channels=(d.im_channels||'').split(',').filter(function(c){return !!CH_LABEL[c];}).slice(0,1);"
    "for(const ch of channels)setPanelEnabled(ch,true);"
    "syncHidden();"
    "if(channels.length)selectCh(channels[0]);else renderTabs();"
    "}catch(e){document.getElementById('wifi-password-status').textContent='读取配置失败';document.getElementById('coze-token-status').textContent='读取配置失败';}}"
    "start.onclick=async function(){start.disabled=true;try{show(await requestJson('/api/wechat/login/start',{method:'POST'}));}"
    "catch(e){s.textContent='状态：'+(e.message||'请求失败');}finally{start.disabled=false;}};"
    "cancel.onclick=async function(){cancel.disabled=true;try{show(await requestJson('/api/wechat/login/cancel',{method:'POST'}));}"
    "catch(e){s.textContent='状态：'+(e.message||'请求失败');}finally{cancel.disabled=false;}};"
    "save.onclick=async function(){save.disabled=true;try{show(await requestJson('/api/wechat/login/persist',{method:'POST'}));}"
    "catch(e){s.textContent='状态：'+(e.message||'请求失败');}finally{save.disabled=false;}};"
    "const CH_LABEL={wechat:'微信',qq:'QQ',feishu:'飞书'};"
    "let channels=[],curCh='';"
    "function panelOf(ch){return document.getElementById('panel-'+ch);}"
    "function setPanelEnabled(ch,on){const nodes=panelOf(ch).querySelectorAll('input');for(let i=0;i<nodes.length;i++)nodes[i].disabled=!on;}"
    "function syncHidden(){document.getElementById('im-channels').value=channels.join(',');}"
    "function renderTabs(){const bar=document.getElementById('im-tabs');bar.innerHTML='';"
    "for(const ch of channels){const t=document.createElement('span');t.className='tab'+(ch===curCh?' active':'');"
    "t.textContent=CH_LABEL[ch];t.onclick=(function(c){return function(){selectCh(c);};})(ch);bar.appendChild(t);}"
    "document.getElementById('im-empty').hidden=channels.length>0;}"
    "function selectCh(ch){curCh=ch;for(const id in CH_LABEL){panelOf(id).hidden=(id!==ch)||channels.indexOf(id)<0;}renderTabs();}"
    "function addCh(ch){for(const old of channels)setPanelEnabled(old,false);channels=[ch];setPanelEnabled(ch,true);syncHidden();"
    "document.getElementById('im-add').hidden=true;selectCh(ch);}"
    "function delCh(ch){channels=channels.filter(function(c){return c!==ch;});setPanelEnabled(ch,false);"
    "panelOf(ch).hidden=true;syncHidden();if(channels.length)selectCh(channels[0]);else{curCh='';renderTabs();}}"
    "document.getElementById('im-add-btn').onclick=function(){const m=document.getElementById('im-add');m.hidden=!m.hidden;};"
    "const addBtns=document.querySelectorAll('[data-add]');for(let i=0;i<addBtns.length;i++)"
    "addBtns[i].onclick=function(){addCh(this.getAttribute('data-add'));};"
    "const delBtns=document.querySelectorAll('[data-del]');for(let i=0;i<delBtns.length;i++)"
    "delBtns[i].onclick=function(){const ch=this.getAttribute('data-del');"
    "if(confirm('删除「'+CH_LABEL[ch]+'」后，点「保存配置」会清除本机保存的该渠道凭据。确定删除？'))delCh(ch);};"
    "let logGen=0;"
    "function badge(id,text,cls){const b=document.getElementById(id);b.textContent=text;b.className='badge'+(cls?' '+cls:'');}"
    "async function refreshStatus(){try{const r=await fetch('/api/status');const d=await r.json();"
    "badge('b-wifi',d.network?('Wi-Fi：已连接 '+(d.sta_ip||'')):'Wi-Fi：未连接',d.network?'ok':'warn');"
    "badge('b-phone',d.phone_joined?'手机：已加入热点':'手机：未加入热点',d.phone_joined?'ok':'');"
    "badge('b-claw',d.claw_started?'ESP-Claw：已启动':'ESP-Claw：未启动',d.claw_started?'ok':'');"
    "const wp={idle:'空闲',waiting_scan:'等待扫码',scanned:'已扫码，待确认',redirected:'已跳转',confirmed:'登录成功',expired:'二维码已过期',cancelled:'已取消',error:'登录失败'};"
    "let wt=wp[d.wechat_phase]||d.wechat_phase||'空闲';if(d.wechat_persisted)wt+='（已保存）';"
    "badge('b-wechat','微信：'+wt,d.wechat_persisted||d.wechat_phase==='confirmed'?'ok':(d.wechat_phase==='error'?'err':''));"
    "}catch(e){}}"
    "async function refreshLog(){try{const r=await fetch('/api/log?since='+logGen);const d=await r.json();logGen=d.gen;"
    "const pre=document.getElementById('log');"
    "const atBottom=pre.scrollTop+pre.clientHeight>=pre.scrollHeight-8;"
    "for(const e of d.entries){const line=document.createElement('div');line.className='lv-'+e.lv;"
    "line.textContent='['+(e.ms/1000).toFixed(0)+'s] '+e.t;pre.appendChild(line);}"
    "if(d.entries.length&&atBottom)pre.scrollTop=pre.scrollHeight;"
    "}catch(e){}}"
    "document.getElementById('claw-clear').onclick=async function(){"
    "if(!confirm('确定清理 ESP-Claw 的全部聊天历史吗？游戏存档不会受影响。'))return;"
    "const b=this,s=document.getElementById('claw-clear-status');b.disabled=true;s.textContent='正在清理...';"
    "try{const d=await requestJson('/api/claw/sessions/clear',{method:'POST'});"
    "s.textContent=d.deleted?'已清理聊天历史':'没有可清理的历史';"
    "}catch(e){s.textContent='清理失败：'+e.message;}finally{b.disabled=false;}};"
    "setInterval(refreshStatus,1500);setInterval(refreshLog,1500);refreshStatus();refreshLog();"
    "loadConfig();refresh();})();</script>"
    "<p><small>密码类字段留空会保留已有值；删除 IM 渠道后点「保存配置」会清除本机保存的该渠道凭据。"
    "保存后请重启设备，配置才会用于 ESP-Claw。"
    "后台地址：http://192.168.4.1/</small></p></body></html>";

esp_err_t setupPageHandler(httpd_req_t* request) {
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    return httpd_resp_send(request, SETUP_PAGE, HTTPD_RESP_USE_STRLEN);
}

esp_err_t setupConfigHandler(httpd_req_t* request) {
    SetupValues values;
    loadSetupValues(values);
    cJSON* object = cJSON_CreateObject();
    if (!object) {
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Out of memory");
    }
    cJSON_AddStringToObject(object, "wifi_ssid", values.wifiSsid);
    cJSON_AddBoolToObject(object, "wifi_password_configured",
                          values.wifiPassword[0] != '\0');
    cJSON_AddBoolToObject(object, "coze_token_configured",
                          values.cozeToken[0] != '\0');
    cJSON_AddStringToObject(object, "coze_bot_id",
                            values.cozeBotId[0] ? values.cozeBotId :
                            "7679649015453597711");
    cJSON_AddStringToObject(object, "coze_base_url",
                            values.cozeBaseUrl[0] ? values.cozeBaseUrl :
                            "https://api.coze.cn");
    cJSON_AddBoolToObject(object, "wechat_configured",
                          values.wechatToken[0] != '\0');
    cJSON_AddStringToObject(object, "im_channels", values.imChannels);
    cJSON_AddStringToObject(object, "qq_app_id", values.qqAppId);
    cJSON_AddBoolToObject(object, "qq_app_secret_configured",
                          values.qqAppSecret[0] != '\0');
    cJSON_AddStringToObject(object, "feishu_app_id", values.feishuAppId);
    cJSON_AddBoolToObject(object, "feishu_app_secret_configured",
                          values.feishuAppSecret[0] != '\0');
    char* rendered = cJSON_PrintUnformatted(object);
    cJSON_Delete(object);
    if (!rendered) {
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "JSON encoding failed");
    }
    httpd_resp_set_type(request, "application/json");
    esp_err_t result = httpd_resp_send(request, rendered, HTTPD_RESP_USE_STRLEN);
    std::free(rendered);
    return result;
}

esp_err_t setupStatusHandler(httpd_req_t* request) {
    char json[448] = {};
    ClawRuntime& runtime = ClawRuntime::instance();
    ClawStatus status;
    runtime.statusSnapshot(status);
    char ssid[33] = {};
    char ip[16] = {};
    runtime.setupPortalInfo(ssid, sizeof(ssid), nullptr, 0, ip,
                            sizeof(ip));
    std::snprintf(json, sizeof(json),
                  "{\"active\":%s,\"network\":%s,\"ssid\":\"%s\","
                  "\"ip\":\"%s\",\"sta_ip\":\"%s\",\"phone_joined\":%s,"
                  "\"claw_started\":%s,\"wechat_phase\":\"%s\","
                  "\"wechat_persisted\":%s,\"log_gen\":%lu}",
                  status.portalActive ? "true" : "false",
                  status.staConnected ? "true" : "false",
                  status.portalActive ? ssid : "",
                  status.portalActive ? ip : "",
                  status.staIp,
                  status.phoneJoined ? "true" : "false",
                  status.clawStarted ? "true" : "false",
                  status.wechatPhase,
                  status.wechatPersisted ? "true" : "false",
                  static_cast<unsigned long>(runtime.logGeneration()));
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, json, HTTPD_RESP_USE_STRLEN);
}

esp_err_t setupClawClearSessionsHandler(httpd_req_t* request) {
    bool deleted = false;
    const esp_err_t result = claw_memory_clear_session_histories(&deleted);
    if (result != ESP_OK) {
        ClawRuntime::instance().logf(ClawStatusLog::Level::ERROR,
                                     "ESP-Claw 会话历史清理失败 %s",
                                     esp_err_to_name(result));
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   esp_err_to_name(result));
    }
    ClawRuntime::instance().logf(ClawStatusLog::Level::OK,
                                 deleted ? "ESP-Claw 会话历史已清理"
                                         : "ESP-Claw 没有可清理的会话历史");
    char json[64] = {};
    std::snprintf(json, sizeof(json), "{\"ok\":true,\"deleted\":%s}",
                  deleted ? "true" : "false");
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, json, HTTPD_RESP_USE_STRLEN);
}

// Appends `text` to out at offset with JSON string escaping (" and \).
size_t appendJsonEscaped(char* out, size_t capacity, size_t offset,
                         const char* text) {
    for (const char* cursor = text; cursor && *cursor; ++cursor) {
        if (offset + 2 >= capacity) break;
        if (*cursor == '"' || *cursor == '\\') out[offset++] = '\\';
        out[offset++] = *cursor;
    }
    return offset;
}

esp_err_t setupLogHandler(httpd_req_t* request) {
    uint32_t since = 0;
    char query[32] = {};
    if (httpd_req_get_url_query_str(request, query, sizeof(query)) == ESP_OK) {
        char value[16] = {};
        if (httpd_query_key_value(query, "since", value, sizeof(value)) ==
            ESP_OK) {
            since = static_cast<uint32_t>(std::strtoul(value, nullptr, 10));
        }
    }
    ClawRuntime& runtime = ClawRuntime::instance();
    // The httpd task stack is small; keep the entry array and the JSON
    // document on the heap.
    auto* entries = static_cast<ClawStatusLog::Entry*>(
        std::malloc(sizeof(ClawStatusLog::Entry) * ClawStatusLog::CAPACITY));
    if (!entries) {
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Out of memory");
    }
    uint32_t generation = 0;
    const size_t count = runtime.copyLogSince(
        since, entries, ClawStatusLog::CAPACITY, &generation);
    // Fixed fields plus worst-case fully-escaped text per entry.
    const size_t capacity =
        64 + count * (ClawStatusLog::TEXT_LEN * 2 + 40);
    char* json = static_cast<char*>(std::malloc(capacity));
    if (!json) {
        std::free(entries);
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Out of memory");
    }
    static const char* const LEVEL_NAMES[] = {"info", "ok", "warn", "error"};
    size_t offset = static_cast<size_t>(std::snprintf(
        json, capacity, "{\"gen\":%lu,\"entries\":[",
        static_cast<unsigned long>(generation)));
    for (size_t i = 0; i < count; ++i) {
        const ClawStatusLog::Entry& entry = entries[i];
        const uint8_t level = static_cast<uint8_t>(entry.level);
        offset += static_cast<size_t>(std::snprintf(
            json + offset, capacity - offset, "%s{\"ms\":%lu,\"lv\":\"%s\",\"t\":\"",
            i == 0 ? "" : ",", static_cast<unsigned long>(entry.ms),
            LEVEL_NAMES[level < 4 ? level : 0]));
        offset = appendJsonEscaped(json, capacity, offset, entry.text);
        offset += static_cast<size_t>(
            std::snprintf(json + offset, capacity - offset, "\"}"));
    }
    std::free(entries);
    offset += static_cast<size_t>(
        std::snprintf(json + offset, capacity - offset, "]}"));
    httpd_resp_set_type(request, "application/json");
    esp_err_t result = httpd_resp_send(request, json, static_cast<ssize_t>(offset));
    std::free(json);
    return result;
}

#if CONFIG_APP_CLAW_CAP_IM_WECHAT
esp_err_t sendJsonResponse(httpd_req_t* request, cJSON* object) {
    if (!request || !object) {
        cJSON_Delete(object);
        return ESP_ERR_INVALID_ARG;
    }
    char* rendered = cJSON_PrintUnformatted(object);
    cJSON_Delete(object);
    if (!rendered) {
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "JSON encoding failed");
    }
    httpd_resp_set_type(request, "application/json");
    esp_err_t result = httpd_resp_send(request, rendered, HTTPD_RESP_USE_STRLEN);
    std::free(rendered);
    return result;
}

esp_err_t writeWechatQrStatus(httpd_req_t* request,
                              const cap_im_wechat_qr_login_status_t& status) {
    const size_t matrixBytes = Stickmon::QrCodeGen::MAX_MATRIX_BYTES;
    uint8_t* qrModules = static_cast<uint8_t*>(std::calloc(matrixBytes, 1));
    char* qrModuleText = static_cast<char*>(std::calloc(matrixBytes + 1, 1));
    if (!qrModules || !qrModuleText) {
        std::free(qrModules);
        std::free(qrModuleText);
        ESP_LOGE(TAG, "WeChat QR response buffers allocation failed");
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "QR response out of memory");
    }
    // qrcode is only the server-side polling key. qrcode_img_content (exposed
    // here as qr_data_url) is the actual URL encoded by WeChat's QR image.
    // Encode that URL locally so the setup phone does not need to load the
    // remote image while connected to the device AP.
    const char* qrPayload = status.qr_data_url;
    const int qrSize = qrPayload[0]
        ? Stickmon::QrCodeGen::encode(qrPayload, qrModules,
                                      matrixBytes)
        : 0;
    if (qrSize > 0) {
        const size_t moduleCount = static_cast<size_t>(qrSize) * qrSize;
        for (size_t i = 0; i < moduleCount; ++i) {
            qrModuleText[i] = qrModules[i] ? '1' : '0';
        }
        ESP_LOGI(TAG, "WeChat QR matrix encoded size=%d modules=%u payload_len=%u",
                 qrSize, static_cast<unsigned>(moduleCount),
                 static_cast<unsigned>(std::strlen(qrPayload)));
    } else if (qrPayload[0]) {
        ESP_LOGW(TAG, "WeChat QR matrix encoding failed payload_len=%u",
                 static_cast<unsigned>(std::strlen(qrPayload)));
    }
    ESP_LOGI(TAG, "WeChat status response active=%d completed=%d persisted=%d phase=%s qr=%d",
             status.active, status.completed, status.persisted,
             status.status[0] ? status.status : "idle",
             qrPayload[0] != '\0');
    cJSON* object = cJSON_CreateObject();
    if (!object) {
        std::free(qrModules);
        std::free(qrModuleText);
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Out of memory");
    }
    cJSON_AddBoolToObject(object, "ok", true);
    cJSON_AddBoolToObject(object, "active", status.active);
    cJSON_AddBoolToObject(object, "completed", status.completed);
    cJSON_AddBoolToObject(object, "persisted", status.persisted);
    cJSON_AddStringToObject(object, "status", status.status);
    cJSON_AddStringToObject(object, "message", status.message);
    cJSON_AddStringToObject(object, "qr_data_url", status.qr_data_url);
    cJSON_AddNumberToObject(object, "qr_size", qrSize);
    cJSON_AddStringToObject(object, "qr_modules", qrModuleText);
    cJSON_AddStringToObject(object, "account_id", status.account_id);
    esp_err_t result = sendJsonResponse(request, object);
    std::free(qrModules);
    std::free(qrModuleText);
    return result;
}

esp_err_t sendWechatHttpError(httpd_req_t* request, const char* action,
                              esp_err_t result) {
    char message[160] = {};
    std::snprintf(message, sizeof(message), "%s: %s", action,
                  esp_err_to_name(result));
    httpd_resp_set_status(request, "500 Internal Server Error");
    httpd_resp_set_type(request, "text/plain; charset=utf-8");
    return httpd_resp_sendstr(request, message);
}

esp_err_t setupWechatLoginStartHandler(httpd_req_t* request) {
    ESP_LOGI(TAG, "HTTP WeChat login start request; sta_connected=%d",
             wifiConnected());
    ClawRuntime::instance().logf(ClawStatusLog::Level::INFO,
                                 "收到微信二维码生成请求");
    if (!wifiConnected()) {
        ESP_LOGW(TAG, "WeChat login start rejected: Wi-Fi is not connected");
        ClawRuntime::instance().logf(ClawStatusLog::Level::WARN,
                                     "微信二维码请求失败：Wi-Fi 未连接");
        httpd_resp_set_status(request, "503 Service Unavailable");
        return httpd_resp_sendstr(
            request, "Wi-Fi is not connected; save Wi-Fi settings and retry");
    }
    cap_im_wechat_qr_login_status_t status{};
    esp_err_t result = cap_im_wechat_qr_login_start(nullptr, false);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "WeChat QR login start failed: %s",
                 esp_err_to_name(result));
        ClawRuntime::instance().logf(ClawStatusLog::Level::ERROR,
                                     "微信二维码生成失败：%s",
                                     esp_err_to_name(result));
        return sendWechatHttpError(request, "Failed to start WeChat login", result);
    }
    result = cap_im_wechat_qr_login_get_status(&status);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "WeChat QR status after start failed: %s",
                 esp_err_to_name(result));
        ClawRuntime::instance().logf(ClawStatusLog::Level::ERROR,
                                     "微信二维码状态读取失败：%s",
                                     esp_err_to_name(result));
        return sendWechatHttpError(request, "Failed to read WeChat login status", result);
    }
    return writeWechatQrStatus(request, status);
}

esp_err_t setupWechatLoginStatusHandler(httpd_req_t* request) {
    ESP_LOGI(TAG, "HTTP WeChat login status request; sta_connected=%d",
             wifiConnected());
    cap_im_wechat_qr_login_status_t status{};
    esp_err_t result = cap_im_wechat_qr_login_get_status(&status);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "WeChat QR status request failed: %s",
                 esp_err_to_name(result));
        ClawRuntime::instance().logf(ClawStatusLog::Level::ERROR,
                                     "微信状态读取失败：%s",
                                     esp_err_to_name(result));
        return sendWechatHttpError(request, "Failed to read WeChat login status", result);
    }
    return writeWechatQrStatus(request, status);
}

esp_err_t setupWechatLoginCancelHandler(httpd_req_t* request) {
    ESP_LOGI(TAG, "HTTP WeChat login cancel request");
    ClawRuntime::instance().logf(ClawStatusLog::Level::INFO,
                                 "收到微信登录取消请求");
    esp_err_t result = cap_im_wechat_qr_login_cancel();
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "WeChat QR login cancel failed: %s",
                 esp_err_to_name(result));
        ClawRuntime::instance().logf(ClawStatusLog::Level::ERROR,
                                     "微信登录取消失败：%s",
                                     esp_err_to_name(result));
        return sendWechatHttpError(request, "Failed to cancel WeChat login", result);
    }
    cap_im_wechat_qr_login_status_t status{};
    result = cap_im_wechat_qr_login_get_status(&status);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "WeChat QR status after cancel failed: %s",
                 esp_err_to_name(result));
        ClawRuntime::instance().logf(ClawStatusLog::Level::ERROR,
                                     "取消后微信状态读取失败：%s",
                                     esp_err_to_name(result));
        return sendWechatHttpError(request, "Failed to read WeChat login status", result);
    }
    return writeWechatQrStatus(request, status);
}

esp_err_t setupWechatLoginPersistHandler(httpd_req_t* request) {
    ESP_LOGI(TAG, "HTTP WeChat login persist request");
    ClawRuntime::instance().logf(ClawStatusLog::Level::INFO,
                                 "收到微信登录保存请求");
    // This handler runs on the small HTTP server task. Keep the QR status and
    // full setup snapshot on the heap instead of consuming several KB of task
    // stack while NVS and logging calls are active.
    auto* status = static_cast<cap_im_wechat_qr_login_status_t*>(
        std::calloc(1, sizeof(cap_im_wechat_qr_login_status_t)));
    if (!status) {
        ESP_LOGE(TAG, "WeChat persist status allocation failed");
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Out of memory");
    }
    esp_err_t result = cap_im_wechat_qr_login_get_status(status);
    if (result != ESP_OK || !status->completed || !status->token[0]) {
        ESP_LOGW(TAG, "WeChat persist rejected: status_result=%s completed=%d token=%d",
                 esp_err_to_name(result), status->completed, status->token[0] != '\0');
        ClawRuntime::instance().logf(ClawStatusLog::Level::WARN,
                                     "微信登录尚未完成，不能保存");
        std::free(status);
        httpd_resp_set_status(request, "400 Bad Request");
        return httpd_resp_sendstr(request, result == ESP_OK
                                      ? "WeChat login is not complete: ESP_ERR_INVALID_STATE"
                                      : "WeChat login status unavailable");
    }

    auto* values = static_cast<SetupValues*>(std::calloc(1, sizeof(SetupValues)));
    if (!values) {
        std::free(status);
        ESP_LOGE(TAG, "WeChat persist setup allocation failed");
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Out of memory");
    }
    loadSetupValues(*values);
    copyText(values->wechatToken, sizeof(values->wechatToken), status->token);
    if (status->base_url[0]) {
        copyText(values->wechatBaseUrl, sizeof(values->wechatBaseUrl), status->base_url);
    }
    if (status->account_id[0]) {
        copyText(values->wechatAccountId, sizeof(values->wechatAccountId), status->account_id);
    }
    ESP_LOGI(TAG, "WeChat persist saving token_len=%u account_len=%u",
             static_cast<unsigned>(std::strlen(values->wechatToken)),
             static_cast<unsigned>(std::strlen(values->wechatAccountId)));
    if (!saveSetupValues(*values)) {
        ESP_LOGE(TAG, "WeChat credentials save failed");
        ClawRuntime::instance().logf(ClawStatusLog::Level::ERROR,
                                     "微信凭据保存失败");
        std::free(values);
        std::free(status);
        return sendWechatHttpError(request, "Failed to save WeChat credentials",
                                   ESP_FAIL);
    }
    cap_im_wechat_client_config_t wechatConfig{};
    wechatConfig.token = values->wechatToken;
    wechatConfig.base_url = values->wechatBaseUrl;
    wechatConfig.cdn_base_url = values->wechatCdnUrl;
    wechatConfig.account_id = values->wechatAccountId;
    result = cap_im_wechat_set_client_config(&wechatConfig);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "WeChat credentials apply failed: %s",
                 esp_err_to_name(result));
        ClawRuntime::instance().logf(ClawStatusLog::Level::ERROR,
                                     "微信凭据应用失败：%s",
                                     esp_err_to_name(result));
        std::free(values);
        std::free(status);
        return sendWechatHttpError(request, "Failed to apply WeChat credentials", result);
    }
    result = cap_im_wechat_qr_login_mark_persisted();
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "WeChat QR login persist finalization failed: %s",
                 esp_err_to_name(result));
        ClawRuntime::instance().logf(ClawStatusLog::Level::ERROR,
                                     "微信登录完成状态保存失败：%s",
                                     esp_err_to_name(result));
        std::free(values);
        std::free(status);
        return sendWechatHttpError(request, "Failed to finalize WeChat login", result);
    }

    cJSON* object = cJSON_CreateObject();
    if (!object) {
        std::free(values);
        std::free(status);
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Out of memory");
    }
    cJSON_AddBoolToObject(object, "ok", true);
    cJSON_AddBoolToObject(object, "persisted", true);
    cJSON_AddStringToObject(object, "message", "微信登录已保存，请重启设备使远程聊天生效。");
    std::free(values);
    std::free(status);
    return sendJsonResponse(request, object);
}
#endif

esp_err_t setupSaveHandler(httpd_req_t* request) {
    char* body = nullptr;
    size_t length = 0;
    if (!readRequestBody(request, &body, &length)) {
        ESP_LOGE(TAG, "配置保存请求体无效或过大 content_len=%u",
                 request ? static_cast<unsigned>(request->content_len) : 0);
        ClawRuntime::instance().logf(ClawStatusLog::Level::ERROR,
                                     "配置保存失败：请求体无效或过大");
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                   "Invalid request body");
    }
    ESP_LOGI(TAG, "配置保存请求 body_len=%u", static_cast<unsigned>(length));
    SetupValues values;
    loadSetupValues(values);
    // Snapshot the channel list before parsing so channels dropped by the
    // page can be detected (and their credentials wiped) after the save.
    char previousChannels[sizeof(values.imChannels)] = {};
    copyText(previousChannels, sizeof(previousChannels), values.imChannels);
    bool parsed = parseSetupForm(body, length, values);
    const char* failedKey = nullptr;
    bool saved = parsed && saveSetupValues(values, &failedKey);
    bool removedBeforeRetry = false;
    if (parsed && !saved &&
        hasRemovedImChannels(previousChannels, values.imChannels)) {
        // A full/fragmented NVS may reject the new channel blob while the
        // deleted channel still occupies entries. Free those entries and make
        // one retry; this path is only entered after the first save failed.
        ClawRuntime::instance().logf(
            ClawStatusLog::Level::WARN,
            "配置首次保存失败，清理已删除渠道后重试");
        deleteRemovedImChannels(previousChannels, values.imChannels);
        removedBeforeRetry = true;
        failedKey = nullptr;
        saved = saveSetupValues(values, &failedKey);
    }
    ESP_LOGI(TAG,
             "配置保存解析=%d channels=%s feishu_app_id_len=%u feishu_secret_len=%u",
             parsed, values.imChannels[0] ? values.imChannels : "(none)",
             static_cast<unsigned>(std::strlen(values.feishuAppId)),
             static_cast<unsigned>(std::strlen(values.feishuAppSecret)));
    std::free(body);
    if (!parsed) {
        ClawRuntime::instance().logf(ClawStatusLog::Level::ERROR,
                                     "配置保存失败：表单解析错误");
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                   "Invalid form data");
    }
    if (!saved) {
        char detail[96] = {};
        std::snprintf(detail, sizeof(detail), "配置保存失败：%s",
                      failedKey ? failedKey : "未知字段");
        ClawRuntime::instance().logf(ClawStatusLog::Level::ERROR, "%s", detail);
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   detail);
    }
    if (!removedBeforeRetry) {
        deleteRemovedImChannels(previousChannels, values.imChannels);
    }
    ClawRuntime::instance().logf(ClawStatusLog::Level::OK,
                                 "配置已保存，重启后生效");
    if (values.wifiSsid[0]) {
        // Keep the AP alive while the newly supplied STA credentials connect,
        // so the user can continue directly to the WeChat QR login section.
        startWifi(values.wifiSsid, values.wifiPassword,
                  ClawRuntime::instance().setupPortalActive());
    }
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    return httpd_resp_sendstr(request,
                              "<meta charset=utf-8><p>配置已保存。请重启设备后生效。</p>"
                              "<p><a href=/>返回配置页</a></p>");
}

const httpd_uri_t SETUP_PAGE_URI = {"/", HTTP_GET, &setupPageHandler,
                                    nullptr};
const httpd_uri_t SETUP_CONFIG_URI = {"/api/config", HTTP_GET,
                                      &setupConfigHandler, nullptr};
const httpd_uri_t SETUP_STATUS_URI = {"/api/status", HTTP_GET,
                                      &setupStatusHandler, nullptr};
const httpd_uri_t SETUP_LOG_URI = {"/api/log", HTTP_GET, &setupLogHandler,
                                   nullptr};
const httpd_uri_t SETUP_CLAW_CLEAR_URI = {"/api/claw/sessions/clear", HTTP_POST,
                                          &setupClawClearSessionsHandler, nullptr};
const httpd_uri_t SETUP_SAVE_URI = {"/api/save", HTTP_POST, &setupSaveHandler,
                                    nullptr};
#if CONFIG_APP_CLAW_CAP_IM_WECHAT
const httpd_uri_t SETUP_WECHAT_START_URI = {"/api/wechat/login/start", HTTP_POST,
                                            &setupWechatLoginStartHandler, nullptr};
const httpd_uri_t SETUP_WECHAT_STATUS_URI = {"/api/wechat/login/status", HTTP_GET,
                                             &setupWechatLoginStatusHandler, nullptr};
const httpd_uri_t SETUP_WECHAT_CANCEL_URI = {"/api/wechat/login/cancel", HTTP_POST,
                                             &setupWechatLoginCancelHandler, nullptr};
const httpd_uri_t SETUP_WECHAT_PERSIST_URI = {"/api/wechat/login/persist", HTTP_POST,
                                              &setupWechatLoginPersistHandler, nullptr};
#endif

void wifiEventHandler(void*, esp_event_base_t eventBase, int32_t eventId,
                      void* eventData) {
    if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_DISCONNECTED) {
        portENTER_CRITICAL(&s_wifiMux);
        s_wifiConnected = false;
        portEXIT_CRITICAL(&s_wifiMux);
        ClawRuntime::instance().logf(ClawStatusLog::Level::WARN,
                                     "Wi-Fi 断开，重连中");
        esp_wifi_connect();
    } else if (eventBase == IP_EVENT && eventId == IP_EVENT_STA_GOT_IP) {
        portENTER_CRITICAL(&s_wifiMux);
        s_wifiConnected = true;
        portEXIT_CRITICAL(&s_wifiMux);
        const auto* gotIp =
            static_cast<const ip_event_got_ip_t*>(eventData);
        char ip[16] = {};
        if (gotIp) esp_ip4addr_ntoa(&gotIp->ip_info.ip, ip, sizeof(ip));
        ClawRuntime::instance().noteStaIp(ip);
    } else if (eventBase == WIFI_EVENT &&
               eventId == WIFI_EVENT_AP_STACONNECTED) {
        ClawRuntime::instance().notePhoneJoined(true);
    } else if (eventBase == WIFI_EVENT &&
               eventId == WIFI_EVENT_AP_STADISCONNECTED) {
        ClawRuntime::instance().notePhoneJoined(false);
    }
}

bool wifiConnected() {
    bool connected = false;
    portENTER_CRITICAL(&s_wifiMux);
    connected = s_wifiConnected;
    portEXIT_CRITICAL(&s_wifiMux);
    return connected;
}

bool startWifi(const char* ssid, const char* password, bool keepAp) {
    if (!ssid || !ssid[0]) return false;
    esp_err_t result = esp_netif_init();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) return false;
    result = esp_event_loop_create_default();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) return false;
    if (!s_staNetif) s_staNetif = esp_netif_create_default_wifi_sta();
    if (!s_staNetif) return false;

    if (!s_wifiEventsRegistered) {
        if (esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                       &wifiEventHandler, nullptr) != ESP_OK ||
            esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                       &wifiEventHandler, nullptr) != ESP_OK) {
            return false;
        }
        s_wifiEventsRegistered = true;
    }

    if (!s_wifiInitialized) {
        wifi_init_config_t initConfig = WIFI_INIT_CONFIG_DEFAULT();
        result = esp_wifi_init(&initConfig);
        if (result != ESP_OK && result != ESP_ERR_WIFI_STATE) return false;
        s_wifiInitialized = true;
    }

    wifi_config_t config{};
    copyText(reinterpret_cast<char*>(config.sta.ssid), sizeof(config.sta.ssid), ssid);
    copyText(reinterpret_cast<char*>(config.sta.password), sizeof(config.sta.password), password);
    wifi_mode_t mode = keepAp && s_apNetif ? WIFI_MODE_APSTA : WIFI_MODE_STA;
    if (esp_wifi_set_mode(mode) != ESP_OK ||
        esp_wifi_set_config(WIFI_IF_STA, &config) != ESP_OK) return false;

    result = esp_wifi_start();
    if (result != ESP_OK && result != ESP_ERR_WIFI_STATE) return false;
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    return esp_wifi_connect() == ESP_OK || wifiConnected();
}

bool startSetupHttpServer() {
    if (s_setupServer) return true;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    // Leave headroom for the four WeChat login routes. Some ESP-IDF builds
    // reserve an additional internal handler, so using exactly the visible
    // route count can make the last registrations fail.
    config.max_uri_handlers = 16;
    config.stack_size = 6144;
    // The default HTTP server accepts seven clients and uses three additional
    // internal sockets. With LWIP_MAX_SOCKETS=10 that can starve outbound
    // WeChat/Coze HTTPS requests while the setup page is polling status and
    // logs. Three browser sessions are sufficient for this single-page portal
    // and leave descriptors available for long polling and QR login.
    config.max_open_sockets = 3;
    config.lru_purge_enable = true;
    esp_err_t result = httpd_start(&s_setupServer, &config);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Setup HTTP server start failed: %s",
                 esp_err_to_name(result));
        s_setupServer = nullptr;
        return false;
    }
    ESP_LOGI(TAG,
             "Setup HTTP server started on port %u, max_uri_handlers=%u, "
             "max_open_sockets=%u, lwip_max_sockets=%u",
             static_cast<unsigned>(config.server_port),
             static_cast<unsigned>(config.max_uri_handlers),
             static_cast<unsigned>(config.max_open_sockets),
             static_cast<unsigned>(CONFIG_LWIP_MAX_SOCKETS));
    if (httpd_register_uri_handler(s_setupServer, &SETUP_PAGE_URI) != ESP_OK ||
        httpd_register_uri_handler(s_setupServer, &SETUP_CONFIG_URI) != ESP_OK ||
        httpd_register_uri_handler(s_setupServer, &SETUP_STATUS_URI) != ESP_OK ||
        httpd_register_uri_handler(s_setupServer, &SETUP_LOG_URI) != ESP_OK ||
        httpd_register_uri_handler(s_setupServer, &SETUP_CLAW_CLEAR_URI) != ESP_OK ||
        httpd_register_uri_handler(s_setupServer, &SETUP_SAVE_URI) != ESP_OK
#if CONFIG_APP_CLAW_CAP_IM_WECHAT
        || httpd_register_uri_handler(s_setupServer, &SETUP_WECHAT_START_URI) != ESP_OK ||
        httpd_register_uri_handler(s_setupServer, &SETUP_WECHAT_STATUS_URI) != ESP_OK ||
        httpd_register_uri_handler(s_setupServer, &SETUP_WECHAT_CANCEL_URI) != ESP_OK ||
        httpd_register_uri_handler(s_setupServer, &SETUP_WECHAT_PERSIST_URI) != ESP_OK
#endif
    ) {
        ESP_LOGE(TAG, "Setup HTTP route registration failed");
        ClawRuntime::instance().logf(ClawStatusLog::Level::ERROR,
                                     "管理页接口注册失败");
        httpd_stop(s_setupServer);
        s_setupServer = nullptr;
        return false;
    }
    ESP_LOGI(TAG, "Setup HTTP routes registered: config/status/log/save/sessions/wechat");
    return true;
}

bool mountBrainFs() {
    static bool mounted = false;
    if (mounted) return true;
    esp_vfs_fat_mount_config_t config = {
        .format_if_mount_failed = true,
        .max_files = 12,
        .allocation_unit_size = 4096,
        .disk_status_check_enable = false,
        .use_one_fat = false,
    };
    esp_err_t result = esp_vfs_fat_spiflash_mount_rw_wl(
        BRAINFS_BASE, BRAINFS_PARTITION, &config, &s_brainWlHandle);
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "brainfs mount failed: %s", esp_err_to_name(result));
        return false;
    }
    mounted = true;
    return true;
}

void appendImGroup(char* output, size_t outputSize, const char* channels,
                   const char* channelId, const char* group) {
    if (!imChannelListed(channels, channelId)) return;
    const size_t used = std::strlen(output);
    std::snprintf(output + used, outputSize - used, used > 0 ? ",%s" : "%s",
                  group);
}

void fillClawConfig(const char* apiKey, const char* baseUrl, const char* model,
                    const char* backend, const SetupValues& values,
                    app_claw_config_t& config) {
    std::memset(&config, 0, sizeof(config));
    copyText(config.llm_api_key, sizeof(config.llm_api_key), apiKey);
    copyText(config.llm_backend_type, sizeof(config.llm_backend_type), backend);
    copyText(config.llm_model, sizeof(config.llm_model), model);
    copyText(config.llm_base_url, sizeof(config.llm_base_url), baseUrl);
    copyText(config.llm_auth_type, sizeof(config.llm_auth_type), "bearer");
    copyText(config.llm_timeout_ms, sizeof(config.llm_timeout_ms), "30000");
    copyText(config.llm_max_tokens, sizeof(config.llm_max_tokens), "512");
    copyText(config.llm_default_image_max_bytes,
             sizeof(config.llm_default_image_max_bytes), "0");
    copyText(config.llm_max_tokens_field, sizeof(config.llm_max_tokens_field), "max_tokens");
    // Coze does not expose OpenAI tool calls directly. The bundled Coze
    // backend translates the small JSON action envelope into local tools.
    copyText(config.llm_supports_tools, sizeof(config.llm_supports_tools), "true");
    copyText(config.llm_supports_vision, sizeof(config.llm_supports_vision), "false");
    copyText(config.llm_image_remote_url_only, sizeof(config.llm_image_remote_url_only), "false");
    copyText(config.tg_bot_token, sizeof(config.tg_bot_token), values.telegramToken);
    copyText(config.qq_app_id, sizeof(config.qq_app_id), values.qqAppId);
    copyText(config.qq_app_secret, sizeof(config.qq_app_secret), values.qqAppSecret);
    copyText(config.feishu_app_id, sizeof(config.feishu_app_id), values.feishuAppId);
    copyText(config.feishu_app_secret, sizeof(config.feishu_app_secret), values.feishuAppSecret);
    copyText(config.wechat_token, sizeof(config.wechat_token), values.wechatToken);
    copyText(config.wechat_base_url, sizeof(config.wechat_base_url),
             values.wechatBaseUrl[0] ? values.wechatBaseUrl
                                     : "https://ilinkai.weixin.qq.com");
    copyText(config.wechat_cdn_base_url, sizeof(config.wechat_cdn_base_url),
             values.wechatCdnUrl[0] ? values.wechatCdnUrl
                                   : "https://novac2c.cdn.weixin.qq.com/c2c");
    copyText(config.wechat_account_id, sizeof(config.wechat_account_id),
             values.wechatAccountId[0] ? values.wechatAccountId : "default");
    // Enable exactly the IM caps that are both compiled in and enabled on
    // the setup page; each cap tolerates missing credentials.
    char imGroups[96] = {};
#if CONFIG_APP_CLAW_CAP_IM_WECHAT
    appendImGroup(imGroups, sizeof(imGroups), values.imChannels, "wechat",
                  "cap_im_wechat");
#endif
#if CONFIG_APP_CLAW_CAP_IM_QQ
    appendImGroup(imGroups, sizeof(imGroups), values.imChannels, "qq",
                  "cap_im_qq");
#endif
#if CONFIG_APP_CLAW_CAP_IM_FEISHU
    appendImGroup(imGroups, sizeof(imGroups), values.imChannels, "feishu",
                  "cap_im_feishu");
#endif
#if CONFIG_APP_CLAW_CAP_IM_TG
    appendImGroup(imGroups, sizeof(imGroups), values.imChannels, "telegram",
                  "cap_im_tg");
#endif
    std::snprintf(config.enabled_cap_groups, sizeof(config.enabled_cap_groups),
                  "%s%scap_skill,stickmon", imGroups,
                  imGroups[0] ? "," : "");
    copyText(config.llm_visible_cap_groups, sizeof(config.llm_visible_cap_groups),
             "stickmon");
}

}  // namespace

// ESP-Claw is kept as an external source tree.  This small C ABI hook lets
// its C components mirror important remote-chat events into StickMon's
// management-page ring buffer without coupling them to the C++ runtime type.
extern "C" void stickmon_claw_status_log(int level, const char* text) {
    ClawStatusLog::Level mapped = ClawStatusLog::Level::INFO;
    switch (level) {
    case 1: mapped = ClawStatusLog::Level::OK; break;
    case 2: mapped = ClawStatusLog::Level::WARN; break;
    case 3: mapped = ClawStatusLog::Level::ERROR; break;
    default: break;
    }
    ClawRuntime::instance().logf(mapped, "%s", text ? text : "");
}

ClawRuntime& ClawRuntime::instance() {
    static ClawRuntime runtime;
    return runtime;
}

void ClawRuntime::beginTaskEntry(void* context) {
    auto* runtime = static_cast<ClawRuntime*>(context);
    runtime->beginTask();
    vTaskDeleteWithCaps(nullptr);
}

void ClawRuntime::beginTask() {
    const uint32_t startedAt = Platform::clock().millis();
    ESP_LOGI(TAG, "ESP-Claw background initialization started");
    const bool result = begin();
    ESP_LOGI(TAG, "ESP-Claw init task stack-free=%u",
             static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
    const uint32_t elapsed = Platform::clock().millis() - startedAt;
    portENTER_CRITICAL(&mux_);
    initializing_ = false;
    portEXIT_CRITICAL(&mux_);
    if (started()) {
        ESP_LOGI(TAG, "ESP-Claw background initialization completed in %lu ms",
                 static_cast<unsigned long>(elapsed));
    } else if (result) {
        ESP_LOGI(TAG, "ESP-Claw background initialization skipped in %lu ms",
                 static_cast<unsigned long>(elapsed));
    } else {
        ESP_LOGW(TAG, "ESP-Claw background initialization failed in %lu ms",
                 static_cast<unsigned long>(elapsed));
    }
}

void ClawRuntime::beginAsync() {
#if !CONFIG_STICKMON_CLAW_ENABLE
    return;
#else
    portENTER_CRITICAL(&mux_);
    if (started_ || initializing_) {
        portEXIT_CRITICAL(&mux_);
        return;
    }
    initializing_ = true;
    lastPlayerActivityMs_ = Platform::clock().millis();
    nextAutonomyAtMs_ = lastPlayerActivityMs_ + AUTONOMY_IDLE_MS;
    autonomyCooldownMs_ = AUTONOMY_COOLDOWN_MS;
    activeAutonomyCooldownMs_ = AUTONOMY_COOLDOWN_MS;
    playerActivityKnown_ = true;
    portEXIT_CRITICAL(&mux_);

    BaseType_t created = xTaskCreateWithCaps(
        &ClawRuntime::beginTaskEntry, "stickmon_claw_init",
        CLAW_INIT_TASK_STACK_SIZE, this, 1, nullptr,
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (created != pdPASS) {
        portENTER_CRITICAL(&mux_);
        initializing_ = false;
        portEXIT_CRITICAL(&mux_);
        ESP_LOGE(TAG, "Failed to create ESP-Claw init task");
    }
#endif
}

bool ClawRuntime::started() const {
    bool value = false;
    portENTER_CRITICAL(&mux_);
    value = started_;
    portEXIT_CRITICAL(&mux_);
    return value;
}

bool ClawRuntime::networkConnected() const {
    bool value = false;
    portENTER_CRITICAL(&mux_);
    value = networkConnected_;
    portEXIT_CRITICAL(&mux_);
    return value;
}

bool ClawRuntime::autonomyActive() const {
    bool active = false;
    portENTER_CRITICAL(&mux_);
    active = autonomySubmitInFlight_ || activeAutonomyRequestId_ != 0;
    portEXIT_CRITICAL(&mux_);
    return active;
}

bool ClawRuntime::begin() {
#if !CONFIG_STICKMON_CLAW_ENABLE
    ESP_LOGI(TAG, "ESP-Claw disabled at build time");
    return true;
#else
    portENTER_CRITICAL(&mux_);
    if (started_) {
        portEXIT_CRITICAL(&mux_);
        return true;
    }
    if (!playerActivityKnown_) {
        lastPlayerActivityMs_ = Platform::clock().millis();
        nextAutonomyAtMs_ = lastPlayerActivityMs_ + AUTONOMY_IDLE_MS;
        autonomyCooldownMs_ = AUTONOMY_COOLDOWN_MS;
        activeAutonomyCooldownMs_ = AUTONOMY_COOLDOWN_MS;
        playerActivityKnown_ = true;
    }
    portEXIT_CRITICAL(&mux_);

    char ssid[65] = {};
    char password[65] = {};
    char cozeToken[321] = {};
    char cozeBotId[65] = {};
    char cozeBaseUrl[321] = {};
    SetupValues setupValues;
    readText(NVS_WIFI_NAMESPACE, "ssid", ssid, sizeof(ssid));
    readText(NVS_WIFI_NAMESPACE, "password", password, sizeof(password));
    readText(NVS_CLAW_NAMESPACE, "coze_token", cozeToken, sizeof(cozeToken));
    readText(NVS_CLAW_NAMESPACE, "coze_bot_id", cozeBotId, sizeof(cozeBotId));
    readText(NVS_CLAW_NAMESPACE, "coze_base_url", cozeBaseUrl, sizeof(cozeBaseUrl));
    loadSetupValues(setupValues);

    if (!ssid[0] || !cozeToken[0]) {
        ESP_LOGI(TAG, "ESP-Claw waiting for NVS credentials (wifi, coze_token)");
        logf(ClawStatusLog::Level::INFO, "ESP-Claw 等待凭据配置");
        return true;
    }
    if (!cozeBaseUrl[0]) copyText(cozeBaseUrl, sizeof(cozeBaseUrl), "https://api.coze.cn");
    if (!cozeBotId[0]) copyText(cozeBotId, sizeof(cozeBotId), "7679649015453597711");

    logf(ClawStatusLog::Level::INFO, "连接 Wi-Fi %s", ssid);
    if (!mountBrainFs() || !startWifi(ssid, password, false)) {
        ESP_LOGW(TAG, "ESP-Claw network/storage prerequisites unavailable");
        logf(ClawStatusLog::Level::ERROR, "Wi-Fi 或存储不可用，ESP-Claw 未启动");
        return true;
    }

    // esp_wifi_connect() only starts the connection attempt.  Starting the
    // WeChat poller and the Agent core before DHCP completes makes their first
    // DNS request fail and can prevent the root agent from being created.
    const uint32_t wifiWaitStarted = Platform::clock().millis();
    while (!wifiConnected() &&
           (Platform::clock().millis() - wifiWaitStarted) <
               CLAW_WIFI_CONNECT_TIMEOUT_MS) {
        vTaskDelay(pdMS_TO_TICKS(250));
    }
    if (!wifiConnected()) {
        ESP_LOGW(TAG, "Wi-Fi did not obtain an IP before ESP-Claw startup");
        logf(ClawStatusLog::Level::ERROR,
             "Wi-Fi 在 %lu 秒内未获取地址，ESP-Claw 未启动",
             static_cast<unsigned long>(CLAW_WIFI_CONNECT_TIMEOUT_MS / 1000));
        return true;
    }
    ESP_LOGI(TAG, "Wi-Fi ready; starting ESP-Claw services");
    logf(ClawStatusLog::Level::OK, "Wi-Fi 已就绪，启动 ESP-Claw 服务");
    if (claw_paths_set(CLAW_PATH_DATA, BRAINFS_BASE) != ESP_OK ||
        claw_paths_set(CLAW_PATH_SYSTEM, "/assets") != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set ESP-Claw storage paths");
        logf(ClawStatusLog::Level::ERROR, "ESP-Claw 存储路径设置失败");
        return false;
    }
    if (!BrainBridge::instance().begin()) {
        ESP_LOGE(TAG, "Failed to start BrainBridge");
        logf(ClawStatusLog::Level::ERROR, "BrainBridge 启动失败");
        return false;
    }
    esp_err_t result = registerStickmonCapability();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to register StickMon capability: %s", esp_err_to_name(result));
        logf(ClawStatusLog::Level::ERROR, "StickMon 能力注册失败");
        return false;
    }

    app_claw_config_t config{};
    fillClawConfig(cozeToken, cozeBaseUrl, cozeBotId, "coze", setupValues, config);
    const claw_core_context_provider_t roleProvider = {
        "StickMon Lead Profile",
        &BrainBridge::collectContext,
        &BrainBridge::instance(),
        0,
    };
    result = app_claw_add_base_context_provider(&roleProvider);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register StickMon role context: %s",
                 esp_err_to_name(result));
        logf(ClawStatusLog::Level::ERROR, "首位精灵角色上下文注册失败");
        return false;
    }
    result = app_claw_start(&config);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "ESP-Claw start failed: %s", esp_err_to_name(result));
        logf(ClawStatusLog::Level::ERROR, "ESP-Claw 启动失败 %s",
             esp_err_to_name(result));
        return false;
    }
    // The dynamic role provider is collected for every request, so swapping
    // team[0] changes the speaking pet on the next turn without restarting.
    portENTER_CRITICAL(&mux_);
    started_ = true;
    networkConnected_ = wifiConnected();
    portEXIT_CRITICAL(&mux_);
    ESP_LOGI(TAG, "ESP-Claw remote chat started");
    logf(ClawStatusLog::Level::OK, "ESP-Claw 已启动");
    return true;
#endif
}

void ClawRuntime::notePlayerActivity(uint32_t nowMs) {
    uint32_t requestId = 0;
    portENTER_CRITICAL(&mux_);
    lastPlayerActivityMs_ = nowMs;
    playerActivityKnown_ = true;
    ++activityGeneration_;
    if (activityGeneration_ == 0) activityGeneration_ = 1;
    nextAutonomyAtMs_ = nowMs + AUTONOMY_IDLE_MS;
    autonomyCooldownMs_ = AUTONOMY_COOLDOWN_MS;
    activeAutonomyCooldownMs_ = AUTONOMY_COOLDOWN_MS;
    requestId = activeAutonomyRequestId_;
    activeAutonomyRequestId_ = 0;
    portEXIT_CRITICAL(&mux_);

    BrainBridge::instance().setAgentAllowed(false);
    BrainBridge::instance().setRuntimeState(true, 0, false);

    if (requestId != 0) {
        claw_core_handle_t core = app_claw_get_core();
        if (core) {
            esp_err_t err = claw_core_cancel_request(core, requestId);
            if (err != ESP_OK && err != ESP_ERR_NOT_FOUND) {
                ESP_LOGW(TAG, "Failed to cancel autonomous request %lu: %s",
                         static_cast<unsigned long>(requestId),
                         esp_err_to_name(err));
            }
        }
    }
}

bool ClawRuntime::playerActive(uint32_t nowMs) const {
    bool known = false;
    uint32_t lastActivity = 0;
    portENTER_CRITICAL(&mux_);
    known = playerActivityKnown_;
    lastActivity = lastPlayerActivityMs_;
    portEXIT_CRITICAL(&mux_);
    return known && (nowMs - lastActivity) < PLAYER_ACTIVE_GRACE_MS;
}

uint32_t ClawRuntime::idleSeconds(uint32_t nowMs) const {
    bool known = false;
    uint32_t lastActivity = 0;
    portENTER_CRITICAL(&mux_);
    known = playerActivityKnown_;
    lastActivity = lastPlayerActivityMs_;
    portEXIT_CRITICAL(&mux_);
    return known ? (nowMs - lastActivity) / 1000U : 0;
}

bool ClawRuntime::agentAllowed(uint32_t nowMs) const {
    bool running = false;
    bool connected = false;
    bool known = false;
    uint32_t lastActivity = 0;
    portENTER_CRITICAL(&mux_);
    running = started_;
    connected = networkConnected_;
    known = playerActivityKnown_;
    lastActivity = lastPlayerActivityMs_;
    portEXIT_CRITICAL(&mux_);
    if (!running || !connected || !known ||
        (nowMs - lastActivity) < AUTONOMY_IDLE_MS) {
        return false;
    }
    BrainBridge::Snapshot current{};
    if (!BrainBridge::instance().snapshot(current)) return false;
    return !current.actionLocked;
}

bool ClawRuntime::startSetupPortalImpl() {
#if !CONFIG_STICKMON_CLAW_ENABLE
    return false;
#else
    if (setupPortalActive()) return true;

    esp_err_t result = esp_netif_init();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) return false;
    result = esp_event_loop_create_default();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) return false;
    if (!s_apNetif) s_apNetif = esp_netif_create_default_wifi_ap();
    if (!s_apNetif) return false;

    if (!s_wifiEventsRegistered) {
        if (esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                       &wifiEventHandler, nullptr) != ESP_OK ||
            esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                       &wifiEventHandler, nullptr) != ESP_OK) {
            return false;
        }
        s_wifiEventsRegistered = true;
    }
    if (!s_wifiInitialized) {
        wifi_init_config_t initConfig = WIFI_INIT_CONFIG_DEFAULT();
        result = esp_wifi_init(&initConfig);
        if (result != ESP_OK && result != ESP_ERR_WIFI_STATE) return false;
        s_wifiInitialized = true;
    }

    uint8_t mac[6] = {};
    if (esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP) != ESP_OK) return false;
    char setupSsid[sizeof(setupSsid_)] = {};
    char setupPassword[sizeof(setupPassword_)] = {};
    std::snprintf(setupSsid, sizeof(setupSsid), "StickMon-%02X%02X", mac[4],
                  mac[5]);
    // Random per portal session; the alphabet avoids ambiguous glyphs
    // (0/O, 1/I/L) and Wi-Fi QR reserved characters (\ ; , : ").
    constexpr char SETUP_PASSWORD_ALPHABET[] =
        "ABCDEFGHJKMNPQRSTUVWXYZ23456789";
    for (int i = 0; i < 8; ++i) {
        setupPassword[i] = SETUP_PASSWORD_ALPHABET[
            esp_random() % (sizeof(SETUP_PASSWORD_ALPHABET) - 1)];
    }

    wifi_config_t config{};
    copyText(reinterpret_cast<char*>(config.ap.ssid), sizeof(config.ap.ssid),
             setupSsid);
    copyText(reinterpret_cast<char*>(config.ap.password),
             sizeof(config.ap.password), setupPassword);
    config.ap.ssid_len = std::strlen(setupSsid);
    config.ap.channel = 1;
    config.ap.max_connection = 2;
    config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_mode_t mode = s_staNetif ? WIFI_MODE_APSTA : WIFI_MODE_AP;
    ESP_LOGI(TAG, "Starting setup portal Wi-Fi mode=%s sta_netif=%d",
             mode == WIFI_MODE_APSTA ? "APSTA" : "AP",
             s_staNetif != nullptr);
    if (esp_wifi_set_mode(mode) != ESP_OK ||
        esp_wifi_set_config(WIFI_IF_AP, &config) != ESP_OK) {
        return false;
    }
    result = esp_wifi_start();
    if (result != ESP_OK && result != ESP_ERR_WIFI_STATE) return false;
    if (s_staNetif) esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

    char setupIp[sizeof(setupIp_)] = "192.168.4.1";
    esp_netif_ip_info_t ipInfo{};
    if (esp_netif_get_ip_info(s_apNetif, &ipInfo) == ESP_OK &&
        ipInfo.ip.addr != 0) {
        esp_ip4addr_ntoa(&ipInfo.ip, setupIp, sizeof(setupIp));
    }
    if (!startSetupHttpServer()) return false;
    portENTER_CRITICAL(&mux_);
    copyText(setupSsid_, sizeof(setupSsid_), setupSsid);
    copyText(setupPassword_, sizeof(setupPassword_), setupPassword);
    copyText(setupIp_, sizeof(setupIp_), setupIp);
    setupPortalActive_ = true;
    portEXIT_CRITICAL(&mux_);
    ESP_LOGI(TAG, "Setup portal ready: SSID=%s IP=%s", setupSsid, setupIp);
    logf(ClawStatusLog::Level::OK, "管理热点已启动 %s，模式 %s",
         setupSsid, mode == WIFI_MODE_APSTA ? "APSTA" : "AP");
    return true;
#endif
}

bool ClawRuntime::startSetupPortal() {
    if (setupPortalActive()) return true;
    if (!startSetupPortalImpl()) {
        logf(ClawStatusLog::Level::ERROR, "热点启动失败");
        return false;
    }
    char ssid[33] = {};
    setupPortalInfo(ssid, sizeof(ssid), nullptr, 0, nullptr, 0);
    logf(ClawStatusLog::Level::OK, "热点已启动 %s", ssid);
    return true;
}

void ClawRuntime::stopSetupPortal() {
#if !CONFIG_STICKMON_CLAW_ENABLE
    return;
#else
    if (s_setupServer) {
        httpd_stop(s_setupServer);
        s_setupServer = nullptr;
    }
    if (!setupPortalActive()) return;

    // Keep the STA connection used by ESP-Claw when it is already running.
    // Only remove the AP interface and leave the Wi-Fi driver alive.
    if (s_wifiInitialized) {
        wifi_mode_t mode = started_ && s_staNetif ? WIFI_MODE_STA
                                                   : WIFI_MODE_NULL;
        esp_wifi_set_mode(mode);
        if (mode == WIFI_MODE_NULL) esp_wifi_stop();
    }
    portENTER_CRITICAL(&mux_);
    setupPortalActive_ = false;
    phoneJoined_ = false;
    setupSsid_[0] = '\0';
    setupPassword_[0] = '\0';
    setupIp_[0] = '\0';
    portEXIT_CRITICAL(&mux_);
    logf(ClawStatusLog::Level::INFO, "热点已关闭");
#endif
}

bool ClawRuntime::setupPortalActive() const {
    bool value = false;
    portENTER_CRITICAL(&mux_);
    value = setupPortalActive_;
    portEXIT_CRITICAL(&mux_);
    return value;
}

bool ClawRuntime::setupPortalInfo(char* ssid, size_t ssidSize,
                                  char* password, size_t passwordSize,
                                  char* ip, size_t ipSize) const {
    portENTER_CRITICAL(&mux_);
    if (!setupPortalActive_) {
        portEXIT_CRITICAL(&mux_);
        return false;
    }
    copyText(ssid, ssidSize, setupSsid_);
    copyText(password, passwordSize, setupPassword_);
    copyText(ip, ipSize, setupIp_);
    portEXIT_CRITICAL(&mux_);
    return true;
}

void ClawRuntime::logf(ClawStatusLog::Level level, const char* format, ...) {
    if (!format) return;
    char text[160] = {};
    va_list args;
    va_start(args, format);
    std::vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    portENTER_CRITICAL(&mux_);
    log_.append(level, Platform::clock().millis(), text);
    portEXIT_CRITICAL(&mux_);
}

uint32_t ClawRuntime::logGeneration() const {
    portENTER_CRITICAL(&mux_);
    const uint32_t generation = log_.generation();
    portEXIT_CRITICAL(&mux_);
    return generation;
}

size_t ClawRuntime::copyLog(ClawStatusLog::Entry* out, size_t maxCount) const {
    portENTER_CRITICAL(&mux_);
    const size_t count = log_.copyRecent(out, maxCount);
    portEXIT_CRITICAL(&mux_);
    return count;
}

size_t ClawRuntime::copyLogSince(uint32_t since, ClawStatusLog::Entry* out,
                                 size_t maxCount,
                                 uint32_t* generationOut) const {
    portENTER_CRITICAL(&mux_);
    const size_t count = log_.copySince(since, out, maxCount);
    if (generationOut) *generationOut = log_.generation();
    portEXIT_CRITICAL(&mux_);
    return count;
}

bool ClawRuntime::setupPhoneJoined() const {
    portENTER_CRITICAL(&mux_);
    const bool joined = phoneJoined_;
    portEXIT_CRITICAL(&mux_);
    return joined;
}

void ClawRuntime::statusSnapshot(ClawStatus& status) const {
    portENTER_CRITICAL(&mux_);
    status.staConnected = networkConnected_;
    copyText(status.staIp, sizeof(status.staIp), staIp_);
    status.portalActive = setupPortalActive_;
    status.phoneJoined = phoneJoined_;
    status.clawStarted = started_;
    // Only the short ASCII phase token is exposed to the UI; the full
    // status/message strings stay in the log.
    copyText(status.wechatPhase, sizeof(status.wechatPhase), lastWechatStatus_);
    status.wechatPersisted = lastWechatPersisted_;
    portEXIT_CRITICAL(&mux_);
}

void ClawRuntime::noteStaIp(const char* ip) {
    portENTER_CRITICAL(&mux_);
    copyText(staIp_, sizeof(staIp_), ip);
    portEXIT_CRITICAL(&mux_);
    logf(ClawStatusLog::Level::OK, "Wi-Fi 已连接 %s", ip && ip[0] ? ip : "");
}

void ClawRuntime::notePhoneJoined(bool joined) {
    bool changed = false;
    portENTER_CRITICAL(&mux_);
    if (phoneJoined_ != joined) {
        phoneJoined_ = joined;
        changed = true;
    }
    portEXIT_CRITICAL(&mux_);
    if (changed) {
        logf(joined ? ClawStatusLog::Level::OK : ClawStatusLog::Level::INFO,
             joined ? "手机已加入热点" : "手机已离开热点");
    }
}

void ClawRuntime::update(uint32_t nowMs) {
    portENTER_CRITICAL(&mux_);
    networkConnected_ = wifiConnected();
    bool running = started_;
#if CONFIG_APP_CLAW_CAP_IM_WECHAT
    bool portal = setupPortalActive_;
    uint32_t lastWechatPoll = lastWechatPollMs_;
#endif
    portEXIT_CRITICAL(&mux_);

#if CONFIG_APP_CLAW_CAP_IM_WECHAT
    // The QR login flow runs on the setup portal even before ESP-Claw itself
    // has started, so poll it independently of the autonomy loop below.
    if (portal && (nowMs - lastWechatPoll) >= 500) {
        pollWechatLogin(nowMs);
    }
#endif
    if (!running) return;

    BrainBridge& bridge = BrainBridge::instance();
    bridge.update(nowMs);

    claw_core_handle_t core = app_claw_get_core();
    claw_core_agent_loop_phase_t phase = core
        ? claw_core_get_agent_loop_phase(core)
        : CLAW_CORE_AGENT_LOOP_PHASE_IDLE;
    portENTER_CRITICAL(&mux_);
    if (activeAutonomyRequestId_ != 0 &&
        phase == CLAW_CORE_AGENT_LOOP_PHASE_IDLE) {
        activeAutonomyRequestId_ = 0;
        if (nextAutonomyAtMs_ < nowMs + activeAutonomyCooldownMs_) {
            nextAutonomyAtMs_ = nowMs + activeAutonomyCooldownMs_;
        }
        activeAutonomyCooldownMs_ = AUTONOMY_COOLDOWN_MS;
    }
    portEXIT_CRITICAL(&mux_);

    bool allowed = agentAllowed(nowMs);
    bridge.setAgentAllowed(allowed);
    bridge.setRuntimeState(playerActive(nowMs), idleSeconds(nowMs), allowed);
    if (!allowed || !core || phase != CLAW_CORE_AGENT_LOOP_PHASE_IDLE) {
        return;
    }

    static const char AUTONOMY_PROMPT[] =
        "这是设备空闲后的自主生活回合。先调用 stickmon_get_context，确认 agent_allowed=true 且 action_locked=false。"
        "根据真实状态只选择一个低风险行为：吃饭、购买普通食物、回家、探险或邀请好友；如果没有必要就等待。"
        "这是机器执行回合，不是聊天回合。决定执行动作时必须只输出一个有效的 action JSON 并调用对应本地能力；禁止用自然语言宣布‘已经出发’或‘已经完成’。"
        "设备只有收到本地能力返回 ok=true 才会执行动作；没有工具调用就代表没有执行，不能声称动作已完成。"
        "玩家一旦操作，立即停止，不要执行后续动作。不要使用 stickmon_say，不要把设备屏幕当作聊天渠道。"
        "只有探险完成、重要发现、危险状态、资源不足、好友加入或动作失败时，才给主人发送一条简短微信通知。";
    claw_core_request_t request{};
    char autonomyChatId[96] = {};
    const bool canNotify = BrainBridge::instance().latestChatId(
        autonomyChatId, sizeof(autonomyChatId));
    request.flags = canNotify ? CLAW_CORE_REQUEST_FLAG_PUBLISH_OUT_MESSAGE
                              : CLAW_CORE_REQUEST_FLAG_SKIP_RESPONSE_QUEUE;
    request.session_id = "stickmon-autonomy";
    request.user_text = AUTONOMY_PROMPT;
    // Keep the source channel as "autonomy" so capability calls cannot be
    // mistaken for player activity. The explicit target routes notifications
    // back through the WeChat binding when a chat has already been seen.
    request.source_channel = "autonomy";
    request.source_chat_id = canNotify ? autonomyChatId : nullptr;
    request.target_channel = canNotify ? "wechat" : nullptr;
    request.target_chat_id = canNotify ? autonomyChatId : nullptr;
    request.source_cap = "stickmon_runtime";

    // Reserve a request under the lock, but submit it outside the lock. The
    // ESP-Claw queue operation can block briefly; keeping the spinlock held
    // would delay a touch or inbound chat from cancelling autonomy.
    uint32_t submitGeneration = 0;
    uint32_t requestId = 0;
    portENTER_CRITICAL(&mux_);
    bool stillAllowed = started_ && networkConnected_ && playerActivityKnown_ &&
        (nowMs - lastPlayerActivityMs_) >= AUTONOMY_IDLE_MS &&
        activeAutonomyRequestId_ == 0 &&
        !autonomySubmitInFlight_ &&
        static_cast<int32_t>(nowMs - nextAutonomyAtMs_) >= 0;
    if (stillAllowed) {
        requestId = nextRequestId_++;
        if (nextRequestId_ == 0) nextRequestId_ = 1;
        request.request_id = requestId;
        submitGeneration = activityGeneration_;
        autonomySubmitInFlight_ = true;
    }
    portEXIT_CRITICAL(&mux_);

    if (requestId == 0) return;

    esp_err_t err = claw_core_submit(core, &request, 100);
    bool cancelSubmittedRequest = false;
    bool requestWasStillAllowed = false;
    uint32_t submittedCooldownMs = 0;
    portENTER_CRITICAL(&mux_);
    autonomySubmitInFlight_ = false;
    requestWasStillAllowed = started_ && networkConnected_ &&
        playerActivityKnown_ &&
        (nowMs - lastPlayerActivityMs_) >= AUTONOMY_IDLE_MS &&
        activityGeneration_ == submitGeneration;
    if (err == ESP_OK && requestWasStillAllowed &&
        activeAutonomyRequestId_ == 0) {
        activeAutonomyRequestId_ = requestId;
        activeAutonomyCooldownMs_ = autonomyCooldownMs_;
        submittedCooldownMs = activeAutonomyCooldownMs_;
        nextAutonomyAtMs_ = nowMs + activeAutonomyCooldownMs_;
        if (autonomyCooldownMs_ >= AUTONOMY_MAX_COOLDOWN_MS / 2) {
            autonomyCooldownMs_ = AUTONOMY_MAX_COOLDOWN_MS;
        } else {
            autonomyCooldownMs_ *= 2;
        }
    } else if (err == ESP_OK) {
        // A player event raced the queue submission. Cancel after releasing
        // the lock so the event handler can never be blocked by the API call.
        cancelSubmittedRequest = true;
    } else if (stillAllowed) {
        nextAutonomyAtMs_ = nowMs + 10000;
    }
    portEXIT_CRITICAL(&mux_);

    if (cancelSubmittedRequest) {
        esp_err_t cancelErr = claw_core_cancel_request(core, requestId);
        if (cancelErr != ESP_OK && cancelErr != ESP_ERR_NOT_FOUND) {
            ESP_LOGW(TAG, "Failed to cancel raced autonomous request %lu: %s",
                     static_cast<unsigned long>(requestId),
                     esp_err_to_name(cancelErr));
        }
    }
    if (err == ESP_OK) {
        ESP_LOGI(TAG,
                 "Autonomous life request submitted: %lu; next cooldown=%lu s",
                 static_cast<unsigned long>(requestId),
                 static_cast<unsigned long>(submittedCooldownMs / 1000));
    } else if (stillAllowed) {
        ESP_LOGW(TAG, "Autonomous life request rejected: %s", esp_err_to_name(err));
    }
}

#if CONFIG_APP_CLAW_CAP_IM_WECHAT
void ClawRuntime::pollWechatLogin(uint32_t nowMs) {
    portENTER_CRITICAL(&mux_);
    lastWechatPollMs_ = nowMs;
    portEXIT_CRITICAL(&mux_);

    cap_im_wechat_qr_login_status_t status{};
    if (cap_im_wechat_qr_login_get_status(&status) != ESP_OK) return;

    char previous[sizeof(lastWechatStatus_)] = {};
    bool previousPersisted = false;
    portENTER_CRITICAL(&mux_);
    copyText(previous, sizeof(previous), lastWechatStatus_);
    previousPersisted = lastWechatPersisted_;
    copyText(lastWechatStatus_, sizeof(lastWechatStatus_), status.status);
    lastWechatPersisted_ = status.persisted;
    portEXIT_CRITICAL(&mux_);

    if (std::strcmp(previous, status.status) != 0) {
        if (std::strcmp(status.status, "waiting_scan") == 0) {
            logf(ClawStatusLog::Level::INFO, "微信二维码已生成，等待扫码");
        } else if (std::strcmp(status.status, "scanned") == 0) {
            logf(ClawStatusLog::Level::INFO, "已扫码，等待确认");
        } else if (std::strcmp(status.status, "confirmed") == 0) {
            logf(ClawStatusLog::Level::OK, "微信登录成功");
        } else if (std::strcmp(status.status, "expired") == 0) {
            logf(ClawStatusLog::Level::WARN, "二维码已过期");
        } else if (std::strcmp(status.status, "cancelled") == 0) {
            logf(ClawStatusLog::Level::INFO, "微信登录已取消");
        } else if (std::strcmp(status.status, "error") == 0) {
            logf(ClawStatusLog::Level::ERROR, "微信登录失败");
        }
    }
    if (status.persisted && !previousPersisted) {
        logf(ClawStatusLog::Level::OK, "微信登录已保存");
    }
}
#else
void ClawRuntime::pollWechatLogin(uint32_t) {}
#endif

}  // namespace Stickmon
