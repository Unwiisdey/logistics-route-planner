# 电商物流配送路径规划系统

C++17 数据结构课程设计项目，使用有向带权图模拟城市配送网络。需求依据为《第2题 电商物流配送路径规划系统》，完整拆解见 [需求文档](docs/requirements.md)。

## 当前进度

当前完成 **第 2 阶段：图数据结构与 JSON 配置加载**。程序可以读取网络、校验输入，并输出邻接表或距离/耗时/成本邻接矩阵。尚未实现 Dijkstra、完整配送路线、时间推进、动态订单、自动路况模拟和图形界面。

| 功能 | 状态 |
| --- | --- |
| C++17 工程、CMake、模块划分和需求文档 | 已完成 |
| Node / Edge 数据校验、Graph 邻接表封装、节点与边增删 | 已完成 |
| JSON 网络配置读取、错误定位、课程规模校验 | 已完成 |
| 邻接表、三种权重的邻接矩阵输出 | 已完成 |
| 示例网络：30 节点、98 有向道路；2 仓库、25 配送点、3 中转站 | 已完成 |
| 单元测试与命令行测试：20 项 CTest 测试 | 已通过 |
| 路径规划、图形展示、时间约束与动态调度 | 后续阶段 |

节点保存默认需求量和可选时间窗口；它们不等于动态订单。现有 Order、Vehicle、模拟状态与展示接口继续保留，车辆/订单配置解析和业务行为尚未实现。

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
│   ├── planning/route_plan.hpp
│   ├── simulation/simulation_state.hpp
│   ├── traffic/traffic_event.hpp
│   └── visualization/network_view.hpp
├── src/
│   ├── main.cpp                     # 命令行入口
│   ├── model/Node.cpp
│   ├── model/Edge.cpp
│   ├── graph/Graph.cpp
│   └── config/ConfigLoader.cpp
├── tests/                           # 图、加载器和应用测试
├── docs/requirements.md
└── third_party/nlohmann/             # JSON 单头文件和 MIT 许可证
```

configs/README.md 是第 1 阶段旧目录的迁移说明；实际配置统一使用 config/。构建目录 build/、临时工具与原始课程 Word 文件不纳入 Git。

## 技术栈

- C++17、CMake 3.16+，MSVC / GCC / Clang 等支持 C++17 的编译器。
- [nlohmann/json v3.12.0](https://github.com/nlohmann/json/releases/tag/v3.12.0)，固定版本单头文件已纳入项目，并核验官方 SHA-256；构建无需联网。来源与许可证见 [第三方依赖](third_party/README.md)。
- 核心为 logistics_core 静态库。JSON 头文件仅供加载器实现使用；图和数据模型不依赖 JSON 或 GUI。
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

已在 Windows、MSVC 19.29、Visual Studio 2019 Build Tools、x64 Debug 下完成编译，并通过全部 20 项 CTest 测试；其他工具链尚未实际验证。测试内容见 [测试说明](tests/README.md)。

## 运行

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
Logistics Route Planner v0.2.0
Nodes: 30 | Edges: 98
Warehouses: 2 | Delivery points: 25 | Transfer stations: 3
Course size requirements: PASS
```

Windows 单配置生成器的程序位于 build/logistics_app.exe；Linux/macOS 通常使用 ./build/logistics_app，参数相同。显式 --config 路径相对当前工作目录解析；省略时先查找当前目录的 config/network.json，再查找调用程序路径旁的 config/network.json。CMake 会复制演示配置到构建目录及程序旁，可直接进入 build/Debug 运行。若编辑了源配置，请重新构建以更新副本。

错误信息写到标准错误输出，退出码为 1；成功为 0。--summary 与 --matrix 不能同时使用。文本输出使用英文标识以减少 Windows 控制台编码问题；字段含义见中文配置说明。

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
- 结构输出保留 closed 道路，列表显示状态；后续 Dijkstra 必须自行跳过关闭道路。数值输出保留两位小数，内存权重保持完整 double 精度。
- Edge::weight() 统一三种权重，供后续 Dijkstra 使用；Graph::update_edge_traffic() 仅更新已校验数据并保留 base_time()，不推进时钟、不自动检测拥堵阈值或重规划。
- Graph 以邻接表存储，空间为 O(V+E)；矩阵按需生成，空间为 O(V²)。道路 ID 查询当前为 O(E)，适合课程规模；以后可增加索引。

## 后续迭代

1. 初始化、需求与骨架：已完成。
2. 图操作、JSON 配置、规模达标数据及测试：已完成。
3. Dijkstra、两种策略、多配送点访问顺序、车辆/订单配置与载重检查、图形展示。
4. 模拟时钟、车辆位置、时间窗口执行、超时记录和动态紧急订单。
5. 路况模拟、拥堵阈值触发、剩余路线重规划与高亮同步。
6. 优化、整体验证、最终文档和展示答问。

第 3/4 阶段仍需明确全局最优口径、penalty 系数与动态订单取货规则，详见需求文档。仓库公开但项目自身尚未指定开源许可证；第三方库按其随附许可证使用。
