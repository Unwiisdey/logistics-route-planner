# 用户手册（含正式 Web 界面）

## 图形界面入口

Windows 双击项目根目录 Start-UI.cmd，或运行 `python scripts/start_ui.py`。启动器会自动构建 C++ 桥接程序、生成页面并打开本机浏览器。需要 Python 3.9+、CMake 和 C++17 编译器；保持终端运行，Ctrl+C 停止。

界面支持手动插单、车辆位置、道路耗时、当前任务、取送任务路径和仿真控制。完整流程见 [正式 UI 手册](ui-guide.md)。以下仍可直接使用 CLI，不必启动网页。

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
# 30 节点、82 道路的规模检查
.\build\Release\logistics_app.exe --summary --check-size
# 邻接表（无选项）及邻接矩阵
.\build\Release\logistics_app.exe
.\build\Release\logistics_app.exe --matrix time
# 距离/成本规划；不含等待与卸货的静态结果
.\build\Release\logistics_app.exe --plan distance --export exports/distance
.\build\Release\logistics_app.exe --plan cost --export exports/cost
# 全部节点任务的旧固定路线仿真，展示实际迟到与按分钟计罚
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
| `--late-penalty-per-minute POINTS` | 每开始迟到一分钟的处罚分，默认 1；迟到分钟向上取整 |
| `--late-penalty POINTS` | 兼容别名，当前同样表示每分钟分值，不再是每单固定分 |
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
| deliveries / insertions.csv | 接单结果、实际取送、迟到时长和分钟罚分 |
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
- 初始载重过大：单车模式减少 initial_orders 或增加 --capacity；双车使用 --vehicles 2，仍不拆分同一笔订单。
- 窗口失败：检查模拟分钟、行驶和等待时间；“未找到可行插入”不等于不存在其他访问顺序。
- 找不到 matplotlib：使用同一个 Python 安装并运行 scripts/requirements.txt；纯 C++ 和无图演示不需要它。
- 旧查看器拒绝版本数据：带 --traffic 的导出使用 plot_traffic.py。
- Windows 发布包缺少运行库：安装匹配架构的 Microsoft Visual C++ 运行库，或在本机从源代码编译。
- 不同平台随机结果不同：随机分布/洗牌的标准库实现可不同；要重现同一图像直接读取仓库保存的 CSV。

发现缺陷时在 GitHub 使用缺陷模板，附版本、工具链、命令、种子和最小配置，勿上传个人信息。

## 双车命令行

```powershell
.\build\Release\logistics_app.exe --plan distance --simulate --vehicles 2 --scenario config/orders.json --capacity 40 --traffic --export exports/fleet
```

--vehicles 省略或 1 时沿用单车；只接受 1/2，必须搭配 --simulate。2 自动使用订单硬窗口规划，可结合 --scenario，或用 --deliveries 从节点需求生成订单。两辆车从 --warehouse 出发，容量分别为 --capacity；默认 ID 为 V01/V02，修改 --vehicle-id 后第二辆追加 _2。

订单先按约束判断，再比较方案。距离/成本路径预计超时，会尝试耗时最短；仍不满足全部剩余窗口、取送和容量则拒绝。旧静态 --plan（未进入硬窗口订单模式）不改变算法。

导出目录含 assignments.csv、V01/、V02/；可分别将车辆子目录交给原 plot_traffic.py 回放，尚无两车叠加画图。现有 Web UI 仍为单车，不会因启动器自动切换双车；[双车 UI 设计方案](fleet-ui-design.md) 仅为后续设计。

## 从中转站取货

运行 config/hub-orders.json：双车用 --vehicles 2 --capacity 40，单车用 --vehicles 1 --capacity 100；其余沿用 --plan distance --simulate --scenario config/hub-orders.json --traffic。HUB01 在 08:02 发布，货物位于 T02，送往 D18；超载和极窄窗口示例会明确拒绝。

正式页面“取货地点”提供仓库与中转站；当前任务显示“取货 · 订单ID（站点ID）”，地图下方显示各站待取数量。货物只在实际取货时装上车；车辆仍返回原仓库。详细规则见 transfer-stations.md。保存的旧演示和 replay.html 保持历史 98 边网格，当前主页面使用新路网。
