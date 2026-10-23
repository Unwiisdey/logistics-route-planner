# 物流配送 UI Demo

此目录保留两次独立 UI 演示：

- **记录回放版**：interface.html + replay-data.json，播放/暂停、时间轴、事件定位、道路耗时、车辆位置。表单只校验输入。
- **实时插单版**：live.js + bridge.cpp + serve_demo.py，手动提交订单，由现有 C++ 算法检查载重、取货及全部订单窗口，接受后实际取货、配送、更新路线。

这是可选的本地网页版 Demo，不由项目默认构建启动，不需要发布外网。它与 CLI 共用 C++ 核心，未修改核心算法。

## 编译和启动

需要 C++17、CMake 3.16+、Python 3.9+。在仓库根目录执行：

```sh
cmake -S demos/ui -B demos/ui/bridge-build -DCMAKE_BUILD_TYPE=Release
cmake --build demos/ui/bridge-build --config Release
python demos/ui/build_live.py
python demos/ui/serve_demo.py
```

打开 http://127.0.0.1:8765/ 。Windows 编译完成后也可双击 Start-Demo.cmd（要求 python 在 PATH 中）。保持服务运行，Ctrl+C 停止。dist/replay.html 可离线打开；实时版需要本地服务。生成页面、可执行文件及截图不纳入 Git。

从 3 笔初始订单开始，不自动加载预设动态订单。填写并提交订单，接受后点击“开始仿真”继续。展开表单或提交时暂停自动推进。编号留空自动生成；示例：W01 → D05，15 kg，08:00—12:00。超载、不可达或窗口不可行会拒绝插入。

本地服务为单个共享仿真实例，多标签页操作同一状态。刷新保留运行状态，“重置仿真”清除手动订单。

## 道路耗时

地图标签为道路当前预计通行耗时；本车进入道路时锁定本段总耗时，途中不反复延长。右侧分别显示本段总耗时、剩余耗时、道路当前预计耗时、预计到达及上段实际耗时。默认每 30 秒模拟时间随机更新约 10% 的可模拟道路，seed=42。

## 文件与数据

回放数据来自本项目已验证的 C++ 演示：30 节点、98 道路、393 帧。replay-data.json 是重建页面需要的演示数据，无本机路径。实时版从仓库 config/network.json、config/orders.json 读取网络和初始订单。

build_demo.py 生成回放页；build_live.py 生成实时页及回放入口。Python 服务按目录自动定位项目、桥接程序，可用 LOGISTICS_PROJECT、LOGISTICS_DEMO_BRIDGE 环境变量覆盖。

## 验证

```sh
python demos/ui/test_bridge.py
```

覆盖接受与路线改变、取货后装载并配送、超载/窗口拒绝、无效输入、重复 ID、当前道路耗时锁定。无第三方 Python 依赖；verify_live.cjs 提供可选的 Playwright 浏览器检查。

Demo 保留独立入口，正式 UI 产品化及外网部署另行决定。
