# 构建、GitHub托管与协作

## 1. 当前真实目录

|目录/文件|作用|
|---|---|
|engine/include/sonnheide、engine/src|C++20有限仿真内核；不链接窗口/GPU|
|apps/headless|施工→港口→存档的贯穿演示|
|tests|内核不变量、种子压力命令、allocation故障原子性|
|tools|编译回退、目录提取、原创建筑生成、来源/几何验证|
|data/catalogs|从原稿提取并带SHA-256来源的定义；不是完整平衡配置|
|data/requirements|27章状态与设计归属；不假造未提供的199条原需求|
|assets/source/buildings|原创参数几何作者源|
|assets/generated/buildings、assets/manifests|小型可读glTF夹具及来源/权威合同|
|docs/design、architecture、research、decisions、planning|不可改原稿、工程规格、来源、ADR、路线与验证|
|CMakeLists/CMakePresets、.github|跨平台构建、CI和工作模板|

新增生产目录只在有功能时创建。后续依次新增platform、renderer、content-cooker、people等；不是先写一百个空hpp。

## 2. 构建与依赖

当前仅C++标准库/Python标准库，所以无需联网获取依赖即可编译已有切片。Python3.9+兼容；CMake3.24+、C++20编译器。CMake负责生产构建图，`tools/build.py`提供当前macOS/Linux编译器回退，Windows用MSVC/CMake。[CMake官方Preset规范](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)

图形接入时将SDL3、bgfx/bx/bimg、shaderc/texturec及其传递依赖用已构建验证的准确commit冻结。`data/dependencies.json`目前是选择清单，所有未接入项明确planned，不叫lockfile。真正lock包含source archive SHA-256、许可证路径、补丁、编译选项、平台、toolchain、shader profile和来源副本。允许从本项目固定Release源归档恢复，不能构建时偷偷取master。

第三方vendored代码/源归档与自研代码分目录，修改上游保留补丁与原许可。SDL/bgfx窗口桥以锁定头文件为准；研究发现bgfx master与在线文档存在接口漂移，不复制旧SDL2/nwh教程。工具/依赖安装和编译cache不是项目作者源，不提交。

## 3. CI实际配置

`ci.yml`提供ubuntu-24.04、macos-14、windows-2022的CMake编译/CTest与目录/资产可重建检查；Linux额外ASan/UBSan。每次push、PR或手动运行。真实结果以GitHub Checks为准，配置文件存在不代表平台构建通过。

Linux/macOS测试Debug，Windows测试Release以避免MSVC Debug容器对整状态复制oracle的额外开销；所有断言使用显式异常/返回值检查，不依赖会被NDEBUG删除的assert。内核场景设置120秒、其他测试60秒上限；测试耗时不代表生产算法的性能。

Actions引用固定commit（checkout v5.0.0、setup-python v5.6.0此次已解析上游tag），contents只读，checkout不保留凭证，不用高权限pull_request_target。[GitHub官方固定SHA建议](https://docs.github.com/en/actions/reference/security/secure-use)

图形GPU/IME、内容压缩、地球转换、完整经济/重放场景分别在实现时增CI作业。无GPU的runner不假称Metal画面通过；需真实设备手动场景记录。机型/OS/编译器/viewport/seed/定义hash一起记录，否则性能无法比较。

## 4. 全部项目文件在GitHub的具体含义

普通Git：C++/Python/shader/UI文本、设计、定义、schema、原创配方、锁、许可证据、manifest、小fixtures。GitLFS：`.blend/.glb`、原始人体/动作、贴图、音频、地理包等二进制。Releases：正式各平台二进制、内容pack、固定版本较大源归档，每块建议≤512MiB并附SHA-256与source commit。

**每个正式分发所需的可再分发原始资源，必须有本项目GitHub的可取得副本。** 上游URL保留来源，不能替代项目托管；Actions artifact/cache有保留期，不能当唯一存档。当前只有3个小型文本glTF，不需要上传LFS大对象；`.gitattributes`已在未来二进制第一次提交前配置。

个人运行存档、本机cache、临时工具安装、凭证不作为项目文件托管。可重建输出要么保留有意义的fixture，要么按正式版本进Releases；不把每个本机.o文件当永久源。Git clone取全部文本，LFS选择性取具体切片；大型包下载由固定manifest管理。

当前仓库公开性沿用原有设置，未修改。原创权利政策见 [OWNERSHIP](../../OWNERSHIP.md)。任何不能再分发的资源不进入此公开仓库，选择允许的替代；无需为了研究建立“偷偷下载的唯一资产源”。额度/文件上限见 [官方核验](../research/ASSET_SOURCES.md#7-github-全量托管可行性)，在实际导入大资源前复核。

## 5. 协作与发布

后续分支`codex/<slice>`；PR围绕具体触发和行为、source/定义变动、测试/性能与兼容风险。二进制`.blend`采用LFS lock或划分模块，文本配方可合并。基础修改前先读设计、ADR和AGENTS，改动跨域由不同agent独立审查。

完成定义：需求ID→权威领域→输入/授权→事务→表现/检查器→失败恢复→不变量/场景→来源/notice→文档状态→可复现构建。不能只提交UI按钮、漂亮模型或一份json就把整项标完成。

发布顺序：验证真实切片→记录平台/性能证据→冻结源commit/依赖/定义/资产→重建pack→核验许可与manifest→生成checksums/notices→版本tag和GitHubRelease。自动发布流程在实际有游戏发行包时增加，本轮只有源码/工程基线，没有发布游戏二进制。
