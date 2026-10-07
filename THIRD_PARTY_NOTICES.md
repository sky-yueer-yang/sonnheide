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

其他依赖与资产处于选择/准入研究阶段，见[依赖状态](data/dependencies.json)。实际下载之前不得列入已集成分发清单。
