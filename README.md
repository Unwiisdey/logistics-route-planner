# 电商物流配送路径规划系统

C++17 数据结构课程设计项目，使用有向带权图模拟城市配送网络。需求依据为《第2题 电商物流配送路径规划系统》，完整拆解见 [需求文档](docs/requirements.md)。

## 当前进度

当前完成 **第 3 阶段：单车辆基础路径规划与简单可视化**。支持按距离或成本运行 Dijkstra，使用贪心最近邻安排配送顺序，从仓库出发、覆盖待配送点并返回仓库；输出真实道路节点序列和三项总量。C++ 导出 CSV，Python 绘制网络和高亮路线。

当前仍不执行时间窗口约束、动态订单、模拟时钟或自动路况重规划；可视化采用静态绘图与可选绘图窗口，不是集成式调度 GUI。整体配送访问顺序是启发式结果，不保证巡回路线全局最优。

| 功能 | 状态 |
| --- | --- |
| C++17 工程、CMake、模块划分和需求文档 | 已完成 |
| Node / Edge 数据校验、Graph 邻接表封装、节点与边增删 | 已完成 |
| JSON 网络配置读取、错误定位、课程规模校验 | 已完成 |
| 邻接表、三种权重的邻接矩阵输出 | 已完成 |
| 示例网络：30 节点、98 有向道路；2 仓库、25 配送点、3 中转站 | 已完成 |
| Dijkstra 距离/成本策略、最近邻完整配送行程 | 已完成 |
| CSV 导出、Python 网络与路线图、演示图片和输出 | 已完成 |
| 37 项 CTest 测试、6 项 Python 数据检查 | 已通过 |
| 时间约束执行、动态订单、时钟与自动重规划 | 后续阶段 |

节点保存默认需求量和可选时间窗口；它们不等于动态订单。规划器当前以配送点集合为任务输入，支持可选载重检查，订单/车辆 JSON 解析与运行状态控制将在后续实现。

## 目录结构

```text
.
├── CMakeLists.txt
├── README.md
├── config/
│   ├── network.json                 # 可运行演示网络
│   └── README.md                    # JSON 字段、单位和约束
├── include/logistics/
│   ├── model/
│   │   ├── common.hpp               # ID、枚举、TimeWindow
│   │   ├── Node.h                   # 节点值对象
│   │   ├── Edge.h                   # 道路值对象
│   │   └── types.hpp                # 订单、车辆及基础类型入口
│   ├── graph/Graph.h                # 图操作与矩阵快照
│   ├── graph/graph.hpp              # 兼容第 1 阶段包含路径
│   ├── config/ConfigLoader.h        # 文件/流加载器
│   ├── config/configuration.hpp     # 后续完整仿真配置原型
│   ├── detail/checked_math.hpp      # 路线总量的有限数值检查
│   ├── planning/Dijkstra.h          # 单源最短路径及重建结果
│   ├── planning/Planner.h           # 最近邻单车辆规划请求
│   ├── planning/route_plan.hpp      # 完整道路与配送停靠序列
│   ├── simulation/simulation_state.hpp
│   ├── traffic/traffic_event.hpp
│   ├── visualization/Visualizer.h  # 网络/路线 CSV 导出
│   └── visualization/network_view.hpp
├── src/
│   ├── main.cpp                     # 命令行入口
│   ├── model/Node.cpp
│   ├── model/Edge.cpp
│   ├── graph/Graph.cpp
│   ├── config/ConfigLoader.cpp
│   ├── planning/Dijkstra.cpp
│   ├── planning/Planner.cpp
│   └── visualization/Visualizer.cpp
├── scripts/plot_network.py          # 使用 matplotlib 绘图
├── scripts/requirements.txt         # 可选 Python 依赖
├── tests/                           # 图、加载器和应用测试
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

已在 Windows、MSVC 19.29、Visual Studio 2019 Build Tools、x64 Debug 下完成编译，并通过全部 37 项 CTest 测试；另外通过 6 项 Python 数据检查。其他 C++ 工具链尚未实际验证。测试内容见 [测试说明](tests/README.md)。

Python 数据检查不需要安装 matplotlib：

```sh
python -m unittest discover -s tests -p test_plot_network.py -v
```

## 路径规划与可视化

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
Logistics Route Planner v0.3.0
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
- Edge::weight() 统一三种权重，供后续 Dijkstra 使用；Graph::update_edge_traffic() 仅更新已校验数据并保留 base_time()，不推进时钟、不自动检测拥堵阈值或重规划。
- Graph 以邻接表存储，空间为 O(V+E)；矩阵按需生成，空间为 O(V²)。道路 ID 查询当前为 O(E)，适合课程规模；以后可增加索引。

## 后续迭代

1. 初始化、需求与骨架：已完成。
2. 图操作、JSON 配置、规模达标数据及测试：已完成。
3. Dijkstra、两种策略、最近邻配送顺序、可选载重检查、CSV 与绘图：已完成。
4. 订单/车辆配置、模拟时钟、车辆位置、时间窗口执行、超时记录和动态紧急订单。
5. 路况模拟、拥堵阈值触发、剩余路线重规划与高亮同步。
6. 优化、整体验证、最终文档和展示答问。

按本阶段要求采用最近邻启发式，不追求全局最优。第 4 阶段仍需明确 penalty 系数与动态订单取货规则，详见需求文档。仓库公开但项目自身尚未指定开源许可证；第三方库按其随附许可证使用。
