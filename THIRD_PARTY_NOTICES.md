# 第三方代码与数据通知

本文件仅说明实际进入本仓库的第三方内容；原创部分继续适用[OWNERSHIP](OWNERSHIP.md)，不因托管而获得开放许可。

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
- 当前用于浏览器主菜单，本地加载；中文使用系统宋体回退，未打包或声称已许可完整中文字库。原生字体整形尚未接入。

用户提供的公司标志与五幅油画分别见[标志通知](assets/source/ui/branding/NOTICES.md)和[油画通知](assets/source/ui/paintings/NOTICES.md)，不因托管或第三方字体许可获得开放授权。

其他依赖与资产处于选择/准入研究阶段，见[依赖状态](data/dependencies.json)。实际下载之前不得列入已集成分发清单。
