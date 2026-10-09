# 决策算法调查、断点与详细设计入口

2026-10-09；v0.8/ADR0016。**设计与有限抽象验算，生产实现未开始。**本轮接续[城市/动作/元控制算法](WORLDBOX_ALGORITHMS_AND_SONNHEIDE_2026-10-09.md)，不重做已经明确的城市规则，不恢复已删除运行程序。原v0.6仅作内容来源；玩家最新要求和当前合同优先。所有新数值为Sonnheide首样参数，需后续真实场景校准。

## 1. 原设计缺口与本次突破

原设计有国家、经济、军队、人格和法律的对象，却缺“谁以什么信息提出，怎样比较，谁同意，实际怎样兑现，失败之后怎么办”的完整连接。仅给人格加权不能补这些断点。

|断点|本次明确的解法|可检验的反例|
|---|---|---|
|国家被当全知人格|机构由实际在任人物、真实程序、授权报告与共享账决策；每人保自己的BeliefSnapshot|君主看不到的敌方仓库改变，不应改变该次提案|
|基因、当前人格、情绪混在一起|life唯一写当前六轴；基因是倾向基线，经历有界改变；技能/信仰/信任独立|编辑DNA不能重置已学经验、当前性格和伤势|
|同一个分数管全部行为|各域有自己的候选、单位、硬门与比较器；共同层仅管知情、承诺、工时、回执|消费者缺衣不该因军事野心买兵器挤掉全部基本预算|
|通过政策就是执行成功|提案、议事批准、实际预算、执行批次、实际接受和反馈分别保存|法律已通过，但粮食不存在：不可发放虚构粮食|
|军役与经济各算满一天|ActorAvailabilityLedger统一实际身体时间；征募重新排民用劳动/照护，留真实代价|同一人不能同时下地、育儿和远征|
|创业者共享无限需求|实际可支付需求、已有可履约供给和已批准项目形成一次配额|十家新工厂不能各把同一缺口当十倍市场|
|错决策被全知核验提前纠正|知情规划和真值提交分离；实际预约/碰撞核验不替指挥官预知胜败|虚假侦察可导致败仗，却不能复制弹药或免费撤军|
|灾损/救济重复归因|源链、原预测、实际receivedTick与唯一学习/领取receipt|减税后玩家淹城，不硬写成减税导致洪水；三次转述不三次奖励|
|算力上限截掉法律/物理|有界高层搜索公平续算；到期法律输入冻结、完整准备后一次发布|4096搜索额度不足不漏选票、碰撞、付款或伤亡|

## 2. 公开资料能支持什么

采用原作者、官方开发者与官方技术资料作方法依据。**没有把WorldBox闭源公式当已知算法，没有复制第三方代码，没有新增依赖。**文献不证明本项目已实现，也不替本项目数值提供心理学或规模保证。

|原始来源|用于哪些设计问题|本项目自行确定的部分|
|---|---|---|
|[Herbert Simon诺贝尔讲稿](https://www.nobelprize.org/uploads/2018/06/simon-lecture.pdf)|有限注意力/搜索、选择可接受方案而非假定全局最优|六轴、候选上限、游戏预算、知情记录；本轮引用官方讲稿索引，PDF直接读取受限，不声称逐页全文复核|
|[Kevin Dill：Nuts and Bolts of Modular AI](https://media.gdcvault.com/gdc2016/Presentations/Dill_Kevin_Nuts_and_Bolts.pdf)|候选动作与考虑项分离、模块化选择|多域比较器、资源/法律硬门、承诺生命周期；不照搬一个万能效用分|
|[Google官方：Minimum Cost Flows](https://developers.google.com/optimization/flow/mincostflow)|供给、需求、边容量、整数流量约束的表达|有限本地报价与真实运力分配；不引入全世界全知最小费用市场，也不依赖OR-Tools运行库|
|[Ariel Rubinstein：Perfect Equilibrium in a Bargaining Model](https://arielrubinstein.org/papers/11.pdf)|分轮报价、等待和具体让步的谈判结构|实际使者、私有保留价、法定批准、误判/拒绝/多议题包；不声称理性均衡必然出现|
|[James Fearon：Rationalist Explanations for War](https://web.stanford.edu/group/fearon-research/cgi-bin/wordpress/wp-content/uploads/2013/10/Rationalist-Explanations-for-War.pdf)|私有信息、可信承诺与谈判失败的战争解释|真实危机事件、有限目标、宪制程序、机会主义扩张、军需与居民代价；不把低关系当自动宣战概率|
|[Guerrilla：Killzone’s AI, Dynamic Procedural Tactics](https://www.guerrilla-games.com/read/killzones-ai-dynamic-procedural-tactics)、[AI信息可视化](https://www.guerrilla-games.com/read/out-of-sight-out-of-mind-improving-visualization-of-ai-info)|按战术角色/局部环境安排位置、显式展示决策信息|前现代七角色/六阵形、真实通路/身体/射界/命令递送和单层Army，不移植现代火力机制|
|[Jeff Orkin作者资料：Applying GOAP to Games](https://www.cs.cmu.edu/~pfr/publications/b2hd-IJCAI2001WS-Planning.html)|以动作前提/结果分解可执行计划|有限阶段与真实任务接口；不承诺全世界最优或用抽象动作直接写实物结果|

## 3. 详细算法入口与因果链

所有合同均为planned_only，正文有字段、单位、前提、候选、比较、状态、失败、存档、预算和未来场景。用同一Ref互通，新增政治记录放现有State/City/Plot/Treaty子面板，不额外堆工具。

|领域|详细文档与合同|完整因果链|
|---|---|---|
|共同核心|[决策核心](../architecture/DECISION_CORE_PERSONALITY_AND_INFORMATION.md) / [contract](../../data/contracts/decision_core_v1.json)|真实观察→来源/过期/订正→人格当前快照→领域候选→承诺→实际工时/资源准入→执行→实际接收结果→唯一归因学习|
|政策、政治和外交|[政策政治](../architecture/POLICY_AND_POLITICAL_DECISIONS.md) / [contract](../../data/contracts/political_decisions_v1.json)|议题→领域草案/预算→八轴宪制权责→修正协商→真实席位表决/签署→精确授权→实际执行批次→覆盖/申诉/再评估；外交有发送/到达/拒绝/让步和真实约束|
|消费、劳动和生产|[经济](../architecture/ECONOMIC_DECISIONS_AND_MARKETS.md) / [contract](../../data/contracts/economic_decisions_v1.json)|家庭基本预算→需求和询价→有限撮合→资金/货物预约→实际领取运输→交付付款/税；排班→真实劳动工资；创业→实际需求配额→资金设备工人→生产/销售→退出/债务|
|战争、战略和战术|[军事](../architecture/WAR_STRATEGY_AND_TACTICAL_DECISIONS.md) / [contract](../../data/contracts/warfare_decisions_v1.json)|真实危机/主动扩张→调查谈判→有限目标与法定授权→实际动员/民用代价→军需路线→有限情报战略→角色/阵形槽位→真实递令→个人行动/武器循环→伤亡/撤退/护理→占领/和平履行|
|社会、生态和研究|[社会生态](../architecture/SOCIAL_ECOLOGICAL_AND_RESEARCH_DECISIONS.md) / [contract](../../data/contracts/social_decisions_v1.json)|双方接触同意→家庭实际资产/照护；实际教师/书籍/语言/时间→学习；既有信仰与实际捐赠；观察问题→实验工时材料→结果→知识→制造训练采用；动物按真实身体/食物/空间决策|

当前六轴为curiosity、piety、altruism、risk_tolerance、order_preference、ambition，q∈[0,10000]。不是六个人格模板；每个角色还受真实职位、技能、经历、关系、信仰、已承诺事项、当前疲劳及知情影响。同一个人可以在不同条件下表现不同，不把国家所有人同步为君主性格。

## 4. 避免千篇一律的具体结果

下表为明确规则下的候选差异示例，不能无条件保证某人格必选某行动；硬门、实际预算和已有承诺优先。

|相同议题|不同主体/背景|为什么会形成不同方案|
|---|---|---|
|缺粮|利他倾向官员、谨慎财政官、被拖欠工资的居民|前者可优先救受困少数；财政官比较真实库存/拨款与分期；居民购买、投诉或真实迁居申请，不能替国库签字|
|建造衣坊|高野心创业者、秩序高且有欠款者|前者搜索新市场和规模；后者可能优先履约/小规模；两者都受同一有限可支付需求配额，不能各拿全部市场|
|敌方兵力不明|求知高的指挥官、冒险高但补给短的指挥官|前者保侦察候选；后者可以偏进攻区间，但没实际首段军需仍不能执行出兵；收到新证据后两者都可能改变|
|宗教争议|同样高piety、相信不同根教义/不信任不同教会者|信仰确信和教会领导信任独立，高虔诚不等于支持同一宗教法或开宗教战争|
|征募|君主、家庭照护者、现役兵|君主提配额与补偿，照护者有真实时间缺口和自己的接受/义务判断；士兵按实际递到命令、身体/训练/危险执行；不是全体瞬间换职业|
|战术受挫|指挥官未收到侧翼报告、士兵已看见冲近敌人|两人K不同，机构战略可能继续，士兵先即时自卫/避险；不会把玩家地图全知直接广播给将军|
|研究失败|求知高且有工具者、谨慎经理|研究者检验假设/实验设计；经理重新看真实资金、采用成本及停线代价；一次失败不删除全部知识，也不免费升级设备|

差异需要可以追溯的主要原因。界面最多四条主要事实，详细指标/候选/证据按需展开；受限控制不泄漏隐藏真值，地图不增加无必要小字。

## 5. 跨系统必须保持的所有权

CurrentPersonality由life写，规划器只读PersonalitySnapshot；社会域ResearchProject唯一写实际研究work/材料/结果，经济域管资金、许可、采用/制造；军事使用现有ActorAvailabilityLedger，不再造一套可重复工时；政治授权不直接改私人所有权或余额。法定身份、owner、controller、custodian、位置继续分开。

同一救济需要有真实NeedReceipt；承诺未到货不消除缺口，到货后从缺口扣一次。same root event保持来源链，可信的新sourceRevision可以订正估计而不当新独立来源。现有有效法不因签署君主死立即撤销，待签/待执行请求仍核验当前合法角色。现有宪制、帝国神授、0/18人口来源、动物不能创文化/语言、正装缺失仍工作领薪和原创自动建筑施工均保留。

## 6. 统一验算的实际范围

入口：[audit_adr0016_design.py](models/audit_adr0016_design.py)，结果：[DESIGN_AUDIT_ADR0016.json](../planning/DESIGN_AUDIT_ADR0016.json)。四份新有限模型批量一次运行：共同知情/人格/工时，席位/财政/ownBelief，有限采购账/工资工时/需求额度，有限动员/弹药/同时命中。两份未改的ADR0015模型只复用原source hash完全一致的通过记录。完整数量和命名反例以结果文件为准。

本轮五合同304个未来生产场景，与ADR0015四合同189合并493；不是493个已通过运行测试。严格JSON/路由/本地链接/原35LAW映射、原稿hash及95来源字节核验也只是设计一致性。没有生产代码，故本轮不调用历史build/CTest，不填写Windows/Steam/GPU/万人性能通过。

## 7. 落地顺序

沿用[11阶段计划](../planning/IMPLEMENTATION_PLAN.md)，先初始地表、主菜单/创建、编辑存载和前置正式美术，再实际人格/动物/工时、公共经济、文化政治社会、企业科研宗教和军事。不是先写一个没有身体、物资和地表的虚拟国家决策程序。每批完整修改与交叉审查后统一构建验收；真实场景通过才登记生产进度。
