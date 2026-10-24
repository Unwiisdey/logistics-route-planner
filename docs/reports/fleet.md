# 省时兜底与双车验证

2026-10-24，Windows x64 / MSVC 19.29 / C++17 Release。

## 结果

- Release 编译通过，无编译警告；CTest 112/112 通过，包含原有 98 项及 11 项双车/兜底测试、3 项新 CLI 检查。
- Python CSV 回放数据测试 23/23 通过。
- 距离策略通过 app.fleet 端到端测试；另实际执行成本策略双车、动态订单、路况与导出命令，完成 4 单，0 超时，0 分处罚；两车实际返仓约为 09:06、10:28，完成时间分别保留。
- ui/ 下无代码修改；原 ui.bridge / ui.http 回归套件通过。双车网页尚未实现，本次只交付设计方案。

## 关键检查

| 检查 | 实际验证 |
| --- | --- |
| 兜底 | 直接路线 1 km / 20 分钟，绕行 6 km / 10 分钟，15 分钟窗口采用绕行；距离/成本均覆盖；宽窗口保留直接道路 |
| 约束 | 兜底后仍保护旧单截止时刻、取货先后、容量；最快方案仍迟到或超载则拒绝 |
| 共享对象 | 比较两车 network()、clock() 与车队返回引用的地址，确认是同一份实际对象 |
| 分配 | 两车容量各 5、初始两单各 4 时分到不同车辆；双仓库 API 按实际仓库装货并各返原仓 |
| 紧急 | 两车均可接时优先更早服务的一辆；只有一车可接时选可行车辆；平局按 ID；落选车辆任务与日志不变 |
| 固定归属 | 完成、拥堵和插入新单后，原订单仍归属原车；已完成 ID 不能再分配 |
| 拒绝 | 超载、未来发布、不可满足截止时间时列出两车原因，不改变路线版本或货物 |
| 路况 | 确定性批次广播一次；入边耗时锁定；只搜索受影响未来区间；非法批次保留原图 |
| 统一随机源 | 与同 seed 的单个 TrafficSimulator 首批样本逐边比对，两车看到同一批变化 |
| 时间 | 早返仓后时钟继续共享、实际完成时间和 ETA 保留；零耗时同时完成、在卸货时插单、全部返仓后接新单 |
| 一致性 | 步长 0.1 / 1 / 5 / 100，缓存 0 / 256，动态订单归属和实际服务时间一致 |
| 导出 | assignments.csv、共享 traffic_options.csv、每车 deliveries/路线/事件等 CSV；共享种子 42、0.5 分钟间隔正确保存 |

## 复现

```powershell
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
python -m unittest discover -s tests -p "test_plot*.py" -v
.\build\Release\logistics_app.exe --plan cost --simulate --vehicles 2 --scenario config/orders.json --capacity 40 --traffic --export exports/fleet-cost
```

日志和构建产物位于忽略目录 work/、build/、exports/，不提交仓库。远端 Windows / Ubuntu 编译及测试结果以本次提交的 GitHub Actions 为准；不同平台随机分布实现可能产生不同精确时刻。
