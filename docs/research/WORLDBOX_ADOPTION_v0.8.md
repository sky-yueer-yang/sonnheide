# WorldBox全功能对照：v0.8逐项处置

2026-10-09。沿用官方优先的248项/17组研究，不凭空声称按钮、动物、biome或公式全集已得到。本文逐项完成设计采纳，**没有任何一项生产实现完成**。

WorldBox Steam官方当前0.51.2；Meta Control/ChessBox仍属官方预告。社区线索仅辅助定位，具体核验/版本证据见[原始完整研究](WORLDBOX_FULL_COMPARISON_2026-10-09.md)。本表的“新增/改造”是Sonnheide本轮设计，不表示WorldBox内部采用我们的公式。

决策：retain=保留既有设计并迁移；adopt=新增适用能力；adapt=保留目的并按我们的因果机制改造；exclude=明确剔除；evidence_boundary=证据/比较边界，自身需求已有明确规则，不冒充未公开原版全集。

|处置|项数|
|---|---|
|adopt|102|
|adapt|59|
|retain|63|
|exclude|15|
|evidence_boundary|9|

## 01 元控制、附身与观察视角

|编号/功能|决定|如何接入或为何排除|设计|
|---|---|---|
|F001 元对象锁定控制|adopt|可锁定真实元对象，控制租约/成员范围可见；WorldBox此项仍是预告。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D20)/[合同](../../data/contracts/civic_control_v1.json)|
|F002 可控成员范围|adopt|成员动态加入/退出及拒绝原因，不能凭元对象控制刷人口。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D20)/[合同](../../data/contracts/civic_control_v1.json)|
|F003 群体移动/建造/砍树|adapt|群体命令转现有任务/实际送料/采伐；楼体仍自动建造无砌筑人物。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D20)/[合同](../../data/contracts/civic_control_v1.json)|
|F004 所属视野战雾|adopt|可选真实成员视野战雾，雾外仿真不停。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D20)/[合同](../../data/contracts/civic_control_v1.json)|
|F005 只查看元对象视野|adopt|选择元对象知情视图，不改变全知默认。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D20)/[合同](../../data/contracts/civic_control_v1.json)|
|F006 ChessBox限制神力|adapt|可选限制神力控制模式，不做竞技地图库/多人ChessBox。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D20)/[合同](../../data/contracts/civic_control_v1.json)|
|F007 发现国家后外交|adopt|受限视野模式外交依实际发现/知情关系。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D20)/[合同](../../data/contracts/civic_control_v1.json)|
|F008 单位附身|adopt|同Actor附身/移动/交互/退出，无能力通行物权豁免。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D20)/[合同](../../data/contracts/civic_control_v1.json)|
|F009 Brain观察|adopt|Brain显示真实决策评分/任务/拒绝原因。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D20)/[合同](../../data/contracts/civic_control_v1.json)|
|F010 禁用行为/设为无心智|adapt|暂停AI行为类/手动租约，不抹除意识或停止生理。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D20)/[合同](../../data/contracts/civic_control_v1.json)|

## 02 城市、国家与疆域编辑

|编号/功能|决定|如何接入或为何排除|设计|
|---|---|---|
|F011 城市/国家边界画笔|adopt|辖区与跨国主权双模式，变化有完整预览。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D13)/[合同](../../data/contracts/civic_control_v1.json)|
|F012 边界桶填充|adopt|边界桶只填无争议连续辖区，不跨他国/他城。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D13)/[合同](../../data/contracts/civic_control_v1.json)|
|F013 跨现有边界重画|adapt|跨界画笔明示神意或真实条约，不伪造同意/转物权。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D13)/[合同](../../data/contracts/civic_control_v1.json)|
|F014 边界选择/悬停|retain|地图政治图层拾取/悬停/关系页。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D21)/[合同](../../data/contracts/civic_control_v1.json)|
|F015 国家/城市/单位三视图|adopt|国家/城市/主体分布视图，名字与区域独立。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D21)/[合同](../../data/contracts/civic_control_v1.json)|
|F016 改名和前名|retain|前名/改名tick/当时名称/稳定Ref保存。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D22)/[合同](../../data/contracts/civic_control_v1.json)|
|F017 旗帜/主题色展示|retain|保13模板23色/国家theme规则，不复制WB旗图。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D13)/[合同](../../data/contracts/civic_control_v1.json)|
|F018 城市创始人|retain|真实城市创始者及建城事件，不用名字作引用。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D13)/[合同](../../data/contracts/civic_control_v1.json)|
|F019 历任国王/领袖|adopt|历任统治者/任期/死档/继位原因互通。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D13)/[合同](../../data/contracts/civic_control_v1.json)|
|F020 城市忠诚与叛乱|adapt|实际税粮权利与继位冲突产生忠诚/反叛，不刷叛军。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D19)/[合同](../../data/contracts/civic_control_v1.json)|
|F021 废城/遗迹衰退|adopt|废城/遗迹保实体残骸与历史，衰退不回补物资。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D14)/[合同](../../data/contracts/game_v0_8.json)|
|F022 自主扩张/定居/建城|retain|真实已有居民/资源/许可自主建设，禁止自动刷新创始人。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D14)/[合同](../../data/contracts/game_v0_8.json)|

## 03 物种、亚种、基因与表型

|编号/功能|决定|如何接入或为何排除|设计|
|---|---|---|
|F023 多个智慧物种|adapt|人类＋玩家启智动物及合法后代；不搬精灵兽人怪物。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D10)/[合同](../../data/contracts/life_genetics_v1.json)|
|F024 Species/Subspecies分层|adopt|固定Species/可编辑Subspecies/个体Genome分层。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D07)/[合同](../../data/contracts/life_genetics_v1.json)|
|F025 同物种多个亚种|adopt|同物种多亚种差异在固定包络内。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D07)/[合同](../../data/contracts/life_genetics_v1.json)|
|F026 亚种名称/详情/编辑|adopt|亚种名/谱系/共享修饰/全成员后果预览。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D07)/[合同](../../data/contracts/life_genetics_v1.json)|
|F027 taxonomy分类学显示|adapt|固定实用species分类目录与谱系，不凭taxonomy名称获得属性。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D07)/[合同](../../data/contracts/life_genetics_v1.json)|
|F028 智慧/非智慧分栏|adopt|普通动物/高级意识/人类可筛分但共用Actor。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D21)/[合同](../../data/contracts/civic_control_v1.json)|
|F029 基因编辑器|adopt|typed遗传编辑与锁定物种上下限。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D07)/[合同](../../data/contracts/life_genetics_v1.json)|
|F030 基因池/染色体展示|adopt|亲代/等位组合/基因池与表型差异；不假称WB内部公式。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D07)/[合同](../../data/contracts/life_genetics_v1.json)|
|F031 基因影响数值属性|adopt|力量、耐寒、耐饿、代谢、感知、学习等落真实生理。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D07)/[合同](../../data/contracts/life_genetics_v1.json)|
|F032 遗传寿命|adopt|寿命/衰老基因有界，编辑不重置已活时间。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D08)/[合同](../../data/contracts/life_genetics_v1.json)|
|F033 亲代性状继承|adopt|亲代来源、组合、有界突变和保存随机流。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D07)/[合同](../../data/contracts/life_genetics_v1.json)|
|F034 表型/外观遗传|adopt|像素外观遗传受固定身体profile约束。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D07)/[合同](../../data/contracts/life_genetics_v1.json)|
|F035 亚种分化/演化|adapt|有真实谱系差异才能分化，非出生数升级获魔法traits。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D07)/[合同](../../data/contracts/life_genetics_v1.json)|
|F036 同物种跨亚种繁衍|adopt|同species跨亚种繁育，归属与亲代证据确定。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D07)/[合同](../../data/contracts/life_genetics_v1.json)|
|F037 亚种人口上限|adapt|新增配额而非到限杀人/停止既有个体。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D08)/[合同](../../data/contracts/life_genetics_v1.json)|
|F038 Monolith启智|adapt|文明之光取代Monolith；不进化换体或自动生成文化语言。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D10)/[合同](../../data/contracts/life_genetics_v1.json)|
|F039 生境影响适应性状|adapt|生境压力/存活与有界突变，不换主题立刻送适应gene。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D07)/[合同](../../data/contracts/life_genetics_v1.json)|
|F040 沙漠/沼泽/冻原适应|adopt|冷/热/湿地/沙地运动与生理分开，不等于免伤。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D07)/[合同](../../data/contracts/life_genetics_v1.json)|
|F041 耐热/防火特性|adapt|耐热有界生理保护，无fire-proof/熔岩免疫魔法。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D05)/[合同](../../data/contracts/terrain_access_v1.json)|

## 04 单位性状、状态与生命周期

|编号/功能|决定|如何接入或为何排除|设计|
|---|---|---|
|F042 完整Unit trait体系|adopt|遗传/训练/伤病/制度性状分域，不混一张buff表。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D07)/[合同](../../data/contracts/life_genetics_v1.json)|
|F043 增删个体性状|adopt|增删允许遗传性状，不回满血/能量。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D07)/[合同](../../data/contracts/life_genetics_v1.json)|
|F044 批量特性雨|adapt|批量性状画笔逐成员后果，不免费加库存人口。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D07)/[合同](../../data/contracts/life_genetics_v1.json)|
|F045 性状发现/解锁|adapt|基因/知识百科与观察发现；不靠成就锁住必要编辑。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D25)/[合同](../../data/contracts/game_v0_8.json)|
|F046 经验/等级/能力|adapt|真实学习/训练/能力，不杀敌送全属性等级。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D18)/[合同](../../data/contracts/game_v0_8.json)|
|F047 饥饿/健康/战伤|retain|实际摄入/能量/伤势/持续后果。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D08)/[合同](../../data/contracts/life_genetics_v1.json)|
|F048 冻结/燃烧/溺水状态|adapt|失温过热/落水/战伤，有真实源；静态炎地不自动灼烧。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D08)/[合同](../../data/contracts/life_genetics_v1.json)|
|F049 幼体到成年身体|exclude|用户要求全龄同成人体规，不建幼体缩放。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D08)/[合同](../../data/contracts/life_genetics_v1.json)|
|F050 物种成熟年龄差异|adapt|人类18成年；动物按固定species成熟，身体仍成人规格。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D08)/[合同](../../data/contracts/life_genetics_v1.json)|
|F051 衰老/自然死亡|retain|年龄与死亡事件，不因关闭后果清零年龄。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D08)/[合同](../../data/contracts/life_genetics_v1.json)|
|F052 妊娠/孕期|adopt|孕期/营养/到期/父母证据保存。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D08)/[合同](../../data/contracts/life_genetics_v1.json)|
|F053 双亲有性繁衍|retain|合法双亲及真实来源，新受孕开关。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D08)/[合同](../../data/contracts/life_genetics_v1.json)|
|F054 卵生/孤雌/分裂等|adapt|按原生species胎生/卵生；不加魔法分裂/跨species繁殖。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D08)/[合同](../../data/contracts/life_genetics_v1.json)|
|F055 生育能力/限制|adopt|成熟/营养/间隔/预算与新增配额共同限制。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D08)/[合同](../../data/contracts/life_genetics_v1.json)|
|F056 伴侣/父母/子女|retain|生物父母/伴侣子女与法律家庭分开。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D13)/[合同](../../data/contracts/civic_control_v1.json)|
|F057 朋友等细分关系|adopt|友谊/冲突/照料/师生等有真实关系事件。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D13)/[合同](../../data/contracts/civic_control_v1.json)|
|F058 幸福/事件哀伤|adopt|幸福/哀伤影响实际偏好，不代替食物/法律。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D08)/[合同](../../data/contracts/life_genetics_v1.json)|
|F059 睡眠/休息|adopt|真实休息恢复体力，需要时间不补物资。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D08)/[合同](../../data/contracts/life_genetics_v1.json)|

## 05 动物生态、食性与生物产物

|编号/功能|决定|如何接入或为何排除|设计|
|---|---|---|
|F060 动物物种与放置|adopt|固定species动物及玩家放置，原生角色目录。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D09)/[合同](../../data/contracts/life_genetics_v1.json)|
|F061 自主繁育/幼体|adapt|真实繁育、出生0，固定成人体规无幼体缩放。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D08)/[合同](../../data/contracts/life_genetics_v1.json)|
|F062 草食/肉食/杂食|adopt|食性/消化/口粮实际有差别。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D09)/[合同](../../data/contracts/life_genetics_v1.json)|
|F063 捕食关系|adopt|真实局部捕食/猎物能量转移与避险。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D09)/[合同](../../data/contracts/life_genetics_v1.json)|
|F064 特殊生物食性|adapt|只原生动物实际食性，不加怪物酸/魔法食物。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D09)/[合同](../../data/contracts/life_genetics_v1.json)|
|F065 肉/骨/皮等产出|adopt|肉骨皮真实可回收reserve，神力放置不赠商品。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D09)/[合同](../../data/contracts/life_genetics_v1.json)|
|F066 蜂群/蜂巢/蜂蜜|adopt|群体蜂/巢/蜂蜜/服务有界且真实产出。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D09)/[合同](../../data/contracts/life_genetics_v1.json)|
|F067 动物Family与谱系|adopt|动物生物亲代谱系与普通/启智筛分。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D09)/[合同](../../data/contracts/life_genetics_v1.json)|
|F068 动物文明|adapt|完整意识、公民能力平等，仅文化/语言创生受限。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D10)/[合同](../../data/contracts/life_genetics_v1.json)|
|F069 生物疾病/传染|adapt|真实感染源/免疫/护理/传染，不随机瘟疫天灾。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D09)/[合同](../../data/contracts/life_genetics_v1.json)|
|F070 亚种/Family消亡标记|adopt|亚种/Family数量0保档，固定Species不删。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D22)/[合同](../../data/contracts/civic_control_v1.json)|
|F071 观察/干预动物行为|adopt|查看动物行为/附身/任务/启智，身体约束不失效。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D20)/[合同](../../data/contracts/civic_control_v1.json)|

## 06 Family、Clan与统治血统

|编号/功能|决定|如何接入或为何排除|设计|
|---|---|---|
|F072 独立跨代Family|adopt|跨代Family稳定Ref，预算Household独立。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D13)/[合同](../../data/contracts/civic_control_v1.json)|
|F073 可视族谱|adopt|可视双亲/子代/收养/血缘图；不同关系不得合并。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D13)/[合同](../../data/contracts/civic_control_v1.json)|
|F074 代际/祖先/家族史|adopt|代际、祖先、成员与档案可追溯。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D22)/[合同](../../data/contracts/civic_control_v1.json)|
|F075 Family名称/颜色/标识|adopt|Family名字/颜色/标识可编辑，历史不改写。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D13)/[合同](../../data/contracts/civic_control_v1.json)|
|F076 Family分布/灭绝|adopt|Family分布/消亡/当前与历史成员。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D22)/[合同](../../data/contracts/civic_control_v1.json)|
|F077 Royal Clan实体|adapt|Dynasty真实组织章程与继位资格，不照搬Clan魔法。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D13)/[合同](../../data/contracts/civic_control_v1.json)|
|F078 宗族祖先/成员血统|adopt|王室血缘与合法成员登记分别存。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D13)/[合同](../../data/contracts/civic_control_v1.json)|
|F079 Clan traits编辑|adapt|王室组织偏好/允许章程编辑，不加血统经济军力buff。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D13)/[合同](../../data/contracts/civic_control_v1.json)|
|F080 宗族统治图层/族长史|adopt|王室统治分布/族长与君主史/成员pins。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D22)/[合同](../../data/contracts/civic_control_v1.json)|
|F081 宗族与忠诚/继任|adopt|明确长子序/选举/摄政，王室与忠诚有实际制度来源。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D13)/[合同](../../data/contracts/civic_control_v1.json)|

## 07 文化、语言、宗教与文献

|编号/功能|决定|如何接入或为何排除|设计|
|---|---|---|
|F082 独立Culture对象|retain|Culture独立实体/多文化强度。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D11)/[合同](../../data/contracts/civic_control_v1.json)|
|F083 文化traits/编辑|adopt|文化习惯制度traits按typed规则编辑，不改基因。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D11)/[合同](../../data/contracts/civic_control_v1.json)|
|F084 独立Language对象|retain|Language独立谱系/多对多文化。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D11)/[合同](../../data/contracts/civic_control_v1.json)|
|F085 语言性状编辑|adopt|音系语法/学习负担/命名规范，与正式父源分开。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D11)/[合同](../../data/contracts/civic_control_v1.json)|
|F086 小谈/传播/语言学习|retain|真实语言接触/学习；动物可学不可创。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D11)/[合同](../../data/contracts/civic_control_v1.json)|
|F087 读书/保存文献知识|retain|书有载体/作者/语言/知识/真实阅读。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D11)/[合同](../../data/contracts/civic_control_v1.json)|
|F088 书传播性状/能力|adapt|书授实际知识，不遗传buff/免费等级。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D11)/[合同](../../data/contracts/civic_control_v1.json)|
|F089 独立Religion对象|retain|玩家创建根、真实信仰与教会。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D12)/[合同](../../data/contracts/civic_control_v1.json)|
|F090 宗教性状/仪式效果|adapt|教义/仪式改变真实行为和任务，不治伤/战斗法术。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D12)/[合同](../../data/contracts/civic_control_v1.json)|
|F091 文化/语言/宗教分裂|retain|文化/语言/教派独立分化来源与资格。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D11)/[合同](../../data/contracts/civic_control_v1.json)|
|F092 元对象共有traits|adopt|共用typed性状metadata，各领域不混基因/制度。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D21)/[合同](../../data/contracts/civic_control_v1.json)|
|F093 命名/Onomastics|retain|来源姓名/语言词根/前名保存可编辑显示名。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D11)/[合同](../../data/contracts/civic_control_v1.json)|

## 08 外交、战争与军队

|编号/功能|决定|如何接入或为何排除|设计|
|---|---|---|
|F094 Plans/Plots对象|adopt|独立Plot计划对象/当事人/期限/目标/投入。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D19)/[合同](../../data/contracts/civic_control_v1.json)|
|F095 计划列表/进度|adopt|阶段/进度/失败原因/任务互通。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D19)/[合同](../../data/contracts/civic_control_v1.json)|
|F096 强制开战/友好神力|adapt|神意关系/开战有明确事件，不伪造签字或免费动员。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D19)/[合同](../../data/contracts/civic_control_v1.json)|
|F097 独立Alliance对象|retain|联盟/条约/成员/持续义务。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D19)/[合同](../../data/contracts/civic_control_v1.json)|
|F098 联盟等级/加成|adapt|实际履约/合作统计替代无来源加成。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D19)/[合同](../../data/contracts/civic_control_v1.json)|
|F099 War信息对象|retain|独立War与参战/目标/结果。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D19)/[合同](../../data/contracts/civic_control_v1.json)|
|F100 战争连线/战斗标记|adopt|战争关系线/实际交战点与selected图层。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D21)/[合同](../../data/contracts/civic_control_v1.json)|
|F101 胜败与参战史|retain|参战/胜败/损失史稳定引用。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D22)/[合同](../../data/contracts/civic_control_v1.json)|
|F102 独立Army对象|retain|单层Army现有人员/器材/补给。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D19)/[合同](../../data/contracts/civic_control_v1.json)|
|F103 指挥/成员/任务栏|retain|成员/主责城市/指挥/任务与补给页。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D19)/[合同](../../data/contracts/civic_control_v1.json)|
|F104 多层Army组织|evidence_boundary|WorldBox多层Army未核实；Sonnheide明确只单层。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D19)/[合同](../../data/contracts/civic_control_v1.json)|
|F105 城墙/城防|adapt|原创前现代防御墙门，真实材料与旧通路证书。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D14)/[合同](../../data/contracts/game_v0_8.json)|

## 09 信息页、选择与编辑体验

|编号/功能|决定|如何接入或为何排除|设计|
|---|---|---|
|F106 地图点选meta|retain|地图点选meta/近景主体/远景政治对象共用Ref。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D21)/[合同](../../data/contracts/civic_control_v1.json)|
|F107 关系页互通|retain|国籍/文化/语言/亲属/雇主/gene等点击关系互通。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D21)/[合同](../../data/contracts/civic_control_v1.json)|
|F108 浏览返回历史|retain|浏览返回历史保对象身份/草案。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D21)/[合同](../../data/contracts/civic_control_v1.json)|
|F109 底部快速资料栏|adopt|底部快速信息与对象相关操作，所有全局工具底栏。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D21)/[合同](../../data/contracts/civic_control_v1.json)|
|F110 多选单位/成员切换|adopt|多选真实单位/筛分成员/批量后果预览。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D21)/[合同](../../data/contracts/civic_control_v1.json)|
|F111 实时任务条|retain|实际任务条/阻碍/计划与对象页。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D21)/[合同](../../data/contracts/civic_control_v1.json)|
|F112 meta成员地图pins|adopt|所选meta成员pins有预算与可关闭。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D21)/[合同](../../data/contracts/civic_control_v1.json)|
|F113 Ctrl数字控制组|adopt|Ctrl+1—9存真实Ref，数字切控制组。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D21)/[合同](../../data/contracts/civic_control_v1.json)|
|F114 快捷收藏|retain|人/动物/物品/meta/标记快捷收藏。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D21)/[合同](../../data/contracts/civic_control_v1.json)|
|F115 死亡/灭绝标识|adopt|死档/灭绝/后继与当前状态明确。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D21)/[合同](../../data/contracts/civic_control_v1.json)|
|F116 搜索/列表/筛选|retain|搜索/筛选/排序/分类对象目录。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D21)/[合同](../../data/contracts/civic_control_v1.json)|
|F117 Interesting Units页|adopt|Interesting Actor按实际事件/风险/收藏解释排序，不瞎抽传奇buff。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D21)/[合同](../../data/contracts/civic_control_v1.json)|
|F118 属性编辑|retain|typed编辑自由属性/程序性制度/不可改事实分开。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D21)/[合同](../../data/contracts/civic_control_v1.json)|
|F119 常驻时间/世界入口|retain|时间/暂停/世界入口始终可从底栏到达。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D21)/[合同](../../data/contracts/civic_control_v1.json)|
|F120 分区/图层开关|retain|八分区工具及可选图层，禁止另一全局工具栏。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D21)/[合同](../../data/contracts/civic_control_v1.json)|
|F121 名字/区域分开开关|adopt|名字/区域可独立显示，不以label控制国界。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D21)/[合同](../../data/contracts/civic_control_v1.json)|
|F122 只突出收藏对象|adopt|仅突出收藏但未选世界正常仿真。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D21)/[合同](../../data/contracts/civic_control_v1.json)|
|F123 极远景物种图标|adopt|远景species/人口分类图标与聚合计数。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D21)/[合同](../../data/contracts/civic_control_v1.json)|

## 10 统计、历史与比较

|编号/功能|决定|如何接入或为何排除|设计|
|---|---|---|
|F124 世界总览统计|retain|世界统计区分人/普通动物/高级意识，避免双算。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D22)/[合同](../../data/contracts/civic_control_v1.json)|
|F125 meta独立统计|adopt|所有新旧meta自身统计与世界可比较。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D22)/[合同](../../data/contracts/civic_control_v1.json)|
|F126 人口组成breakdown|adopt|文化/语言/宗教/物种/年龄组成同口径分解。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D22)/[合同](../../data/contracts/civic_control_v1.json)|
|F127 跨对象比较图表|retain|跨对象同单位/期间/口径比较。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D22)/[合同](../../data/contracts/civic_control_v1.json)|
|F128 长期历史曲线|adopt|长期历史低频采样，有缺口不伪造。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D22)/[合同](../../data/contracts/civic_control_v1.json)|
|F129 图表变化量/数值提示|adopt|变化量/采样日期/真实数值提示。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D22)/[合同](../../data/contracts/civic_control_v1.json)|
|F130 出生/死亡/迁入迁出|retain|出生/死亡/实际迁入迁出有来源。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D22)/[合同](../../data/contracts/civic_control_v1.json)|
|F131 战争/联盟资金声望|adopt|战争/盟约/资金/履历声望分域，无buff转钱。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D22)/[合同](../../data/contracts/civic_control_v1.json)|
|F132 灭绝meta历史|adopt|灭绝对象保档与历史，不删固定species。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D22)/[合同](../../data/contracts/civic_control_v1.json)|
|F133 历史事件回查|retain|事件回查当时名/真实关系/稳定Ref。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D22)/[合同](../../data/contracts/civic_control_v1.json)|
|F134 跨地图累计玩家统计|adopt|本地跨World玩家履历，和单World人口库存分开。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D22)/[合同](../../data/contracts/civic_control_v1.json)|

## 11 World Laws全球规则

|编号/功能|决定|如何接入或为何排除|设计|
|---|---|---|
|F135 文明生育开关|retain|civ_reproduction门控新受孕；已有孕期继续。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D23)/[合同](../../data/contracts/civic_control_v1.json)|
|F136 动物生育开关|adopt|animal_reproduction门控新动物受孕；启智需两开关。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D23)/[合同](../../data/contracts/civic_control_v1.json)|
|F137 人口/亚种限制|adapt|新增配额/亚种软限额，不删除既有成员。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D23)/[合同](../../data/contracts/civic_control_v1.json)|
|F138 植物种子传播|adapt|新野生幼株/生态种子恢复，不能刷木果库存。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D06)/[合同](../../data/contracts/terrain_access_v1.json)|
|F139 矿物随机出现开关|adapt|地下受控矿速率开关，真实采掘转库存。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D18)/[合同](../../data/contracts/game_v0_8.json)|
|F140 文明组建士兵开关|adopt|AI动员/组Army仅现有人/实际装备，不自动刷士兵。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D23)/[合同](../../data/contracts/civic_control_v1.json)|
|F141 外交/新战争规则|adopt|AI外交与新战争分开，既有战争不消失。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D23)/[合同](../../data/contracts/civic_control_v1.json)|
|F142 叛乱独立开关|adopt|AI新反叛组织开关，既有债务/压迫/叛军保留。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D23)/[合同](../../data/contracts/civic_control_v1.json)|
|F143 边界吞并规则|adopt|AI新扩张/边界吞并门控，玩家明确边界事务独立。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D23)/[合同](../../data/contracts/civic_control_v1.json)|
|F144 特殊生物/魔法规则|exclude|怪物魔法灾变规则不符合原生多物种文明。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D23)/[合同](../../data/contracts/civic_control_v1.json)|
|F145 地表保护/熔岩规则|adapt|Godterrain开关/生态规则；炎地静态熔岩不变流体灾害。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D23)/[合同](../../data/contracts/civic_control_v1.json)|
|F146 规则顺序重排|adopt|规则可收藏/展示重排，顺序不改变优先级。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D23)/[合同](../../data/contracts/civic_control_v1.json)|
|F147 年龄/饥饿后果|retain|aging/hunger后果与实际年龄/能量分开。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D23)/[合同](../../data/contracts/civic_control_v1.json)|
|F148 元控制规则|adopt|元控制/限制视野可启用，但全知默认保留。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D23)/[合同](../../data/contracts/civic_control_v1.json)|

## 12 地表生态、植物与资源

|编号/功能|决定|如何接入或为何排除|设计|
|---|---|---|
|F149 地形/生态分离|retain|height/material/主题/Plant/decor分开。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D06)/[合同](../../data/contracts/terrain_access_v1.json)|
|F150 biome种子画笔|adapt|绘八主题生态/幼株，无果木商品复制。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D06)/[合同](../../data/contracts/terrain_access_v1.json)|
|F151 生态扩散/竞争|adopt|有界种子传播/适生竞争，不免费改整棵树species。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D06)/[合同](../../data/contracts/terrain_access_v1.json)|
|F152 生态生物/性状提示|adopt|生境页显兼容species、温度、食物/固定trait边界。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D21)/[合同](../../data/contracts/civic_control_v1.json)|
|F153 树木采伐|retain|真实TreeRef采伐/预约/路程/木储。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D06)/[合同](../../data/contracts/terrain_access_v1.json)|
|F154 可采小植物/草药|adapt|纯装饰小植物不采；需食用草/药时独立机制资源/配方。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D09)/[合同](../../data/contracts/life_genetics_v1.json)|
|F155 持续果物/berries|retain|果树批次渐长/熟色/损耗/采收。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D06)/[合同](../../data/contracts/terrain_access_v1.json)|
|F156 生态相关矿物/食物|adapt|主题不能产矿/食物；资源/ForagePatch与地下源分别建账。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D18)/[合同](../../data/contracts/game_v0_8.json)|
|F157 农业/施肥神力|adapt|真实农业/肥料批次劳动，神力只能播零库存幼株。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D09)/[合同](../../data/contracts/life_genetics_v1.json)|
|F158 自然/幻想biome集合|adapt|八原创soil主题+干沙椰树，不复制糖果/腐化等全部WB生态。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D06)/[合同](../../data/contracts/terrain_access_v1.json)|
|F159 Age影响作物/植被|exclude|仅两种光环境Age，不给作物/基因时代buff。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D23)/[合同](../../data/contracts/civic_control_v1.json)|
|F160 地形修改后人楼变化|retain|地形→支承/水路/人货/城市的同提交后果。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D05)/[合同](../../data/contracts/terrain_access_v1.json)|
|F161 水雨推动生态/作物|adapt|可编辑有界湿度用于真实植物；无凭空赠粮雨/新天气灾变。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D06)/[合同](../../data/contracts/terrain_access_v1.json)|
|F162 蜂授粉/花繁殖|adopt|真实蜂群授粉服务读机制Plant/农田，不把decor变库存。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D09)/[合同](../../data/contracts/life_genetics_v1.json)|
|F163 特定生物死后生树花|adapt|尸体真实腐败/养分入账可促机制生态，不死即刷树花。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D09)/[合同](../../data/contracts/life_genetics_v1.json)|

## 13 地图创建、画笔、环境与镜头

|编号/功能|决定|如何接入或为何排除|设计|
|---|---|---|
|F164 空白世界|retain|Blank海/平陆二基础，不模板。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D03)/[合同](../../data/contracts/terrain_access_v1.json)|
|F165 程序地形模板库|exclude|用户明确不做程序世界模板库。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D03)/[合同](../../data/contracts/terrain_access_v1.json)|
|F166 世界尺寸/生成seed|retain|尺寸/预算/种子/固定profile，与镜头分开。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D03)/[合同](../../data/contracts/terrain_access_v1.json)|
|F167 噪声/多次细化参数|exclude|不做模板库对应噪声/多次细化生成面板。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D03)/[合同](../../data/contracts/terrain_access_v1.json)|
|F168 随机附加陆块|exclude|不自动附加随机大陆；玩家自由画地形。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D03)/[合同](../../data/contracts/terrain_access_v1.json)|
|F169 中心湖/陆/边缘形状|adapt|局部圆/方/线/带状画笔可做湖陆；无世界生成模板。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D04)/[合同](../../data/contracts/terrain_access_v1.json)|
|F170 群岛/大陆/圆环/棋盘等|exclude|不制作群岛/大陆/圆环/棋盘程序模板库。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D03)/[合同](../../data/contracts/terrain_access_v1.json)|
|F171 初始植被/矿物开关|retain|初始树/动物/矿储开关与显式来源预算。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D03)/[合同](../../data/contracts/terrain_access_v1.json)|
|F172 随机biome生成|adapt|创建主题/生态密度可配；不自动随机大陆biome库。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D03)/[合同](../../data/contracts/terrain_access_v1.json)|
|F173 Earth可缩放全球选区|retain|项目特色Earth最高可玩精度海陆选区。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D03)/[合同](../../data/contracts/terrain_access_v1.json)|
|F174 水域/陆地/山地画笔|retain|八地形快捷预设+升降平整；水深/高度派生。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D04)/[合同](../../data/contracts/terrain_access_v1.json)|
|F175 复制附近地形|adapt|只复制几何材质主题，不复制财产生命储量。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D04)/[合同](../../data/contracts/terrain_access_v1.json)|
|F176 笔形/半径/强度|adopt|圆/方/线/带状笔形/半径/强度/间距与预览。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D04)/[合同](../../data/contracts/terrain_access_v1.json)|
|F177 缩放/平移/定位/跟随|retain|真实3D镜头平移/缩放/跟随/定位。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D04)/[合同](../../data/contracts/terrain_access_v1.json)|
|F178 镜头惯性|adopt|本地相机缓动与可关闭惯性，不影响仿真。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D24)/[合同](../../data/contracts/game_v0_8.json)|
|F179 真360°三维|retain|360真3D是自身特色，不声称WB缺点。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D04)/[合同](../../data/contracts/terrain_access_v1.json)|
|F180 暂停/时间倍率|retain|暂停/倍率只改仿真时钟推进，无成果补生。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D23)/[合同](../../data/contracts/civic_control_v1.json)|
|F181 十种Ages|exclude|用户限定Light/Darkness两Age，排除十时代灾变。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D23)/[合同](../../data/contracts/civic_control_v1.json)|
|F182 Age手动/自动|retain|手动关闭自动、tick余量保存。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D23)/[合同](../../data/contracts/civic_control_v1.json)|
|F183 Age序列拖放/排程|adapt|两Age顺序/期限可编辑，无额外Age。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D23)/[合同](../../data/contracts/civic_control_v1.json)|
|F184 天气/冷暗/热影响|adapt|海拔冷热独立模型；无太阳昼夜/随机天气灾害。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D05)/[合同](../../data/contracts/terrain_access_v1.json)|

## 14 物品、建设、存载与Steam

|编号/功能|决定|如何接入或为何排除|设计|
|---|---|---|
|F185 装备耐久/修理|retain|衣物/装备实物耐久、有限维修、更换。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D15)/[合同](../../data/contracts/game_v0_8.json)|
|F186 装备品质/修饰属性|adopt|品质/材料/工艺影响实物性能，无附魔来源。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D17)/[合同](../../data/contracts/game_v0_8.json)|
|F187 物品资料/归属/收藏|retain|物品归属/保管/位置/批次与收藏。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D17)/[合同](../../data/contracts/game_v0_8.json)|
|F188 Loot Rain增删装备|adapt|实物装备调拨/领取/回收替代凭空Loot Rain。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D17)/[合同](../../data/contracts/game_v0_8.json)|
|F189 克隆单位/Clone Rain|exclude|只玩家放置或合法出生，不复制现有人口。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D08)/[合同](../../data/contracts/life_genetics_v1.json)|
|F190 磁铁/神手搬人|retain|神手合法空间移动，峰禁区与身份连续。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D20)/[合同](../../data/contracts/civic_control_v1.json)|
|F191 分类清理|retain|分类清理含真实人货/接入/死亡后果。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D22)/[合同](../../data/contracts/civic_control_v1.json)|
|F192 文明自主建设/生产|retain|实际需求/预算/劳力/材料生产与建设。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D16)/[合同](../../data/contracts/game_v0_8.json)|
|F193 船/港/运输|retain|真实Port/船/航次/物流/补给。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D17)/[合同](../../data/contracts/game_v0_8.json)|
|F194 存档/加载列表|retain|本地稳定多槽保存载入，新工程重新实现。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D25)/[合同](../../data/contracts/game_v0_8.json)|
|F195 自动档/自动保存列表|adopt|滚动自动档独立列表/周期/份数与故障安全。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D25)/[合同](../../data/contracts/game_v0_8.json)|
|F196 退出菜单保存入口|adopt|退出保存入口与失败保当前世界。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D25)/[合同](../../data/contracts/game_v0_8.json)|
|F197 保存镜头/缩放|retain|保存相机位置/旋转/zoom为展示数据。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D25)/[合同](../../data/contracts/game_v0_8.json)|
|F198 地图预览/描述/多槽|adopt|本地槽描述/缩略图/元数据无地图社区。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D25)/[合同](../../data/contracts/game_v0_8.json)|
|F199 地图导入导出分享|adopt|安全本地文件导入导出，需版本与守恒校验。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D25)/[合同](../../data/contracts/game_v0_8.json)|
|F200 Workshop上传/订阅|exclude|用户明确不做Steam Workshop。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D25)/[合同](../../data/contracts/game_v0_8.json)|
|F201 Steam Cloud|exclude|用户明确不做Steam Cloud。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D25)/[合同](../../data/contracts/game_v0_8.json)|
|F202 社区地图独立入口|exclude|不建设Workshop配套社区地图入口/替代服务。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D25)/[合同](../../data/contracts/game_v0_8.json)|
|F203 成就/解锁奖励|adapt|非阻塞本地里程碑；不锁必需编辑/刷物资。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D25)/[合同](../../data/contracts/game_v0_8.json)|
|F204 知识/发现资料入口|adopt|知识/物种/基因/制度/工具百科与实际发现记录。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D25)/[合同](../../data/contracts/game_v0_8.json)|
|F205 模组/扩展支持|adapt|版本化数据/资产包；固定新World清单，不执行任意脚本/改物种包络。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D25)/[合同](../../data/contracts/game_v0_8.json)|
|F206 单人模式|retain|单人沙盒，仅Steam发行。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D01)/[合同](../../data/contracts/game_v0_8.json)|

## 15 设置、辅助交互与表现

|编号/功能|决定|如何接入或为何排除|设计|
|---|---|---|
|F207 工具栏/窗口尺度|retain|八工具栏/窗口尺度，所有全局按钮底部。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D25)/[合同](../../data/contracts/game_v0_8.json)|
|F208 tooltip/地图名字独立尺度|adopt|tooltip/名字/区域独立显示与尺寸。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D25)/[合同](../../data/contracts/game_v0_8.json)|
|F209 隐藏UI/工具栏|adopt|隐藏UI/工具栏可恢复快捷键。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D25)/[合同](../../data/contracts/game_v0_8.json)|
|F210 快捷键/键盘导航|adopt|重绑/控制组/三语无框可见键盘焦点。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D25)/[合同](../../data/contracts/game_v0_8.json)|
|F211 音效与音乐音量|retain|音乐/环境/效果音独立音量。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D24)/[合同](../../data/contracts/game_v0_8.json)|
|F212 背景/场景音乐驱动|adopt|菜单/光暗/和平战争音乐慢混合，只表现。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D24)/[合同](../../data/contracts/game_v0_8.json)|
|F213 云/灯/风/Age粒子开关|adopt|云/灯/风/粒子预算开关只影响表现。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D24)/[合同](../../data/contracts/game_v0_8.json)|
|F214 素材预加载开关|adopt|预加载策略/预算，不能导致不同World事实。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D25)/[合同](../../data/contracts/game_v0_8.json)|
|F215 图表记录开关|adopt|历史采样可关/标缺口，不补假曲线。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D25)/[合同](../../data/contracts/game_v0_8.json)|
|F216 自动保存开关|adopt|自动档周期/份数/关停保持旧有效档。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D25)/[合同](../../data/contracts/game_v0_8.json)|
|F217 帧率上限|adopt|本地帧率/画质上限不改tick因果。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D25)/[合同](../../data/contracts/game_v0_8.json)|
|F218 帮助/教程|adopt|可跳过分步教程/按需帮助，无常驻无意义小字。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D25)/[合同](../../data/contracts/game_v0_8.json)|
|F219 游戏内更新日志|adopt|版本更新日志入口，不常驻碎片。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D25)/[合同](../../data/contracts/game_v0_8.json)|
|F220 多语言|retain|仅简中/英文/德文首批，UI与模拟语言分开。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D21)/[合同](../../data/contracts/civic_control_v1.json)|

## 16 社区目录补充线索，待原版逐项核验

|编号/功能|决定|如何接入或为何排除|设计|
|---|---|---|
|F221 Forest Soil独立地形|adapt|林地由soil主题+机制树构成，不增Forest Soil几何类别。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D06)/[合同](../../data/contracts/terrain_access_v1.json)|
|F222 Vortex打乱地形|exclude|不加无目的vortex随机打乱；自由可控地形刷足够。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D04)/[合同](../../data/contracts/terrain_access_v1.json)|
|F223 Sponge清焦痕/废墟|adapt|焦痕清理纯表现，真实废墟需物权/回收结算。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D22)/[合同](../../data/contracts/civic_control_v1.json)|
|F224 Sickle仅清花草|retain|镰刀只清decor，不获草花库存。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D22)/[合同](../../data/contracts/civic_control_v1.json)|
|F225 Spade仅清biome|adopt|清主题/地皮覆盖保持几何和已存在树/农田资源。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D22)/[合同](../../data/contracts/civic_control_v1.json)|
|F226 Bucket删水/熔岩|adapt|删水必须改变真实高度，熔岩仅静态覆盖。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D22)/[合同](../../data/contracts/civic_control_v1.json)|
|F227 Pickaxe/Axe等细分清理|retain|树/矿源/库存/残骸分类区分，不空赠开采物。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D22)/[合同](../../data/contracts/civic_control_v1.json)|
|F228 Scissors/Paint辖区编辑|adopt|城市/国家画笔桶/剪分合并，官方证明与社区按钮名分开。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D13)/[合同](../../data/contracts/civic_control_v1.json)|
|F229 形状打印器|adapt|局部几何笔形可作形状，不提供程序世界打印模板库。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D04)/[合同](../../data/contracts/terrain_access_v1.json)|
|F230 Healing/Shield/Blessing等|adapt|护理/救援/训练/祝福仪式需要真实人手与资源，不免费治伤/护盾。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D08)/[合同](../../data/contracts/life_genetics_v1.json)|
|F231 Sleep/Dispel直接神力|adapt|行为暂停/休息任务；不魔法消除饥饿疾病意识。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D20)/[合同](../../data/contracts/civic_control_v1.json)|
|F232 Dust抹除语言/家族等|adapt|typed关系编辑/合法退出文化资格预览，不抹历史/亲代/源语言。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D21)/[合同](../../data/contracts/civic_control_v1.json)|
|F233 Smooth Jazz繁育干预|adapt|繁育环境/同伴安排与真实条件，不神力秒孕/生免费人。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D08)/[合同](../../data/contracts/life_genetics_v1.json)|
|F234 Eye of Insight发现特性|adapt|观察/百科揭示允许特性，无必须祭献解锁。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D25)/[合同](../../data/contracts/game_v0_8.json)|
|F235 地图关系箭头|adopt|所选关系箭头/条约/迁移/物流图层可分开开关。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D21)/[合同](../../data/contracts/civic_control_v1.json)|
|F236 思想泡泡/金钱流动画|adopt|任务/思想/真实资金流的按需表现，非新增货币。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D21)/[合同](../../data/contracts/civic_control_v1.json)|
|F237 肥料催生树/植物|adapt|真肥料/土壤湿度/幼株，不催生免费成熟木果。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D06)/[合同](../../data/contracts/terrain_access_v1.json)|

## 17 未核实项与撤回规则

|编号/功能|决定|如何接入或为何排除|设计|
|---|---|---|
|F238 全部当前底栏按钮数量/排序|evidence_boundary|官方没有穷尽当前按钮总数；完整采用自身八栏注册，不伪造WB全集。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D21)/[合同](../../data/contracts/civic_control_v1.json)|
|F239 全部现行biome数量/名称|evidence_boundary|社区biome名单不当完整官方表；用户八主题明确覆盖自身需求。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D06)/[合同](../../data/contracts/terrain_access_v1.json)|
|F240 全部动物/智慧物种目录|evidence_boundary|官方动物全集未穷尽；明确当前自身species名单/后续锁版本，不宣称全照搬。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D09)/[合同](../../data/contracts/life_genetics_v1.json)|
|F241 完整Cold Resistance/Immunity公式|evidence_boundary|WB冷热内部公式未公开；自行设计权威体温模型并标参数工程来源。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D05)/[合同](../../data/contracts/terrain_access_v1.json)|
|F242 普通树/花基因编辑|evidence_boundary|未证明WB树基因编辑；本版Actor遗传完整，Plant真实生长独立。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D06)/[合同](../../data/contracts/terrain_access_v1.json)|
|F243 任意不同物种杂交|exclude|不同物种杂交无证据且突破固定species包络；只同species跨亚种。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D07)/[合同](../../data/contracts/life_genetics_v1.json)|
|F244 全部meta复制/强制改属|evidence_boundary|不猜WB全对象复制全集；Sonnheide强制改属必须typed预览且不复制物资/人。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D21)/[合同](../../data/contracts/civic_control_v1.json)|
|F245 当前全部Plot/World Laws数量|evidence_boundary|未证明WB全Plot/WorldLaws数量；自身有明确规则/计划注册及默认。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D23)/[合同](../../data/contracts/civic_control_v1.json)|
|F246 亚种按出生数升级得trait|exclude|官方旧预告段落撤回，不做出生数升等级送trait。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D07)/[合同](../../data/contracts/life_genetics_v1.json)|
|F247 Meta Control已发/0.60玩家帖|evidence_boundary|官方Meta Control预告尚无正式发布证据；我们自主纳入设计，不称原版已发。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D20)/[合同](../../data/contracts/civic_control_v1.json)|
|F248 最新预告农场重做/尸体腐败/复活|adapt|采用真实农田与尸体腐坏/养分任务；免费复活排除；WB此项仍预告。|[规则](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D09)/[合同](../../data/contracts/life_genetics_v1.json)|

## 额外的用户特色，不伪装成WorldBox原版功能

真实连续坡道与移除后果、峰顶全移动生命禁区、海拔冷热与衣食能量耦合、可读不可改Species包络、文明之光的文化语言来源限制、原始公共制衣/耐久修补、正装缺失不阻工作、Original高分辨率像素美术、离线Earth选区、实际送料和长方体铁丝网施工，均以本项目用户要求/明确设计选择为来源。

程序模板库、Workshop/Cloud、私人马/马槽明确去掉；骑乘/马拉车为消除旧运输依赖的本版停用选择。八soil主题不扩成WorldBox幻想biome全集，两Age不扩成十时代。物品雨、clone、免费复活、物种自由切换与随机灾害不适配资源/人口硬约束。
