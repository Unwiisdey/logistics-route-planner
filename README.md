# 电商物流配送路径规划系统

C++17 数据结构课程设计项目，使用有向带权图模拟城市配送网络。需求依据为《第2题 电商物流配送路径规划系统》，完整拆解见 [需求文档](docs/requirements.md)。

## 当前进度

当前完成 **第 6 阶段：模拟实时路况与自动局部重规划**。默认每 30 秒更新约 10% 道路；耗时相对上次值增加至少 20% 时，只重新搜索受影响的取送区间。支持路况日志、前后路径对比、预计到达时间，以及按历史版本显示拥堵和车辆位置的交互回放。

penalty 继续使用**每个超时订单固定 10 分**（可配置）。严格模式在接单前检查全部剩余窗口、取货及载重；后续拥堵可能使已接受订单迟到，此时提示风险并在实际完成时记录处罚。旧静态规划与固定路线仿真仍可使用。

| 功能 | 状态 |
| --- | --- |
| C++17 工程、CMake、模块划分和需求文档 | 已完成 |
| Node / Edge 数据校验、Graph 邻接表封装、节点与边增删 | 已完成 |
| JSON 网络配置读取、错误定位、课程规模校验 | 已完成 |
| 邻接表、三种权重的邻接矩阵输出 | 已完成 |
| 示例网络：30 节点、98 有向道路；2 仓库、25 配送点、3 中转站 | 已完成 |
| Dijkstra 距离/成本策略、最近邻完整配送行程 | 已完成 |
| CSV 导出、Python 网络与路线图、演示图片和输出 | 已完成 |
| SimClock、Vehicle、Simulation、等待/卸货与固定超时处罚 | 已完成 |
| 车辆坐标、事件日志、路径进度着色与交互/GIF 回放 | 已完成 |
| TimeWindow / Order / DynamicOrder、同点多单、订单 JSON | 已完成 |
| 硬窗口规划、仓库取货、动态插入、全程载重与路线版本 | 已完成 |
| 84 项 CTest 测试、23 项 Python 数据检查 | 已通过 |
| TrafficSimulator / Replanner、局部修复、ETA、动态路线与路况回放 | 已完成 |

`config/network.json` 保存节点默认信息；`config/orders.json` 保存实际订单数量、窗口、取货仓库和发布时间。同一配送点可有多笔独立订单，按订单 ID 跟踪；不会将途经节点自动算作已交付订单。车辆和时钟参数仍通过命令行配置。

## 目录结构

```text
.
├── CMakeLists.txt
├── README.md
├── config/
│   ├── network.json                 # 可运行演示网络
│   ├── orders.json                  # 初始订单与动态订单演示
│   └── README.md                    # JSON 字段、单位和约束
├── include/logistics/
│   ├── model/
│   │   ├── common.hpp               # ID、枚举及公共类型入口
│   │   ├── TimeWindow.h             # 等待、截止边界和固定处罚
│   │   ├── Order.h                  # 独立订单值对象
│   │   ├── Node.h                   # 节点值对象
│   │   ├── Edge.h                   # 道路值对象
│   │   ├── Vehicle.h                # 车辆载重、节点/道路位置与状态
│   │   └── types.hpp                # 订单、车辆及基础类型入口
│   ├── graph/Graph.h                # 图操作与矩阵快照
│   ├── graph/graph.hpp              # 兼容第 1 阶段包含路径
│   ├── config/ConfigLoader.h        # 文件/流加载器
│   ├── config/OrderConfigLoader.h   # 初始/动态订单加载器
│   ├── config/configuration.hpp     # 后续完整仿真配置原型
│   ├── detail/checked_math.hpp      # 路线总量的有限数值检查
│   ├── planning/Dijkstra.h          # 单源最短路径及重建结果
│   ├── planning/Planner.h           # 最近邻单车辆规划请求
│   ├── planning/DynamicOrder.h      # 硬约束候选评估与取送插入
│   ├── planning/Replanner.h         # 仅重搜受影响的取送区间
│   ├── planning/route_plan.hpp      # 完整道路与配送停靠序列
│   ├── simulation/SimClock.h        # 可配置时间与步长
│   ├── simulation/Simulation.h      # 事件推进、配送记录与快照
│   ├── simulation/simulation_state.hpp # 兼容的视图快照入口
│   ├── traffic/traffic_event.hpp
│   ├── traffic/TrafficSimulator.h   # 路况采样、批量校验、20% 阈值
│   ├── visualization/Visualizer.h  # 网络/路线 CSV 导出
│   └── visualization/network_view.hpp
├── src/
│   ├── main.cpp                     # 命令行入口
│   ├── model/Node.cpp
│   ├── model/Edge.cpp
│   ├── model/Vehicle.cpp
│   ├── model/TimeWindow.cpp
│   ├── model/Order.cpp
│   ├── graph/Graph.cpp
│   ├── config/ConfigLoader.cpp
│   ├── config/OrderConfigLoader.cpp
│   ├── planning/Dijkstra.cpp
│   ├── planning/Planner.cpp
│   ├── planning/DynamicOrder.cpp
│   ├── planning/Replanner.cpp
│   ├── traffic/TrafficSimulator.cpp
│   ├── simulation/SimClock.cpp
│   ├── simulation/Simulation.cpp
│   ├── simulation/SimulationOrders.cpp # 接单、承诺边界与路线切换
│   ├── simulation/SimulationTraffic.cpp # 路况事件、预测与导出
│   └── visualization/Visualizer.cpp
├── scripts/plot_network.py          # 使用 matplotlib 绘图
├── scripts/plot_simulation.py       # 车辆位置、进度着色与回放
├── scripts/plot_traffic.py          # 拥堵、新路线、车辆与 ETA 回放
├── scripts/requirements.txt         # 可选 Python 依赖
├── tests/                           # 图、配置、规划、仿真和绘图数据测试
├── docs/requirements.md
├── docs/examples/                   # 两种策略的 CSV、文本输出和 PNG
└── third_party/nlohmann/             # JSON 单头文件和 MIT 许可证
```

configs/README.md 是第 1 阶段旧目录的迁移说明；实际配置统一使用 config/。构建目录 build/、临时工具与原始课程 Word 文件不纳入 Git。

## 技术栈

- C++17、CMake 3.16+，MSVC / GCC / Clang 等支持 C++17 的编译器。
- [nlohmann/json v3.12.0](https://github.com/nlohmann/json/releases/tag/v3.12.0)，固定版本单头文件已纳入项目，并核验官方 SHA-256；构建无需联网。来源与许可证见 [第三方依赖](third_party/README.md)。
- 核心为 logistics_core 静态库。JSON 头文件仅供加载器实现使用；图和数据模型不依赖 JSON 或 GUI。
- Python 3.10+ 与 matplotlib 3.10.7 仅用于可视化。C++ 编译、规划、测试和 CSV 导出不需要 Python。
- 测试使用 CTest 和轻量 C++ 检查函数，无额外测试框架；不使用会在 Release 中失效的 assert。
- 类型使用 PascalCase，函数与变量使用 snake_case，命名空间为 logistics。按本阶段交付约定使用 Node.h/.cpp 等类文件名称。UTF-8、4 空格缩进。

## 编译与测试

在项目根目录执行：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
cd build
ctest -C Debug --output-on-failure
cd ..
```

单配置生成器（Ninja/Makefiles）使用 CMAKE_BUILD_TYPE；Visual Studio 等多配置生成器使用 --config Debug。切换生成器时使用新的构建目录。需要仅构建应用时可配置 -DBUILD_TESTING=OFF。

当前机器若找不到 cmake 命令，可使用已安装的 Visual Studio 2019 Build Tools：

```powershell
$taskCmake = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$taskCtest = Join-Path (Split-Path $taskCmake) 'ctest.exe'
& $taskCmake -S . -B build -G 'Visual Studio 16 2019' -A x64
& $taskCmake --build build --config Debug
Push-Location build
& $taskCtest -C Debug --output-on-failure
Pop-Location
```

已在 Windows、MSVC 19.29、Visual Studio 2019 Build Tools、x64 Debug 下完成编译，并通过全部 84 项 CTest 测试；另外通过 23 项 Python 数据检查。其他 C++ 工具链尚未实际验证。测试内容见 [测试说明](tests/README.md)。

Python 数据检查不需要安装 matplotlib：

```sh
python -m unittest discover -s tests -p "test_plot*.py" -v
```

## 实时路况与重规划：第 6 阶段

```powershell
.\build\Debug\logistics_app.exe --plan distance --simulate --scenario config/orders.json --capacity 100 --traffic --traffic-seed 42 --export exports/traffic
python scripts/plot_traffic.py --input-dir exports/traffic --frame 80 --output exports/traffic.png
python scripts/plot_traffic.py --input-dir exports/traffic --show
```

绘图依赖仍为 `scripts/requirements.txt` 中固定版本 matplotlib。也可将 `--plan distance` 改为 `--plan cost`，或对旧节点任务使用 `--simulate --traffic`。路况参数必须配合 `--plan` 和 `--simulate`；指定任一路况参数会自动启用路况。

| 参数 | 默认值 | 含义 |
| --- | --- | --- |
| `--traffic-interval` | 30 | 模拟秒；内部使用 0.5 分钟，与 `--step` 独立 |
| `--traffic-fraction` | 0.1 | 每批选取比例，范围 `(0,1]`；对可模拟道路数向上取整，不重复抽取 |
| `--traffic-seed` | 42 | uint32 随机种子；同一工具链中复现相同道路与倍率序列 |

可模拟道路为未关闭且初始耗时大于 0 的边。本例 98 条，每批选择 10 条；畅通边变拥堵，耗时取初始值的 1.2～2.0 倍，拥堵边恢复畅通时还原初始值，不累计乘倍率。C++ `TrafficOptions` 另提供倍率范围配置。随机道路扫描、边权更新不改变拓扑、距离或成本。

触发基准为**变化前的耗时**：`(新耗时 - 旧耗时) / 旧耗时 >= 20%`（浮点比较容差 1e-12）。重复相同值、恢复畅通不触发搜索；外部事件从零增为正值会触发。低于阈值或恢复畅通仍会刷新当前路线 ETA。`Simulation::apply_traffic()` 可接入确定性事件；本阶段不接受封路或拓扑删除事件。

**局部更新规则：** 锁定当前行驶段的入边耗时，车辆驶完该边再切换；等待/卸货中的服务也先完成。从衔接点起，按已接受的取货/配送动作划分区间，尾段为返仓区间。只有原区间包含触发道路时，才调用以**耗时**为权重的点到点 Dijkstra；其余区间原样保留。动作顺序不变、货物不瞬移。为避开拥堵，受影响区间可能增加距离/费用；初始规划和动态插单仍使用所选 distance/cost 策略。

这是路线区间级增量更新，不重排全部订单、不重算所有区间、不运行全点对最短路。点到点搜索在目标出堆时停止；单个困难区间最坏仍可能访问整个连通分量。ETA 的后续时间累加会线性刷新，以传播等待与迟到风险，不属于重新搜索。恢复畅通不会主动寻找新的捷径。

即使没有更快绕路，也会输出重规划原因与前后路径。已接订单不会因预测超时而被丢弃，记录 `window_risk`，实际完成时按每单固定处罚统计；无法满足所有旧订单窗口的新订单仍拒绝。ETA 是按当前权重的预测，未来路况可使其再次变化。

导出包含：`traffic.csv`（路况前后值和触发标记）、`replans.csv`（原因、衔接时刻、前后路径、搜索区间数、返仓 ETA、预测超时订单）、`route_revisions.csv` / `route_stops.csv`（已生效路线）、`remaining_routes.csv`（每帧待执行路线，含尚待当前边结束的候选）、`traversals.csv`（跨版本已走道路）以及车辆、事件和订单 CSV。`traffic_options.csv` 保存随机参数。网络 CSV 保存启用路况时的初始权重；回放按每帧 `traffic_version` 重建当时路况，支持同一时刻多事件和倒退查看。

图中红色道路表示拥堵，紫色表示更新后的剩余路线，橙色表示锁定的当前道路，绿色表示已走道路，星形表示车辆；显示下一配送到达 ETA 和返仓 ETA。`--show` 提供滑块和播放/暂停。界面回放 C++ 已导出的模拟数据，不是与正在运行的 C++ 进程实时通信。

见 [第 6 阶段示例、日志与截图](docs/examples/stage6/README.md)。

## 硬约束与动态订单：第 5 阶段

在项目根目录运行：

```powershell
.\build\Debug\logistics_app.exe --plan distance --simulate --scenario config/orders.json --warehouse W01 --departure 480 --capacity 100 --step 5 --late-penalty 10 --export exports/stage5
```

`--scenario` 自动启用严格模式，使用独立订单，不与 `--deliveries` 合用。将 `distance` 改为 `cost` 可按道路成本评估路径；时限校验始终使用实际耗时。也可不提供订单文件，用节点默认需求及窗口生成初始订单：

```powershell
.\build\Debug\logistics_app.exe --plan distance --simulate --hard-windows --deliveries D01,D02 --capacity 100
```

主要规则：

- **硬窗口**：在候选计划中考虑行驶、等待和每单卸货时间；所有剩余订单必须在截止时间内开始卸货。初始规划找不到可行排序则报错，动态插入不可行则拒绝。运行中若实际迟到，仍单独记录迟到分钟数及每单固定处罚；不会按迟到时长乘罚分。
- **真实取货**：初始订单在出发仓库装车；动态订单的货物位于其 `warehouse_id`，必须实际驶往该仓库后装载，装载耗时暂为 0。接单时不增加载荷，卸货完成后才扣载荷。允许先配送旧货腾出空间，再回仓取新货，不要求所有动态货物同时装车。
- **紧急优先**：固定旧任务剩余动作的相对顺序，枚举新订单“取货在前、送货在后”的插入位置，逐个检查完整剩余路线。对紧急新订单优先选择其开始卸货最早的可行方案，再比较距离/成本。不会牺牲其他已接受订单的截止时间。
- **在途衔接**：正在行驶时走完当前道路再切换；已进入等待/卸货时先完成该订单服务。新路线从该节点、该预测时刻和预测剩余载荷开始，不会瞬移回仓。衔接前连续接单会重新检查全部已接受任务。
- **订单身份**：支持同点多单，逐单检查窗口和卸货。所有已接受 ID（包括已完成）不可重复；仅经过配送点不代表交付。取货仓库可以不同，最终返回本车出发仓库。
- **简化中转子网络**：把子网络展开为全局唯一节点和有向边，经中转站连接即可；当前无分级车队调度、中转卸货交接或真实嵌套图对象。

窗口判定延续“开始卸货”口径，恰好截止为按时；分钟与 kg 的比较采用 `1e-9` 级数值容差处理小数舍入，非业务宽限时间或超载额度。算法是插入启发式，两点仍使用 Dijkstra；“未找到可行插入”不证明所有访问顺序或其他两点路径均不可行。

示例包含 3 笔初始订单，08:02 接受一笔要求 1 小时内送达的紧急订单；08:03 的超载订单和 08:04 的过紧时限订单被拒绝。最终完成 4 单、超时 0 单、处罚 0 分。详见 [第 5 阶段输出与复现](docs/examples/stage5/README.md)，配置字段见 [配置说明](config/README.md)。

导出文件包括 `deliveries.csv`（订单完成及取货记录）、`insertions.csv`（接受/拒绝原因与候选新路径）、`route_revisions.csv`（每个生效版本的真实道路节点）、`route_stops.csv`（取送动作、预计时刻和载荷），以及状态、事件、参数 CSV。状态通过 `route_revision` 对应正确版本，避免用新路线解释旧位置；衔接前的候选路线在插入记录中保留。

第 5 阶段原始示例以文本和 CSV 展示新路线。第 4 阶段绘图器仍只接受固定路线；第 6 阶段使用带 `--traffic` 的新导出和 `plot_traffic.py` 回放动态版本。

C++ API：`Simulation(graph, warehouse, orders, start, options)` 创建严格仿真；`insert_order(order)` 返回接单结果；`DynamicOrder::urgent(..., received_at, 60)` 创建一小时截止的紧急单。`advance()` 推进、`snapshot()` 读取车辆、`route_revisions()` 读取生效路线。已返仓时可 `wait_until(time)` 等待之后发布的订单；仿真中途应使用 `advance()`。

## 配送仿真：第 4 阶段兼容模式

在项目根目录执行（先按上节完成编译）：

```powershell
.\build\Debug\logistics_app.exe --plan distance --simulate --warehouse W01 --departure 480 --step 5 --capacity 1000 --unload-minutes 2 --late-penalty 10 --export exports/simulation
```

将 `distance` 改成 `cost` 可仿真成本策略；用 `--deliveries D01,D08,D20` 可选择任务子集。命令行自动推进到返仓结束；C++ 接口可逐次调用 `advance()` 或 `advance(1)`，不调用即暂停。仿真时间不依赖电脑时钟，不会真实等待数小时。

| 参数 | 默认值 | 含义 |
| --- | --- | --- |
| `--departure` | 仿真模式 480 | 模拟日开始后的分钟数，即 08:00 |
| `--step` | 5 | 每次推进分钟数，必须大于 0，可为小数 |
| `--vehicle-id` | V01 | 车辆 ID |
| `--capacity` | 仿真模式 1000 | kg，初始装载所有选定任务，不中途补货 |
| `--unload-minutes` | 2 | 每个配送点的卸货时长，允许 0 |
| `--late-penalty` | 10 | **每个超时订单固定处罚分**，允许 0，非每分钟费率 |

时间窗口按**开始卸货时刻**判定：`service_start = max(arrival, earliest)`。早到等待；恰好 `latest` 开始卸货视为按时；超过则记录迟到分钟数。即使卸货结束超过截止，只要按时开始便不算迟到。这是本阶段按到达时间判断的明确实现口径。

卸货完成后扣减货物、标记任务完成，并且每个超时任务只记一次固定处罚。处罚分与运输成本分别统计。车辆状态为 idle / driving / waiting / unloading；行驶时 `node_id` 为空，记录真实道路及起终点、剩余时间和进度，不把上一节点当作当前位置。坐标按道路耗时在线性端点间插值，无实际道路几何数据。

`Planner::schedule()` 在既定路径上计算包含等待、卸货的预计到达/离开时间；`Simulation` 按事件时刻执行，步长内可以经历多个到达、等待或卸货事件。1 分钟和 5 分钟步长改变日志采样频率，不改变实际到达和处罚结果。返仓时停止，不补齐最后一个步长；跨天显示 `D+1 00:00`，内部始终保留连续分钟数，小数分钟在 CSV 中保留。

仿真导出保留第 3 阶段的 5 个网络/路线 CSV，另加：

- `simulation.csv`：初始、每步及最终状态，包含车辆坐标与已走道路数量。
- `events.csv`：精确事件时刻与状态，步长内事件也完整记录。
- `deliveries.csv`：到达、开始卸货、完成时刻，迟到分钟数与固定处罚。
- `simulation_options.csv`：车辆、起始时间、步长、卸货时长和处罚参数。

其中 `route.csv` / `stops.csv` 仍是第 3 阶段仅行驶时间的静态估计，**真实仿真时刻以事件和配送记录为准**。

安装绘图依赖的方法见下节。可用 Python 导出一帧、生成 GIF，或打开时间滑块及播放/暂停窗口：

```powershell
python scripts/plot_simulation.py --input-dir exports/simulation --frame 47 --output exports/vehicle.png
python scripts/plot_simulation.py --input-dir exports/simulation --gif exports/simulation.gif
python scripts/plot_simulation.py --input-dir exports/simulation --show
```

`--frame` 从 0 开始，47 适用于本例默认 5 分钟步长。使用虚拟环境时，将 `python` 换成 `.\.venv\Scripts\python.exe`。图中星形标出车辆；绿色、橙色、蓝色分别表示已走、正在走、未走的道路遍历，同一道路重复经过会分开显示。回放读取已生成数据，尚未连接实时 C++ 进程或在线路况。

默认示例：500 kg 货物、25 个配送点、08:00 出发，13:21 返仓；行驶 202 分钟、等待 69 分钟、卸货 50 分钟。D20 迟到 17 分钟、D25 迟到 1 分钟，各处罚 10 分，合计 20 分。见 [完整日志、快照和动画](docs/examples/stage4/README.md)。

仿真持有独立图副本，外部修改不会悄悄改变在途行程。订单通过 `insert_order()` 插入，路况通过 `enable_traffic()` 或 `apply_traffic()` 进入统一时钟；两者都在当前边/服务结束后衔接路线。

## 静态路径规划与可视化（保留第 3 阶段模式）

在项目根目录执行以下命令（Windows / Visual Studio）：

```powershell
# 从 W01 出发，配送全部配送点；480 分钟代表模拟日 08:00
.\build\Debug\logistics_app.exe --plan distance --warehouse W01 --departure 480 --check-size --export exports/distance
.\build\Debug\logistics_app.exe --plan cost --warehouse W01 --departure 480 --check-size --export exports/cost

# 只配送选定地点，并用节点默认 demand 校验一次装载量
.\build\Debug\logistics_app.exe --plan distance --warehouse W01 --deliveries D01,D08,D20 --capacity 100
```

绘图推荐使用项目内的 Python 虚拟环境（不要求激活环境）：

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r scripts/requirements.txt
.\.venv\Scripts\python.exe scripts/plot_network.py --input-dir exports/distance --output exports/distance-route.png
.\.venv\Scripts\python.exe scripts/plot_network.py --input-dir exports/cost --output exports/cost-route.png
```

Linux/macOS 使用 `.venv/bin/python` 和 `./build/logistics_app`，其余参数一致。若已有合适的 Python 环境，可直接 `python -m pip install -r scripts/requirements.txt` 后运行脚本。

左图按节点坐标显示完整网络、道路方向和权重标签；右图高亮所选道路，箭头旁的 `#` 表示真实道路遍历序号，同一道路重复经过时列出多个序号。仓库、配送点和中转站使用不同形状和颜色。脚本默认按规划策略显示距离或成本，可用 `--weight time` 查看耗时，或加 `--show` 打开可缩放的绘图窗口。默认不打开窗口，适合无图形桌面的环境。

无需安装绘图库也可以查看已保存的 [完整演示及复现方法](docs/examples/README.md)。

![距离策略网络与路线](docs/examples/distance-route.png)

### 规划规则与结果含义

- `--plan distance|cost` 分别选择距离/成本作为 Dijkstra 权重和最近邻选择依据；总距离、耗时、成本始终累加实际经过的道路，重复经过也重复计入。
- 未指定仓库时选择 ID 字典序最小的仓库；未指定配送点时处理全部 `delivery_point`。中转站和其他仓库可作为通行节点，不是必须配送的目的地。
- 从当前节点计算最短路径，在剩余配送点中选择目标权重最小的一个；代价相同按节点 ID 确定顺序。沿途经过其他待配送点时同时完成其配送，不重复计为未配送。
- “Delivery order” 是起点、实际配送停靠点和返仓终点；“Path nodes” 是每一条道路的端点序列，包含中转、回程和重复节点。
- 每次 Dijkstra 都跳过 `closed` 道路，使用当前权重。零权重道路合法；缺失节点是输入错误，不可达返回明确失败。过大而无法用有限 double 表达的候选路径不参与选择；实际输出的累计数值溢出则报错。
- 如某配送点不可达、无法返仓或载重不足，返回失败，不把尝试过的部分路线冒充完整可执行计划。不能据此断言贪心以外的所有访问顺序都不可行。
- `--capacity` 可选，单位 kg；检查全部所选节点 demand 的和。省略时不检查载重；不模拟补货、多趟运输或实际装卸。服务时间暂为 0，预计到达时间仅为出发时间加累计行驶时间，不检查窗口、不等待。
- `PlanningRequest` 保留剩余配送点、起始节点、返回仓库和出发时间接口；`PlannedStop` 保存预计时间和对应道路步号，供后续仿真使用。此接口当前只接受节点上的起点，不处理正在道路中途行驶的车辆。

示例结果（W01 出发、25 个配送点）：

| 贪心策略 | 总距离 | 总耗时 | 总成本 |
| --- | ---: | ---: | ---: |
| distance | 72.00 km | 202.00 min | 86.10 元 |
| cost | 76.00 km | 205.00 min | 89.70 元 |

成本策略只保证每次两点最短路按成本计算，**不保证整条贪心巡回路线更便宜**。本例中局部低成本选择形成了更长的后续行程，最终成本高于距离策略；这是最近邻启发式的局限，未将其表述为全局最优结果。

### CSV 导出接口

`--export DIRECTORY` 生成 `nodes.csv`、`edges.csv`、`route.csv`、`stops.csv`、`summary.csv`。保存完整网络、道路遍历顺序、逐步累计量、配送停靠事件和汇总。CSV 正确转义引号与逗号，数字使用固定小数点和足够精度。

导出前检查路线与当前网络的道路连接、状态、累计量和停靠记录是否一致；图被修改后，过期计划不能直接导出。绘图脚本只读取导出结果，不自行重新计算路线。日常导出建议放在被 Git 忽略的 `exports/`；`docs/examples/` 则保留可审查的演示产物。

## 图查看命令

在项目根目录执行（Windows / Visual Studio）：

```powershell
# 默认加载 config/network.json，输出统计和邻接表
.\build\Debug\logistics_app.exe

# 校验题目规定的规模，只显示摘要
.\build\Debug\logistics_app.exe --summary --check-size

# 输出距离矩阵；也可以使用 time 或 cost
.\build\Debug\logistics_app.exe --matrix distance

# 使用自定义 JSON，不强制满足演示规模，允许小图调试
.\build\Debug\logistics_app.exe --config config/network.json

.\build\Debug\logistics_app.exe --help
```

摘要示例：

```text
Logistics Route Planner v0.6.0
Nodes: 30 | Edges: 98
Warehouses: 2 | Delivery points: 25 | Transfer stations: 3
Course size requirements: PASS
```

Windows 单配置生成器的程序位于 build/logistics_app.exe；Linux/macOS 通常使用 ./build/logistics_app，参数相同。显式 --config 路径相对当前工作目录解析；省略时先查找当前目录的 config/network.json，再查找调用程序路径旁的 config/network.json。CMake 会复制演示配置到构建目录及程序旁，可直接进入 build/Debug 运行。若编辑了源配置，请重新构建以更新副本。

错误信息写到标准错误输出；输入或文件错误的退出码为 1，规划未找到可行行程为 2，成功为 0。--summary、--matrix、--plan 三种模式互斥，规划选项必须与 --plan 一起使用。文本输出使用英文标识以减少 Windows 控制台编码问题；字段含义见中文配置说明。

## 图接口与约定

```cpp
#include "logistics/graph/Graph.h"
#include <iostream>

int main() {
    logistics::Graph graph;
    graph.add_node(logistics::Node("W01", logistics::NodeType::Warehouse, 0, 0));
    graph.add_node(logistics::Node("D01", logistics::NodeType::DeliveryPoint,
                                   2, 0, 10, logistics::TimeWindow{540, 660}));
    graph.add_edge(logistics::Edge("E001", "W01", "D01", 2, 6, 3));
    graph.print_adjacency_list(std::cout);
    graph.print_adjacency_matrix(std::cout, logistics::WeightMetric::Distance);
    graph.remove_edge("W01", "D01");
    graph.remove_node("D01");
}
```

- 简单有向图：不允许自环、同方向平行边、重复节点 ID 或重复道路 ID。反向道路需要另行添加，权重可以不同。
- add_node/add_edge 对无效输入抛出 std::invalid_argument；remove_node/remove_edge 对存在项返回 true、缺失项返回 false。删除节点会清理所有入边和出边。
- 查询使用只读的 nodes()、find_node()、find_edge()、find_edge_by_id() 和 outgoing_edges()；找不到对象的 find_* 返回空指针，查询不存在节点的出边抛出 std::out_of_range。调用图修改操作后应重新获取查询引用，不跨修改保存指针。
- 邻接表和矩阵都按节点 ID 排序，输出可重复。矩阵行是起点、列是终点；对角线为 0，无直接边为 INF（内部 std::nullopt），存在的零权重边为 0.00。它是直接边权矩阵，不是最短路径结果。
- 结构输出保留 closed 道路，列表显示状态；Dijkstra 已实现跳过关闭道路。数值输出保留两位小数，内存权重保持完整 double 精度。
- Edge::weight() 统一三种权重，供 Dijkstra 使用；Graph::update_edge_traffic() 仅更新已校验数据并保留 base_time()，不推进时钟、不自动检测拥堵阈值或重规划。
- Graph 以邻接表存储，空间为 O(V+E)；矩阵按需生成，空间为 O(V²)。道路 ID 查询当前为 O(E)，适合课程规模；以后可增加索引。

## 后续迭代

1. 初始化、需求与骨架：已完成。
2. 图操作、JSON 配置、规模达标数据及测试：已完成。
3. Dijkstra、两种策略、最近邻配送顺序、可选载重检查、CSV 与绘图：已完成。
4. 时钟、车辆、逐步仿真、等待/卸货、固定处罚和固定路线车辆回放：已完成。
5. 按最新要求完成硬窗口、独立订单 JSON、动态取送插入、载重和路线衔接。
6. 实时路况、拥堵触发局部重规划、动态路线和 ETA 回放：已完成。
7. 后续按需推进性能优化、课程整体报告及最终展示。

静态模式使用最近邻，订单模式使用插入启发式，均不承诺全局最优。实时路况处理畅通/拥堵，封路及运行中删边尚未实现。仓库公开但项目自身尚未指定开源许可证；第三方库按其随附许可证使用。
