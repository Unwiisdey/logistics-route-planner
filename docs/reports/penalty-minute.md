# 按迟到分钟计罚验证

2026-10-24 用户确认：不足一分钟向上取整，默认每分钟处罚 1 分。开始卸货时确定实际迟到，完成配送时每单只记账一次，与运输成本分开。

## 规则与接口

示例：未迟到为 0；迟到 15 秒为 1 分；1 分钟为 1 分；1 分 1 秒为 2 分；2 分 30 秒为 3 分。

- C++：TimeWindow::late_penalty(service_start, points_per_minute)，SimulationOptions::late_penalty_per_minute，默认 1。
- CLI：--late-penalty-per-minute POINTS；旧 --late-penalty 为同义别名，含义改为每分钟费率。
- 新 simulation_options.csv：late_penalty_per_minute、penalty_rounding=ceil。
- 实时 UI 显示新规则，历史回放标记旧规则，已发布 v1.0.0 及旧导出不重写。
- 负值、NaN、无穷费率拒绝。单单处罚或累计处罚溢出时抛错，尚未记账的订单不标记完成。
- 浮点边界只沿用 1e-9 分钟数值容差，不是业务宽限期。

## 本机实测

MSVC x64 Release 构建通过，98 项 CTest 全部通过。原有 minute_penalty、窗口、步长与路况用例扩展验证：

- 小数向上取整、整分钟边界、0 费率和自定义费率。
- 不同时间步长结果一致；早到等待、截止时开始卸货为 0；卸货结束超过窗口不额外计罚。
- 重复推进不会重复记账；极大费率的单笔与累计溢出不会写入无穷结果。
- 已接受订单因拥堵实际迟到 46.25 分钟，处罚 47 分。
- UI HTTP 返回 rate_per_minute=1、rounding=ceil、based_on=service_start，正式页不再显示每单 10 分。

CLI 另用单点 D01、出发 800.25 分钟验证：实际迟到 55.25 分钟，默认处罚 56；费率 2 为 112；旧参数别名同为 112；费率 0 为 0。负数、NaN、无穷费率均退出报错。

两种路径选择及单车辆范围未改变。远端双平台结果以当前提交的 GitHub Actions 为准。
