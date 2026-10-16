# 电商物流配送路径规划系统

使用 C++17 开发的数据结构课程设计项目。系统计划使用有向带权图模拟城市物流配送网络，围绕仓库、配送点和中转站，实现单车辆配送规划、订单约束处理及动态调度。

需求依据为课程题目《第2题 电商物流配送路径规划系统》。详细需求、原文要求与设计约定的区别见 [需求拆解](docs/requirements.md)。

## 当前进度与功能列表

当前为第 1 阶段，已提供可编译入口、数据结构声明、模块目录和需求文档。运行程序只输出阶段说明，不读取配置、不计算路线、不启动图形界面。结构体用于讨论数据接口，尚不具备数据校验或业务行为。

| 功能 | 状态 | 计划阶段 |
| --- | --- | --- |
| C++17 工程、CMake 配置、数据模型与需求拆解 | 已完成 | 1 |
| 有向带权图、节点和边增删、配置读取、邻接表输出 | 待实现 | 2 |
| Dijkstra、两种权重策略、完整配送路线、基本载重检查 | 待实现 | 3 |
| 配送网络图形展示与路线高亮 | 待实现 | 3 |
| 时间窗口、模拟时钟、车辆位置、动态紧急订单 | 待实现 | 4 |
| 路况模拟、拥堵触发重规划与界面更新 | 待实现 | 5 |
| 整体优化、完整场景验证、最终文档和演示 | 待完成 | 6 |

## 目录结构

```text
.
├── CMakeLists.txt              # 构建入口，要求 C++17
├── README.md                   # 项目状态、构建运行与迭代计划
├── .gitignore                  # 构建产物、临时文件和 IDE 状态
├── .gitattributes              # 文本换行与二进制文件规则
├── .editorconfig               # 编码、缩进和换行约定
├── include/logistics/          # 核心模块公开声明
│   ├── model/types.hpp         # 节点、道路、订单、车辆等基础类型
│   ├── graph/graph.hpp         # 邻接表存储原型
│   ├── config/configuration.hpp # 配置读取后的数据集合
│   ├── planning/route_plan.hpp # 策略、停靠点和路线结果
│   ├── simulation/simulation_state.hpp # 时钟及运行状态
│   ├── traffic/traffic_event.hpp # 路况变化事件
│   └── visualization/network_view.hpp # 与 GUI 框架无关的展示接口
├── src/main.cpp               # 当前可运行的控制台入口
├── configs/README.md           # 后续配置与演示数据约定
├── tests/README.md             # 分阶段验证计划
└── docs/requirements.md        # 需求、验收点和待定规则
```

当前核心部分只有头文件声明，因此 `logistics_core` 使用 CMake 的 `INTERFACE` 目标管理包含路径和 C++ 标准。后续增加算法实现时，在 `src/` 下按相同模块划分 `.cpp` 文件，将核心目标改为普通库并显式列出源文件；应用入口与核心逻辑保持分离。`build/` 由构建过程生成，不纳入版本控制。

## 技术栈与代码约定

- C++17 标准库；当前没有第三方依赖。
- CMake 3.16 或更新版本。
- 支持 C++17 的编译器，例如 MSVC、GCC 或 Clang。
- 图采用邻接表原型；图形界面方案和配置格式在后续阶段确定。
- 类型使用 `PascalCase`，函数、变量、成员和文件名使用 `snake_case`；代码位于 `logistics` 命名空间。
- UTF-8 编码，4 空格缩进，头文件独立包含其依赖；业务模块不直接依赖图形界面框架。

## 编译方式

先安装 CMake 和 C++17 编译工具链，并保证 `cmake` 可在终端中运行。Windows 使用 Visual Studio 时，需要安装“使用 C++ 的桌面开发”或对应 Build Tools 组件；必要时使用开发者终端。

在项目根目录执行：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

`CMAKE_BUILD_TYPE` 用于 Makefiles、Ninja 等单配置生成器；Visual Studio 等多配置生成器由构建命令中的 `--config Debug` 选择配置。

如果当前 Windows 终端找不到 `cmake`，但已安装 Visual Studio 2019 Build Tools 的 CMake 组件，可使用以下方式（其他版本需调整路径和生成器）：

```powershell
$taskCmake = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
& $taskCmake -S . -B build -G "Visual Studio 16 2019" -A x64
& $taskCmake --build build --config Debug
```

同一个构建目录应使用同一个生成器；切换工具链时使用新的构建目录。当前骨架已在 Windows、Visual Studio 2019 Build Tools、MSVC 19.29、x64 Debug 配置下完成编译与运行验证；其他平台尚未实际验证。

## 运行方式

Windows，使用 Visual Studio 生成器：

```powershell
.\build\Debug\logistics_app.exe
```

Windows，使用 Ninja 或 MinGW Makefiles 等单配置生成器：

```powershell
.\build\logistics_app.exe
```

Linux / macOS，使用单配置生成器：

```sh
./build/logistics_app
```

预期输出：

```text
Logistics Route Planner v0.1.0
Stage 1: project skeleton and requirements.
Configuration loading, routing, simulation and GUI are not implemented.
```

当前没有命令行参数、可加载的示例配置或自动化业务测试；分阶段验证计划见 [tests/README.md](tests/README.md)。

## 后续迭代计划

1. **初始化与需求拆解：** 完成工程骨架、模型原型、模块分工与需求文档，对应第一次检查的准备工作。
2. **图与配置：** 实现图操作、配置校验和邻接表输出；准备至少 30 个节点、50 条有向路径的演示数据，包含至少 2 个仓库、15 个配送点、3 个中转站。
3. **基础规划与可视化：** 实现 Dijkstra、距离与成本两种策略、配送访问顺序和返回仓库的完整路线，增加载重可行性检查与网络展示，准备第二次检查。
4. **时间与订单：** 实现模拟时钟、车辆运行位置、时间窗口、超时记录和紧急订单插入。
5. **路况与重规划：** 模拟拥堵与畅通，按题目阈值触发剩余路线重规划，同步更新高亮。
6. **优化与交付：** 完成跨模块测试、异常处理、算法改进、文档与演示，准备第三次检查。每个阶段均进行对应验证，不把所有测试推迟至最后。

仓库收录源代码、项目文档和构建配置；原始课程题目 Word 文件、构建产物和临时工具保留在本地。项目尚未指定开源许可证，仓库公开不代表授予开源许可。
