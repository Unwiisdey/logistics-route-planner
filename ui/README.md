# 正式双车 Web 界面

运行根目录 Start-UI.cmd 或 python scripts/start_ui.py，访问 http://127.0.0.1:8765/ 。默认 V01/V02 每车 40 kg，使用已批准双车 Demo 的视觉和交互；不再依赖外部 Demo 目录。

- fleet.html / fleet.css / live.js：正式页面源。
- bridge.cpp：JSON 适配，持有真实 FleetSimulation，从核心 remaining_actions 读取任务。
- build_live.py：生成被忽略的 dist/ 与历史单车 replay.html。
- server.py：本机 HTTP、来源检查与序列化进程通信。
- verify_live.cjs / verify_hubs.cjs：正式浏览器交互与中转站验证；verify_demo.cjs 验证历史回放。

完整启动与操作见 [UI 手册](../docs/ui-guide.md)。普通运行无需 Node.js、npm 或第三方前端库，Playwright 仅用于可选浏览器测试。模型和算法不依赖具体 GUI。
