# 第 5 阶段：硬窗口与动态订单演示

使用 `config/network.json` 的 30 节点、98 道路网络，订单来自 `config/orders.json`。车辆 V01 从 W01 出发，08:00 开始，容量 100 kg，初始货物 60 kg；每步 5 分钟，卸货 2 分钟，每超时单固定处罚 10 分。

## 接单结果

| 请求 | 发布时间 | 结果 |
| --- | --- | --- |
| URGENT01：W01 取 15 kg，送 D05，09:02 前开始卸货 | 08:02 | 接受，当前路段完成后回仓取货 |
| OVERLOAD01：2000 kg，送 D02 | 08:03 | 拒绝，装载超出 100 kg 上限 |
| IMPOSSIBLE01：5 kg，送 D25，08:05 截止 | 08:04 | 拒绝，未找到满足时间窗口的插入 |

距离策略的关键事件：

```text
08:02  接受 URGENT01；此时车辆仍在 W01 -> D06，道路进度及 60 kg 载荷不变
08:06  抵达 D06，启用第 2 版路线
08:13  实际回到 W01 取货，载荷从 60 kg 增至 75 kg
08:42  URGENT01 在 D05 开始卸货，早于 09:02 截止
08:44  紧急订单完成，载荷降至 60 kg
10:35  所有 4 笔已接受订单配送完成并返仓，载荷为 0
```

两个被拒绝请求均未改变原车辆、任务和已接受路线。新路线包含取货仓库以及真实道路节点，不会让车辆直接出现在仓库，也不会因为途经其他配送点而自动完成尚未安排服务的订单。

| 运行策略 | 完成单数 | 超时单数 | 处罚总分 | 返仓时间 |
| --- | ---: | ---: | ---: | --- |
| distance | 4 | 0 | 0 | 10:35 |
| cost | 4 | 0 | 0 | 10:31 |

本例中全部已接受订单可按时完成，所以处罚为零；固定处罚及实际迟到记录仍有单元测试覆盖，第 4 阶段还保留[实际超时示例](../stage4/README.md)。

## 文件

- 距离策略：[完整日志](distance-log.txt)、[接单决定与新路径](distance/insertions.csv)、[订单完成/取货记录](distance/deliveries.csv)、[路线版本](distance/route_revisions.csv)、[取送停靠及预测载荷](distance/route_stops.csv)。
- 成本策略：[完整日志](cost-log.txt)、[接单决定与新路径](cost/insertions.csv)、[订单完成记录](cost/deliveries.csv)、[路线版本](cost/route_revisions.csv)。
- 状态和事件分别保存在各策略目录的 `simulation.csv` / `events.csv`，其 `route_revision` 引用生效路线版本；插入记录保留尚待衔接的候选新路径。

本阶段以日志与 CSV 展示动态路径，旧 Python 绘图器仅支持第 4 阶段固定路线，不能直接绘制这些版本数据。

## 复现

在项目根目录编译后运行：

```powershell
.\build\Debug\logistics_app.exe --plan distance --simulate --scenario config/orders.json --warehouse W01 --departure 480 --capacity 100 --step 5 --late-penalty 10 --export exports/stage5-distance
.\build\Debug\logistics_app.exe --plan cost --simulate --scenario config/orders.json --warehouse W01 --departure 480 --capacity 100 --step 5 --late-penalty 10 --export exports/stage5-cost
```

Linux/macOS 通常使用 `./build/logistics_app`。参数写到 exports，不覆盖本目录参考文件。算法保留旧动作相对顺序并使用目标权重的 Dijkstra，拒绝代表当前启发式未找到可行插入，不代表数学上已证明无解。
