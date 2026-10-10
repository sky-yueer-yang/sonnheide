# 第三方代码与数据通知

本文件仅说明实际进入本仓库的第三方内容；原创部分继续适用[OWNERSHIP](OWNERSHIP.md)，不因托管而获得开放许可。

2026-10-10新像素原生程序实际使用SDL3/bgfx/RmlUi/FreeType、Unicode、GSHHG和以下Fusion Pixel字体。旧PBR/海洋/MakeHuman/serif记录保留为历史来源，不能视为新运行依赖。

## Fusion Pixel 街机像素字体

官方来源：[TakWolf/fusion-pixel-font](https://github.com/TakWolf/fusion-pixel-font)，固定2026.07.20版、commit `c29615e1f629c6bb27dac3e6dcaa4556e629d09f`。使用未改字节的12px proportional简体中文OTF，覆盖本批中英德UI；运行时别名Sonn Arcade不修改字体文件。原始字节经确定性gzip保存并按原始SHA恢复。

完整OFL-1.1与各贡献字体通知见[Fusion Pixel通知](assets/source/ui/fonts/fusion-pixel/NOTICE.md)，源归档/member/原始及压缩SHA见[字体锁](assets/manifests/arcade_fonts.json)。字体随分发携带许可，不单独出售或授予原创游戏内容开放许可。

## Hex-Tiling 算法源

- 来源：[mmikk/hextile-demo](https://github.com/mmikk/hextile-demo)，固定 commit `43c3ed7e18e1e4539fa9323f72d5e2a65147ebb3`，Copyright (c) 2022 mmikk。
- 四个原始 MIT 文件完整保留：[LICENSE](third_party/hextile/LICENSE)、两种 Hex-Tiling header、surface-gradient framework；准确路径、大小及 SHA-256 见 [源锁](third_party/hextile/UPSTREAM.json)。原始换行不修改；分发原始/派生代码保留完整 MIT 文本与作者通知。
- 未导入 DXUT 工程、第三方示例贴图或示例场景。已将算法移植到bgfx shader并一致处理PBR三通道，原始示例工程不进入运行时，不增加游戏引擎依赖。

## Poly Haven 地表与 Pure Sky

- 八套2K PNG三通道材质与两张8K HDR原始字节已经取得并核验，各项作者、源URL、物理尺度、MD5/SHA-256和不可变ZIP SHA见[资源锁](assets/manifests/ground_sky_sources.json)。完整CC0-1.0文本与[来源通知](assets/source/third_party/poly_haven/NOTICES.md)保留；大文件通过项目 `ground-sky-sources-v1` Release托管，精确原件与配方不依赖本机缓存。
- 只引入明确CC0资产，不导入网站预览、Logo、网页文案或现成建筑。不将原始源称为GPU ready；PBR/Hex、SH9漫反射、GGX粗糙度预滤波和固定明暗校准已接入实际原生GPU，派生产物按哈希/配方重建。资产CC0不授予原创游戏内容开放许可。

## Abyssal Ocean

- 作者：Sacha (@squall01337)，Copyright (c) 2026。
- 官方源：[squall01337/abyssal-ocean](https://github.com/squall01337/abyssal-ocean)，commit `142265f5013b6f27bea4f4f819b832dec75c7bad`。
- 准确源文件、字节数、SHA-256、遗漏的非运行资料与派生范围：[UPSTREAM](third_party/abyssal-ocean/UPSTREAM.json)。
- 原许可：[完整MIT文本](third_party/abyssal-ocean/LICENSE)。三个归档源文件未修改，派生CPU蝶形/二维inverse FFT代码内保留完整通知。发布其源或二进制必须携带该MIT许可和作者通知。
- 上游HTML依赖three.js 0.180.0 CDN；该浏览器依赖尚未归档、不参与原生构建。归档HTML是移植参考，不能标为完整离线游戏客户端。原生GPU海洋尚未移植。

## NOAA ETOPO 2022

- 来源：NOAA National Centers for Environmental Information, 2022, ETOPO 2022 15 Arc-Second Global Relief Model，DOI [10.25921/fd45-gt74](https://doi.org/10.25921/fd45-gt74)，访问日期2026-10-06。
- 实际文件：v1 Ice Surface的N60E000 TIFF头和tile 201准确字节范围，以[源窗口清单](data/geo/sources/etopo2022_n60e000_tile201.json)内base64保留；原始范围合计199247字节。未归档整幅TIFF或全球数据集，未编造完整文件hash。
- 官方[原始metadata/CC0声明](data/geo/sources/etopo2022_noaa_metadata.xml)提供数据授权与垂直基准证据。许可：[CC0-1.0完整文本](data/geo/sources/CC0-1.0.txt)，来自[Creative Commons官方原文](https://creativecommons.org/publicdomain/zero/1.0/legalcode.txt)，SHA-256 `a2010f343487d3f7618affe54f789f5487602331c0a8d03f49e9a7c547cf0499`。
- 由[标准库导入器](tools/import_etopo_sample.py)离线解码25×25原始像元中心：[阿尔卑斯样本](data/geo/etopo2022_alps_sample.json)。保留出处、datum、NoData、源分辨率与转换配方；样本不是全球游戏地形，也不是房屋/车道测绘。

## MakeHuman核心数据与三款社区服装

- 核心来源：[MakeHuman v1.3.0](https://github.com/makehumancommunity/makehuman/tree/v1.3.0)，commit `1f508f6083b2f823dab15de924b3bde72e08d77c`。实际源件是`base.obj`、`default.mhskel`、`default_weights.mhw`及两份上游许可原文；核心图形数据适用[上游CC0原文](assets/source/third_party/makehuman/core/LICENSE.ASSETS.md)，程序代码适用[AGPL等分项说明](assets/source/third_party/makehuman/core/LICENSE.md)。程序代码未导入/链接。
- 社区衣物作者Makehuman（男款经Elvaerwyn编辑）、Joel Palmius、Cortu Johnstone；实际使用男女crude T-shirt、jeans shorts的OBJ、MHClO、MHMat源成员。每项作者、准确下载来源、资产表/文件头证据、字节数和SHA-256见[源件清单](assets/manifests/makehuman_sources.json)。这三款明确CC0；社区其他资产不因此获得相同许可。整ZIP未归档、未声明全包hash，缺失贴图明确登记。
- MPFB v2.0.17 commit `80919fa4682335c41847f761a4d79dcad4124732`仅锁定为离线工具路线；未导入或运行插件。其GPL-3.0-or-later与核心图形CC0分别适用。MakeClothes/MakeTarget同属离线生态，后续实际引入时保留自身许可，不当运行时库。
- [标准库离线核验器](tools/import_makehuman_sources.py)复核实际源件、索引、绑定和证据；这些源数据尚未输出游戏glTF/LOD、材质、动作或GPU角色。详细准入与缺口见[生态记录](docs/research/MAKEHUMAN_ECOSYSTEM.md)。

## Cinzel界面字体

- Copyright 2020 The Cinzel Project Authors（[作者项目](https://github.com/NDISCOVER/Cinzel)）。准确源来自[Google Fonts固定commit](https://github.com/google/fonts/tree/3dd78844021e948ceb633d1dcee3f7885561b5d9/ofl/cinzel)，未改字体字节；本地文件名为`Cinzel.ttf`。
- SIL Open Font License 1.1：[完整OFL](assets/source/ui/fonts/OFL-Cinzel.txt)。字体、许可、上游metadata的准确URL、字节数与SHA-256见[界面源件清单](assets/manifests/interface_assets.json)。随项目分发时保留作者通知与OFL；不单独出售字体，不变更字体许可。
- Cinzel同时用于浏览器与原生主菜单。原生中文加载独立核验的Noto Serif CJK SC Regular；浏览器仍保留其原有系统回退。

用户提供的公司标志与五幅油画分别见[标志通知](assets/source/ui/branding/NOTICES.md)和[油画通知](assets/source/ui/paintings/NOTICES.md)，不因托管或第三方字体许可获得开放授权。

其他依赖与资产处于选择/准入研究阶段，见[依赖状态](data/dependencies.json)。实际下载之前不得列入已集成分发清单。

## 原生主页面依赖与中文字体

SDL 3.2.28 / RmlUi 5.1 / bgfx、bx、bimg / bgfx.cmake / FreeType 2.13.3 的准确提交、源归档与每项notice哈希见[原生锁](data/native_dependencies.lock.json)。完整发行通知见[Native NOTICES](third_party/native/NOTICES.md)，其中包含FreeType Team credit、FTL及独立模块许可、SDL附属模块、Apple metal-cpp Apache-2.0；源归档原文不修改。

Noto Serif CJK SC Regular Version2.003来自固定notofonts/noto-cjk提交，原始24,543,080字节；[OFL原文](assets/source/ui/fonts/noto-serif-cjk-sc/OFL.txt)、[作者通知](assets/source/ui/fonts/noto-serif-cjk-sc/NOTICE.md)、[源件清单](assets/manifests/native_fonts.json)独立保存。其原始二进制与七份库源码归档使用项目`native-sources-v1` Release托管并逐项验hash；不把第三方许可授给原创游戏内容。

## GSHHG 2.3.7 全球海陆/岸线

官方来源 Paul Wessel / Walter H. F. Smith 的 [GSHHG](https://www.soest.hawaii.edu/pwessel/gshhg/)。完整官方binary ZIP与五LOD成员准确大小/SHA及原始许可证、README见[来源锁](data/geo/gshhg_sources.lock.json)。数据适用LGPL-3.0-or-later，保留原版[LICENSE](data/geo/sources/gshhg/LICENSE.TXT)、[LGPL全文](data/geo/sources/gshhg/COPYING.LESSERv3)、原ZIP缺少的[GNU GPL主许可证补本](data/geo/sources/gshhg/COPYINGv3)及[README](data/geo/sources/gshhg/README.TXT)。原字节不改；本项目 `geography-sources-v1` Release提供完整原ZIP与这些notices，发行包携带notice。源较旧，full指未简化岸线，不宣称现实米级精度；不导入真实高程。地表是明确版本化的游戏派生，与原始数据分开。
