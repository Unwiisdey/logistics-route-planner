# 新路网与中转站取货验证

2026-10-25，Windows x64 / MSVC 19.29 / C++17 Release。

- 编译通过；121/121 CTest 通过，含旧功能回归、7 项新增 pickup 测试及 2 项新单车/双车 CLI 场景。
- 23/23 Python CSV 回放数据测试通过，旧 CSV 读取器可继续读取追加来源列后的导出。
- ui/verify_live.cjs 浏览器检查通过：真实手动插入、拒单、播放暂停、当前边锁定耗时、实际完成返仓、刷新保持状态、320/390/1024/1280 宽度及无 JS 错误。
- ui/verify_hubs.cjs 通过：下拉只提供两个仓库和三个中转站；T02 → D18 的 HUB-DEMO 实际接受、到站取货、送达并空车返回 W01；取货前等待清单为 1，取货后为 0。
- 当前配置含 30 节点、82 有向边，D12 ↔ D18 两向均存在；规模、连通性及绘图交叉检查通过。docs/maps/network.svg/PNG 与 docs/maps/ui-hub.png 已实际检查。

## 业务边界

验证旧 warehouse_id 与新 pickup_node_id 均可读取，同时出现或都缺失会报错；配送点不能当取货源。动态取货前载荷保持原值，执行 Pickup 后增加，送达后卸货，完成后仍返原仓。重复 ID、超载、无法满足窗口都不会新增待取货物或改变原车辆计划。过路不装货，双车只有选中者实际领取；拥堵、不同步长下取货/服务结果保持一致。

货物备妥是本次明确的模型假设，不是已实现的库存/补货服务。取货瞬时，不引入装货耗时。历史演示数据保持原路网和原记录，当前运行结果以新配置为准。

## 复现

```powershell
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
python -m unittest discover -s tests -p "test_plot*.py" -v
python scripts/draw_network_map.py
python scripts/start_ui.py --no-build
# 另开终端；需安装 Playwright，指定 DEMO_PLAYWRIGHT（若不在默认模块路径）
node ui/verify_live.cjs
node ui/verify_hubs.cjs
```

构建、临时日志及运行 CSV 留在忽略目录。GitHub Windows / Ubuntu 自动构建、测试和演示状态以本次提交的 Actions 结果为准。
