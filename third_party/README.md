# 第三方依赖

包含 nlohmann/json **v3.12.0** 的官方单头文件和 MIT 许可证，不需要安装或在 CMake 配置时下载。

- 官方版本：[v3.12.0](https://github.com/nlohmann/json/releases/tag/v3.12.0)
- 原始头文件：[json.hpp](https://raw.githubusercontent.com/nlohmann/json/v3.12.0/single_include/nlohmann/json.hpp)
- 许可证：[LICENSE.MIT](https://raw.githubusercontent.com/nlohmann/json/v3.12.0/LICENSE.MIT)
- 已核验官方 SHA-256：`aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63`

由于本次 GitHub 文件下载连接不稳定，头文件通过 jsDelivr 的同版本 GitHub 镜像取得，与上述官方摘要一致。未修改第三方代码。JSON 库仅由配置加载实现使用，不暴露在业务模块的公开接口中。
