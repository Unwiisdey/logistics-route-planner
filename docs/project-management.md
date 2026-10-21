# 最终交付与 GitHub 管理

最终版本：`v1.0.0`。所有阶段在同一仓库保留提交历史，发布标签固定代码和文档；发布后如需修复应新增提交及补丁版本，不移动已有正式标签。

## 验收条目

| 条目 | 范围与证据 |
| --- | --- |
| [Milestone 1：v1.0.0 最终课程设计交付](https://github.com/Unwiisdey/logistics-route-planner/milestone/1) | 关联以下三个验收 issue；全部完成后关闭 |
| [Issue 1：缓存与增量规划优化](https://github.com/Unwiisdey/logistics-route-planner/issues/1) | 版本失效、LRU、缓存开关对照和性能数据 |
| [Issue 2：综合测试与双平台 CI](https://github.com/Unwiisdey/logistics-route-planner/issues/2) | 本地 Debug/Release、Python、多种子集成、Ubuntu/Windows CI |
| [Issue 3：文档、答辩与正式发布](https://github.com/Unwiisdey/logistics-route-planner/issues/3) | 四类文档、一键演示、截图/GIF、答辩问答及 Release |

条目是对真实交付的验收记录，并非捏造开发过程时间线。已完成的本地事项勾选，远端 CI 与发布只有核验成功后才勾选并关闭；最新状态见 GitHub 页面。

代码交付提交为 `6f592be`；[双平台 CI 实测成功记录](https://github.com/Unwiisdey/logistics-route-planner/actions/runs/39587424534) 同时覆盖 Ubuntu 和 Windows。后续提交只整理发布验收文档，正式代码与文档以 v1.0.0 标签为准。

## 变更与发布流程

1. 修改代码时同步相关配置、API、手册，审查差异。
2. 跑相关测试；跨模块最终交付另做 Release、端到端和性能检查。
3. 获取远端进展，创建描述明确的提交，正常推送，不强制覆盖历史。
4. 读取 GitHub CI 实际结论；不把本机通过写成远端已通过。
5. 标签 `v1.0.0` 指向发布提交，使用保存的 [Release 说明](releases/v1.0.0.md) 发布；核对远端 main、标签和 Release。
6. 关闭已验收的 issue 与 milestone；后续缺陷使用仓库模板另开 issue。

发布页面：[v1.0.0](https://github.com/Unwiisdey/logistics-route-planner/releases/tag/v1.0.0)。GitHub 自动提供此标签的源代码 zip/tar.gz，不需要把 build/ 上传到仓库。

## 维护约定

Issue 的复现信息应包括工具链、命令、最小配置、随机种子、版本和实际输出。影响缓存、窗口或事件顺序时增加对应回归测试。PR 模板要求说明问题、结果、验证与限制。CI action 固定提交版本，第三方 JSON 和绘图依赖固定版本；升级需重新验证。

本项目为课程设计最终交付；多车、实时网络 UI、真实地图等仍属于新需求，不能通过关闭本 milestone 宣称它们已经实现。
