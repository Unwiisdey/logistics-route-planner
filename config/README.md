# 网络配置格式

`network.json` 使用 UTF-8 JSON，`schema_version` 必须是整数 `1`。根对象仅允许 `schema_version`、`nodes`、`edges` 三个字段。加载器拒绝语法错误、重复 JSON 键、未知字段及不匹配的数据类型，错误通常包含文件名和 `nodes[i]` / `edges[i]` 的位置（下标从 0 开始）。

## 最小示例

```json
{
  "schema_version": 1,
  "nodes": [
    {"id": "W01", "type": "warehouse", "x": 0, "y": 0},
    {"id": "D01", "type": "delivery_point", "x": 2, "y": 0,
     "demand": 10, "time_window": {"earliest": 540, "latest": 660}}
  ],
  "edges": [
    {"id": "E001", "from_id": "W01", "to_id": "D01",
     "distance": 2, "time": 6, "cost": 3, "traffic_status": "clear"}
  ]
}
```

该例用于解释格式，规模不足且没有回程道路。实际演示使用同目录的 `network.json`。

## 节点字段

| 字段 | 类型 | 要求 |
| --- | --- | --- |
| id | 字符串 | 必填且全图唯一；只允许英文字母、数字、下划线和连字符 |
| type | 字符串 | `warehouse` / `delivery_point` / `transfer_station` |
| x、y | 数值 | 必填、有限；允许负坐标；示例使用以公里表示的平面坐标 |
| demand | 数值 | 可选，默认 0；有限且非负；本项目示例约定为 kg，后续车辆载重使用同一单位 |
| time_window | 对象或 null | 可省略/null，表示没有节点默认窗口；非空时必须包含 earliest 和 latest |
| earliest、latest | 数值 | 从模拟日 00:00 起的分钟数，有限且 `0 <= earliest <= latest`；不限制为 24 小时以内 |

例如 540—660 表示 09:00—11:00。网络加载器只保存并校验窗口；仿真执行等待和超时判断。节点需求量/窗口是默认信息，第 5 阶段的独立订单可以覆盖它们。

## 道路字段

| 字段 | 类型 | 要求 |
| --- | --- | --- |
| id | 字符串 | 可选；若省略，生成 `from_id->to_id`；显式/生成 ID 均须唯一，使用无空格可打印 ASCII |
| from_id、to_id | 字符串 | 必填，引用已有节点，起终点不能相同 |
| distance | 数值 | 必填、有限且非负，公里 |
| time | 数值 | 必填、有限且非负，分钟；加载时同时作为初始及当前耗时 |
| cost | 数值 | 必填、有限且非负，元 |
| traffic_status | 字符串 | 可选，默认 `clear`；可选值为 `clear`、`congested`、`closed` |

数字字符串和布尔值不作为数值接受。零权重合法。一个起终点组合仅允许一条边；双向通行配置两条有向边。`closed` 道路保留在结构中，邻接矩阵不表达通行限制，Dijkstra 会跳过关闭道路。

## 加载与规模校验

`ConfigLoader::load_network(path)` 从文件返回全新的 `Graph`；`read_network(stream)` 支持流输入。失败抛出 `ConfigError`，不留下部分加载的图。现有图执行 `graph = load_network(...)` 时，加载失败不会覆盖原图。

默认允许空图、小图，用于单元测试。将第二参数设为 `true`，或使用应用的 `--check-size`，会额外要求：节点 ≥30、仓库 ≥2、配送点 ≥15、中转站 ≥3、有向道路 ≥50。只检查规模不等于检查连通性或配送可行性；示例的强连通性由测试验证。

当前演示为三个简化片区、连接主路、支路环线和 D12 ↔ D18 捷径，含 30 个节点（2 仓库、25 配送点、3 中转站）、41 组双向连接 / 82 条有向边。全部节点连通。坐标以 km 表示；距离按节点间欧氏距离保留三位小数，主路/捷径/支路的示例速度分别为 0.8/0.6/0.45 km/分钟；反向耗时为正向的 1.08 倍，费用为距离 ×0.85 元/km，反向再加 0.2 元。这是课程示例权重，不是真实交通数据。图示分类不增加 JSON 字段，算法仍使用配置权重。

网络文件不接收订单和车辆字段。订单使用下述独立文件，车辆载重、ID、起始时间及步长通过命令行设置；`Configuration` 全量配置原型尚未用于加载。

## 独立订单文件（第 5 阶段）

使用 `--scenario config/orders.json`，由 `OrderConfigLoader` 读取。根对象必须包含整数 `schema_version: 1`、`initial_orders` 数组和 `dynamic_orders` 数组。拒绝未知字段、重复 JSON 键和重复订单 ID。

| 订单字段 | 规则 |
| --- | --- |
| `id` | 非空字符串，全文件唯一，按订单而非节点区分任务 |
| `pickup_node_id` | 推荐字段，取货节点 ID；仓库或中转站。初始订单仍须在车辆出发仓库预装 |
| `warehouse_id` | 旧取货字段的兼容别名，与 pickup_node_id 必须且只能提供一个 |
| `delivery_node_id` | 配送点 ID，允许不同订单引用同一点 |
| `quantity` | 有限非负 kg，作为实际装卸货量 |
| `time_window` | 必填，`null` 或含数值 `earliest` / `latest` 的对象 |
| `priority` | 可选，`normal` / `urgent`；初始默认 normal，动态默认 urgent |
| `release_time` | 动态必填；初始默认 0；连续非负分钟，不能晚于本单截止时间 |

订单窗口独立于节点窗口；`null` 表示该订单无窗口。初始订单须在出发前已发布并一次装车，超载则初始化失败。动态发布时间不得早于 CLI 起始时间，程序会在步长内精确推进到发布时间再插入；相同发布时间按文件顺序处理。接口允许拒绝后重试，JSON 为可复现演示要求全文件 ID 唯一。

正常执行中的接单拒绝属于仿真结果，不导致进程失败；非法文件或严格初始规划失败会报错并以退出码 1 结束。

例如 08:02 发布、要求一小时内送达：`release_time: 482`，窗口 `{"earliest":482,"latest":542}`。本例有三笔初始单和三笔动态请求，分别展示成功插入、超载拒绝和窗口拒绝。

中转子网络可简化展开为同一个 `network.json` 中的节点与边，经 `transfer_station` 节点连接；所有 ID 仍全局唯一。当前不解析嵌套 JSON 图或分级车辆调度。

## 第 6 阶段路况参数

网络 JSON 不增加字段，原有 `time` 是启用模拟前的基准耗时，`traffic_status` 是初始状态。通过 `--traffic` 启用路况，`--traffic-interval 30` 单位为秒，`--traffic-fraction 0.1` 为抽样比例，`--traffic-seed 42` 为 uint32 种子；均可覆盖默认值。只采样未关闭且基准耗时大于零的道路，默认每批 9 条（82 条中的 10% 向上取整），拥堵时乘 1.2～2.0，恢复时还原。

增时阈值按变化前耗时比较；仿真已有行驶段保持入边耗时，路况仅影响未来进入道路的行程。导出的 `traffic_options.csv` 保存参数，`traffic.csv` 保存实际变化，便于审查和回放。CLI 参数无需新增 JSON 库或绘图库依赖。

## 中转站订单示例

config/hub-orders.json 中 HUB01 使用 pickup_node_id=T02，在 482 分钟发布后由车辆到站取货，再送往 D18。货物发布时备妥，装货瞬时完成；站点不使用 demand 存库存。初始订单仍在出发仓库预装。旧 warehouse_id 请求继续有效，但不能与新字段同时出现。车辆最终返回自己的 home warehouse，不能将中转站误当返仓点。
