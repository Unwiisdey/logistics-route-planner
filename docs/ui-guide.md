# 正式 Web 界面使用说明

本地 Web 界面已纳入正式项目。浏览器负责展示，Python 标准库提供本机 HTTP 服务，logistics_ui_bridge 调用与 CLI 相同的 C++17 核心。无需 npm、matplotlib 或外网服务。

## 启动

需要 Python 3.9+、CMake 3.16+ 和 C++17 编译器。Windows 在项目根目录双击 **Start-UI.cmd**，或运行：

```sh
python scripts/start_ui.py
```

启动器配置 CMake、构建 UI 桥接、生成网页并打开 http://127.0.0.1:8765/ 。Windows 会查找 Visual Studio 自带 CMake，也可用 CMAKE_COMMAND 指定。保持终端运行，Ctrl+C 退出。只监听本机。

```sh
python scripts/start_ui.py --no-build
python scripts/start_ui.py --no-build --no-browser --port 8766
```

自定义构建目录使用 --build-dir PATH；LOGISTICS_UI_BRIDGE 可指定桥接程序。若端口被旧 Demo 占用，先停止旧服务或换端口。

标准构建默认生成 logistics_app 和 logistics_ui_bridge。只需 CLI 时用 -DLOGISTICS_BUILD_UI=OFF。无 Python 仍可编译 C++，但无法启动网页。

## 配送操作

1. 从 config/orders.json 的 3 笔初始订单开始，网络包含 30 节点、98 道路，不自动发布文件中的动态订单。
2. 点击“开始仿真”或“单步 +1 分钟”；速度只控制时间推进，不改变业务规则。
3. 填写仓库、配送点、货物量、优先级和时间窗口，提交动态订单。编号留空自动生成。展开表单及提交时暂停自动推进。
4. 示例：W01 → D05，15 kg，08:00—12:00。C++ 检查新旧全部窗口、取货、载重及返仓，可行才接受，拒绝时显示原因。
5. 货物实际到仓库取货时装车。当前道路、等待或卸货阶段不会中断，新路线从阶段结束后衔接。

本机服务只有一个共享仿真实例，多标签页操作同一状态。刷新保留状态，重置清除手动订单；关闭服务不持久保存本轮订单。

## 道路与任务

后续路径为道路节点；当前任务为第一个未完成业务动作；任务路径为剩余取送顺序。例如：

```text
当前任务：取货 · MANUAL1
任务路径：取货 · MANUAL1 → 送货 · MANUAL1 → 送货 · O02 → 返仓 · W01
```

任务来自 C++ 规划器，同点多单以 ID 区分；完成后移除，卸货中的动作保持首位，最终返仓后显示全部完成。

地图标签为道路当前预计耗时，随模拟拥堵变化；本车入边时锁定本段耗时。右侧分别显示总耗时、剩余耗时、道路当前预计耗时、本段到达和上段实际耗时。入边时为 6 分钟，途中标签变成 12 分钟，本车仍在入边后第 6 分钟到达。

默认每 30 秒模拟时间随机更新约 10% 的可模拟道路，seed=42。满足阈值时修复受影响区间。实际超时每单默认累计处罚 10 分，与运输成本分开。

## 回放与验证

“查看原记录回放”保留此前 393 帧演示，生成后 ui/dist/replay.html 可离线打开。回放不改变实时仿真。

```sh
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

找到 Python 时，CTest 包含 UI 桥接和 HTTP 集成检查。浏览器检查位于 ui/verify_live.cjs、ui/verify_demo.cjs，可按 ui/package.json 安装固定 Playwright 1.62.1（Apache-2.0）运行。Windows 使用 Edge，其他系统需相应 Chromium；交互检查会重置当前仿真。

本次正式集成的编译、98 项 CTest 和浏览器验证记录见 [UI 集成验证](reports/ui-integration.md)。
