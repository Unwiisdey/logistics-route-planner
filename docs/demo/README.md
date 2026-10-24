# 演示与答辩安排

本目录的截图和 GIF 来自 v1.0.0 Release 实际运行。GIF 从 393 个记录帧中取 32 帧、4 fps，保留末帧；是加速抽样展示，不代表墙钟与模拟时间 1:1。

![路况与当前路线](traffic.png)

![完整配送回放](traffic.gif)

## 一键准备

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
python -m pip install -r scripts/requirements.txt
python scripts/demo.py --app build/Release/logistics_app.exe --output exports/final-demo --plots --gif
```

结果在 exports/final-demo：5 类日志、两种静态规划数据、订单数据、路况数据、summary.json、traffic.png、traffic.gif。若现场无 Python，可先展示已保存 GIF，再执行 C++ 命令和日志。请提前运行一次，确认配置路径和 Python 依赖；不要在答辩现场首次安装工具。

## 8 分钟讲解脚本

| 时间 | 操作 | 说明 |
| --- | --- | --- |
| 0:00–0:45 | 展示 README 与需求表 | 单车辆、30 节点、98 有向道路；说明仓库/配送点/中转站 |
| 0:45–1:30 | `--summary --check-size`，打开 network.json | 图、边权、配置校验和邻接表，矩阵按需生成 |
| 1:30–2:30 | 分别 `--plan distance` / `--plan cost` | 两点 Dijkstra 与访问顺序是两个层次；贪心整条路线不保证最优 |
| 2:30–3:45 | 展示 03-orders.txt、insertions.csv | 紧急单真实回仓取货；超载和时限过紧的拒单；载荷不凭空增加 |
| 3:45–5:00 | 打开 GIF 或 `plot_traffic.py --show` | 当前边锁定；拥堵变红、未来路径紫色、车辆星形、ETA 随事件变化 |
| 5:00–6:00 | 展示 replans.csv 的 searched_legs | 只对受影响取送区间找路，不重排全部订单；无法绕开时报告迟到风险 |
| 6:00–7:00 | 打开性能中位数与 cache_tests.cpp | 缓存版本失效、边外降耗时反例、关闭缓存对照、实际搜索次数下降 |
| 7:00–8:00 | 展示测试报告、GitHub Release 与项目管理 | 96 项 CTest、23 项 Python、CI 和版本管理；主动交代局限 |

## 现场命令备忘

```powershell
.\build\Release\logistics_app.exe --summary --check-size
.\build\Release\logistics_app.exe --plan distance
.\build\Release\logistics_app.exe --plan cost
.\build\Release\logistics_app.exe --plan distance --simulate --scenario config/orders.json --capacity 100 --traffic --export exports/live-demo
python scripts/plot_traffic.py --input-dir exports/live-demo --show
ctest --test-dir build -C Release -R "cache|integration|traffic" --output-on-failure
```

不要逐屏展示全部 4000 行路况日志。选取一次真实路径变化、一笔拒单、一项缓存失效测试即可；日志有完整证据可供追问。演示应区分“候选新路线”和“已生效版本”，当前边结束前的多次候选可能被再次替换。

## 常见答辩问题

1. **为什么用邻接表？** 城市道路通常稀疏，O(V+E) 空间适合遍历出边；矩阵 O(V²) 仅用于题目要求的输出。
2. **Dijkstra 能用负权吗？** 不能。本项目输入拒绝负权，用最小堆处理非负权；零权合法，并用严格改善避免前驱环。
3. **优先队列如何降复杂度？** 不再每轮线性扫描全部未访问节点，堆负责取最小暂定距离，旧条目出堆时跳过。
4. **多点顺序为什么不是 Dijkstra 直接完成？** Dijkstra 解决两点/单源问题；多点巡回属于更复杂组合优化，本项目用最近邻或订单插入启发式。
5. **成本策略为什么可能更贵？** 每次局部成本最小不保证整条巡回更便宜；阶段 3 示例有真实反例，不能宣称全局最优。
6. **缓存里放什么？** 图版本下的源、目标、权重类型对应完整路径与三种累计量，包含不可达结果；默认最多 256 项，LRU 淘汰。
7. **为什么边不在旧路径上也清缓存？** 那条边变快后可能形成新最优路；只看旧路径交集会漏掉它。距离路径的耗时汇总也会过期。
8. **清缓存是否违反增量重规划？** 清缓存不执行搜索。只有受影响取送区间有查询，其余路线保留，后续 ETA 只线性累加。
9. **20% 与 30 秒怎么算？** 相对变化前耗时增加至少 20%，30 秒是 0.5 模拟分钟，不是让程序 sleep；同刻事件有固定顺序。
10. **车辆会不会瞬移？** 当前道路保留入边耗时，驶完到下一节点才切换；等待/卸货也先完成，动态货物必须在指定仓库装入。
11. **“硬窗口”为什么仍有迟到？** 接单时严格保证当前条件下可行，未来不可预知拥堵仍可破坏承诺；保留已接订单、记录风险和按迟到时长计算的实际罚分，拒绝进一步破坏旧窗口的新单。
12. **罚分怎样计算？** 开始卸货时的迟到分钟数向上取整，再乘每分钟分值（默认 1）；迟到 1 秒计 1 分、2 分 30 秒计 3 分。实际完成时只记一次，不与运输费用直接相加。
13. **早到怎么办？** 等待到 earliest 再开始卸货；恰好 latest 开始服务算按时，卸货耗时影响后续订单。
14. **如何证明优化没有改错结果？** 随机图与 Floyd 对照，开/关缓存逐项比较路况仿真的接单、道路序列、到达时刻和罚分。
15. **性能数据可靠吗？** 同一 Release、相同请求、五轮中位数，检查实际搜索次数和路径；不承诺任意工作负载都得到该倍数。
16. **如何扩展？** 道路 ID 索引、事件流存储、可证明的精细失效、多车与在线界面。新增功能要补业务规则和测试，当前没有实现真实交通平台。

## 提交前检查

从干净 checkout 构建、跑测试与 demo；确认 README 图像可读、配置随程序可用、Release 标签指向最终提交、issue/milestone 状态与真实完成情况一致。Git 不包含构建目录、临时工具、登录凭据或原始课程 Word。
