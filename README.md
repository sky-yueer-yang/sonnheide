# SONNHEIDE

2026-10-09：当前 **v0.9 / ADR0018**。自研C++20框架，真正360°三维像素世界，唯一Steam发行。普通地形八档固定高度；坡道连接不同平台；取消自然山海湖身份/命名和岸线/外缘过渡。冷兵器起步，后期真实研究、制造和装备枪械。

**当前交付设计、版本化运行数据与Python有限参考模型。没有原生游戏或C++构建入口，生产阶段未开始。** 数据与模型验算不等于实装、最终平衡、万人容量或GPU/Steam证据。

|入口|内容|
|---|---|
|[v0.9总设计](docs/design/Sonnheide_Design_v0.9_Executable_Rules.md)、[ADR0018](docs/decisions/0018-fixed-terrain-and-executable-game-specification.md)|最新规则与完整领域接续|
|[运行基础](docs/architecture/DETERMINISTIC_WORLD_AND_RUNTIME.md)|Earth栅格/固定档/坡道/全域tick/存档与库采用|
|[文化语言遗传群](docs/architecture/EMERGENT_CULTURE_LANGUAGE_AND_LINEAGES.md)|共同实践成核、实际交流收敛/隔离分化、繁育群与持久迟滞|
|[商品科技与开局](docs/architecture/EXECUTABLE_CONTENT_AND_BOOTSTRAP.md)|商品/配方/建筑功能/408科技、冷启动与首次货币化|
|[交互规格](docs/architecture/TYPED_INTERACTION_AND_INPUT.md)|70页面具体字段/命令、拾取、输入、草案和三语错误|
|[机器总入口](data/contracts/game_v0_9.json)|全部权威合同与四份运行定义|
|[程序结构](docs/architecture/PIXEL_GAME_ARCHITECTURE.md)、[施工计划](docs/planning/IMPLEMENTATION_PLAN.md)|先基础应用/地表/创建/UI/存载，然后立即正式原创美术|
|[验收](docs/planning/VALIDATION.md)、[本批报告](docs/planning/DESIGN_AUDIT_ADR0018.json)|统一规格与有限模型核验；生产验收待实际施工|

统一命令：`python3 tools/validate_design.py`，Python3.9+标准库。只运行本批新增参考模型，复用未改历史模型的明确结果，不运行已删除的旧游戏。

人类仅玩家18岁放置或真实0岁出生，全龄成人体规；启智动物保原Actor，只能学习维护人类文化语言。八soil主题、机制树果、纯装饰植物、真实衣物耐久需求、缺正装仍工作领薪、前侧实物送料和铁丝网自动施工、批准油画/Logo主菜单、八底栏/中英德/无框继续有效。

所有楼、人、动物和衣物原创像素；自然地理无身份，机制树木、生物、实物仍有真实Ref。改地保生命及实际随身递归物品，植物建筑等按变化闭包清除，生命后续实际危险继续。

不做程序模板库、Workshop/Cloud、私家马/马车/骑兵、随机灾害或现代毁灭武器。环境只有Light/Darkness两Age。

[原v0.6](docs/design/Sonnheide_Complete_Design_v0.6.md)逐字保留；[历史v0.8](docs/design/Sonnheide_Design_v0.8_Living_Pixel_World.md)已被覆盖。`data/catalogs`是不可变来源，`data/content`是版本化运行定义。95份源件、许可、油画/Logo/hash保留，不启用MakeHuman/PBR为新生产路线。原创权利见[OWNERSHIP](OWNERSHIP.md)和[第三方通知](THIRD_PARTY_NOTICES.md)。必需源件托管GitHub，本机缓存、个人档与凭证不提交。
