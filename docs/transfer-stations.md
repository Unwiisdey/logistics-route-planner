# 中转站取货与新版路网

2026-10-25 经用户批准实现。本次扩展复用原有 Pickup/Delivery 动作、载重和时间窗口检查，不引入多级转运系统。

## 运行规则

- 动态订单的取货地点可为仓库或中转站，配送目的地仍须为配送点。订单明确指定货物所在地，规划不能随意换成另一座更近的站点。
- 发布时假定该单货物已在指定地点备好。每单货物独立，不维护 SKU、站点总库存、仓库补货或跨车交接；站点 demand 不当作库存使用。
- 接单/试算不增加车辆载荷，只有真实到站执行 Pickup 才装货。路过中转站而没有取货动作不会装货。取货仍按现有模型瞬时完成。
- waiting_pickups 从已接受且 picked_up_at 为空的订单记录计算。没有另一份需要手工扣减的库存表；拒单不产生幽灵待取货物，实际取货后清单自动减少。
- 双车分别只读试算，正式接受后仅选中车辆获得该单；已接及已完成的订单 ID 保留归属，防止重复领取。全部剩余窗口、取送顺序与每次装卸后的容量照常校验。
- 初始订单保持在出发仓库预装的规则，中转站取货在动态订单中演示。返仓地点来自车辆的 home warehouse，始终独立于订单取货点。

## 配置与兼容

新字段 pickup_node_id 替代原 warehouse_id 的“取货地点”语义；旧字段继续读取，但同一订单必须且只能提供其中一个。C++ Order::pickup_node_id() 为规范接口；warehouse_id() 保留旧接口别名。

DeliveryRecord 与 UI 状态包含规范 pickup_node_id 和兼容别名；deliveries.csv 保留旧列并在末尾追加 pickup_node_id，便于既有回放读取器继续工作。API 的 home warehouse 及 RoutePlan::warehouse_id 仍表示返仓仓库，不改为中转站。

```json
{
  "id": "HUB01",
  "pickup_node_id": "T02",
  "delivery_node_id": "D18",
  "quantity": 5,
  "time_window": {"earliest": 482, "latest": 650},
  "release_time": 482,
  "priority": "urgent"
}
```

完整文件为 config/hub-orders.json，含三笔仓库初始单、一笔中转站成功插单，以及超载/过窄窗口拒单。

## 使用

```powershell
# 双车，每车 40 kg
.\build\Release\logistics_app.exe --plan distance --simulate --vehicles 2 --scenario config/hub-orders.json --capacity 40 --traffic --export exports/hub-fleet
# 单车，初始三单合计 60 kg，容量设为 100
.\build\Release\logistics_app.exe --plan distance --simulate --scenario config/hub-orders.json --capacity 100 --traffic --export exports/hub-single
```

正式本地 UI 的取货地点可选 W01/W02/T01/T02/T03，任务显示“取货 · HUB-DEMO（T02）”，并显示各站待取数量；该页面保留单车布局。双车核心由 CLI 使用，双车页面仍为此前的独立 Demo，未在本次替换正式界面布局。

## 当前路网

![当前路网](maps/network.png)

config/network.json 使用批准的简化主路、支路环线与斜向捷径，30 节点、41 组双向连接 / 82 条有向边，含 D12 ↔ D18。距离、耗时和成本已明确写入配置；修改坐标不会自动重算这些数值。两个方向权重可不同。

scripts/draw_network_map.py 直接读取当前配置生成 docs/maps/network.svg，并检查全图连通及直线绘制下没有无节点的道路交叉；PNG 是该 SVG 的浏览器渲染。图中主路/支路只是说明性分组，Dijkstra 使用 JSON 权重。旧 release、保存的旧 CSV/GIF 和 replay.html 保留历史网格，不用于表示当前配置。
