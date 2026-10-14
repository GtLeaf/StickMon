#include "OtaRuntime.h"
#include "OtaBuild.h"
#include "AmoledPlatform.h"
#include "core/FontResource.h"
#include "core/ResourceFS.h"
#include "core/ResourcePack.h"
#include "core/SaveCodec.h"
#include "core/UiStrings.h"
#include "presentation/Canvas565.h"
#include "ui/UiCommon.h"
#include <atomic>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <ctime>
#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_random.h"
#include "esp_sntp.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mbedtls/base64.h"
#include "mbedtls/pk.h"
#include "mbedtls/sha256.h"
#include "lwip/sockets.h"
#include "nvs.h"
#include "nvs_flash.h"

namespace AmoledV2::Ota {
static_assert(OtaBuild::saveSchema == SaveCodec::SCHEMA_VERSION &&
              OtaBuild::saveVersion == Game::SAVE_VERSION,
              "Application-only OTA requires unchanged save compatibility");
namespace {
SemaphoreHandle_t mutex;
std::atomic<bool> redraw{true}, busy{false}, cancel{false}, committing{false};
std::atomic<int> command{0}; // 1=check, 2=install
std::atomic<bool> connected{false};
httpd_handle_t portal;
esp_netif_t* setupAp;
esp_timer_handle_t guard;
char ssid[33], password[17], token[33];
char router[33], routerPassword[65], endpoint[201];
char status[128] = "用手机连接设备热点配置网络";
unsigned progress = 0;
bool available = false;
std::string downloadUrl;
char releaseVersion[32] = {};
uint32_t releaseBuild = 0;
size_t releaseSize = 0;
char releaseHash[65] = {};

struct Lock {
    Lock() { xSemaphoreTake(mutex, portMAX_DELAY); }
    ~Lock() { xSemaphoreGive(mutex); }
};
void message(const char* value, unsigned percent = 0) {
    Lock lock;
    if (!std::strcmp(status, value) && progress == percent) return;
    std::snprintf(status, sizeof(status), "%s", value);
    progress = percent;
    redraw = true;
}
void restart(void*) { esp_restart(); }
void hex(const unsigned char* digest, char output[65]) {
    for (unsigned i = 0; i < 32; ++i) std::sprintf(output + i * 2, "%02x", digest[i]);
}
bool layoutHealthy() {
    auto a = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
    auto b = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr);
    auto data = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, nullptr);
    auto resources = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "resources");
    return a && b && data && resources && a->address == 0x20000 && a->size == 0x300000 &&
           b->address == 0x320000 && b->size == 0x300000 && data->address == 0xf000 &&
           data->size == 0x2000 && resources->address == 0x620000 && resources->size == 0x400000;
}
bool resourcesHealthy() {
    if (!layoutHealthy() || !ResourceFS::ins().begin()) return false;
    unsigned char buffer[2048], digest[32];
    for (const auto& entry : OtaBuild::resources) {
        std::string path = std::string("/") + entry.path;
        auto file = ResourceFS::ins().open(path.c_str());
        if (!file) return false;
        mbedtls_sha256_context sha;
        mbedtls_sha256_init(&sha);
        mbedtls_sha256_starts(&sha, 0);
        size_t count;
        while ((count = file.read(buffer, sizeof(buffer))) > 0) {
            mbedtls_sha256_update(&sha, buffer, count);
            vTaskDelay(1);
        }
        mbedtls_sha256_finish(&sha, digest);
        mbedtls_sha256_free(&sha);
        file.close();
        char actual[65]; hex(digest, actual);
        if (std::strcmp(actual, entry.sha256)) return false;
    }
    return true;
}
bool saveHealthy() {
    auto snapshot = std::make_unique<SaveCodec::Snapshot>();
    uint8_t bytes[SaveCodec::MAX_ENCODED_BYTES];
    bool found = false, valid = false;
    for (auto key : {"state_a", "state_b"}) {
        size_t size = Platform::blobs().blobSize("stickmon", key);
        if (!size) continue;
        found = true;
        if (size <= sizeof(bytes) && Platform::blobs().readBlob("stickmon", key, bytes, size)) {
            valid |= SaveCodec::decode(bytes, size, *snapshot);
        }
    }
    // The first OTA baseline always writes a modern snapshot before restarting.
    return valid || (!found && !Platform::blobs().blobSize("stickmon", "state"));
}
bool https(const std::string& url) {
    return url.size() < 2048 && url.rfind("https://", 0) == 0 &&
           url.find_first_of("\r\n") == std::string::npos;
}
const char* string(cJSON* root, const char* name) {
    auto item = cJSON_GetObjectItemCaseSensitive(root, name);
    return cJSON_IsString(item) ? item->valuestring : "";
}
bool number(cJSON* root, const char* name, uint32_t& result) {
    auto item = cJSON_GetObjectItemCaseSensitive(root, name);
    if (!cJSON_IsNumber(item) || item->valuedouble < 0 || item->valuedouble > UINT32_MAX) return false;
    result = static_cast<uint32_t>(item->valuedouble);
    return result == item->valuedouble;
}
bool equal(cJSON* root, const char* name, const char* value) {
    return std::strcmp(string(root, name), value) == 0;
}
bool value(cJSON* root, const char* name, uint32_t expected) {
    uint32_t result;
    return number(root, name, result) && result == expected;
}
using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
Json parse(const std::string& body) { return Json(cJSON_Parse(body.c_str()), cJSON_Delete); }

esp_http_client_handle_t openHttp(const std::string& url) {
    if (!https(url)) return nullptr;
    esp_http_client_config_t cfg{};
    cfg.url = url.c_str();
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.timeout_ms = 15000;
    cfg.buffer_size = 2048;
    cfg.disable_auto_redirect = true; // Only explicitly validated HTTPS destinations.
    auto client = esp_http_client_init(&cfg);
    if (!client) return nullptr;
    if (esp_http_client_open(client, 0) != ESP_OK || esp_http_client_fetch_headers(client) < 0 ||
        esp_http_client_get_status_code(client) != 200) {
        esp_http_client_cleanup(client);
        return nullptr;
    }
    return client;
}
bool readManifest(const std::string& url, std::string& body) {
    auto client = openHttp(url);
    if (!client) return false;
    char chunk[512]; int count;
    while (!cancel && (count = esp_http_client_read(client, chunk, sizeof(chunk))) > 0) {
        body.append(chunk, count);
        if (body.size() > 8192) break;
    }
    bool ok = !cancel && body.size() <= 8192 && esp_http_client_is_complete_data_received(client);
    esp_http_client_cleanup(client);
    return ok;
}
bool verify(cJSON* envelope) {
    auto payload64 = string(envelope, "payload"), sig64 = string(envelope, "signature");
    unsigned char payload[2048], signature[80], digest[32]; size_t payloadSize, sigSize;
    if (mbedtls_base64_decode(payload, sizeof(payload) - 1, &payloadSize,
            reinterpret_cast<const unsigned char*>(payload64), std::strlen(payload64)) ||
        mbedtls_base64_decode(signature, sizeof(signature), &sigSize,
            reinterpret_cast<const unsigned char*>(sig64), std::strlen(sig64))) return false;
    payload[payloadSize] = 0;
    mbedtls_sha256(payload, payloadSize, digest, 0);
    mbedtls_pk_context key;
    mbedtls_pk_init(&key);
    int result = mbedtls_pk_parse_public_key(&key,
        reinterpret_cast<const unsigned char*>(OtaBuild::publicKey), sizeof(OtaBuild::publicKey));
    if (!result) result = mbedtls_pk_verify(&key, MBEDTLS_MD_SHA256, digest, sizeof(digest), signature, sigSize);
    mbedtls_pk_free(&key);
    if (result) return false;
    auto meta = parse(std::string(reinterpret_cast<char*>(payload), payloadSize));
    uint32_t build, size;
    auto partition = esp_ota_get_next_update_partition(nullptr);
    if (!meta || !partition || !cJSON_IsFalse(cJSON_GetObjectItem(meta.get(), "debug")) || !value(meta.get(), "schema", 1) ||
        !equal(meta.get(), "target", OtaBuild::target) || !equal(meta.get(), "flavor", OtaBuild::flavor) ||
        !equal(meta.get(), "layout", OtaBuild::layout) ||
        !equal(meta.get(), "resourceSha256", OtaBuild::resourceSha256) ||
        !value(meta.get(), "saveSchema", SaveCodec::SCHEMA_VERSION) ||
        !value(meta.get(), "saveVersion", Game::SAVE_VERSION) ||
        !number(meta.get(), "build", build) || build <= OtaBuild::build ||
        !number(meta.get(), "size", size) || !size || size > partition->size ||
        std::strlen(string(meta.get(), "sha256")) != 64 ||
        !std::strlen(string(meta.get(), "version")) || std::strlen(string(meta.get(), "version")) > 31) return false;
    std::string url = string(envelope, "url");
    if (!https(url)) return false;
    Lock lock;
    releaseBuild = build; releaseSize = size;
    std::snprintf(releaseHash, sizeof(releaseHash), "%s", string(meta.get(), "sha256"));
    std::snprintf(releaseVersion, sizeof(releaseVersion), "%s", string(meta.get(), "version"));
    downloadUrl = url;
    return true;
}
bool connectNetwork() {
    if (connected) return true;
    wifi_config_t sta{};
    {
        Lock lock;
        std::memcpy(sta.sta.ssid, router, std::strlen(router));
        std::memcpy(sta.sta.password, routerPassword, std::strlen(routerPassword));
    }
    if (!sta.sta.ssid[0]) { message("请先设置 2.4 GHz Wi-Fi"); return false; }
    esp_wifi_disconnect();
    if (esp_wifi_set_config(WIFI_IF_STA, &sta) != ESP_OK || esp_wifi_connect() != ESP_OK) {
        message("Wi-Fi 连接失败，请检查配置"); return false;
    }
    message("正在连接 Wi-Fi");
    for (unsigned i = 0; i < 200 && !connected && !cancel; ++i) vTaskDelay(pdMS_TO_TICKS(100));
    if (!connected || cancel) { message("连接超时，请检查 Wi-Fi 名称与密码"); return false; }
    if (!esp_sntp_enabled()) {
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, const_cast<char*>("pool.ntp.org"));
        esp_sntp_setservername(1, const_cast<char*>("ntp.aliyun.com"));
        esp_sntp_init();
    }
    for (unsigned i = 0; i < 200 && std::time(nullptr) < 1700000000 && !cancel; ++i) vTaskDelay(pdMS_TO_TICKS(100));
    if (std::time(nullptr) < 1700000000 || cancel) { message("网络校时失败，请稍后重试"); return false; }
    return true;
}
bool check() {
    { Lock lock; available = false; }
    if (!connectNetwork()) return false;
    message("正在检查更新");
    if (!resourcesHealthy()) { message("素材版本不同，请通过 USB 安装完整包"); return false; }
    std::string base;
    { Lock lock; base = endpoint; }
    if (!https(base)) { message("请先填写刷机站的 HTTPS 地址"); return false; }
    while (!base.empty() && base.back() == '/') base.pop_back();
    std::string query = base + "/api/ota/v1/latest?target=" + OtaBuild::target + "&flavor=" + OtaBuild::flavor + "&build=" + std::to_string(OtaBuild::build);
    std::string response;
    if (!readManifest(query, response)) { message("暂时无法访问更新站点，请稍后重试"); return false; }
    auto envelope = parse(response);
    if (envelope && cJSON_IsTrue(cJSON_GetObjectItem(envelope.get(), "noUpdate"))) {
        message("当前没有可用的新版本"); return false;
    }
    if (!envelope || !verify(envelope.get())) { message("更新验证未通过，或此版本需要 USB 安装"); return false; }
    { Lock lock; available = true; }
    message("新版本已就绪，请确认安装");
    return true;
}
bool install() {
    // Refresh a short-lived object-store URL and re-verify the release.
    uint32_t expected = releaseBuild;
    if (!check() || expected != releaseBuild) { message("发布版本已改变，请重新检查并确认"); return false; }
    auto partition = esp_ota_get_next_update_partition(nullptr);
    auto client = openHttp(downloadUrl);
    if (!partition || !client) { message("暂时无法下载，请稍后重试"); return false; }
    esp_ota_handle_t handle = 0;
    esp_err_t result = esp_ota_begin(partition, releaseSize, &handle);
    unsigned char buffer[2048], digest[32]; size_t total = 0;
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha); mbedtls_sha256_starts(&sha, 0);
    const int64_t deadline = esp_timer_get_time() + 600000000;
    while (result == ESP_OK && !cancel) {
        if (esp_timer_get_time() >= deadline) { result = ESP_ERR_TIMEOUT; break; }
        int count = esp_http_client_read(client, reinterpret_cast<char*>(buffer), sizeof(buffer));
        if (count == 0) break;
        if (count < 0 || total + count > releaseSize) { result = ESP_FAIL; break; }
        result = esp_ota_write(handle, buffer, count);
        mbedtls_sha256_update(&sha, buffer, count); total += count;
        message("正在下载，请保持电源连接", static_cast<unsigned>(total * 100 / releaseSize));
        vTaskDelay(1);
    }
    bool complete = esp_http_client_is_complete_data_received(client);
    esp_http_client_cleanup(client);
    mbedtls_sha256_finish(&sha, digest); mbedtls_sha256_free(&sha);
    char actual[65]; hex(digest, actual);
    if (result != ESP_OK || cancel || total != releaseSize || !complete || std::strcmp(actual, releaseHash)) {
        if (handle) esp_ota_abort(handle);
        message(cancel ? "已取消，原版本保留" : "下载或验证失败，原版本保留");
        return false;
    }
    bool cancelledBeforeCommit;
    {
        Lock lock;
        cancelledBeforeCommit = cancel;
        if (cancelledBeforeCommit) esp_ota_abort(handle);
        else {
            committing = true;
            result = esp_ota_end(handle);
            if (result == ESP_OK) result = esp_ota_set_boot_partition(partition);
        }
    }
    if (cancelledBeforeCommit) {
        message("已取消，原版本保留"); return false;
    }
    if (result != ESP_OK) {
        committing = false;
        message("程序验证失败，原版本保留"); return false;
    }
    message("更新验证成功，正在重启");
    esp_restart();
    return true;
}
void worker(void*) {
    for (;;) {
        int action = command.exchange(0);
        if (action == 1) check();
        if (action == 2) install();
        if (action) {
            if (cancel && !committing) message("已取消，原版本保留");
            busy = false; redraw = true;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
const char PAGE[] = R"html(<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>StickMon 系统更新</title><style>body{font:16px system-ui;max-width:480px;margin:24px auto;padding:16px;background:#142329;color:#eee}input,button{box-sizing:border-box;width:100%;padding:12px;margin:8px 0;font:inherit}button{background:#8ae0b5;border:0}#status{white-space:pre-wrap}label{display:block}input[type=checkbox]{width:auto}</style><h1>StickMon 系统更新</h1><p>仅适用于微雪 V2。游戏、素材和存档会保留。使用 2.4 GHz Wi-Fi。</p><form id="config"><input id="ssid" maxlength="32" placeholder="Wi-Fi 名称" required><input id="password" maxlength="64" type="password" placeholder="Wi-Fi 密码"><input id="endpoint" maxlength="200" placeholder="刷机站 HTTPS 地址" required><button>保存联网设置</button></form><button id="check">检查更新</button><p id="info"></p><label><input id="power" type="checkbox">已接通稳定电源，更新完成前保持连接</label><button id="install" disabled>安装更新</button><button id="cancel">取消下载</button><p id="status"></p><p>返回游戏请点击设备上的返回按钮。热点切换信道时手机可能暂时断开，请重新连接。</p><script>
let token=''; async function call(path,body){let r=await fetch('/api/'+path,{method:'POST',headers:{'Content-Type':'application/json','X-Ota-Token':token},body:JSON.stringify(body||{})});if(!r.ok)alert((await r.json()).error||'操作失败');}
config.onsubmit=e=>{e.preventDefault();call('config',{ssid:document.getElementById('ssid').value,password:document.getElementById('password').value,endpoint:document.getElementById('endpoint').value})};
document.getElementById('check').onclick=()=>call('check');document.getElementById('install').onclick=()=>call('install',{powerConfirmed:document.getElementById('power').checked});document.getElementById('cancel').onclick=()=>call('cancel');
async function poll(){try{let r=await fetch('/api/status',{cache:'no-store'}),s=await r.json();token=s.token;document.getElementById('status').textContent=s.status+' '+s.progress+'%';document.getElementById('info').textContent='当前 '+s.flavor+' '+s.version+(s.available?' → '+s.releaseVersion:'');document.getElementById('install').disabled=!s.available||s.busy;if(!document.getElementById('endpoint').value)document.getElementById('endpoint').value=s.endpoint;}catch{}setTimeout(poll,1500)}poll();
</script></html>)html";
bool fromSetupHotspot(httpd_req_t* req) {
    sockaddr_in local{}; socklen_t length = sizeof(local);
    esp_netif_ip_info_t ip{};
    return setupAp && esp_netif_get_ip_info(setupAp, &ip) == ESP_OK &&
           getsockname(httpd_req_to_sockfd(req), reinterpret_cast<sockaddr*>(&local), &length) == 0 &&
           local.sin_family == AF_INET && local.sin_addr.s_addr == ip.ip.addr;
}
esp_err_t forbidden(httpd_req_t* req) {
    return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Join device setup hotspot");
}
esp_err_t page(httpd_req_t* req) {
    if (!fromSetupHotspot(req)) return forbidden(req);
    httpd_resp_set_type(req, "text/html; charset=utf-8"); return httpd_resp_send(req, PAGE, HTTPD_RESP_USE_STRLEN); }
esp_err_t response(httpd_req_t* req, cJSON* body) {
    char* json = cJSON_PrintUnformatted(body);
    httpd_resp_set_type(req, "application/json"); httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    auto result = httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN); cJSON_free(json); cJSON_Delete(body); return result;
}
esp_err_t error(httpd_req_t* req, const char* message) {
    httpd_resp_set_status(req, "409 Conflict"); auto body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "error", message); return response(req, body);
}
esp_err_t state(httpd_req_t* req) {
    if (!fromSetupHotspot(req)) return forbidden(req);
    Lock lock; auto body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "token", token);
    cJSON_AddStringToObject(body, "status", status); cJSON_AddNumberToObject(body, "progress", progress);
    cJSON_AddBoolToObject(body, "busy", busy); cJSON_AddBoolToObject(body, "available", available);
    cJSON_AddStringToObject(body, "version", OtaBuild::version); cJSON_AddStringToObject(body, "flavor", OtaBuild::flavor);
    cJSON_AddStringToObject(body, "releaseVersion", releaseVersion); cJSON_AddStringToObject(body, "endpoint", endpoint);
    return response(req, body);
}
esp_err_t action(httpd_req_t* req) {
    if (!fromSetupHotspot(req)) return forbidden(req);
    char submitted[40] = {};
    if (httpd_req_get_hdr_value_str(req, "X-Ota-Token", submitted, sizeof(submitted)) != ESP_OK || std::strcmp(token, submitted))
        return error(req, "请从设备热点重新打开更新页");
    if (!std::strcmp(req->uri, "/api/cancel")) {
        Lock lock;
        if (committing) return error(req, "已完成校验，正在重启");
        cancel = true;
        return response(req, cJSON_CreateObject());
    }
    if (req->content_len <= 0 || req->content_len > 1024) return error(req, "请求过长");
    std::string input(req->content_len, '\0'); size_t received = 0;
    while (received < input.size()) {
        int count = httpd_req_recv(req, &input[received], input.size() - received);
        if (count <= 0) return ESP_FAIL;
        received += count;
    }
    auto body = parse(input); if (!body) return error(req, "配置格式错误");
    bool expected = false;
    if (!busy.compare_exchange_strong(expected, true)) return error(req, "正在操作，请稍后重试");
    if (!std::strcmp(req->uri, "/api/config")) {
        const char* name = string(body.get(), "ssid"), *pass = string(body.get(), "password"), *site = string(body.get(), "endpoint");
        if (!*name || std::strlen(name) > 32 || std::strlen(pass) > 64 || std::strlen(site) > 200 || !https(site) ||
            std::strchr(site, '?') || std::strchr(site, '#')) { busy = false; return error(req, "检查 Wi-Fi 名称、密码和 HTTPS 地址"); }
        nvs_handle_t h;
        bool ok = nvs_open("ota", NVS_READWRITE, &h) == ESP_OK;
        if (ok) {
            ok = nvs_set_str(h, "ssid", name) == ESP_OK && nvs_set_str(h, "password", pass) == ESP_OK &&
                 nvs_set_str(h, "endpoint", site) == ESP_OK && nvs_commit(h) == ESP_OK;
            nvs_close(h);
        }
        if (!ok) { busy = false; return error(req, "保存配置失败"); }
        { Lock lock; std::snprintf(router, sizeof(router), "%s", name); std::snprintf(routerPassword, sizeof(routerPassword), "%s", pass);
          std::snprintf(endpoint, sizeof(endpoint), "%s", site); available = false; }
        connected = false; esp_wifi_disconnect(); busy = false;
        message("配置已保存，可以检查更新");
    } else {
        if (!std::strcmp(req->uri, "/api/install")) {
            bool ready; { Lock lock; ready = available; }
            if (!ready || !cJSON_IsTrue(cJSON_GetObjectItem(body.get(), "powerConfirmed"))) {
                busy = false; return error(req, "先检查更新，并确认已接通电源");
            }
            cancel = false;
            command = 2;
        } else { cancel = false; command = 1; }
    }
    return response(req, cJSON_CreateObject());
}
void event(void*, esp_event_base_t base, int32_t id, void*) {
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) connected = true;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) connected = false;
}
void loadString(nvs_handle_t h, const char* key, char* output, size_t capacity) {
    size_t size = capacity; nvs_get_str(h, key, output, &size);
}
} // namespace

bool pendingVerification() {
    esp_ota_img_states_t state;
    return esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY;
}
void armBootGuard() {
    if (!pendingVerification()) return;
    AmoledPlatform::instance().setOtaWriteProtection(true);
    esp_timer_create_args_t cfg{}; cfg.callback = restart; cfg.name = "ota_boot_guard";
    ESP_ERROR_CHECK(esp_timer_create(&cfg, &guard));
    ESP_ERROR_CHECK(esp_timer_start_once(guard, 120000000));
}
bool bootResourcesHealthy() { return resourcesHealthy() && saveHealthy(); }
bool confirmBoot() {
    if (!ResourcePack::ins().active() || !FontResource::ins().loaded()) return false;
    if (pendingVerification() && esp_ota_mark_app_valid_cancel_rollback() != ESP_OK) return false;
    if (guard) { esp_timer_stop(guard); esp_timer_delete(guard); guard = nullptr; }
    AmoledPlatform::instance().setOtaWriteProtection(false);
    return true;
}
bool requestMaintenance() {
    nvs_handle_t h;
    if (nvs_open("ota", NVS_READWRITE, &h) != ESP_OK) return false;
    bool ok = nvs_set_u8(h, "maintenance", 1) == ESP_OK && nvs_commit(h) == ESP_OK;
    nvs_close(h); if (ok) esp_restart();
    return false;
}
bool consumeMaintenanceRequest() {
    if (nvs_flash_init() != ESP_OK || pendingVerification()) return false;
    nvs_handle_t h; uint8_t requested = 0;
    if (nvs_open("ota", NVS_READWRITE, &h) != ESP_OK) return false;
    nvs_get_u8(h, "maintenance", &requested);
    bool ok = nvs_erase_key(h, "maintenance") == ESP_OK && nvs_commit(h) == ESP_OK;
    nvs_close(h); return requested && ok;
}
bool beginMaintenance() {
    mutex = xSemaphoreCreateMutex(); if (!mutex) return false;
    ResourceFS::ins().begin(); ResourcePack::ins().begin(); FontResource::ins().begin();
    std::snprintf(ssid, sizeof(ssid), "StickMon-OTA-%04X", static_cast<unsigned>(esp_random() & 65535));
    std::snprintf(password, sizeof(password), "%08x%08x", static_cast<unsigned>(esp_random()), static_cast<unsigned>(esp_random()));
    std::snprintf(token, sizeof(token), "%08x%08x%08x%08x", static_cast<unsigned>(esp_random()), static_cast<unsigned>(esp_random()), static_cast<unsigned>(esp_random()), static_cast<unsigned>(esp_random()));
    std::snprintf(endpoint, sizeof(endpoint), "%s", OtaBuild::endpoint);
    nvs_handle_t h;
    if (nvs_open("ota", NVS_READONLY, &h) == ESP_OK) {
        loadString(h, "ssid", router, sizeof(router)); loadString(h, "password", routerPassword, sizeof(routerPassword));
        loadString(h, "endpoint", endpoint, sizeof(endpoint)); nvs_close(h);
    }
    if (esp_netif_init() != ESP_OK) return false;
    esp_err_t result = esp_event_loop_create_default(); if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) return false;
    esp_netif_create_default_wifi_sta(); setupAp = esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&cfg) != ESP_OK || esp_wifi_set_storage(WIFI_STORAGE_RAM) != ESP_OK) return false;
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event, nullptr);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event, nullptr);
    wifi_config_t ap{}; std::memcpy(ap.ap.ssid, ssid, std::strlen(ssid)); ap.ap.ssid_len = std::strlen(ssid);
    std::memcpy(ap.ap.password, password, std::strlen(password));
    ap.ap.authmode = WIFI_AUTH_WPA2_PSK; ap.ap.max_connection = 2; ap.ap.channel = 6;
    if (esp_wifi_set_mode(WIFI_MODE_APSTA) != ESP_OK || esp_wifi_set_config(WIFI_IF_AP, &ap) != ESP_OK || esp_wifi_start() != ESP_OK) return false;
    httpd_config_t http = HTTPD_DEFAULT_CONFIG(); http.max_uri_handlers = 8; http.stack_size = 8192;
    if (httpd_start(&portal, &http) != ESP_OK) return false;
    for (const char* uri : {"/", "/api/status", "/api/config", "/api/check", "/api/install", "/api/cancel"}) {
        httpd_uri_t route{}; route.uri = uri;
        route.method = (!std::strcmp(uri, "/") || !std::strcmp(uri, "/api/status")) ? HTTP_GET : HTTP_POST;
        route.handler = !std::strcmp(uri, "/") ? page : (!std::strcmp(uri, "/api/status") ? state : action);
        if (httpd_register_uri_handler(portal, &route) != ESP_OK) return false;
    }
    return xTaskCreate(worker, "ota_worker", 12288, nullptr, 3, nullptr) == pdPASS;
}
bool consumeRedraw() { return redraw.exchange(false); }
void render(Canvas565& canvas) {
    using namespace AmoledV1::UiCommon;
    canvas.fillRect(0, 0, 368, 448, rgb(14, 30, 36));
    drawPageHeader(canvas, Ui::Amoled::SYSTEM_UPDATE);
    text(canvas, 20, 90, Ui::Amoled::UPDATE_CONNECT, rgb(226, 238, 233));
    text(canvas, 20, 126, ssid, rgb(115, 226, 183));
    text(canvas, 20, 164, Ui::Amoled::UPDATE_PASSWORD, rgb(226, 238, 233));
    text(canvas, 20, 200, password, rgb(115, 226, 183));
    text(canvas, 20, 240, Ui::Amoled::UPDATE_OPEN, rgb(226, 238, 233));
    text(canvas, 20, 278, Ui::Amoled::UPDATE_POWER, rgb(248, 210, 105));
    { Lock lock; char line[48]; std::snprintf(line, sizeof(line), "%s %s  %u%%", OtaBuild::flavor, OtaBuild::version, progress);
      text(canvas, 20, 316, line, rgb(226, 238, 233)); }
    text(canvas, 20, 358, busy ? Ui::Amoled::UPDATE_WORKING : Ui::Amoled::UPDATE_EXIT, rgb(115, 226, 183));
}
void exitMaintenance() {
    if (busy) {
        { Lock lock; if (committing) return; cancel = true; }
        message("正在取消，请稍后再次点击返回");
    }
    else esp_restart();
}
} // namespace AmoledV2::Ota
