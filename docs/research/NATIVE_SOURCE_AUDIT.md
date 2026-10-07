# 原生主页面：来源、发行与接口独立审查

审查日期：2026-10-07。这里记录实取文件、固定源版本和已验证边界；原生程序的整体编译、视觉和交互结果由主集成记录提供。SDL、bgfx、FreeType、RmlUi 是分别负责窗口、GPU、字体和原生界面的库，没有引入游戏引擎或网页容器。当前平台实现使用 macOS 系统 ImageIO 解码原始 PNG/JPEG，不再引入第三方图片解码器。

## 来源与许可证

固定版本和原始归档哈希的权威记录为 [native_dependencies.lock.json](../../data/native_dependencies.lock.json)。本次独立重新计算 `.build/native-archives` 的七份原始归档：**149,578,169 bytes，全部 SHA-256 与锁记录相符**。没有把本机源码缓存视为已经托管的发行资源。

| 组件 | 固定版本 / commit | 实核验许可 | 发行处理 |
| --- | --- | --- | --- |
| SDL | 3.2.28 / `7f3ae3d57459e59943a4ecfefc8f6277ec6bf540` | zlib | 保留原始声明；修改上游源须标记；HIDAPI、yuv2rgb 等独立声明一并保留 |
| RmlUi | 5.1 / `40edf1acfa7f13f0c9b2af91d6f09ed47aa2c2c9` | MIT | 包含版权和许可文本；接口以这个版本头文件为准 |
| FreeType | 2.13.3 / `42608f77f20749dd6ddc9e0536788eaad70ea4b5` | 选择 FTL，另有独立模块许可 | 发行说明明确 FreeType credit，保留 FTL 及模块声明；未选择 alternate GPL |
| bgfx.cmake | `f2ea8fb0438721d754aefa22b14fe161f948a383` | **CC0-1.0** | 这是构建脚本许可，不覆盖其子模块 |
| bgfx | `cca91681c953d2de9531197b0f580c866ffaa775` | BSD-2-Clause + 各第三方文件许可 | 保留原声明；不能把整个 `3rdparty` 统称 BSD |
| bx | `d86e4ea9d9da6e832a3ff41398587d82b772c69b` | BSD-2-Clause + 独立文件许可 | 完整源归档保持上游声明 |
| bimg | `101b5b5fd4670f82cfdec8e98aa1ab9ee93bb2a1` | BSD-2-Clause + 各编解码器许可 | 当前 ImageIO 图片路径不使用 bimg 解码；源归档仍保留全部声明 |

以上依据固定版本的 [SDL 许可](https://github.com/libsdl-org/SDL/blob/7f3ae3d57459e59943a4ecfefc8f6277ec6bf540/LICENSE.txt)、[RmlUi 许可](https://github.com/mikke89/RmlUi/blob/40edf1acfa7f13f0c9b2af91d6f09ed47aa2c2c9/LICENSE.txt)、[FreeType 许可说明](https://github.com/freetype/freetype/blob/42608f77f20749dd6ddc9e0536788eaad70ea4b5/LICENSE.TXT)、[bgfx.cmake CC0](https://github.com/bkaradzic/bgfx.cmake/blob/f2ea8fb0438721d754aefa22b14fe161f948a383/LICENSE)、[bgfx BSD](https://github.com/bkaradzic/bgfx/blob/cca91681c953d2de9531197b0f580c866ffaa775/LICENSE)、[bx BSD](https://github.com/bkaradzic/bx/blob/d86e4ea9d9da6e832a3ff41398587d82b772c69b/LICENSE)、[bimg BSD](https://github.com/bkaradzic/bimg/blob/101b5b5fd4670f82cfdec8e98aa1ab9ee93bb2a1/LICENSE)。

独立查询固定 bgfx.cmake commit 的官方 Git tree，三个 gitlink **逐一等于**锁中的 bgfx、bx、bimg commit。不能下载 wrapper 归档之后任意拿三个最新分支补齐；wrapper 的 codeload 归档本身不包含 gitlink 内容。[官方固定 Git tree](https://api.github.com/repos/bkaradzic/bgfx.cmake/git/trees/f2ea8fb0438721d754aefa22b14fe161f948a383)

本次发现并提交集成修正的两类许可遗漏：

1. 固定 bgfx 的 `3rdparty/metal-cpp/LICENSE.txt` 是 **Apache-2.0**，版权所有者为 Apple，不能根据历史版本印象标成 MIT。发行中保持该原文及原始声明。[固定 metal-cpp 许可](https://github.com/bkaradzic/bgfx/blob/cca91681c953d2de9531197b0f580c866ffaa775/3rdparty/metal-cpp/LICENSE.txt)
2. FreeType 的 FTL 不自动覆盖其所有文件：BDF/PCF、fthash 有 X11 风格声明，`ft-hb.c/h` 有 Old MIT 声明，内置 gzip 有 zlib 声明。关闭外部 zlib 依赖不等于删除内置 gzip 模块。应把这些原始声明放进发行 notices，完整源码归档也原样保留；具体路径由上面的官方 `LICENSE.TXT` 明列。

## 随项目发行的中文衬线字体

实际采用官方 **Noto Serif CJK SC Regular 2.003** 静态 CFF OpenType 文件，具有适合中文正文的宋体风格。它作为包内字体，不依赖用户电脑安装 Songti、SimSun 或浏览器字体服务；既有 Cinzel 原件及其来源记录保持不变。[官方 Noto CJK 仓库](https://github.com/notofonts/noto-cjk)

完整 provenance 为 [native_fonts.json](../../assets/manifests/native_fonts.json)。本次已下载的原始字体：

| 字段 | 实测结果 |
| --- | --- |
| 原始 commit | `f8d157532fbfaeda587e826d4cd5b21a49186f7c` |
| 上游路径 | `Serif/OTF/SimplifiedChinese/NotoSerifCJKsc-Regular.otf` |
| 本地运行资源路径 | `assets/source/ui/fonts/noto-serif-cjk-sc/NotoSerifCJKsc-Regular.otf` |
| 文件体积 | **24,543,080 bytes** |
| SHA-256 | `2a2eae2628df83556c54018c41e20fa532c1b862c5256ae8b3f23feb918d12ca` |
| 上游 Git blob | `cba8a4783cc38574ac7cda52cae7d9b4241c07a5` |
| 格式 | `OTTO`，CFF，17 张 SFNT 表；没有 `fvar` |
| 字形数 | 65,535；Unicode cmap format 12，共 15,651 个范围组 |
| 当前菜单覆盖 | 181 个唯一字符全部映射到有效非零字形，覆盖 ASCII、中文、中文标点和德文字符 |

原始 [OFL.txt](../../assets/source/ui/fonts/noto-serif-cjk-sc/OFL.txt) 已取入，4,301 bytes，SHA-256 `6a73f9541c2de74158c0e7cf6b0a58ef774f5a780bf191f2d7ec9cc53efe2bf2`。字体内嵌版权为 `© 2017-2024 Adobe (http://www.adobe.com/).`，已保存到 [NOTICE.md](../../assets/source/ui/fonts/noto-serif-cjk-sc/NOTICE.md)。OFL-1.1 允许随软件分发，包括商业软件；保留版权及许可，不能单独销售字体；改造字体须遵守命名和衍生许可规则。本项目保持原始字体字节，没有擅自给原创 Logo、油画或游戏内容授予开放许可。[官方 Serif OFL](https://github.com/notofonts/noto-cjk/blob/f8d157532fbfaeda587e826d4cd5b21a49186f7c/Serif/LICENSE)

已运行离线结构与覆盖校验：

```sh
python3 assets/source/ui/fonts/noto-serif-cjk-sc/verify_font.py
```

该工具只使用 Python 3.9+ 标准库，检查固定原始 SHA/长度、SFNT 表界限、CFF/静态身份、内嵌家族/版本/OFL 声明、cmap 范围顺序及字形编号，并读取当前原生菜单文字检查覆盖。181 是本次菜单版本的快照，改文字后重新执行工具，不能把这个数字当作永久固定功能要求。

另用本次真正构建的 `.build/native/deps/freetype/libfreetype.a` 链接独立临时 C++ 探针，运行 `FT_New_Face`、选择 Unicode charmap、设置 36px、对 68 个中文和德文字符调用 `FT_Load_Char(..., FT_LOAD_RENDER)`，全部产生非空 bitmap。实测库版本 **FreeType 2.13.3，PASS**。探针在忽略的 `.build/review` 中；这已验证 CFF 字体兼容实际 FreeType 构建，但不代替 RmlUi/Metal 的界面视觉测试。

## Metal、HiDPI 与 RmlUi 5.1 边界

### 单一窗口和渲染器拥有者

固定 bgfx Metal `SwapChainMtl::init` 接受 macOS 的 NSWindow、NSView 或 CAMetalLayer，因此 SDL 的 Cocoa window property 作为 NSWindow 可以交接；不必为了得到 Metal 额外叠加 SDL renderer。bgfx 自己 retain/release layer，SDL 的窗口活到 bgfx shutdown 之后。[固定 Metal 实现](https://github.com/bkaradzic/bgfx/blob/cca91681c953d2de9531197b0f580c866ffaa775/src/renderer_mtl.cpp)

首次从 bgfx render thread 安装 Cocoa layer 时，上游可能把工作发给主 runloop 并等待；主线程同时等待 `bgfx::init` 会形成启动死锁风险。当前渲染器在 init 前调用 `bgfx::renderFrame()` 选择同线程路径，解除该相互等待。窗口建立、事件和 Cocoa 生命周期保持主线程；若未来启用双线程，必须重新设计主线程 layer 建立和事件循环，不能简单删除这个调用。

另一合法路径是主线程 `SDL_Metal_CreateView` 并交接 `SDL_Metal_GetLayer`，但它创建单独 view，不自行指定 MTLDevice；采用它时须明确谁管理 device/drawable size，在 bgfx shutdown 后才 `SDL_Metal_DestroyView`。当前程序没有同时使用两条拥有者路径。[SDL CreateView](https://wiki.libsdl.org/SDL3/SDL_Metal_CreateView)、[SDL GetLayer](https://wiki.libsdl.org/SDL3/SDL_Metal_GetLayer)

### 逻辑坐标、真实像素与输入

窗口尺寸和 framebuffer 像素尺寸分开，SDL 事件坐标仍为窗口坐标。当前方案以逻辑坐标排版 RmlUi、投影使用逻辑尺寸，bgfx backbuffer/view rect 使用真实像素尺寸，scissor 用独立 X/Y 比率映射一次。不要同时缩放鼠标、UI 和投影导致双倍坐标；显示器比例也不能固定为 2。缩放/全屏/跨屏后重取两个尺寸，零像素窗口暂停提交。[SDL HiDPI 说明](https://wiki.libsdl.org/SDL3/README-highdpi)

macOS 包需要 `NSHighResolutionCapable` 及高像素密度窗口设置。逻辑 18px 字体放大到 Retina framebuffer 的视觉清晰度，须在实机截图核验；窗口/鼠标/裁剪一致并不自动证明字形已经以最高密度栅格化。

### 5.1 的渲染合同和一次实际缺陷

RmlUi 5.1 的接口是 `RenderGeometry/CompileGeometry`、RGBA 四字节纹理、可为零的 texture handle、独立 translation、`SetTransform(nullptr)` 恢复单位变换，以及开关/矩形裁剪。不能直接使用较新主线 6.x 的接口文档复制实现。[固定 RenderInterface.h](https://github.com/mikke89/RmlUi/blob/40edf1acfa7f13f0c9b2af91d6f09ed47aa2c2c9/Include/RmlUi/Core/RenderInterface.h)

本次独立检查发现最初复用的 `vs_ocornut_imgui` 只使用 `u_viewProj`，忽略 bgfx `setTransform`；这样 RmlUi 的局部文字/控件顶点丢失 translation，几何会堆到原点。已通知集成，当前改用上游 `vs_debugdraw_fill_texture`，源码确实使用 `u_modelViewProj`，同时顶点布局改成三维位置，并保留匹配的颜色/UV。应以渲染实测确认，不把 API 调用成功当作变换生效。[固定新 vertex shader](https://github.com/bkaradzic/bgfx/blob/cca91681c953d2de9531197b0f580c866ffaa775/examples/common/debugdraw/vs_debugdraw_fill_texture.sc)

本次源审查另外核实：RmlUi 5.1 默认矩阵为 column major，与当前 bgfx 模型矩阵直接复制相符；FreeTypeInterface 的 RGBA 输出将彩色字形反预乘，普通字形使用白 RGB 加 coverage alpha，因此当前 straight-alpha shader/混合路径合理；系统 ImageIO 的预乘字节须先反预乘。texture=0 显式绑定白纹理，不能沿用上一张字形 atlas。32-bit 索引避免中文界面顶点数超过 65,535 时截断。关闭/销毁 context 和 `Rml::Shutdown` 在 bgfx shutdown 之前，避免 atlas/compiled geometry 的释放回调落到已经关闭的 GPU。

SDL_TEXT_INPUT 的 UTF-8 字符串交给 `ProcessTextInput(String)`；字符输入不能从 KEYDOWN 的 ASCII keycode 推导。当前菜单没有文本编辑框，输入法预编辑 UI 尚未实现；将来加入名称输入时须单独实现 IME composition/candidate rectangle、focus 与 text input 开关，不声称当前版本已支持完整 IME。

## 所有必要资源在本项目 GitHub 托管

选用项目 **`native-sources-v1` Release** 保存七个原始源归档和原始 OTF；Git 保存版本/hash锁、许可、notice、工程源码及配方。最大 bgfx 原始归档 **124,627,236 bytes**，不能作为普通 Git blob 提交；Release 适合保持原件，也不会像 Actions 临时 artifact 那样因保留期到期消失。

已确定可执行发布路线：由 root 集成的 GitHub Actions 工作流使用仓库自己的 `GITHUB_TOKEN` 和 `contents: write`，从 lock/manifest 中固定 commit 的官方 URL 下载，在上传前验证精确 bytes 和 SHA-256，以固定资产名上传到该项目 Release。不得使用上游 `latest`、下载未经校验的替代版本或重打包改变哈希。字体的 Release 资产名、官方 URL 和项目 URL 已在 font manifest 记录。普通开发/重建优先从项目 Release 获取，显式 bootstrap 才使用锁定官方源；离线模式只用已验证缓存。

发布之后必须从项目 Release 重新下载每个资产验证 bytes/hash，确认可用后才报告“已托管完成”；已经存在同名资产时校验它，不能默认覆盖。GitHub 的 Release 上传 API 接受二进制资产，工作流可用 `gh release upload`，无需在聊天或本机暴露个人凭证。[官方 Release assets API](https://docs.github.com/en/rest/releases/assets#upload-a-release-asset)

本审查没有自行发布 Release、改 PR 或写入全局依赖记录；发行完成状态由 root 的工作流与回读校验结果提供。原始 OTF 已在本机实际取得，manifest 中的 Release URL 仍不能单凭文字算作已上传。与此不同，Git 中的许可证原文和 provenance 已经具体落地，可供发布前复核。
