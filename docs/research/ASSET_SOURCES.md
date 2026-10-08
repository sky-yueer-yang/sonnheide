# 开源资源来源与准入记录

2026年10月7日陆地表面专项见 [写实地表开源系统比较](REALISTIC_GROUND_SYSTEMS.md)，含八种具体扫描材质候选、实际覆盖尺寸、离线材质/地被生成与导出限制。本次仅完成研究，未新增正式材质资产；逐文件准入与项目托管要求继续适用。

核验日期：2026-10-06。本文区分候选与已归档来源：Abyssal原始代码、ETOPO窗口、MakeHuman人体/骨架/权重与三款服装的选定源件已逐文件核验，其余名单为准入研究。规范来源是 [Sonnheide v0.6](../design/Sonnheide_Complete_Design_v0.6.md)；用户追加的“不用游戏引擎、建筑全部自己做”和正式MakeHuman/服装经济规则优先于原稿旧措辞。

## 1. 先解决真正的复用边界

可复用的是满足许可和技术契约的基础数据、人体、动画、材质、植物、声音及独立算法。现成建筑、建筑构件套件、码头、桥梁、岸壁、CitySquare 的组成建筑都由 Sonnheide 自制。即使某建筑库是 CC0，也不列入采购白名单。开源纹理可贴在自制建筑上；不得通过拆散现成建筑再组合，冒充自制建筑。

代码的开源许可与美术的开放许可分别记录。`free`、`royalty-free`、可以用于商业游戏、GitHub 上能看到源码，都不能单独证明允许将原始文件放入本项目 GitHub。需要分别确认：修改、商业使用、原始文件再分发、衍生产物再分发、署名、相同方式共享、专利与第三方内容条款。

下表的“可准入”仅表示已核验官方公开许可足以支持下一步选取，不表示任何将来的下载文件自动通过。实际进入仓库前必须取得具体版本、具体文件、原始许可证副本和 SHA-256；保留网站证据日期，并检查压缩包中的单独许可。用户并未指定仓库公开或将原创代码、美术开源；GitHub 托管与作品公开许可是不同决定，不能默认把原创资产许可改成 CC0。

## 2. 官方证据及采用决定

|资源|公开许可核验|工程用途与决定|技术／内容限制|
|---|---|---|---|
|GSHHG 2.3.7|官方数据主页说明从 2.2.2 开始使用 LGPL；官方维护仓库 LICENSE 为 LGPL v3 文本|**可准入，版本固定 2.3.7**。作为天然 LAND/WATER 的离线源；保留原包、许可、转换脚本与 GeoSourceManifest|GSHHG 的上游原始数据来自公有领域，不代表加工后的 GSHHG 可标成 CC0；源、修改与转换产物分别保留许可记录|
|MakeHuman 官方核心资产／导出|官方 LICENSE 区分 AGPL 程序与 CC0 核心资产；官方 FAQ 确认核心资产导出可再分发|**正式人体来源**。已锁v1.3.0并归档base.obj、default骨架/权重及许可证原文；体型targets、皮肤/头发/眼睛仍待选取|源件不是已运行角色。必须统一单位、权重/骨架、体型、隐藏面、LOD与动画；不把默认高密网格用于全城。准确范围见[生态核验](MAKEHUMAN_ECOSYSTEM.md)|
|MPFB / MakeClothes / MakeTarget|官方说明MPFB/离线工具GPL与核心资产CC0分别适用|**正式离线生态路线**。MPFB v2.0.17 commit已锁；用Blender制作体型/服装适配与原创军装|未安装/运行工具，不复制整个工具包，也不链接运行时。社区材质/衣物逐项授权，不继承插件许可|
|MakeHuman社区服装包|官方资产表、成员头部与源件许可逐项相互核对|**已归档三款CC0源件**：男女crude T-shirt、jeans shorts；与[manifest](../../assets/manifests/makehuman_sources.json)逐文件对应|不是整个包/全套衣柜；未归档纹理须列出缺口。胸罩候选授权冲突不能当CC0导入；内衣/正装/鞋等逐步准入，军装原创建模|
|Quaternius Universal Base Characters|官方包页标 CC0；含 Regular 与 Teen 等比例|**动画绑定／渲染测试备选**，不直接认定为最终写实人体|风格需审查。Teen 不是所有年龄儿童。公开免费子集与付费 Source 子集逐项核验，不以主页 CC0 替代包内内容审查|
|Quaternius Universal Animation Library 1／2|官方包页标 CC0，提供 humanoid 动作及可重定向格式|**首选开放动作候选**。只选站立、行走、奔跑、搬运、采集、劳动、上下马和必要成年战斗动作|接入 Sonnheide 语义骨架；引擎兼容说明不等于无引擎客户端可直接加载；删除设计范围外动作，不引入其衣服或建筑|
|Poly Haven 材质、HDRI、非建筑植物|官方资产许可页明确 CC0，允许原始资产再分发|**正式 PBR／环境光优先候选**。砖、石、木、灰泥、土、织物及少量植物；不取任何建筑模型|网站页面、商标和示例渲染不自动同许可；资产许可与 API／网站下载条款分开，使用单项下载或遵守官方 API 条款|
|ambientCG 材质|官方说明资产 CC0，允许游戏包含原始文件|**正式 PBR 备选**。用具体资产 ID 锁定版本与物理纹理尺寸|只下载实际需要的分辨率；不把整库搬进 GitHub；保持每张贴图的来源、通道和颜色空间记录|
|Blender Sapling Tree Gen 0.3.7|官方扩展版本页标 GPL-3.0-or-later|**离线树形生成候选**。参数化生成树干和枝条，叶材用开放纹理或原创|工具代码许可与输出分开；检查模板／预设与输出有无第三方素材，不因工具 GPL 就给原创树mesh误标 GPL，也不把插件链接进运行时|
|Abyssal Ocean|固定commit的MIT原文及三个准确源文件已归档|**用户指定水体来源**；原生CPU FFT参考已构建，native GPU移植未完成|原项目WebGL2/three.js，需shaderc/pass/岸线mask移植；HTML CDN依赖未归档，不能标为完整离线客户端|
|NOAA ETOPO 2022 v1 Ice Surface|官方metadata明确CC0-1.0，完整许可原文已保留|**历史研究，已停止作为地表依赖**；25×25阿尔卑斯原字节及配方保留可核验，停止新增高程采集|ADR 0010只用真实海陆/岸线；新世界不消费ETOPO/DEM或垂直datum，不删改原始许可证据|
|2Retr0/GodotOceanWaves|仓库 LICENSE 为 MIT；README 单列 OTFFT Stockham 算法 MIT、天空图 CC0|**历史对照来源；本次选用Abyssal**。只挑选并审查谱／FFT／着色算法，移植到自研渲染器|这是 Godot 工程，不运行 Godot、不复用其场景／插件；保留复制代码和 OTFFT 的通知；不是直接可用的海洋模块|
|gasgiant/FFT-Ocean|仓库 LICENSE 为 MIT；README 明确是 Unity 原型，且不推荐实际项目直接使用|**对照参考**，不作为生产直接依赖|只在需要时审查独立 shader／数学代码；不运行 Unity；注意其他包、素材及上游说明的逐项许可|

对应官方证据：

- GSHHG：[数据主页及层级说明](https://www.soest.hawaii.edu/pwessel/gshhg/)、[官方维护仓库](https://github.com/GenericMappingTools/gshhg-gmt)、[许可证](https://raw.githubusercontent.com/GenericMappingTools/gshhg-gmt/master/LICENSE)。
- 人体：[MakeHuman 许可](https://github.com/makehumancommunity/makehuman/blob/master/LICENSE.md)、[MakeHuman／MPFB 许可说明](https://static.makehumancommunity.org/about/license.html)、[导出 FAQ](https://static.makehumancommunity.org/makehuman/faq/can_i_sell_models_created_with_makehuman.html)、[Quaternius 基础人体](https://quaternius.com/packs/universalbasecharacters.html)。
- 动作：[Universal Animation Library 1](https://quaternius.com/packs/universalanimationlibrary.html)、[Library 2](https://quaternius.com/packs/universalanimationlibrary2.html)。
- 材质：[Poly Haven 资产与网站许可](https://polyhaven.com/license)、[API 独立说明](https://polyhaven.com/our-api)、[ambientCG 许可](https://docs.ambientcg.com/license/)。
- 植物：[Sapling 官方版本与许可](https://extensions.blender.org/add-ons/sapling-tree-gen/versions/)。
- 高程：[NOAA官方数据页](https://www.ncei.noaa.gov/products/etopo-global-relief-model)、[原字节/许可证据](../../data/geo/sources/etopo2022_n60e000_tile201.json)、[CC0原文](../../data/geo/sources/CC0-1.0.txt)。
- 水体：[Abyssal原始项目](https://github.com/squall01337/abyssal-ocean)、[固定版本/哈希/范围](../../third_party/abyssal-ocean/UPSTREAM.json)、[MIT原文](../../third_party/abyssal-ocean/LICENSE)；历史参考：[GodotOceanWaves 来源及上游通知](https://github.com/2Retr0/GodotOceanWaves)、[MIT 许可](https://raw.githubusercontent.com/2Retr0/GodotOceanWaves/main/LICENSE)、[FFT-Ocean 原型说明](https://github.com/gasgiant/FFT-Ocean)、[MIT 许可](https://raw.githubusercontent.com/gasgiant/FFT-Ocean/master/LICENSE)。

## 3. 不进入默认白名单的资源

|来源或许可|决定|原因与替代|
|---|---|---|
|CMU Graphics Lab 动捕|不纳入“开放资产”默认白名单，不镜像原始库|官方允许商业产品使用，但禁止直接转售，包括转换版本；属于自定义条件而非标准开放许可。优先 CC0 动作；确有缺口再针对具体文件与再分发方式审查。见 [官方条件](https://mocap.cs.cmu.edu/)|
|Mixamo／Adobe、MetaHuman、各引擎商城、Sketchfab Standard、CGTrader／TurboSquid 商业许可|不作为公共原始资产来源|“允许游戏使用”不等于允许开源仓库再分发源模型；更不能代替明确开放许可证。当前没有核验具体资产，因此不采购|
|Shadertoy 或个人演示中的无 LICENSE 水 shader|拒绝复制代码|演示可见性与版权许可不同；采用上述 MIT 候选或原创公式实现|
|CC-BY-NC、Editorial、仅个人用、No redistribution|拒绝进入默认内容包|与商业发行、GitHub 原始文件托管或自由再分发目标冲突|
|CC-BY-SA、GPL 美术／其他强 copyleft 素材|单独审查，首批不选|不是“不开放”；需先确定衍生、共享与发行边界，不能把它们误当作 CC0。无需为首批内容引入这种额外组合复杂度|
|Quaternius／Poly Haven 等开放建筑、桥、码头、城墙套件|拒绝作为项目建筑资产|用户明确要求所有建筑自制；永久城墙／城门／防御塔也在游戏范围外|
|Quaternius 低模／卡通植物|仅限性能与内容契约测试候选|许可合格不等于写实美术合格。正式近中景优先审查 Poly Haven 植物或 Sapling 加 PBR 材质；最终不以卡通素材替代写实验收|

来源网站不会被整体“信任”。例如一个许可清晰的作者上传第三方照片、标志、武器品牌或外部纹理，仍需逐文件检查。

## 4. 工具许可证与导出内容的边界

Blender 官方 FAQ 明确 .blend 通常是用户的程序输出；同时针对使用 Blender Python API 的共享脚本给出 GPL 发布说明。因此离线 bpy exporter 或 add-on 放在独立工具目录，保留适用工具许可；游戏 C++／其他运行时代码只消费导出的 glTF／内容包。不能把“Blender 是 GPL”推导成所有游戏模型必须 GPL，也不能从“输出属于用户”推导第三方纹理和人体不受其原许可约束。[Blender 官方 FAQ](https://www.blender.org/support/faq/)

首批 graybox 由 Python 标准库直接生成 glTF，不使用 bpy；它是格式、占地、入口与流水线测试样本，不是最终写实美术。未来使用 Blender 创建所有建筑，不违反“不用游戏引擎”：Blender 是离线创作工具，发行客户端仍是自研。

## 5. 采购记录最小字段

计划中的 `assets/registry.json` 每个文件集应具有以下事实；字段缺失时不得发布该资产：

```text
assetId                     永久ID，不由文件名自动改写
category                    character/animation/material/vegetation/geo/audio/shader
origin                      original/third_party/derived
sourceUrl, sourceVersion    官方出处与固定tag/commit/资产版本
upstreamFiles[]             实际使用路径、字节数、sha256
author, copyrightNotice     原作者和原通知；不能把原作者改成我们
licenseExpression           原文确认的SPDX表达式或LicenseRef
licenseFile, evidenceUrl     仓库中的完整许可副本、官方证据
redistributionStatus        approved/pending/rejected；默认pending
changes, derivedFrom[]      重拓扑、降采样、重定向、通道处理等派生链
toolchain, recipeHash       工具固定版本、构建配方哈希
githubLocation              LFS路径或项目GitHub Release对象URL
runtimeOutputs[]            产物哈希、字节数、平台profile
verifiedAt, reviewer        最近核验时间与记录责任人
```

复制上游代码时保留代码通知；移植 shader 也属于代码复制／修改，不能仅在 README 放一个链接替代许可证通知。CC0 资产虽通常无署名要求，也保留来源记录，以便审计、重新构建和诚实致谢。原创建筑登记 `origin=original`，记录作者与所用材质的派生链，作者信息不能因用了开放材质而被覆盖。

## 6. GSHHG 的具体数据选择

采用官方 native binary 或 shapefile 数据，而非为 GMT 设计的复杂 netCDF 渲染包。保留 L1 海陆、L2 湖、L3 湖中岛、L4 岛中湖的嵌套语义；不加载政治国界和河流线来暗中生成现代世界。南极需要在冰前沿／接地线两种源中明确选一种并写入 manifest，不能同时当成两个陆地层。计划初始选择接地线，并用包含南极的验证用例审核这一选择。[GSHHG 格式与层级](https://www.soest.hawaii.edu/pwessel/gshhg/)

`EarthLandWater.pack` 是从固定版本源构建的瓦片化表现／查询数据。全球预览可用低 LOD；世界冻结底图必须按明确的高精度源与栅格化规则生成，不能由当前相机 LOD 决定。局部投影参数、统一比例、数据版本、源哈希、转换配方和最终掩码哈希都必须可追溯。转换 pack 不洗掉 GSHHG 的来源或许可；保守工程政策是把地理派生包独立于原创代码和资产发布，同时提供原始源、修改记录与可重建配方。具体发行通知在纳入固定下载包时复核，本文没有声称已经完成法律审查。

## 7. GitHub 全量托管可行性

“全部文件在 GitHub 托管”采用三层：可读源码／JSON／文档用普通 Git；编辑源模型、原始纹理、经批准的第三方源包用 Git LFS；按版本发布的游戏和内容大包放项目 GitHub Releases。所有发布需要的原始资产都必须有**本项目 GitHub** 的可取得副本，不能仅依赖未来可能消失的第三方 URL。第三方 URL 仍保留为来源证据。

当前官方限制：普通 Git 阻止超过 100 MiB 的单文件；普通仓库建议小于 1 GB、强烈建议小于 5 GB。Git LFS 单文件限额取决于计划：Free／Pro 2 GB、Team 4 GB、Enterprise Cloud 5 GB。[普通文件限制](https://docs.github.com/en/repositories/working-with-files/managing-large-files/about-large-files-on-github)、[LFS 限制](https://docs.github.com/en/repositories/working-with-files/managing-large-files/about-git-large-file-storage)

当前 LFS 包含配额：Free／Pro 10 GiB 存储和每月下载带宽，Team／Enterprise Cloud 250 GiB；历史版本与重复下载会消耗配额，不只是当前工作树大小。预算设为零时超额会阻止 LFS 使用，因此不能用“大量资产已配置 LFS”冒充可持续的托管方案。[官方计费](https://docs.github.com/en/billing/concepts/product-billing/git-lfs)

Releases 官方说明每个对象小于 2 GiB、一个 Release 最多 1000 个对象，发布总量与下载带宽无该条款中的总限额。工程上统一按不超过 512 MiB 分块，附 manifest 和 SHA-256，留余量并避免单文件冲突。[Release 配额](https://docs.github.com/en/repositories/releasing-projects-on-github/about-releases)

这些限制与价格会变化，实际采购和发布前复核。首批只引入被切片使用的资产，不下载整库。CI 快速路径不 smudge 全部 LFS；带 `content` 标签的工作流才选择性拉取并验证指针对应的真实对象。GitHub Actions artifact／cache 有保留期，不能作为唯一保存位置。项目源资产、发布内容包与许可证据必须落在 Git/LFS/Releases 的永久版本引用上。GitHub 托管 SDK／编译器缓存不属于项目文件；个人运行时存档与本机缓存也不是应提交的源文件。

## 8. 下一次资源引入的验收顺序

1. 从上表选择满足当前切片的一小份具体资产，明确缺口及技术预算。
2. 保存官方许可、版本和派生范围；许可不足则换候选，不先下载后解释。
3. 检查网格、骨架、贴图与上游混合许可，登记不可变 SHA-256。
4. 输出 canonical glTF，经内容验证后构建平台内容包。
5. 本项目 GitHub 原始文件副本、源和产物哈希、署名／许可通知全部完整后，状态才从 pending 转 approved。
6. 在 Sonnheide 实际场景中验证近景写实、中景识别、远景 LOD、资源流送和占地／人物语义；合格许可不能替代这一步。

保留Abyssal准确源码、历史ETOPO源窗口和MakeHuman选定源件。按[ADR 0010](../decisions/0010-flat-land-and-coastal-transition.md)停止新增高程采集，取消全球DEM/局部DTM/Copernicus DSM作为未来地表依赖；只推进真实海陆/岸线来源准入。MakeHuman身体/绑定与三款衣物经过离线源格式检查，尚缺游戏转换/材质/动画/性能验收；原始ZIP只保留实际使用成员，整包hash未冒称已知，不下载大型人物/材质整库。
