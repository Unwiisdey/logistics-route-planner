# 用户手册（v1.0.0）

## 环境与编译

C++17 编译器、CMake 3.16+。C++ 使用随仓库固定版本的 nlohmann/json 单头文件，无需联网下载；Python 3.10+ 只用于测试数据、自动演示及可选绘图。

Windows（Visual Studio C++ 工具已安装；在项目根目录）：

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\build\Release\logistics_app.exe --help
```

Linux / macOS 单配置生成器：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./build/logistics_app --help
```

Windows 本地实测环境与远端 CI 记录见 [测试报告](test-report.md)。请不要在源码目录直接构建，也不要把 build/、exports/、work/ 加入 Git。

## 从基本功能到动态演示

以下用 Windows 路径；其他平台替换可执行文件路径即可。

```powershell
# 30 节点、98 道路的规模检查
.\build\Release\logistics_app.exe --summary --check-size
# 邻接表（无选项）及邻接矩阵
.\build\Release\logistics_app.exe
.\build\Release\logistics_app.exe --matrix time
# 距离/成本规划；不含等待与卸货的静态结果
.\build\Release\logistics_app.exe --plan distance --export exports/distance
.\build\Release\logistics_app.exe --plan cost --export exports/cost
# 全部节点任务的旧固定路线仿真，展示实际迟到与固定处罚
.\build\Release\logistics_app.exe --plan distance --simulate --export exports/static-simulation
# 严格窗口、动态订单、真实仓库取货
.\build\Release\logistics_app.exe --plan distance --simulate --scenario config/orders.json --capacity 100 --export exports/orders
# 加入实时路况模拟和自动区间重规划
.\build\Release\logistics_app.exe --plan distance --simulate --scenario config/orders.json --capacity 100 --traffic --traffic-seed 42 --export exports/traffic
```

默认从字典序最小仓库出发。独立订单场景接受一笔紧急订单，拒绝一笔超载、一笔时限过紧请求；拒单是正常仿真结果，不是程序异常。

## 参数

| 参数 | 默认 / 说明 |
| --- | --- |
| `--config FILE` | config/network.json；显式路径相对当前目录 |
| `--summary` / `--matrix distance\|time\|cost` / `--plan distance\|cost` | 三种模式互斥；无模式输出邻接表 |
| `--check-size` | 强制题目规模；小图测试可省略 |
| `--warehouse ID` | 默认字典序最小仓库 |
| `--deliveries D01,D02` | 节点任务子集；不能与 --scenario 同用 |
| `--departure MINUTES` | 静态规划默认 0，仿真默认 480（08:00） |
| `--capacity KG` | 仿真默认 1000；静态模式仅显式指定时检查 |
| `--simulate` | 逐事件推进车辆 |
| `--hard-windows` | 用节点默认需求和窗口创建严格订单 |
| `--scenario FILE` | 独立订单 JSON，自动开启硬窗口，需要 --simulate |
| `--step MINUTES` | 5，必须正数；路况事件在步长内准确发生 |
| `--unload-minutes MINUTES` | 2，可为 0 |
| `--late-penalty POINTS` | 每超时订单固定 10 分，可改为其他非负值 |
| `--vehicle-id ID` | V01 |
| `--cache-capacity COUNT` | 256，整数 0～65536；0 关闭最短路缓存 |
| `--traffic` | 开启模拟路况，需要 --simulate |
| `--traffic-interval SECONDS` | 30；注意单位是秒，而 step/departure 是分钟 |
| `--traffic-fraction FRACTION` | 0.1，范围 (0,1]，选边数量向上取整 |
| `--traffic-seed UINT32` | 42；任一路况参数都会启用路况 |
| `--export DIRECTORY` | 保存 CSV；建议放 exports/ 下 |

配置字段和完整示例见 [配置说明](../config/README.md)。需求窗口用连续模拟分钟，例如 09:00～11:00 是 540～660；跨天允许超过 1440。成本单位元、距离 km、货物 kg。默认配置首先在当前工作目录查找，找不到时查找可执行文件旁的 config/network.json；显式 --scenario 必须提供有效路径。

## 可视化与一键演示

```sh
python -m venv .venv
# Windows 用 .venv/Scripts/python.exe；Linux/macOS 用 .venv/bin/python
python -m pip install -r scripts/requirements.txt
python scripts/demo.py --plots --gif
python scripts/plot_network.py --input-dir exports/distance --output exports/distance.png
python scripts/plot_simulation.py --input-dir exports/static-simulation --output exports/vehicle.png --frame 47
python scripts/plot_traffic.py --input-dir exports/traffic --frame 80 --output exports/traffic.png --show
```

请将上面的 python 替换为已安装依赖的虚拟环境解释器，或先激活环境。`demo.py` 自动查找 Release 程序；可用 `--app PATH` 明确指定，`--output DIR` 改输出位置。无 --plots/--gif 时只依赖 Python 标准库。它依次运行规模检查、两种规划、动态订单、路况场景和非法容量示例，并核对回放数据、空车返仓及所有已接任务完成；任何失败返回非零。

网络图显示节点类型、方向、权重。旧固定路线仿真用 plot_simulation；带路况的版本数据用 plot_traffic。路况图左侧红色为拥堵，右侧紫色为待执行路线、橙色为当前边、绿色为已走道路，星形为车辆。窗口提供滑块、播放/暂停；截图与 GIF 为导出数据回放，不是在线地图。

## 输出阅读

| 文件 / 日志 | 用途 |
| --- | --- |
| nodes / edges.csv | 初始网络；路况历史另存 |
| route / stops / summary.csv | 仅静态计划的路径及汇总 |
| simulation / events.csv | 车辆快照、事件、版本和 ETA |
| deliveries / insertions.csv | 接单结果、实际取送、迟到和固定罚分 |
| route_revisions / route_stops.csv | 已生效版本及取送动作 |
| remaining_routes.csv | 每帧待执行路线，可能尚未在当前边结束处生效 |
| traffic / replans.csv | 路况变化、触发原因、前后路线、搜索区间数、预测风险 |
| traversals.csv | 真实完成的道路，跨版本累计 |
| simulation_options / traffic_options.csv | 复现参数 |
| path_cache.csv | 容量、命中、未命中、失效、淘汰统计 |

Path cache 日志的 misses 表示实际最短路请求，hits 表示避免了重复搜索。缓存失效不等于立即全图重算。ETA 包含等待与卸货，但依赖当前权重，未来路况仍可能改变它。

## 排错

退出码 0 为成功；非法参数、配置、严格初始规划失败为 1；静态规划无法生成可行行程为 2。优先看 Error/Planning failed、订单拒绝原因和 window_risk。

- 找不到配置：在项目根目录运行，或传 --config / --scenario。修改配置后重新构建可更新程序旁副本。
- 初始载重过大：减少 initial_orders 数量或增加 --capacity，不会自动拆车。
- 窗口失败：检查模拟分钟、行驶和等待时间；“未找到可行插入”不等于不存在其他访问顺序。
- 找不到 matplotlib：使用同一个 Python 安装并运行 scripts/requirements.txt；纯 C++ 和无图演示不需要它。
- 旧查看器拒绝版本数据：带 --traffic 的导出使用 plot_traffic.py。
- Windows 发布包缺少运行库：安装匹配架构的 Microsoft Visual C++ 运行库，或在本机从源代码编译。
- 不同平台随机结果不同：随机分布/洗牌的标准库实现可不同；要重现同一图像直接读取仓库保存的 CSV。

发现缺陷时在 GitHub 使用缺陷模板，附版本、工具链、命令、种子和最小配置，勿上传个人信息。
