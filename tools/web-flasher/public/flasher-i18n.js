'use strict';
/**
 * StickMon 刷机站文案常量 —— 玩家可见文案集中在此，方便后续多语言。
 *
 * 结构说明：
 * - dialog：esp-web-tools 弹窗（英文 → 中文）对照表。
 *   exact    文本节点去除首尾空白后的精确匹配；值为空字符串表示「隐藏该入口
 *            所在的整个列表项 / 按钮」（如 Logs & Console、Erase User Data）。
 *   patterns 按顺序尝试的正则替换，用于带动态内容的进度 / 错误消息；
 *            同时作用于文本节点与 ewt-page-message 的 label 属性。
 * - debug：页面自带串口调试面板的全部文案。
 *
 * 接入多语言时，按 locale 提供同名结构替换 window.FLASHER_I18N 即可。
 */
window.FLASHER_I18N = {
  dialog: {
    exact: {
      // ---- 隐藏的入口（值为空串 = 整块隐藏）----
      'Logs & Console': '', // 调试入口已移到页面右下角，弹窗内不再提供
      'Erase User Data': '', // 本站永不提供擦除入口
      // ---- 按钮 / 菜单 ----
      'Install': '安装',
      'install': '安装',
      'Update': '更新',
      'update to': '更新到',
      'Back': '返回',
      'Next': 'ok', // 安装成功页唯一的按钮，点击直接关闭弹窗（见 localizeDialog）
      'Close': '关闭',
      'Continue': '继续',
      'Skip': '跳过',
      'Connect': '连接',
      'Reset Device': '重启设备',
      'Download Logs': '下载日志',
      'Connect to Wi-Fi': '连接 Wi-Fi',
      'Add to Home Assistant': '添加到 Home Assistant',
      'Fund Development': '资助开发',
      'Visit Device': '访问设备',
      'Erase device': '擦除设备',
      // ---- 标题 / 进度 ----
      'Connecting': '正在连接',
      'Confirm Installation': '确认安装',
      'Installing': '正在安装',
      'Installation failed': '安装失败',
      'Erasing': '正在擦除',
      'Preparing installation': '正在准备安装',
      'Wrapping up': '即将完成',
      'This will take': '大约需要',
      'a minute': '1 分钟',
      '2 minutes': '2 分钟',
      'Keep this page visible to prevent slow down': '安装期间请保持本页面可见，避免速度下降',
      'Do you want to': '确认要',
      '?': '吗？',
      'Your device is running': '当前设备运行的是',
      'Installation complete!': '安装完成！',
      'All done!': '全部完成！',
    },
    patterns: [
      [/^Initialized\. Found (.+)$/, '已识别芯片：$1'],
      [/^Downloading firmware .+ failed: (\d+)$/, '固件下载失败（HTTP $1）'],
      [/^Your (.+) board is not supported\.$/, '不支持当前设备（$1）'],
      [/^Failed to initialize\..*$/, '初始化失败。请复位设备，或按住 BOOT 键再点安装。'],
      [/^Failed to download manifest$/, '固件清单下载失败'],
      [/^Serial port is not (readable\/writable|ready)\..*$/, '串口被占用或未就绪，请关闭其他占用串口的程序后重试。'],
      [/^Writing progress: (\d+)%$/, '写入进度：$1%'],
    ],
  },
  debug: {
    entry: '调试',
    title: '串口调试控制台',
    connect: '连接设备',
    disconnect: '断开连接',
    clear: '清空',
    close: '关闭',
    statusIdle: '未连接',
    statusConnected: '已连接 · 115200',
    disconnected: '—— 已断开 ——',
    connectFailed: '连接失败：',
  },
};
