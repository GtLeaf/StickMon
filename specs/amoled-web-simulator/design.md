# 技术设计

- `WebPlatform` 从既有 Web 分支引入当前源码树，支持目标尺寸、存档前缀和受控时钟。目标构建使用明确源码清单、AMOLED UI 公共清单及隔离的对象缓存。
- `main_amoled.cpp` 绑定真实 AmoledApp/Canvas565；纯 TouchEvent 头与硬件驱动分离。浏览器管理帧循环，通过导出函数传入逻辑时间和触摸，复用脏区域传屏。
- 页面使用 Pointer Events 和指针捕获。浏览器取消触摸调用独立 cancelTouch，不进入 UP 的业务动作路径。
- localStorage 使用设备前缀；Web Locks 保障同一设备目标的单写实例；所有写入失败向 SaveManager 返回失败。快照格式共用现有 schema 3 / save v5。
- iframe 消息均校验 origin/source。支持 ready、resize、input、close/closed、pause/resume，关闭有界等待确认，保存失败给出提示。
- 刷机站复用组合与模拟器表，后端校验产物完整性。启动补登记已有 V2 组合；管理端支持新增登记，保留管理员的停用选择。
- 发布器在临时目录完成全目标验证，先复制哈希资源、最后切换入口。自动发布直接构建同一提交的两个目标，避免再依赖长驻 Web 分支合并。

使用与验证结果见 `doc/微雪V2-网页模拟器.md`。
