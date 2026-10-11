> 现行v0.9/ADR0018：普通地形八档固定高度，无岸边/选区过渡和自然地理实体身份；坡道是连续几何例外。时钟20tick/运动秒、12000tick/游戏日。具体计算以runtime_foundation、life/combat/economy/technology内容包和emergence/interaction合同为准；原生生产仍未开始。

# 决策核心、人格、知情与结果学习

2026-10-09，ADR0016，design_version仍为0.8；生产未实现。机器合同见[decision_core_v1](../../data/contracts/decision_core_v1.json)。本文件补足[v0.8](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md)和[自然动作](NATURAL_BEHAVIOR_AND_LOCOMOTION.md)中尚未展开的决策层；不恢复旧代码，不安装AI引擎，不调用云模型。

## 1. 真正要解决的四个问题

1. 多样性不能只由随机数制造：同样缺粮，受托官员、受困居民、企业经营者和指挥官承担的权限、义务、时间与风险不同。需要不同领域的候选生成、门控、指标与履行链。
2. 不完美不能等于程序错误：人物可以因过时情报、偏好或错误预测做出失败的选择；执行仍不能复制货物、透支不存在的劳动、越过高峰或伪造合法批准。
3. 集体不能是另一个全知人物：国家、城市、公司、教会和Army的决定由真实职位者、程序、报告和资源形成；董事、君主、居民与士兵不能共享一份脑。
4. 反馈不能靠“发生后就奖励”：用原预测、实际观察及可归因事件比较；同一物损、税款、救济或战败不能被五个系统重复学习或扣忠诚。

[Simon原讲稿](https://www.nobelprize.org/uploads/2018/06/simon-lecture.pdf)的有限搜索与满意化、[Dill公开模块AI资料](https://media.gdcvault.com/gdc2016/Presentations/Dill_Kevin_Nuts_and_Bolts.pdf)的考虑项/动作分离是设计依据。以下数值、状态机、人格映射与知情规则是Sonnheide选择，不声称心理学测量、WorldBox内部算法或理论最优保证。

## 2. 分层主体与五个共同记录

`DecisionSubject`是Actor或现有机构；机构决策必须有`accountableActorRef`和当前职位/委托权证明。无人任职则只能继续已合法承诺且有人可执行的流程，不创造默认经理、摄政、使者或军官。

|记录|必须字段|作用|
|---|---|---|
|DecisionEpisode|episodeId、subjectRef、accountableActorRef、roleAuthorityRevision、domain、decisionCounter、startTick/dueTick、BeliefSnapshotId、candidateSetVersion、selectedPlanId、CommitmentRef、logicalWorkCursor、OutcomeAttributionRefs|一次因果决策及其继续计算状态|
|BeliefSnapshot|持有者/角色、观察条目、证据版本、生成tick、定义hash|此主体实际知道/相信的情况，不是World拷贝|
|CandidatePlan|planId、typed目标、预计前提、资源/时间claim、阶段、horizon、指标单位、情景区间、退出/失败规则、子任务接口|完整候选，不把最高分当已执行|
|Commitment|明确主体与受益者、接受/授权记录、允许资源上限、实际预约、到期、退出条件、履行receipt|别人不会因被写进候选就同意或让出财物|
|OutcomeAttribution|原预测、已观察结果、关联effect/root-cause、混杂标记、一次学习receipt、尚未知字段|学习不修改真实历史，也不编造反事实|

Actor有一个主身体ActionEpisode，多项DecisionEpisode可以提出候选但由统一时间/保管资源准入。机构决策把执行拆成现有Actor/产线/设施任务，不给机构虚拟身体。已合法生效法条/既有机构授权不因签署者后来死亡自动失效；尚未提交的个人签署重验当前角色，实际实施由现任合法executor或有效代理执行，不调用死者。集团、城市和Army可以同时规划，但不能给同一人三份全时工作。

## 3. 实际信息如何进入决策

观察条目为`ObservedFactId/subject/predicate/estimateLo/estimateHi/unit/confidenceQ/observedTick/receivedTick/sourceChain/visibilityClass`。可知来源：实际感官、本人当前身体与亲自持有物、有权读取的账户/登记/生产记录、实际教学/信件/使者/侦察报告、真实公开报价。远仓库账面存量是带时间的记录，不保证此刻没有被毁；自动会计不能向每个路人广播物损。

状态明确为`KNOWN_CURRENT、LAST_KNOWN、DISPUTED、UNKNOWN`。Unknown不是0兵、0价、0风险或不存在国家。数量用区间，信心是游戏指标而非严格概率；不把confidence与quantity相乘当新库存。新报告有实际receivedTick，双方说话需要接触/语言或既有原始交流接口，信件需要实际搬运/媒介，不添加即时远程网络。

每种谓词冻结失效时限与区间扩张率，例如移动敌人位置比法律章程更快过期。历史事实不随时间变成“从未发生”；当前状态估计可失效。转述保持源观察ID，同一消息从三名朋友回来只是一条相关证据；矛盾证据分别留档并扩区间，不能投票式把同源传闻变成确认。原来源的实际订正/撤稿以新的sourceRevision和实际receivedTick替代该源旧估计，保历史与supersedes链；不计为第二条独立证据，迟到旧版本也不能覆盖新版本。

`planner(K)`只接BeliefSnapshot和合法自有可用资源视图，不接World私有查询。两个World只在该主体未知事实不同、K与定义/随机计数相同，必须产生相同决策提案；真实执行结果可以不同。玩家God检视/切镜头/看敌人资料不自动进入人物K；受限meta视野使用该主体的同一知情边界。

## 4. 提案不全知，提交不放弃真实核验

计划按已知信息判定“预计可行”，允许误判。准备提交时校验实际身份、可用账额、预约冲突、身体/空间、许可及对象寿命，但不会替人物预测未知远敌并自动选最好战略。买货先按真实公开报价锁定数量与资金；闭盒未知内容不能通过ReserveQuantity失败信息读取。

实际失败生成`NO_LONGER_AVAILABLE、CONTACT_BLOCKED、ROLE_REVOKED、OWN_BUDGET_CHANGED、NEEDS_NEW_OBSERVATION`等此主体可知的结果，不返回隐藏敌军坐标、他人余额或秘密研究。相同未知请求有episode级冷却与有限一次试探额度；一个opaque拒绝只能产生“请求未获接受”，不能推出箱里恰有几件货。正常拿到的新报价/回执/接触证据可解除冷却。

三类约束不能混淆：不可破坏的世界规则/来源/守恒；合法动作的角色权限/合同；人物对法律与风险的认知。已有Plot、反叛、攻击可以明确采用`EXPLICIT_UNLAWFUL_ACTION`任务，记录真实行为、见证和法律后果，不能伪造法令/合法所有权或使用privileged账户转账。这里只开放已有领域定义的违法动作，不添加万能绕权API。

## 5. 六人格的状态来源

保留且只保留求知`curiosity`、虔诚倾向`piety`、利他`altruism`、冒险`risk_tolerance`、秩序偏好`order_preference`、野心`ambition`。每项当前值是整数q∈[0,10000]。这是游戏尺度，不定义六种阶级或强制“暴君/圣人”职业。

life是`CurrentPersonality`唯一作者，导出带revision的`PersonalitySnapshot`；规划器不重新表达allele。遗传中的六项是**predisposition**，出生时按生命域表达初始化潜在基线和当前轴。当前轴、基因、情绪、技能、信仰确信、教会信任、政治意见、个人审美与关系是不同记录。高piety不会自动改宗、听主教、支持宗教战争或得到神授。

真实长期经历可生成有依据的`PersonalityExperienceDelta`，同一root-effect对同一轴只积分一次；各轴保存lastIntegratedTick和整数余数。样例自然变化总绝对量≤120q/30游戏日、每日≤8q；长期滑窗实际receipt计额，存载/临时换域不重开额度。人物也可多年保持稳定。基因编辑只改变潜在基线/之后有界发展方向，不重置当前轴、感情、学到的事实、技能、健康、决定或随机计数。玩家当前人格编辑是独立typed事件，可直接设合法q但不修改DNA；退出控制不会撤回编辑。

## 6. 同一轴在不同领域的具体位置

|轴|候选生成/偏好影响|不能成为|
|---|---|---|
|求知|主动询价/侦察、尝试未知研究/教学、搜索样例预算分配、检查旧判断|免费知识、无限搜索、发明成功概率常数|
|虔诚|接触已有教义、真实仪式/教会服务意愿、与根信仰一致的政策理由|自动改信、文化门禁豁免、教会财富或神圣攻防buff|
|利他|自愿救助时间/预算、公共利益权重、保护依赖者、俘虏照料意愿|花别人钱、代替旁人同意、无代价全民满意|
|冒险|对结果区间的保守程度、可承受自有资金损失、选择不确定路径/战役的倾向|破坏现金/食物底线、峰禁区豁免、不怕真实伤亡|
|秩序|履约、既有流程、换方案成本、守队列/建制度意愿|永久服从任何政权、没有反叛候选、强制道德正确|
|野心|长期地位/经营规模/公职/战争目标、提名/创业和晋升的候选|自动加冕、复制企业、任意抢他人财产|

样例可计算映射：研究信息搜索分额`2+floor(6*curiosityQ/10000)`，包含在固定总预算内；自主方案更换门槛`150+floor(350*orderQ/10000)`个本领域校准效用点；非刚需风险情景采纳系数`1000+floor(7000*riskQ/10000)`，仅对本领域明确损失区间适用。这些是定义包校准项，不取代领域算法，也不把skill和curiosity合并。

感情/疲劳/疼痛影响当前评估视野、等待耐心和身体可用时长，但不能把既有客观义务擦掉。实际相似经历/关系影响特定对象/领域的预测；泛化须明示类别和有界更新，不能一次被兔子欺骗就永久仇恨所有兔species。

## 7. 每个领域自己的比较器

共同流程只统一可追溯性，不统一一个万能总分：

```text
receive permitted observations and actual own-role changes
update only due decisions under saved logical budget
construct domain-specific alternatives INCLUDING wait/observe/refuse
classify engine-hard guards, lawful authority, and expected feasibility
compute domain metrics with explicit units and uncertain intervals
choose within the domain's admissible objective tier and commitment rules
prepare actual resource/time/authority transaction; reject stale proposals
publish commitment + task claims + receipt once
execute real actions; observe outcome later; learn once from eligible evidence
```

消费先保护真实基本预算；政策先权责/宪制和可执行预算；战术先当前实际接触与身体/弹药，战略允许误情报但不赠军队；科研先知识与工具资格。这些硬阶层不能被“野心+1000”抵消。需要加权时先把同一目标各有量纲指标通过冻结曲线变成本领域效用点；钱/日/米/伤亡不直接相加。Pareto未知区间不声称严格支配；只有所有比较维度均不差且至少一项严格好才淘汰。

必要义务/即时危险优先层使用确定性规则。可选近优候选才做有界变体选择：先冻结top-band与稳定CandidateId排序，使用`WorldSeed/subjectRef/domain/decisionCounter/definitionHash`派生整数随机。counter在DecisionEpisode第一次冻结候选时推进一次；反复preview、同episode工作分包、不同worker/查询、存载重试不加抽签。长期不同历史会产生不同候选，不能每帧抽随机动作掩盖无决策。

## 8. 承诺、退出和多主体冲突

计划生命周期：`COLLECTING → PROPOSED → NEGOTIATING → AUTHORIZED → RESERVING → COMMITTED → EXECUTING → OBSERVING → CLOSED`，可进入WAITING/BLOCKED/REJECTED/EXPIRED/CANCELLED。不需要协商的个人动作跳NEGOTIATING；政策授权、企业订单和战争目的采用各自状态机，不能用此总状态吞掉领域阶段。

承诺有最小真实持续量、阶段检查点、最大总支出、最迟完成tick与止损。已走路/已吃材料/已付工资不取消回滚；换方案比较未来增量损失，不把所有沉没成本永远当必须坚持理由。短期排队不重选整生涯；真实危险、目标消失、权力撤回、无法履行或预设止损可中断。

共享`ActorAvailabilityLedger`在同一SimTick禁止身体劳动/移动/军役/照料重叠；兼容轻量感知可以并行但不各计全时工。先执行已有不可分割交接，再按即时安全、真实照护/基本供给、法定义务、已承诺期限、机构批准优先级、累计等待和稳定ID准入。自愿者还必须接受；任职不等于无限待命。新增军役不能挤掉同一小时已锁照护并假称家庭有人照料。

稀缺资源请求按明确角色/权利优先类与requestTick、overdueAge、保存轮转cursor分配，稳定ID只最后tie break；不得永久“ID小者每天先买光”。事务冲突后保原episode/历史/公平等待，只重新核验剩余可行候选。不可让线程先返回者占资源。

## 9. 预测与学习必须有因果边界

冻结原预测窗口、决定当时K、选中与未选候选摘要、claim和退出规则。结果分`EXECUTION、RESOURCE、CONTACT、SOCIAL_ACCEPTANCE、ENVIRONMENT、EXTERNAL_INTERVENTION`。刚实行减税后玩家淹城，粮食损失有Godterrain cause，不记作“减税导致饥荒”；缺证据时保`CONFOUNDED/UNKNOWN`而非硬归咎君主。

只更新主体后来真实收到的结果。对该类已观测估计，用定点有界EWMA `new=old+floor(alphaQ*(observed-old)/10000)`；alpha由sample数/DomainSkill定义，非“智力越高一次就全知”。预测结果未发生或被中途干预，不能评为成功；自己造成同一救济再花钱摆平不能刷无限声望。学习receipt以`subjectRef/rootEffect/domain/metric`唯一，转述不重复。

不启用在线神经网络训练或云LLM为核心；知识、记忆、熟练度和有界参数更新是明确可存的游戏记录。人物能保留偏见或误判，信息/信任改善也有真实路径，不强制所有人最终成为全知最优算法。

## 10. 行为授权与玩家编辑

`LAWFUL_AUTONOMOUS、SUPPORTED_UNLAWFUL_PLOT、LIMITED_META_LEASE、PLAYER_GOD_INTERVENTION`明示来源，彼此不能靠payload冒充。选择对象/打开Brain只是query。关闭一个自主规划类别只禁新的该类候选，不停身体风险、已到期负债、已有pregnancy或已执行effects；既有承诺依据该类别合同完成/暂停/撤回。

玩家人格/机构优先级/法律/预算编辑有版本和后果；预算偏好不能写账户余额。`SetCurrentPersonalityAxes`允许六轴q，不清当前承诺，正常下个due重新评估；即时危险照常打断。强制法律或开战另God event，不能把未同意的普通议案改成“全员投赞成”。默认自动治理继续，不把每次日常AI决策变成玩家审批弹窗。

## 11. 可解释界面

现有person/animal的activity面板、机构对象页和Brain子面板显示：正在做什么、主要原因、知情时间、谁批准、承诺资源、下一次复议、失败/等待原因。最多四个主要事实，按需展开候选对比/证据/分项账/人格作用，不在地图常驻小字。

God可看真实后果与人物当时估计的差异；limited meta只可见授权情报，隐藏真值不能从拒绝提示/图表排序泄漏。当前人格typed可改，遗传基线另链到genome/subspecies。领域算法的parameters为冻结定义包/世界规则或合法政策，不把每个内部权重塞给玩家。

## 12. 性能、存档和工程依赖

物理仍每tick；个人行为低频到期、经济/政策/战略按游戏日或真实重大事件，保存due和逻辑work cursor。样例核心一次候选24、节点32、观测合并128；机构候选32、预测scenario4；单tick全部高层decision工作4096个primitive并公平续算。这是启动校准值，不能据此承诺万人容量。不同skill/curiosity只是既定预算内的分配和预测精度，不让CPU更快者多想两轮。

即时身体危险不等等待高层决策，已批准会计/生理/物损在本批不可分割事务照常处理。积压保旧承诺/明确等待，不能吃掉期限或当不存在。4096 primitive配额只限制可抢占的高层搜索；已到法定截止的选票集合、实际会计、生理危险及不可分割effects不能截成部分结果。重准备计算量大时使用隔离候选/保存前沿，在原法定截止冻结输入，完成依赖闭包后原子发布；延迟计算不延长投票/受孕/付款资格，不按墙钟跳过去。异步结果携World/Session/subject/episode/role/knowledge/definition/logicalSlice版本，旧版本丢弃但不给新随机抽签。UI可显示计划计算中，不捏造已批准结果。

保存：人格当前/基线/显式编辑/经历额度、知识与信任源链、观测时刻/到期、DecisionEpisode前沿/计数、实际claims/承诺、消息在途容器、结果归因与学习receipt、公平cursor/积压。只读候选展示cache可重建，规范候选排序和原决定receipt不可靠载入重选；大史按稳定event/root引用分层存而非每tick复制所有K。

## 13. 决策族的完整入口

政策/政治/外交见[政治算法](POLICY_AND_POLITICAL_DECISIONS.md)；消费/劳动/生产/市场/金融/税/建设见[经济算法](ECONOMIC_DECISIONS_AND_MARKETS.md)；战争/动员/战略/战术/士兵见[军事算法](WAR_STRATEGY_AND_TACTICAL_DECISIONS.md)；家庭/教育/文化语言/宗教/科研/动物生态见[社会与生态算法](SOCIAL_ECOLOGICAL_AND_RESEARCH_DECISIONS.md)。城市扩张与迁建继续使用ADR0015规划器，不用本文件另造城算法。

## 14. 未来验收

机器合同列DC01—DC48。重点反例：隐藏World不同但同K应同提案；同源消息回路不能加信心；角色失权不得以野心继续签署；玩家看敌人不得更新AI；DNA编辑不重置当前人格；同人跨域时间不可双占；受灾不得错误归咎同时政策；同effect只学一次；query/locale/worker/存载不能改变抽样；再可怜的人也不能花不存在的公共钱。

有限参考模型仅验证明确抽象的知情隔离、基因与当前人格分离、样例不同人格选择、接收消息时刻、劳动准入/回执/稳定重试；不验证真实游戏的所有行为、自然语言理解、全经济/政治/战斗、GPU或Steam运行。
