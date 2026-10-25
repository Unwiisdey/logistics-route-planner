# C++ API 说明（main）

命名空间 `logistics`，公开头文件位于 `include/logistics/`，核心链接目标 `logistics_core`。接口以实际头文件为准；C++17，无 GUI 类型和 nlohmann/json 类型泄漏到模型。时间为 double 分钟，数量/距离/费用为非负有限 double。

## 模型与网络

| 类型 / 接口 | 行为 |
| --- | --- |
| Node(id, type, x, y, demand=0, time_window=nullopt) | 仓库、配送点、中转站；只读访问器 |
| Edge(id, from, to, distance, time, cost, status=Clear) | 有向边；base_time 保留初始耗时 |
| Graph::add_node / add_edge | 校验后插入；重复或缺失端点抛 invalid_argument |
| Graph::remove_node / remove_edge | 返回是否删除；删点同时清理关联边 |
| Graph::find_node / find_edge / find_edge_by_id | 找不到返回 nullptr，返回对象只读 |
| Graph::outgoing_edges / nodes | 只读容器；未知出边起点抛 out_of_range |
| Graph::update_edge_traffic(from,to,time,status) | 修改边权和状态，保持 base_time；仅改图，不自动推进仿真 |
| Graph::revision() | const shared_ptr 版本令牌；查询缓存用，调用者不修改 |
| adjacency_matrix(metric) / print_adjacency_* | 排序输出；无边为 nullopt / INF，允许零权边 |
| ConfigLoader::load_network(path, check_size=false) | 返回独立 Graph；read_network 从输入流读取（见头文件） |
| OrderConfigLoader::load(path) / read(stream) | 返回初始与按发布时间排序的动态订单 |

图发生修改后重新获取指针和引用，不跨修改保存。Graph 可复制；未改副本共享版本标识，但各自拥有网络数据，任何修改都会分离版本。核心对象不是并发写入安全的；应在同一仿真线程串行处理事件。

## 最短路、缓存、规划

```cpp
#include "logistics/config/ConfigLoader.h"
#include "logistics/planning/ShortestPathCache.h"

auto graph = logistics::ConfigLoader::load_network("config/network.json");
logistics::ShortestPathCache cache(256);
auto path = cache.query(graph, "W01", "D03", logistics::WeightMetric::Time);
if (path.reachable) {
    // path.node_sequence / road_sequence / total_travel_minutes
}
auto again = cache.query(graph, "W01", "D03", logistics::WeightMetric::Time);
auto hits = cache.stats().hits;
```

- `Dijkstra::single_source(graph, source, metric)` 返回 ShortestPaths 快照；`distance_to(target)` 为 optional，`path_to(target)` 返回 PathResult。缺失节点/非法权重类型抛 invalid_argument，不可达不抛异常。
- `Dijkstra::shortest_path(graph, source, target, metric)` 在目标确定后停止；默认 Distance，支持 Time/Cost。结果累计三项实际边权。
- `ShortestPathCache::query` 与最短路结果一致，按图版本自动失效；`size/capacity/stats` 可读，`clear()` 清空条目并重置统计。容量 0 禁用，不可达也可缓存。无外部图指针生命周期依赖。
- `Planner::plan(graph, PlanningRequest)` 返回 RoutePlan；检查 `status == PlanStatus::Feasible` 后才能执行/导出。字段包括仓库、可选起点、配送子集、出发时刻、容量、distance/cost 策略。失败结果不含冒充可执行行程的探索前缀。
- `Planner::schedule(graph, route, unload_minutes)` 返回逐节点到达/出发、总等待/卸货；验证路线与图是否一致。
- `DynamicOrder::initial(graph, context, cache=nullptr)`：初始已装货订单的启发式规划。
- `DynamicOrder::insert(graph, context, order, cache=nullptr)`：保留已有动作相对顺序，枚举取送插入。无 cache 时本次调用内自动复用缓存。
- `DynamicOrder::evaluate(graph, context, actions, cache=nullptr)`：先按距离/成本规划；因窗口失败时，对同一动作序列以 Time 重试。所有取货、载重、窗口及返仓检查仍执行；成功兜底时 OrderRoute::time_fallback=true。未提供缓存时直接查最短路。
- `DynamicOrder::urgent(id, warehouse, destination, quantity, received_at, deadline_after=60)`：创建紧急订单。
- `Replanner::repair(graph, context, remaining, triggers, cache=nullptr)`：仅重搜受影响的动作区间；返回 OrderRoute、searched_legs 和预测迟到订单。searched_legs 为受影响区间请求数，使用缓存时不一定等于实际 Dijkstra 次数。

`InsertionResult` 必须先检查 accepted；reason 说明可行或拒绝原因，route 只在成功时使用。`OrderPlanningContext` 含衔接节点/时刻、返仓点、容量/实际货物、已装订单集合、剩余订单和动作序列。手工构造时务必保持一致；常规业务优先使用 Simulation 生成上下文。

## 仿真与路况

```cpp
#include "logistics/config/OrderConfigLoader.h"
#include "logistics/simulation/Simulation.h"

auto scenario = logistics::OrderConfigLoader::load("config/orders.json");
logistics::SimulationOptions options;
options.capacity_kg = 100;
options.path_cache_capacity = 256;
logistics::Simulation sim(graph, "W01", scenario.initial_orders, 480, options);
sim.enable_traffic();
// 本例仅执行 initial_orders；动态请求必须由调用者在 release_time 到达时逐个 insert_order。
while (!sim.finished()) { sim.advance(); }
sim.export_csv("exports/api");
```

| 接口 | 契约 |
| --- | --- |
| Simulation(Graph, RoutePlan, options) | 执行节点任务的既定路线，允许记录迟到；初始从仓库出发 |
| Simulation(Graph, warehouse, orders, start, options, strategy=ShortestDistance) | 严格初始规划；容量/窗口失败抛异常 |
| advance() / advance(minutes) | 正值推进，内部精确处理阶段和路况；完成后不推进 |
| wait_until(time) | 仅完成且空闲时等待，路况时钟继续运行 |
| preview_order(order) | 只读试算，不改变任务、事件、载荷或路径；使用临时缓存 |
| insert_order(order) | 要求已发布、ID 未用过；可拒绝；成功后在当前边/服务结束处切换 |
| enable_traffic(options={}) | 从当前时间起按 interval 安排随机路况 |
| apply_traffic(vector<TrafficUpdate>) | 当前时刻注入确定性事件，先整批校验，自动局部修复 |
| clock / vehicle / network | 只读运行状态；外部不能直接改内部车辆或图 |
| snapshot / history / events | 位置、状态、路线/路况版本、下一订单到达和返仓预测 |
| deliveries / insertions / route_revisions / replanning_records | 实际结果及规划审计记录 |
| path_cache_stats | 命中、未命中、失效、淘汰统计 |
| write_log(stream) / export_csv(path) | 输出日志、参数、模型和运行数据；I/O 错误抛异常 |

`SimulationOptions` 默认 step=5、unload=2、late_penalty_per_minute=1（迟到分钟向上取整）、vehicle_id=V01、capacity=1000、cache capacity=256。`TrafficOptions` 默认 interval_minutes=0.5、fraction=0.1、倍率范围 [1.2,2.0]、seed=42。TrafficUpdate 含 road_id、更新后的绝对耗时和 Clear/Congested；不是增加量。当前阶段不接受运行中 Closed 事件。

`TimeWindow::late_penalty(service_start, points_per_minute)` 依据窗口计算迟到分钟，向上取整后乘费率；未迟到或费率为 0 时计 0。它取代原 fixed_penalty 接口，SimulationOptions 的费率字段为 late_penalty_per_minute。计算及累计溢出会报错，实际完成前不会写入无穷分值。窗口与整分钟边界沿用 1e-9 分钟数值容差，不构成业务宽限。

新版 simulation_options.csv 使用 late_penalty_per_minute 和 penalty_rounding=ceil，取代旧 fixed_late_penalty 字段。旧 CSV 和 v1.0.0 发布说明仍表示历史固定规则；新运行的处罚以服务开始时的迟到时长计算，并在完成时只记一次。

`TrafficSimulator::sample(graph)` 不修改图；`validate` 校验整批，`apply` 更新指定边并返回前后变化；`significant_increase` 使用含 20% 的阈值。直接调用 Graph/TrafficSimulator 不会替你更新 Simulation，应使用 sim.apply_traffic。

## 导出与错误

`Visualizer::export_csv(graph, plan, directory)` 验证完整静态计划；`export_network_csv` 只导出节点/道路。动态仿真使用 Simulation::export_csv，由其保存版本历史与缓存统计。CSV UTF-8，正确转义引号/逗号，数值使用独立于 locale 的小数点。

输入不合法通常为 invalid_argument；未知出边起点/待更新边为 out_of_range；累计数值溢出为 overflow_error；I/O、无法完成修复等为 runtime_error。业务性拒单使用 accepted=false；图不可达用 reachable=false；调用者不得把它们当作成功路线使用。完整示例见 CLI 与 tests/integration_tests.cpp。

## 双车调度接口

头文件 logistics/simulation/FleetSimulation.h。FleetVehicleSpec 包含 id、warehouse、capacity_kg；ID 仅允许 ASCII 字母、数字、下划线和连字符，保证导出子目录安全。API 可为两车指定不同仓库与容量；CLI 提供同仓库、同容量的两车入口。

- FleetSimulation(Graph, vector<FleetVehicleSpec>, initial_orders, start=480, options={}, strategy=ShortestDistance)：按紧急程度、截止时间和 ID 逐单分配。归属 map<OrderId,string> 对已接受订单长期保留；初始无法分配的业务拒单写入 assignments，不默默丢单。非法车辆配置、重复初始订单 ID、尚未发布的初始单抛异常。
- insert_order(order)：先 preview_order 试算所有车辆，只对选中者正式 insert_order；返回 FleetAssignment，含 time/order_id/vehicle_id/accepted/reason/path。拒绝时无车归属，各车状态不变。紧急单先比较该单 service_start，再比较新增距离/成本，最后车辆 ID；普通单先新增距离/成本，再返仓时刻和 ID。
- advance() / advance(minutes)：统一事件循环推进同一 SimClock。处理所有车辆阶段结束与单一路况计时；即使全车空闲也可推进，方便未来发布。非正数/非有限步长拒绝，不能为各车分别调用 advance。
- enable_traffic / apply_traffic：统一采样或注入一批事件，候选图上预检两车修复后发布同一共享版本；仅受影响区间搜索。每车保留独立缓存、事件和路线版本。
- clock / network / vehicle(id) / vehicle_specs / ownership / assignments：只读查询。不存在车辆抛 invalid_argument。两车 network()/clock() 引用指向同一对象。
- finished()：所有车辆均完成；write_log 汇总与逐车日志；export_csv(path) 写 assignments.csv 与每车 ID 子目录的原仿真 CSV。车辆完成后，在共同时间线上其 snapshot.time 仍是当前时间，实际返仓时间看 Simulation::completed_at()，返仓预测和完成日志保留该时刻。根目录 traffic_options.csv 保存共享路况源参数；各车的 random_enabled=0 表示自身不采样。

Simulation 不可复制、可移动，避免意外复制活动仿真时共享可变状态。单车公开构造函数仍创建独立网络和时钟，只有 FleetSimulation 内部构造才注入共享对象。

## 指定中转站取货（2026-10-25）

Order::pickup_node_id() 为规范取货地点访问器，允许仓库或中转站；warehouse_id() 作为旧接口别名保留，不再表示车辆返仓点。DeliveryRecord 同时输出 pickup_node_id 与兼容 warehouse_id，两者内容一致；deliveries.csv 在末列追加 pickup_node_id，原 warehouse_id 列仍是取货点别名。

Simulation::waiting_pickups(node_id)、FleetSimulation::waiting_pickups(node_id) 返回已接且未取的订单 ID；普通配送点或未知节点抛 invalid_argument。清单从订单记录计算，预览/拒绝不增加货物，实际执行 Pickup 后移除。

OrderConfigLoader 允许 pickup_node_id 或旧 warehouse_id，恰好提供一个；初始订单仍从出发仓库预装，动态单可在中转站备货。home warehouse / OrderPlanningContext::warehouse 始终是返仓仓库。

UI API 接受 pickup_node_id，兼容旧 warehouse 请求参数，二者同时出现拒绝；状态 pickup_points 包含中转站的 node_id 和 waiting_orders。订单状态返回规范 pickup_node_id 及旧别名。取货仍为瞬时动作，不引入装货服务时长。

## 正式双车 UI 协议（ui_version=2）

默认桥接进程持有 FleetSimulation，V01/V02 从 W01 出发、每车 40 kg。GET /api/state 返回 app=logistics-route-planner、ui_version=2、demo=false、engine、time、finished、nodes、edges、vehicles、orders、events、traffic_version、pickup_points、penalty_rule、latest_insertion。此为原单车 UI 协议的升级：原顶层 frame/segment/task_path 改为共享 time 及 vehicles 中各车状态；CLI 和 C++ 单车接口不变。

vehicles 含 id/capacity/load/status、节点或边位置、当前 segment 的 total/remaining/arrival、下一单与返仓 ETA、completed_at、完成/超时/处罚、future_nodes/future_roads、traversals、tasks/current_task、replan、revision。任务源自 Simulation::remaining_actions()，返回未完成动作，当前卸货/等待动作保持首位；路线取送顺序与站点来源以核心状态为准。

POST /api/command 接受 state/reset/advance/insert/traffic。advance.minutes 为 (0,10] 的有限分钟；所有车只推进共享时钟一次，空闲车仍可等未来订单。insert.order 使用 pickup_node_id（兼容旧 warehouse，不能同时提供）、destination、quantity、earliest/latest、priority、可选 id（最多 40 字节）；留空生成不与已用 ID 冲突的 MANUALn。成功响应 insertion 含 accepted/id/vehicle_id/reason/path；业务拒单 ok=true 且 accepted=false，非法输入 ok=false。

traffic.road_id 将道路当前耗时乘 1.8；可选 minutes 直接指定绝对耗时（核心校验有限非负）。只在当前已验证的本机来源下接受 HTTP 请求，仍保留 Host/Origin 与 8192 字节上限。latest_insertion 保存最近一次已接受的插入前后路径，刷新页面可继续查看；拒单不覆盖它。reset 清除本轮订单与比较记录，保留初始配置。
