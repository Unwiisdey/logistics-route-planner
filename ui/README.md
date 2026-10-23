# 正式本地 Web 界面

运行根目录 Start-UI.cmd 或 python scripts/start_ui.py。

完整启动、插单、任务路径、耗时规则与测试见 [Web 界面手册](../docs/ui-guide.md)。

网页生成到被忽略的 dist/。桥接程序由根 CMake 构建为 logistics_ui_bridge，与 CLI 共用 logistics_core。历史回放由 replay.html 保留，算法不依赖界面。
