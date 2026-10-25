# 正式双车 UI 集成验证

2026-10-25，Windows x64 / MSVC 19.29 / C++17 Release。

## 交付范围

用户批准将独立双车 Demo 的视觉与交互部署到正式项目。ui/fleet.html、fleet.css、live.js 保存正式源码，ui/build_live.py 生成页面；原 Start-UI.cmd、scripts/start_ui.py 和 8765 默认入口直接启动双车系统。普通运行不依赖外部 Demo 目录，不要求 npm 或第三方前端库。

默认 V01/V02 每车 40 kg，共享实际 Graph / SimClock，初始订单为 config/orders.json 的三笔。保留新版 82 边路网、中转站取货、固定归属、分钟处罚、局部重规划及历史单车回放。

## 验证结果

- Release 编译通过；121/121 CTest 通过，原核心和 CLI 回归保持通过。UI 桥接/HTTP 测试升级为双车场景，覆盖双车共同时钟、只有获选车辆接单、两车拒单不改任务、重复 ID/自动 ID、共享路况与当前道路锁定、中转站待取、实际取送返仓及卸货中插单。
- 浏览器 ui/verify_live.cjs 通过：正式标识、两车卡片与地图标记、仓库插单和真实完成、超载/窗口拒绝、播放暂停、车辆聚焦、日志筛选、模拟道路拥堵、入边耗时锁定、1440/1280/1024/390/320 宽度及无 JS 错误。
- 浏览器 ui/verify_hubs.cjs 通过：两个仓库/三个中转站下拉，T02 → D18 真实自动分配，任务标出取货点、待取数量为 1，刷新保留最近插入对比，取货后待取清单清空，两车均空车返回 W01。
- 当前实际截图为 docs/maps/ui-hub.png；已检查桌面和窄屏图。

## 实现审查

任务顺序直接来自 Simulation::remaining_actions()，包括当前锁定服务与尚待生效的新计划；桥接不维护另一套动作队列，也不为显示结果额外调用插单试算。分配、路况和业务约束仍由 FleetSimulation 执行。

HTTP 保留本机 Host/Origin 校验与请求长度上限。正式 v2 状态返回共享 time 及 vehicles 数组，旧顶层单车 frame 不再使用；参数 pickup_node_id 兼容原 warehouse。手动 traffic 现在是受支持的本机命令。跨标签页共享一个车队，重置清除本轮状态，不做跨服务重启持久化。

历史 replay.html 继续使用原记录，未改写旧发布数据；单车 CLI 仍可用。远端 Windows / Ubuntu 自动编译、Python、演示与绘图结果以本次提交的 GitHub Actions 为准。
