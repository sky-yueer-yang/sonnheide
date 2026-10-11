# MakeHuman人体与服装生态：选型、原始来源和接入边界

2026-10-08版本裁决：本文旧平陆不可编辑、七分区、RECLAIMED-only港口或MakeHuman生产条款由[ADR0012](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/decisions/0012-editable-3d-pixel-world.md)覆盖；未冲突的身份/经济/菜单/许可规则继续有效。当前像素路线见[v0.7](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/design/Sonnheide_Design_v0.7_Pixel_World.md)，本文既有实测/源锁仅代表其原范围。

核验日期：2026-10-06。用户已经指定采用MakeHuman人体与生态；本记录落实这一选择。
经济与时代规则见[服装系统](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/architecture/CLOTHING_ECONOMY.md)，本记录负责来源、许可和
人物内容管线，不覆盖原始游戏设计源文档。

## 1. 已经实际取得什么

`assets/manifests/makehuman_sources.json`记录14份原始UTF-8文件，共**3,129,823字节**，
逐项有SHA-256、Git blob哈希、来源、许可证据和体积。它们已经下载、归档和结构校验，
不是仅列URL。原始字节不重写；适配后的派生资产应另存并记录生成配方。

| 部分 | 实际文件 | 状态 |
| --- | --- | --- |
| hm08源人体 | `base.obj` | 19,158顶点、18,486面、21,334 UV；含body与辅助几何 |
| 原始骨架与权重 | `default.mhskel`、`default_weights.mhw` | 163骨、326关节辅助点、57,107权重记录 |
| 朴素男/女T恤 | `elvs_crude_t-shirt_male`、`joepal_crude_t-shirt_female`各OBJ/MHCLO/MHMAT | 每件1,095顶点、1,034面；hm08绑定索引匹配 |
| 短裤源件 | `cortu_jeans_shorts`的OBJ/MHCLO/MHMAT | 285顶点、250面；丹宁外观必须另作早期朴素材质适配 |
| 官方许可 | `LICENSE.md`、`LICENSE.ASSETS.md` | 保留完整原件；项目附带NOTICE说明范围 |

三件衣服的九份源文件合计354,649字节。`shirts01`、`pants01`使用官方ZIP的精确HTTP byte
range取得选中成员；ZIP压缩块SHA-256、位置、CRC、成员SHA-256均固定。**完整ZIP未下载、
未声称有完整ZIP哈希**。核心文件使用固定commit的raw URL。公开托管的必要源材料可独立
离线验收，不依赖上游当前网页、下载服务器或本机DCC安装。

目前缺少四张原始纹理：`crude_male_tex.png`、`CrudeFemaleTshirtDiffuse.png`、
`jean_shorts_diff.png`、`jean_shorts_norm.png`。MHMAT还带有MakeHuman旧渲染器的shader字段；
这些字段不能直接送给自研渲染器。当前没有导出游戏人物、体型morph、皮肤/眼睛/头发、
步行动画或GPU蒙皮；MakeHuman、MPFB和Blender也未运行。`runtime_ready=false`是事实状态。

## 2. 官方版本与许可证分离

MakeHuman独立程序固定使用[`v1.3.0`](https://github.com/makehumancommunity/makehuman/releases/tag/v1.3.0)
来源commit `1f508f6083b2f823dab15de924b3bde72e08d77c`。官方
[分离许可原文](https://github.com/makehumancommunity/makehuman/blob/1f508f6083b2f823dab15de924b3bde72e08d77c/LICENSE.md)
区分程序代码AGPL-3.0-or-later与核心图形资产CC0-1.0，并明确第三方下载资产另遵守自己的许可。
本次没有把AGPL程序移入游戏内核。

离线人物、服装制作工具固定选择MPFB
[`v2.0.17`](https://github.com/makehumancommunity/mpfb2/releases/tag/v2.0.17)，commit
`80919fa4682335c41847f761a4d79dcad4124732`。该版本
[extension manifest](https://github.com/makehumancommunity/mpfb2/blob/80919fa4682335c41847f761a4d79dcad4124732/src/mpfb/blender_manifest.toml)
声明GPL-3.0-or-later、最低Blender4.2.0。最低版本声明不是对所有更高Blender版本的实际
运行兼容性测试；正式DCC构建还要锁定具体Blender发布版本并做导出测试。

官方[生态说明](https://static.makehumancommunity.org/about/ecosystem.html)提供MakeHuman、
MPFB、MakeClothes、MakeTarget和MakeSkin；后三者功能也包含在MPFB中。它们负责离线的人体
变形、服装绑定和材质创作，自研游戏运行时只读导出的几何、骨架、动画、材质与元数据。
没有游戏引擎依赖，也不以Blender人物生成器承担每个游戏tick的人口仿真。

**免费不等于所有文件都是CC0。** 官方[资产包索引](https://static.makehumancommunity.org/assets/assetpacks.html)
区分CC0与CC-BY资产包；每件网格、纹理、姿态、动画、材质变体和派生导出须检查来源与
相互一致的许可。CC-BY可在满足对应版本的署名、许可链接和修改声明时采用，不能在尚未
确定实际版本时标成CC0。来源与许可不明的成员不进入可分发包。人物第三方许可也不改变
Sonnheide原创建筑、原创军装或其他原创内容的权利；不擅自给原创内容授予开源许可。

## 3. 早期衣服和后期正装的具体来源

已导入的两件朴素T恤在[shirts01官方表](https://static.makehumancommunity.org/assets/assetpacks/shirts01.html)
与原始MHCLO均标CC0，作者分别为Makehuman/Elvaerwyn、Joel Palmius。短裤在
[pants01官方表](https://static.makehumancommunity.org/assets/assetpacks/pants01.html)与MHCLO均
标CC0，作者Cortu Johnstone。这些是几何源件；早期朴素样式应烘焙单独材质，使用已经确定的
游戏配色，不把现代丹宁纹理、扣件等未经审查地直接纳入上古服装。

内裤/胸部覆盖件和正装的候选详单保存在`assets/manifests/makehuman_candidates.json`：

| 用途 | 精确候选与来源 | 审查状态 |
| --- | --- | --- |
| 原始时代成年女性上身覆盖 | `joepal_plain_bra`，Joel Palmius，[underwear03](https://static.makehumancommunity.org/assets/assetpacks/underwear03.html) | 网页CC-BY，所取MHCLO/MHMAT header写unknown/CC0；许可冲突，未入项目 |
| 原始时代下身覆盖候选 | `elvs_crude_bootyshorts`，Elvaerwyn，[underwear02](https://static.makehumancommunity.org/assets/assetpacks/underwear02.html) | 网页CC-BY但ZIP名含cc0，不能按包名授权；待逐件版本/适配核验 |
| 成年男性下身覆盖候选 | `mindfront_male_swimming_trunks_01`，Mindfront，[pants03](https://static.makehumancommunity.org/assets/assetpacks/pants03.html) | 表列CC-BY，具体版本和成员待核验 |
| 后期女性正装 | `toigo_female_suit`，MargaretToigo，[suits01](https://static.makehumancommunity.org/assets/assetpacks/suits01.html) | 表列CC0，未取入源成员、未验绑定和材质 |
| 后期男性正装 | `toigo_male_suit_tie_and_jacket`，MargaretToigo，同上 | 表列CC0，未取入源成员、未验绑定和材质 |

`joepal_plain_bra`的临时上游审查确认MHCLO SHA-256为
`20f84c6bb90c5200d126221b641ae8f7ee55f3128aadaf4361b1e4773aaf8c81`。
导出器的默认unknown/CC0字段不能推翻官方包的不同声明。因此它只留下审查事实，原始
几何与材质不复制到项目；继续在选定生态中寻找一致授权源件或取得权利人的明确说明。
这不阻止先定义与验证原始时代衣服的经济SKU。基础遮蔽渲染资源的补齐是角色上线的验收
条件；不会把缺资源或缺库存表示成裸露人物。按后续[ADR 0005](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/decisions/0005-unified-inspectors-and-world-tools.md)全龄使用
同一成人身体尺寸、骨架及衣物fit，不按年龄缩小人物或服装；年龄仍决定生命周期资格。

## 4. 必须攻克的实际接入问题

### 4.1 一个共有人体拓扑，不等于任意衣服立即通用

官方[basemesh与helpers说明](https://static.makehumancommunity.org/mpfb/docs/assets/basemesh_and_helpers.html)
确认hm08是MakeHuman与MPFB共有拓扑。所有绑定先锁定这个顶点语义和索引顺序，再进行
体型变形、衣服拟合、关节定位和蒙皮。helpers是绑定与关节信息，不是可见游戏身体；只能
在这些计算完成后移除，再保存旧索引到游戏网格的重映射。

本次明确排除仓库`base.mhclo`：它是`alpha_7`遗留转换映射，不能仅凭同目录/同名把它与
hm08 `base.obj`拼成一个衣服基底。这个问题在真实文件核验中发现，已经从selected set排除。

MHCLO使用三个hm08顶点、三个**可为负的仿射系数**和三维offset定位每个服装顶点，另外
保存axis scale与delete_verts遮挡信息。它不是普通OBJ附属标签，也不是非负skin weight；
把负系数clamp掉会破坏服装形状。三件衣物已验证所有fit索引、scale索引、仿射系数总和、
delete mask范围、MHCLO/OBJ顶点数和局部OBJ/MHMAT引用相符。它们还没有通过任意体型或
动作穿插测试；这些事实不能扩大成“所有生态服装自动适配全部人群”。

### 4.2 原始骨权重不能原封不动作为GPU影响权重

当前源骨架parent关系无环，全部关节引用hm08合法顶点。原始MHW覆盖全部19,158顶点，
其每顶点权重总和实际为0.321到1.673。官方
[权重读取实现](https://github.com/makehumancommunity/makehuman/blob/1f508f6083b2f823dab15de924b3bde72e08d77c/makehuman/shared/animation.py)
读取时会规范化和合并；本工具独立验证全部正总和可归一、归一总和最大误差2.22e-16。
正式导出仍须合并重复、按渲染骨预算裁剪/保留影响、重新归一、生成inverse bind并做动画
姿态回归测试。163骨原始骨架是创作来源，不是每个全球人口个体必须持续计算的渲染预算。

### 4.3 单位、材质、体型与实例边界

MakeHuman内部单位为分米，默认Y-up、面向正Z，见
[官方导出说明](https://static.makehumancommunity.org/makehuman/docs/exports_and_file_formats.html)。
源单位到米的0.1缩放以及feet-on-ground/坐标轴变换只执行一次，并进入生成配方。人物
世界坐标与地图worldScale的规则由引擎规定，不靠DCC导出选项默默改变。

正式内容构建计划：锁定来源与DCC版本 → 按有限年龄/体型archetype变形 → 拟合衣服及人体
遮挡mask → 同骨架蒙皮/动画重定向 → 去helper/生成LOD/UV与材质烘焙 → 固定game export
与生成配方 → 检查行走、搬运、坐下、进屋等真实动作的穿插与覆盖 → 进入原生人物渲染。
日常NPC按archetype、服装mesh、材质palette共享资源，耐久度作为仿真状态与可选磨损外观
参数；购买一件衣服不触发生成一份新的MakeHuman人体。

MPFB2.0.17的[随机人物功能](https://static.makehumancommunity.org/mpfb/releases/release_2017.html)
提供带seed的离线变体与batch功能，适合作为创作辅助。该功能仍标有实验限制；工具批次
不是游戏人口吞吐证明。运行时使用已验证的变体和持久化ID，不读取创作工具随机状态。

军装独立建原创统一版型和国家主色/辅色mask：国家变更材质palette，不复制国家专属几何；
不同体型保留同款设计意图但需要各自拟合。军装库存、保管者、发放和耐久度属于仿真；
颜色和镜头可见性不能创造库存或改变损耗。所有建筑继续由项目自行制作。

## 5. 可复现验证与当前完成度

```sh
python3 tools/import_makehuman_sources.py --verify
python3 tools/import_makehuman_sources.py --fetch --verify
```

默认与`--verify`完全离线；核验失败返回非零退出码。`--fetch`只恢复已审核的14份精确
源字节，逐项检查hash，ZIP另核验range/Deflate/CRC，不自动扩大导入范围。Python3.9+
标准库即可。两个命令本次均实际运行通过。

完成的是来源取入、许可边界、几何/绑定/骨架结构与可归一性检查。没有声称完成DCC运行
兼容测试、全部生态资产接入、身体变形、原生GPU角色、服装动态穿插、图形预览或全球
人口渲染。源库的丰富度不自动保证服装经济长久有需求；需求与耐久度的因果规则必须由
服装仿真和贯穿测试单独证明。
