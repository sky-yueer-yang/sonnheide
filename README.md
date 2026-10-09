# SONNHEIDE

SONNHEIDE 是**自研框架的三维像素风人类文明模拟游戏**。世界能从360°观察，玩家可以塑造海陆与山地；人物使用原创块状几何和精细像素贴图，所有建筑与人物服装自行制作。不采用游戏引擎，核心C++20，唯一发行渠道为Steam。

当前有效设计是 [v0.7 像素世界](docs/design/Sonnheide_Design_v0.7_Pixel_World.md) 和 [ADR0012](docs/decisions/0012-editable-3d-pixel-world.md)，地表/果树由 [ADR0013](docs/decisions/0013-layered-surfaces-and-fruiting-trees.md)进一步定义。原始v0.6逐字保留；旧平坦不可编辑地形、MakeHuman人物、写实PBR默认世界、RECLAIMED-only港口及七分区界面由明确新版覆盖。

**本轮完成设计与工程合同重构；当前可执行程序仍是旧写实空世界，新的像素世界与地形编辑尚未实现。** 不把文档、机器合同或旧阶段1测试算作新游戏功能。仓库公开托管于 [GitHub](https://github.com/sky-yueer-yang/sonnheide)，不自动授予原创代码、美术和设计开放许可，见 [权利策略](OWNERSHIP.md)。

## 当前设计入口

|内容|入口|
|---|---|
|完整规则、八种地形、城市后果、两种创建、27章逐章审查|[v0.7设计](docs/design/Sonnheide_Design_v0.7_Pixel_World.md)|
|版本覆盖、不变量和保持不变的范围|[ADR0012](docs/decisions/0012-editable-3d-pixel-world.md)|
|真正需要攻克的问题、模块owner与初步工程结构|[工程总纲](docs/architecture/OVERVIEW.md)、[程序结构](docs/architecture/CLIENT_STRUCTURE.md)|
|高度/材料/生态分层、水深海湖、神力编辑与人货原子后果|[地形事务](docs/architecture/PIXEL_TERRAIN_TRANSACTIONS.md)、[世界](docs/architecture/WORLD.md)|
|原创像素人楼衣物、水天空、rig/图集/LOD与预算|[像素资产合同](docs/architecture/PIXEL_RENDERING_AND_ASSETS.md)、[渲染](docs/architecture/RENDERING.md)、[内容流水线](docs/architecture/CONTENT_PIPELINE.md)|
|坡地基础、真实台阶坡道、人行/车库/接货及后建保护|[地形与建址](docs/architecture/TERRAIN_AND_SITES.md)|
|Blank/Earth同路径创建、取消失败隔离、首存档发布|[应用流程](docs/architecture/APPLICATION_FLOW.md)|
|八底栏分区、三语、互通信息页与编辑|[UI架构](docs/architecture/UI_ARCHITECTURE.md)、[交互](docs/architecture/INTERACTION.md)、[编辑矩阵](docs/architecture/EDITING.md)|
|三层地表、八soil主题＋沙地、树木/渐长果实与存载|[地表树木合同](docs/architecture/SURFACE_ECOLOGY_AND_TREES.md)、[机器登记](data/contracts/surface_ecology.json)|
|66工具/23命令设计、三语预设与27章覆盖|[像素机器合同](data/contracts/pixel_world.json)|
|WorldBox官方依据、采纳/改造/不加入的功能|[研究与采纳矩阵](docs/research/WORLDBOX_PIXEL_REFACTOR.md)|
|社会经济、服装实物、国家法规/宗教及知识战争|[仿真](docs/architecture/SIMULATION.md)、[领域映射](docs/architecture/DOMAIN_MAP.md)、[服装经济](docs/architecture/CLOTHING_ECONOMY.md)|
|单写者、预览重验、回执、完整存载及显式旧档复制迁移|[合同](docs/architecture/CONTRACTS.md)、[持久化](docs/architecture/PERSISTENCE.md)|
|先基础创建/地表/界面，随后原创楼体人物，最终Steam|[11阶段施工计划](docs/planning/IMPLEMENTATION_PLAN.md)、[路线图](docs/planning/ROADMAP.md)|
|已批准油画菜单、无框按钮、Light/Darkness两Age|[视觉合同](docs/architecture/VISUAL_STYLE.md)、[光暗规则](docs/architecture/LIGHT_AGES.md)|
|依赖来源、许可/哈希、GitHub/LFS/Releases与发行|[工程运维](docs/architecture/ENGINEERING.md)、[第三方通知](THIRD_PARTY_NOTICES.md)、[依赖锁](data/native_dependencies.lock.json)|
|逐字保留的原始27章、技术法律业务提取|[v0.6原稿](docs/design/Sonnheide_Complete_Design_v0.6.md)、[来源hash](data/catalogs/design_source.json)|

## 新版的关键规则

- 地形预设：Shallow Water、Close Ocean、Deep Ocean、Sand、Soil、Hill、Mountain、High Peak。水深/高度、表层材质、生态是不同字段；沙土默认保高度，明确造陆模式才改变高度。八soil主题＋干沙椰树生境采用地皮/装饰花草/机制树三层，独立涂抹；果实逐渐长大变色，真实采收；枫叶全红、樱野深绿草/零星落瓣、炎地枯树/无小植物、圣树有界柔光，矿率/真实库存不随地形免费产生。
- 分块单表面高度列是真正三维顶面与立面。首样本2m格、0.5m高度量子、64²chunk为待校准目标，人物贴图texel不等于碰撞体素；不是整世界密集XYZ体素。首版固定水位0，不做洞穴、悬空自然块、高位湖或体积流体。
- 神力可免费塑造有效地形，有保护/明确破坏模式；普通文明填海和基础仍需真实材料劳动。淹水/断路/改山同事务处理楼基、住户、货物托管、船客、施工和远港通路，保留身份、所有权、债务与城市账本。抢救/迁址需要真实路线、空间、容量和时间，不免费重建或瞬移。
- 空白全海/平陆和真实Earth选区转像素共用候选、最高预览、量化、水域及首档发布。Earth默认只导入海陆，派生高度不称真实海拔；风格化起伏需显式选择。人口建筑均0。来源证明不可改，发布后有效地形可改。
- 人物仅玩家放置18岁或合法出生0岁，全龄同成人身体。衣物是有耐久和替换需求的真实物品；家庭公共制衣先于企业，缺衣不直接致死，缺正装仍正常全薪上班。军装款式统一且绑定实际服役国家色。
- 地图中心、底栏八分区：观察、地形与生态、人物、文明制度、建设资源、经济物品、世界、设置。英汉德界面独立于模拟Language。保留已批准油画主菜单与Sonnreich标志，所有按钮无边框，不添无必要小字。

## 已实现与证据边界

|已有内容|实际范围|
|---|---|
|原生SDL3/bgfx/RmlUi/FreeType窗口、主菜单与设置|三语、油画/Logo、键盘焦点/全屏/降低动态已接入；不是完整游戏界面|
|旧阶段1真实地理与空世界|完整GSHHG导航、旧PBR/Hex/PureSky最高预览、静态海、可靠首checkpoint/继续；见[历史验收](docs/planning/PHASE1_ACCEPTANCE.md)和[证据](docs/planning/PHASE1_EVIDENCE.json)|
|有限headless内核|单写者、材料预约/劳动造陆、正交港口、导航/回执/文本checkpoint；旧冻结底图/RECLAIMED-only规则是legacy fixture，不是新生产地形|
|独立服装与交互oracle|实物衣物耐久/有限修补、人口18/0、稳定引用/规则/统计/清理；尚未连接完整城市生产存档|
|浏览器信息页与主菜单/网罩预览|明确示例数据/只读呈现，非生产3D玩法；旧七分区和只读地形不能当新前端完成|
|三份原创建筑灰盒|住宅、CitySquare组件、港口用于格式/空间验证；当前像素正式作者源/全部楼体尚待制作|
|原始目录与来源|424技术定义、35法族、13业务、27章追踪；定义数量不等于effect已实现|
|旧开放资源|固定Abyssal MIT源/CPU FFT参考、MakeHuman图形源、Poly Haven/Hex/天空锁与历史ETOPO样本保留；MakeHuman不进入新人物生产，写实PBR与FFT不是新默认世界门槛|

新像素地形/人楼衣物、Godterrain、完整领域存档、城市因果接入和旧档复制迁移器尚待实施。Windows编译、Metal开发实机、Windows GPU/Steam实机证据分别记录，不能互相替代。

## 运行现有原生程序

以下运行的是**旧写实地表版本**，可验证菜单、创建/最高预览与最小继续，不是新版像素试玩：

```sh
python3 tools/build_native.py --run
```

入口为 [apps/game/main.cpp](apps/game/main.cpp)，平台/渲染桥为 [platform](platform) 与 [presentation/native](presentation/native)，客户端行为为 [client/native](client/native)，界面为 [ui/application](ui/application)。固定源从本项目Releases恢复并逐项核hash；缓存/可执行文件在忽略的 `.build`。已有缓存可用 `--offline`。个人偏好与存档不入仓。

现有CPU切片需要C++20和Python3.9+：

```sh
python3 tools/build.py
python3 tools/build.py --sanitizers
```

CMake路径：

```sh
cmake --preset debug
cmake --build --preset debug --config Debug
ctest --preset debug -C Debug
```

项目源/资产/文档与新版设计合同校验：

```sh
python3 tools/validate_project.py
```

该检查包含来源hash、目录/许可锁、已有灰盒、旧交互合同、新像素与三层地表合同、三语标签及文档链接；不证明地形编辑、GPU画面或完整文明已运行。每次实施批次先完成全部修改和跨模块审查，再统一构建全部目标与完整CTest；仅设计修改不重复编译未变的游戏代码。

下一工程门槛是新版阶段0—1：锁定列地形/人物尺度与生命周期接口，完成Blank/Earth同路径三维像素初态和最小首档。阶段2接地形工具、八分区及完整存载；通过基础体验后阶段3立即制作正式原创楼体、块状人物和衣服。详见[施工计划](docs/planning/IMPLEMENTATION_PLAN.md)。
