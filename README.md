# Sonnheide

Sonnheide 是真实地球水陆底图上的三维人类文明模拟。**不采用游戏引擎；仿真、工程框架与客户端由我们搭建，所有建筑几何与外观由我们制作。** 独立开源库承担窗口、GPU、动画、字体和资产处理；第三方人物、动作、纹理、植物及地理数据按具体文件许可证复用。

本仓库托管初步工程设计、C++20 有限内核和可运行的原生主页面，**尚不是可游玩的完整游戏**。GitHub：[sky-yueer-yang/sonnheide](https://github.com/sky-yueer-yang/sonnheide)。当前仓库原本即为公开仓库；托管不自动授予原创代码、美术或设计开放许可，见 [原创与第三方权利策略](OWNERSHIP.md)。

## 从这里阅读

|内容|入口|
|---|---|
|架构总览、真正需要攻克的问题、模块边界|[工程总纲](docs/architecture/OVERVIEW.md)|
|27章逐章对应对象、事务、不变量|[领域映射](docs/architecture/DOMAIN_MAP.md)|
|五类共享基础、经济起步、调度、文化、战争与规模|[仿真架构](docs/architecture/SIMULATION.md)|
|真实地球、投影、岸线、格网、填海与导航|[世界工程](docs/architecture/WORLD.md)|
|原生写实客户端、水体、动画、LOD、拾取与UI|[渲染架构](docs/architecture/RENDERING.md)|
|底部分区工具栏、地图窗口与英汉德交互|[最新前端ADR 0006](docs/decisions/0006-bottom-toolbar-trilingual-editing.md)、[UI架构](docs/architecture/UI_ARCHITECTURE.md)、[WorldBox参考](docs/research/WORLDBOX_FRONTEND.md)|
|初步程序目录、构建目标与单一事实接入|[程序结构](docs/architecture/CLIENT_STRUCTURE.md)|
|主菜单、创建世界、初始地表、保存载入与失败恢复|[最新顺序ADR 0007](docs/decisions/0007-foundation-experience-first.md)、[应用流程](docs/architecture/APPLICATION_FLOW.md)|
|油画艺术主菜单、极简无框交互、光暗纪元|[最新视觉ADR 0009](docs/decisions/0009-monumental-minimal-interface.md)、[光暗ADR 0008](docs/decisions/0008-sacred-interface-and-light-ages.md)、[视觉合同](docs/architecture/VISUAL_STYLE.md)、[Light/Darkness规则](docs/architecture/LIGHT_AGES.md)|
|40类字段编辑、人物六轴、国家宗教与35法族|[编辑矩阵](docs/architecture/EDITING.md)、[机器合同](data/interaction_schema.json)|
|材料实送、自动建房、分层围网与反向撤网|[建筑施工变更 ADR 0002](docs/decisions/0002-building-construction.md)|
|统一平坦陆地、近岸下降、沿海可选地基与通道保护|[最新变更ADR 0010](docs/decisions/0010-flat-land-and-coastal-transition.md)、[地形与建址](docs/architecture/TERRAIN_AND_SITES.md)|
|早期服装业、实物衣物/耐久、购买排序与原创军装|[服装变更ADR 0004](docs/decisions/0004-clothing-and-makehuman.md)、[服装经济](docs/architecture/CLOTHING_ECONOMY.md)|
|正式MakeHuman/MPFB路线、选定源件与逐项许可|[人物生态接入](docs/research/MAKEHUMAN_ECOSYSTEM.md)、[源件manifest](assets/manifests/makehuman_sources.json)|
|命令、预览、授权、共享合同与错误协议|[接口契约](docs/architecture/CONTRACTS.md)|
|存档、恢复、版本迁移与确定性级别|[持久化](docs/architecture/PERSISTENCE.md)|
|自制建筑、人体、材质、LOD、资产编译与流送|[内容流水线](docs/architecture/CONTENT_PIPELINE.md)|
|代码依赖决定、可复用部件与拒绝理由|[开源技术栈](docs/research/OPEN_SOURCE_STACK.md)|
|人物/动作/植物/水体/材质/地理资源及许可证|[资产来源](docs/research/ASSET_SOURCES.md)|
|构建、GitHub/LFS/Releases、协作与交付|[工程运维](docs/architecture/ENGINEERING.md)|
|切片顺序、必须先验证的实验与通过标准|[开发路线](docs/planning/ROADMAP.md)|
|11阶段完整施工，先基础体验再建筑与玩法资产|[完整施工计划](docs/planning/IMPLEMENTATION_PLAN.md)|
|工程风险、决策与当前验证边界|[风险清单](docs/planning/RISKS.md)、[架构决策](docs/decisions/0001-foundation.md)、[验证记录](docs/planning/VALIDATION.md)|
|原始游戏规则，逐字保留与SHA-256来源|[设计基线v0.6](docs/design/Sonnheide_Complete_Design_v0.6.md)、[来源清单](data/catalogs/design_source.json)|

## 已经落地

本轮新增[集中交互ADR 0005](docs/decisions/0005-unified-inspectors-and-world-tools.md)、[全域信息/编辑合同](docs/architecture/INTERACTION.md)与[40种页面registry](data/interaction_schema.json)。[可点击信息页原型](tools/previews/world-inspector.html)可直接在浏览器离线打开：近景人物→国籍国家→城市/企业/语言等关联页面；远景点国家区域；世界统计/比较、规则、收藏/标记、分类清理、18岁玩家放置与0岁繁衍演示。原型使用明确标注的示例数据，不是可玩的3D世界或生产RmlUi客户端。

前端按[ADR 0006](docs/decisions/0006-bottom-toolbar-trilingual-editing.md)改为地图中心、固定底部7个section与按需对象窗口。界面中文/English/Deutsch使用[同一消息目录](data/ui_locales.json)和离线镜像；UI locale不改游戏Language。新增六人格轴、国家宗教地位/官方宗教和35法族的草案/预览；法律记录仅“批准待领域执行”，未实现的完整税/宪制/过渡结算明确说明。最新[ADR 0007](docs/decisions/0007-foundation-experience-first.md)把[施工计划](docs/planning/IMPLEMENTATION_PLAN.md)重排为先主菜单、创建世界、真实初始地表、基础界面和存档往返，通过原生空世界门槛后才开始建筑/人物服装制作。原生主菜单和设置已经接入 SDL3/bgfx/RmlUi；创建和载入入口显示真实的缺源/无存档状态，尚未创建生产世界。

独立C++ `sonnheide_interaction` oracle验证人口来源、全龄同BodySpec、typed引用/档案、单写者原子命令、世界规则、去重统计及清理保护；它尚未连接kernel、服装Ledger、完整法务/交通或生产存档。所有新增人物只允许玩家亲自放置（固定18岁）或合法繁衍（0岁）；年龄不生成儿童体型。

最新可审阅[油画主菜单原型](tools/previews/main-menu.html)使用用户提供的五幅JPEG：小范围中央亮焦点、深暗边缘、随机四向极慢微移、微放大与交叠切换。左上向内使用原始Sonnreich标志，配静态柔光、三层内圆、窄空隙、几乎贴近内圆的细三层外圆和360条长短粗细变化明显的低亮密集射线；原创SVG可由离线配方重建。大写SONNHEIDE使用本地Cinzel字体，和右侧五项菜单统一18px字号和0.01em字距，原标题顶部固定，菜单以24px行距向上收紧、暖白微黄带柔光，和圆环/射线共用#fff6df；纯文字菜单在右下紧凑右对齐，语言与画控收进设置。新增硬约束：绝不添加无必要小字，文字按钮不再强制icon，所有按钮仍无框。世界信息页同步克制深色主题，并在环境窗口说明Light/Darkness呈现演示的范围。正式世界只采用Age of Light与Age of Darkness，支持自动交替和手动切换，不做天体昼夜；日历/人物年龄仍按原合同推进。这些浏览器呈现不代表原生新建/存载或权威光暗状态已实现。

- C++20 headless 内核：只读天然底图、整数施工材料预约、分步劳动、取消、完工永久造陆、道路与正交港口几何、泊位保护、导航版本失效与全图连通正确性基准。
- 命令 ID 去重、负载/授权冲突拒绝、预览修订检查；世界与回执在同一屏障提交；存档重载检查空间、材料、身份、回执及历史约束。
- 18组内核场景、2000条确定种子压力命令、分配失败注入事务测试、headless 贯穿演示。
- 从原稿实际提取40能力突破＋96谱系＋288槽位、35类法律、13类业务；未提供的原参考代码和配置没有被伪称导入。
- 3份原创建筑几何配方及自包含 glTF 灰盒：住宅、CitySquare 的 CivicHall 组件、港口。它们用于格式/空间验证，正式写实模型、三时代外观与全建筑目录尚待制作。
- CMake 构建、三平台 GitHub Actions 与 Linux sanitizer 作业、内容/来源哈希校验、LFS 规则。
- 历史ETOPO阿尔卑斯25×25高程窗口及原始来源/许可保留；ADR 0010已取消真实高程，停止新高程采集，新世界只需要海陆/岸线包。
- 旧ADR 0003坡地建址oracle保留19组历史反例/贯穿回归，现独立为test-only库，不再链接当前kernel。新的平地零找平地基/沿海可选支撑合同已确定，实际生产建址尚未实现。
- 用户指定Abyssal Ocean的MIT准确源文件与commit/hash，已提取原生CPU蝶形/二维inverse FFT参考并用直接IDFT验证；native GPU海洋尚未移植。见[第三方通知](THIRD_PARTY_NOTICES.md)。
- MakeHuman v1.3.0核心身体/骨架/权重及三款CC0服装的14份准确源文件，合计3,129,823字节；标准库离线核验覆盖hash、网格/fit索引、骨架父图、权重和材质属性。未运行DCC或生成游戏glTF/LOD/动画，未导入缺失纹理与许可冲突的胸罩。
- 独立C++服装经济oracle：真实投入/劳时、工资、资金托管、预约/交付、两槽有界衣橱、整数穿用损耗/有限修补与购买需求。公司正装规范只提高购买优先级，无正装仍能工作并获得正常工资；具体已执行场景与长期循环范围见[验证记录](docs/planning/VALIDATION.md)。

建筑施工的新设计为：小人实际把材料送到前侧接货区，到齐后建筑自动推进工期；无人物砌筑动作。原创长方体铁丝网约3秒逐层围起，中段完全包围，完工后约3秒反向拆去。已有[可播放视觉预览](tools/previews/building-construction.html)和参数示例，生产建筑/物流与GPU动画尚未实现；填海仍沿用真实材料和劳动规则。

## 运行原生主页面

唯一发行渠道为 **Steam**。Windows Direct3D11 原生客户端已由 MSVC 完整编译链接，三项原生交互/解码测试通过；开发设备已实测 Metal 原生窗口。Windows GPU 实机与 Steam 发行打包仍需后续验证。程序没有浏览器、JavaScript或游戏引擎运行时。

```sh
python3 tools/build_native.py --run
```

脚本从本项目固定 Release 恢复七套准确源归档与原始中文字体，逐项验 SHA-256；编译缓存、工具缓存和可执行文件都在忽略的 `.build`。首次来源包发布前可显式加 `--allow-upstream` 从固定官方来源 bootstrap；已有缓存可以 `--offline`。仅本机缺 CMake 时恢复官方校验过的便携工具。资源随程序打包，偏好写入用户目录。当前原生菜单有中文、English、Deutsch、油画交叠/极慢微移/微放大、原始Logo与原创圆环射线、全屏/窗口、降低动态、键盘焦点/返回/退出。无有效地形包时创建入口明确不可用，无存档时继续不可用。

生产入口为 [apps/game/main.cpp](apps/game/main.cpp)，平台/渲染桥为 [platform](platform) 与 [presentation/native](presentation/native)，菜单行为为 [client/native](client/native)，原生界面为 [ui/application](ui/application)。来源与许可见 [锁](data/native_dependencies.lock.json)、[字体清单](assets/manifests/native_fonts.json)和[独立审查](docs/research/NATIVE_SOURCE_AUDIT.md)。真实地表、生产世界、地图底栏、世界保存载入和原生水体仍须按后续阶段接入。

## 运行现有切片

需要 C++20 编译器与 Python 3.9+。macOS/Linux 在没有 CMake 时也能立即构建并验证：

```sh
python3 tools/build.py
python3 tools/build.py --sanitizers
```

生产构建描述使用 CMake 3.24+。Windows 使用 CMake、MSVC 和 Python，其他平台也可使用此路径：

```sh
cmake --preset debug
cmake --build --preset debug --config Debug
ctest --preset debug -C Debug
```

重建可读目录与自制灰盒：

```sh
python3 tools/extract_catalogs.py
python3 tools/build_grayboxes.py
python3 tools/import_etopo_sample.py --verify
python3 tools/import_makehuman_sources.py --verify
python3 tools/validate_project.py
```

headless 演示会施工两格人工陆地、建一个港口、验证天然底图未变并执行存档往返。它没有窗口、真实地球数据、人物劳动分配、经济循环或写实水体。`KernelPlacePort` 只实现空间/导航子集；生产 `PlacePort` 的科技、产权、预算和许可尚待接入。

`sonnheide_site_tests`仅回归历史ADR 0003坡地数学，ETOPO导入器仅核验保留的历史研究来源，二者不再是新世界的地表方案。新世界按ADR 0010保留真实海陆轮廓、统一陆面和狭窄水边过渡，内陆普通房屋无额外找平地基；原生地表/新建址尚未实现。`sonnheide_ocean_fft_tests`验证上游FFT数学；`sonnheide_clothing_tests`核对独立服装实物循环，没有连接全世界道路货运、公共生存、科技传播或生产存档。原稿本身逐字保留。

## 当前实现不能直接作为生产规模方案

信息页前端行为验收可运行 `node tools/validate_interaction_preview.js`；没有PATH中的Node时可用`SONNHEIDE_NODE`指定已有运行时再执行`python3 tools/build.py`。主菜单图片/呈现回归另运行`node tools/validate_menu_preview.js`与`node tools/validate_world_inspector.js`。Node仅用于原型JavaScript验证，无npm包或生产引擎依赖。收藏、导航与地图marker属于PlayerView，不改变世界事务revision。40种有内容的示例页面不代表40个领域业务已实现；程序性编辑的生产法务命令仍须按各域实现。

内核采用整状态复制事务、`std::map/set`、同步全图水域 flood-fill，以及有界文本 checkpoint，目标是提供正确性基准。生产版将使用写集事务、热列/冷记录、Chunk与门户图、增量快照及分块存档，详见架构文档。当前 `water_reachable` 在导航未就绪时保守返回false；生产查询须返回 `Pending`，不能据此自动取消真实合同。

SDL3、bgfx/bx/bimg、RmlUi与FreeType已接入原生主页面；ozz未接入；MakeHuman正式路线已保留选定源件，游戏角色输出未完成。仓库没有为了增加文件数量创建空的渲染器或社会系统。用户当前要求仅完成原生接入，暂缓真实区域地理包和创建世界实现。后续仍按[新路线](docs/planning/ROADMAP.md)施工；建筑及服装接入在此门槛以后，随后按真实贯穿切片完成其余设计。
