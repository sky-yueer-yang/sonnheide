# 战争因果、真实动员与分层战术决策

2026-10-09，ADR0016＋ADR0018，设计版本0.9。**待实现规范；本文件与有限参考模型不代表战争、渲染或运行游戏已经完成。** 配套[warfare_decisions_v1.json](../../data/contracts/warfare_decisions_v1.json)，共享接口由[决策核心](DECISION_CORE_PERSONALITY_AND_INFORMATION.md)定义。接续[v0.8 D19](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D19)、[政治合同](../../data/contracts/civic_control_v1.json)、[自然动作](NATURAL_BEHAVIOR_AND_LOCOMOTION.md)、[城市生命周期](SETTLEMENT_LIFECYCLE_AND_EXPANSION.md)、[地理和元控制](GOD_CONTROL_AND_GEOGRAPHY_MUTATIONS.md)、[生命](GENETICS_ANIMALS_AND_UPLIFT.md)与实际坡道。旧v0.6军备章节仅作来源；本版已有坡地、可建真实墙门，骑乘/骑兵/马车停用，不能照搬来源的平陆/无墙/八兵种描述。

## 1. 实质难点与研究边界

国家决定开战、指挥官安排作战、士兵执行动作是三个不同决策。统治者可能误判敌人，军官可能收到迟报，个人可能害怕、耗尽体力或主动救助熟人；不能将Army的一个分数写进所有脑。方案认为可行也不等于现实准许：预测可以错，弹药、身体、授权和时间账必须正确。伤亡不是减人数指令：军人仍是原有居民，其家庭、劳动、欠薪、衣物、王冠与城市连续性必须接续。

[Guerrilla官方动态战术报告](https://www.guerrilla-games.com/read/killzones-ai-dynamic-procedural-tactics)描述依据静态环境与动态局势评估位置。本版采用候选位置逐项评估，不移植现代枪械、资产或原参数。[CMU作者分布式计划研究](https://www.cs.cmu.edu/~pfr/publications/b2hd-IJCAI2001WS-Planning.html)研究周期通信下的团队计划和对手模型，启发有期限的协同与有限解释；足球实验不是军事真实性证明。[SHARCNET研究记录](https://www.sharcnet.ca/my/publications/show/2188)讨论含伤亡历史的时空影响图，启发有来源、可过期的威胁场。[Guerrilla官方解释工具报告](https://www.guerrilla-games.com/read/out-of-sight-out-of-mind-improving-visualization-of-ai-info)启发保存“为何/为何没有”的决策输入。

下面的公式、状态机、参数是Sonnheide自主工程选择，不是WorldBox内部公式。研究引用不等于第三方运行依赖或许可锁定。目标是有来由的多样选择与错误，同时禁止全知、刷兵、免费补给、瞬移阵位、重复扣账和载入刷射击。

## 2. 主体、数据与时间

`CrisisRef`记录争议、证据和外交过程，不先自动设敌对再倒编原因。`WarRef`记录政治参战者、声明事件、授权、目标版本、阶段、实际行动、占领、损失、谈判和条约，不保存神奇战争分数。`ArmyRef`是唯一持久军事组织，直接引用现有Actor、指挥官、驻籍主责城市、预算合同、有限实物SupplyContainer和任务/阵位。

`TacticalAssignment`仅为一个Army内有期限的Actor→role/slot/task映射，不得有独立国籍、预算、征募、库存或永久团营编号。正式分兵创建另一个平级Army，真实人员/器材只转一次。多个Army用`CoordinationToken`关联现有计划，不能建立军团。船籍和产权仍归Port或合法owner，军队取得实际乘载合同。

每次决策使用共享`DecisionEpisode/BeliefSnapshot/CandidatePlan/Commitment/OutcomeAttribution`；StateEpisode负责者是现行法定角色，ArmyEpisode是军官，SoldierEpisode是个人。行为预设不替换六轴、技能、身体和历史。

首样20tick/运动秒、12000tick/游戏日、360日/年。物理、接触和生理每tick；战术10tick错相复评，作战120tick、危机1游戏日低频与事件复评。全为冻结WorldProfile的工程样例。紧急碰撞/死亡/危险进入必算队列，不随镜头、帧率或worker先完成改变结果。

## 3. 当前人格、经历与分层评分

只读`PersonalitySnapshot`当前六轴`curiosity,piety,altruism,risk_tolerance,order_preference,ambition`，q∈[0,10000]；基因只给初始/潜在倾向，不覆盖当前人格。身体能力只读`PhenotypeCapacities`；恐惧、士气、训练和关系另账。

|轴|政治领导者|指挥官|个人士兵|
|---|---|---|---|
|curiosity|愿付真实调查成本、检验单一敌情解释|重侦察收益、可证替代路线|观察陌生声源，仍受真实安全/职责约束|
|piety|既有信仰与神圣地点影响价值/合法性解释|既有誓约和目标认同影响信任|真实共同信仰/仪式形成认同，不给伤害加成|
|altruism|重居民伤亡、民粮、征募家庭损耗|重救伤、补给人员、有限追击|更愿承担救助真实同伴的实际风险|
|risk_tolerance|比较方案均值/尾部损失时不同权重|影响暴露、余粮安全量、撤路偏好|影响迟疑/坚持/逃逸时机，不增生命值|
|order_preference|重条约程序和授权稳定性|稳阵同步或局部自主变位偏好|等待命令/遵守队形/借遮挡偏好，非永久服从锁|
|ambition|重扩张、履约和声望的实际职务目标|争取主攻/成果，仍计职责与损耗|偏好表现与岗位竞争，不凭野心涨技能/射速|

同一轴可支持不同结果：虔诚者可能因慈悲拒战，也可能重视既有宗教通行争议；须结合真实信仰、价值与证据，不能“虔诚必攻异教”。高野心也可能选更有希望的贸易和联盟。缺粮、伤痛、照料家属、欠薪、既往背叛、共同训练使同人格者不同；piety不等于根宗教归属或教会信任。

每候选先硬门控，再按领域指标评价。人、克、钱、tick先保原量与不确定区间，仅在冻结参考尺度转有界dimensionless q后加权，禁止直接混量纲求和。政治候选保留目标改善、可观察民损、财政/粮时和未来退出；战术候选保留职责、到位/暴露/支援/退路和个人代价。风险分布只来自有限Belief场景：低风险偏好提高lower-tail权重，高风险偏好提高有证据mean收益权重，所有候选仍有风险成本。未知不当0；调查、等待、撤退候选始终存在。

成年、工具fit、库存、唯一劳动排班、实际支承、高峰和伤害账是不可破坏的门。法定授权阻未经授权的公共签署/账户/动员，不保证人人守法；仅已有领域明确的EXPLICIT_UNLAWFUL_ACTION可以提出实际袭击/违令，保守恒/来源/World Laws，写犯罪见证与法律后果，不能伪造WarAuthorization、征用令或privileged转账。勇敢不能穿墙，纪律不能不饿，神意控制不能无限体力。合法但差的未来预测可导致失败，真实commit不能把领导者过滤成全知完美统治者。

## 4. 有限情报与命令信息

ObservedFact保存subject/predicate、estimateLo/Hi和单位、confidenceQ、observedTick、receivedTick、sourceChain。来源限定本人真实视听、现有侦察者、合法公告、送达盟友/城市/运输报告、已有地图档案。PlayerGod全图不复制给AI。同根消息转述五遍仍一条证据；矛盾报告保留范围，不以五次转述提高五次可信。旧observedTick的迟报不能覆盖较新直接观察。

敌踪过期扩为已知几何/可能运动能力约束的可达区域，不精确追随隐藏真位置，也不当超时已死。视线使用实际眼/观察接口、身体采样点、真实遮挡、Light/Darkness和实际灯具；声音只给不确定方向/范围，不泄精确敌ActorRef或私库存。已知地图会被Godterrain改坏；走到封路产生局部受阻事实，不主动拿到整张新海岸。

WorldTruth只给执行器做守恒/碰撞/权限。失败反馈限定授权信息`BLOCKED_LOCALLY/RESOURCE_UNAVAILABLE/AUTHORITY_EXPIRED/CONTACT_LOST`，不得泄敌隐藏库存量/远路几何。相同opaque失败无新证据保存backoff，不通过枚举假攻击做探测oracle。自身手持ammo/身体即时可知；远方自家库存仍需报告/合法查询，不以Army关系当全球即时通讯。

## 5. 危机来源、目标与状态机

因果源限定真实事件或有来源争议：越界/占领、通行封锁、条约违约、赔偿到期、受保护成员受攻击、土地title冲突、宗主义务/独立、继承合法性、既有文化宗教权利受限、运输货物争议。以`sourceEventId+claimPredicate+claimant`唯一归因，同一次粮短缺不能算三次被侵害。领导者可主动扩张，但标`OPPORTUNISTIC_CLAIM`，不能伪“自卫”。关系低/邻国相邻本身不每隔一天roll战争。

机会主义扩张与政治域OPPORTUNISTIC_EXPANSION_INTENT对接：可为已知土地/交通/资源提出有限吞并目标，国内授权不证明对外无侵略或无条约违约，必须分记评价。目标typed为DEFEND_MEMBER、END_BLOCKADE、ENFORCE_EXISTING_TREATY、OBTAIN_CITY_SOVEREIGNTY、SEEK_DEPENDENCY、SEEK_INDEPENDENCE、SECURE_LIMITED_ACCESS、SEEK_COMPENSATION，包含对象、授权范围、证据、满足条件、最大主张和退出条件。主张不等于主权已获得，爵位不赠军力。

状态：`OBSERVED→VERIFYING→CLAIM_FRAMED→CONTACT_REQUESTED→NEGOTIATING→AUTHORIZATION_PENDING→DECLARED→ACTIVE→ARMISTICE_NEGOTIATING→SETTLED`，可转DEFERRED/DEESCALATED/CLAIM_WITHDRAWN/AUTHORITY_LOST。收到实际袭击可立即局部防卫，政治声明仍单独授权，不倒写以前已经签宣战。

每次最多16一级候选：忽略/承认争议、调查、真实使者、履约/赔偿、交换通行、局部警戒、仲裁、有限封锁提案、动员、请求联盟援助、撤主张、宣战。提出封锁不自动关敌港；细化真实人物/费用/路线/签署/期限，保留等待回退。有限对手解释来自收到证据，不做全图未来minimax。

## 6. 宣战授权、World Laws与和平政治接口

普通AI新声明必须ai_new_wars开启、主体活且角色有效、目标和对象生命周期合法、宪制门/签署/quorum真实、当前启动费用可承诺、版本/冷却有效、发现规则满足。RULER_DECREE/ESTATES_APPROVAL/CHARTER_COUNCIL/PARLIAMENT_APPROVAL按现行宪制求真实批准者；摄政不自动得无限权力。批准者各用自己的情报、人格、民损与预算责任，不为quorum刷代表；可批准防卫而否决吞城/过度征粮。

ai_new_wars关同时阻新自主声明、战争Plot和新有组织未宣战进攻，不能改名防卫绕过；孤立违法袭击留实际犯罪证据，不伪WarAuthorization或自动国战。关闭该开关不终结既有War，不禁止实际即时有限自卫，也不暗造新War。防卫必须关联正在发生的实际攻击/本地保护，不允许把跨境攻城或持续外扩改名成自卫。ai_recruitment关只阻新自主征募，ai_diplomatic_plans关阻新范围外交计划，已签履行继续。动员/进攻授权和声明独立。最低启动资源是声明/使者/首段真实承诺费用，不要求真相保证未来胜利；未来供给预测可错。DECLARE_ONLY威吓可0出征人，需明确无军队执行，不能免费占领。普通AI进攻首段人员与军需必须真实兑现。

联盟/帝国军事义务用已有条约触发现有成员决策、真实请求答复，不自动造第三国军队/钱/库存。未到援兵只计预计到达；同信仰/联盟等级不赐跨国库存支配。新和平候选由政治域处理，军事输出PeaceFeasibilityReport：己方授权支出、已知损失、核验占领、Belief敌需求/损失范围、有效目标、各方签署权限、真实交付能力、不可达交接与居民权利风险。不能读敌绝密财政当底价。

## 7. 动员、经济代价与真实配装

只从现有合格CivicActor征募：人18、启智动物species成熟、普通动物不当战士；实名资格、合法志愿/征召、伤势、岗位身体/器具fit、单一Army义务、照料和有效ControlLease都要核。启智不给手、枪和人形制服。

`SurveyLabor→ProposeRecruitment→ObtainLegalConsentOrOrder→ReserveBudgetAndEquipment→AssignDutySchedule→TravelToMuster→IssueActualEquipment→Train→Ready`。已认征召不等于已到场。军役从共享权威`ActorAvailabilityLedger`领取排他时段（`ActivityTimeLedger`仅为经济工作视图别名，无第二份额度），通勤、劳动、训练、照料/休息各占真实tick，不能同人同时满农工+军役；雇约停工/欠薪/辞职沿法律记账。

LaborDisplacementForecast给实名农业、配给装卸、运输、护理、基础制衣/维护和家属收入缺口，保的是**有工具/可达岗位的真实排班能力**，不是留四个没有器材的名字。AI有界人员匹配先满足民用保留/护理/法律再减服务缺口和技能错配。玩家明确合法超额可覆盖AI安全偏好，仍不得越法/身体/唯一排班。战争需求有实际授权资金/采购或明确征用才成订单；unfunded未来愿望不能占企业ProjectDemandQuota/创业有效需求或变现货。

制服武器护具从真实stock领取，按fit与岗位核；颜色服役国theme而非国籍。没有制服不能免费变装，未配装者只能承担当前可行非战斗岗位；初期可合法已有简装，不强迫先造现代成衣厂。训练用现有人/教官/可达空间/器材/时间/实际消耗，不能一次Armylevel给全员经验。

工资和口粮分账，BudgetContract指定主责City/拨款者；欠薪due/partialpaid唯一，不同赞助城不能重复付/重复恨。指挥官死不删Army，停止新授权；已有效送达订单按保存条件续，现有合格继任/低权限代理经实际门，命令不瞬时传全军。退役/解散保Actor、欠薪、仍有借用实物归还任务和回家路；God转国END_OLD_DOMESTIC_SERVICE同事务处理，不自动加入新Army/发第二套军衣。

## 8. 实物补给、陆水运输

ArmySupply是有限容器，批次有ItemBatchId、owner/custodian、quantity、品质expiry和预约。实际接口领/到货后可用；预算、订单、预约、在途、真正到货分开。foodCoverageTicks按个体可食实际能量/实际预测消耗，腐败/不可消化/取不到不算。粮覆盖、弹药次数、衣物保温、备件和伤员护理不合为万能supply百分比。

线路`CitySquare→LoadingApron→actual Road/Port/Voyage→合法真实unload→finite ArmySupply`。敌控桥/门/港改变实际路线，现场运输者受阻报告；远方脑不自动知断桥。截供给不能删除另一大陆库存；夺货需实际接触、战争法、产权/托管转移。军供优先需真实授权/民粮保留，挤民粮产生实际社会后果，不凭按钮无限补给。

船按真实舱位/负重、船籍合同、WetRegion连续水路、水深/泊位与卸载点；实名人进入实际容纳，船上不在陆地阵形/隔岸近战。港楼毁保船籍/在途人货；海滩登陆需合法可达卸载点和有限随军容器，不赠广场。无安全点只能等候/返航/真实救援。无私家马/骑兵/马拉车，也无第二套海军层级；民用车仅运输，不挂现代武器。

## 9. 战略方案的有限分解

模板：守真实交通节点、保供给、护航、集中救援、牵制已观察敌人、解封、夺可用据点、围困、撤出重整、护平民撤离。最多12模板、每模板3路线/时序、48展开节点、深度4；耗尽保存logicalWorkCursor，不看worker先返回。空候选安全等待/局部防御，不默认攻未知敌。

CandidatePlan保目标/满足predicate、real或estimated人数、下一段资源排班claim、route revision、部署空间、粮覆盖、撤路、伤员运输、并行互斥、观察前提、条件分支/回退和deadline。先硬非法/不可兑现当前承诺，再评职责/居民险/目标改善、预计损失/钱/粮时和退出。对手保最多5种有证据假设如守桥/去供给/分散，只有实际收到其行动更新，不给国籍固定智力标签。

多线实名人员器材互斥，不能总Army人数算两遍；集中按到达时窗、道路/门/泊位容量和集合空间分路线出发批，不向一个点挤。CoordinationToken记录各peer已收订单/ack，死亡取消/失效使条件撤销，按deadline回退，不能永等灭Army。

## 10. 战术站位、角色与承诺

Army最多10目的：继续、守位、保供给、接敌、可证侧路、支援、掩护重整、有限追击、撤退、停火/投降请求；个人最多8动作/24站位。从已知任务走廊/邻域和真实cover接口生成，非全图撒点。必须真实SurfaceRef、body净空/坡宽/转弯/水深/峰禁区。

指标保到达tick、容纳表面、观测威胁角区、观测友军、退路、己方射线遮挡、负重能耗/湿冷、民用接口阻塞和预约。近战重实际触及/授权通道；射手重真实射界/友军遮挡/装填支承；医护重伤者接触/搬运退出；炮手重真实器材/crew/回转净空。掩体为真墙楼树干材料，decor草/色板不挡弹。

侧方位置不直接加伤害：敌朝向、反应转身、射线、接触和避障实际产生优势。高处也不加基础伤害，实际温度/路线/视线照常。承诺10–40tick样例下限+switchCost；死亡危险、外部有效命令、目标接触失效、地形撤证立即抢占；无新证据不反复切更好同类点，换命令不清装填cycle。

### 战略/战术比较器的首样可计算版本

先按职责/授权tier保留可准入集合：即时生命危险→已承诺保护/撤退→现行WarGoal可行阶段→可选侦察/重整。tiers是当前动作门，不把永久“进攻永远高于休息”写死；有足够退出理由可结束旧承诺。各领域自己的wait/observe也在集合，不能先删等待逼开战。

raw成本x的归一化为`N(x;r)=10000*min(max(x,0),r)//r`，r是当次冻结正参考量；r=0且x>0为不可承担/需要新授权，不用epsilon允许支付，r=0且x=0归0。钱以真实允许支出的当前余额/承诺额度作r，路径tick以阶段horizon作r，粮/照料/衣物流缺口使用经济接口各自实际基本需要r；民损取这些分量的最大缺口q并保完整raw向量。对人数风险使用决定时已知候选真实己方成员数作r，不能用地图总人口稀释伤亡。未知的r/成本不得自填0，转observe/带明示区间的合法等待。

首样OperationScore仅用于同tier合法候选（w与指标为有界dimensionless）：

```
riskLossQ = ((10000-riskQ)*upperLossQ + riskQ*meanLossQ)//10000
S = (wGoal*goalImprovementQ + wSupply*remainingSupplyQ
     + wCivil*civilProtectionQ + wInfo*informationGainQ
     + wValue*existingValueAlignmentQ
     - wLoss*riskLossQ - wTime*arrivalDelayQ - wCost*costQ
     - wSwitch*futureSwitchLossQ) // sum(abs(w))
wGoal=2200+ambitionQ//5; wSupply=2000+orderQ//10
wCivil=1200+altruismQ//3; wInfo=400+curiosityQ//10
wValue=400+pietyQ//10; wLoss=2400-riskQ//10
wTime=800; wCost=1200; wSwitch=300+orderQ//10
```

这些是可改校准值，不是真实心理模型。goalImprovementQ由typed满足条件的预计完成幅度产生；remainingSupplyQ由真实已分配可食能量的预测覆盖时窗算，预计到货单列区间；informationGainQ是本次观察能区分的有限假设比例乘可信新源覆盖，不用同源转述提高。existingValueAlignmentQ是既有价值/目标的[-10000,10000]支持程度，不能因piety高默认反异教；wValue只是使该既有价值更重要。meanLoss/upperLoss来自≤4可见情报scenario预测，非WorldTruth伤亡；当前已受伤不在未来损失中再扣一遍，继续life实际账。

个人站位/动作采用另一比较器：先能合法到达并不锁死实际退路/同伴接口，再比较职责接触Q、观察到的支援Q、实际coverQ、exitQ与到达/负重/曝冷/友军挡线/切槽成本。近战、射手、护理、炮手使用不同role模板权重；不得直接套国家扩张wGoal到士兵。角色所需weapon/body硬不足没有分数。选择保存`rawMetric→normalizerRevision→q→weight→contribution`，近优变体仅共享决策核心counter抽一次；承诺存新目标相对旧目标的真实未来收益和切换门槛，不能每tick重抽。

方案细化逐层验证各request尚未consume，ActorAvailabilityLedger共用；score只决定提出哪个请求，不保证资源竞争赢。因缺粮被拒会给出合法本地回执并重估，不再读取隐藏库存到原score里。自主故意违法动作使用既有明示模板的单独准入集合，不能用这份普通合法OperationScore伪造王冠批准。

## 11. 六阵形槽位算法

Line/Column/Square/Wedge/Loose/Echelon；Square为多向保护和围住傷员/器材，旧抗骑作用停用。阵形安排真实位置/间距/朝向/纪律，无全属性buff。

1. 冻实名参与者、body/负重/器材，排船载、昏迷、有效外部lease和正在保留救护者，解释排除。
2. 拖线/AI空间产生anchor；Line spacing≥相邻body半宽+gap，Column沿真corridor，Square边长从人数体规算，Wedge逐层，Loose用合法cover，Echelon有限offset。牛不压成人槽。
3. ≤n+16备选slot以实际SurfaceRef/完整sweep核验，不能同XY配到错楼层。未知地形先接近确认。
4. 角色/body/domain过滤，再按到达tick/能耗/队列穿越/切槽的领域q匹配。保已合法近slot；最多64人稳定计算块、lower-bound插入、有限swap，非全军O(n³)分配；分块非新Army。
5. 一slot一预约，planned不等于已到；自然motion和公平窄口token实际展开，空间不足缩窄/Column/保旧，途中真可受攻击，绝不瞬移。
6. 到位/朝向/邻接误差形成解释cohesion，影响协同与命令执行，不乘基础damage。

## 12. 命令递送与个人判断

ArmyOrder保存orderId/sourceKind/issuer/authorityRevision/Army/actorSetSnapshot/objective/routeOrArea/engagementPolicy/formationIntent/issueTick/earliestStart/expiry/revision/token。普通命令经有范围/训练/共同Language的实际信号语音，远处用实际传令路线，保存receivedTick和未送达消息；一声号角不传敌人全图私坐标。UI英文不使人物会英语。

军官给意图/范围/纪律/退路，士兵用自己的感知/身体执行。守位者失支承/即时危险可直接避险再报告；职责、承诺、信任/风险参与判断，orderPreference低不代表每tick反叛。玩家ControlLease是明确外部高层intent，可直接分发到实际leased集合，并非伪军官瞬时通讯；未lease者仍走普通传令。God战争政治事件不自动lease，选Army只开UI。

队列条件仅真实到区域、收到友军就位、自己供给到阈、观察到目标、指定SimTick等有限AND；未知不当已满足。齐射/协同有实际收到信号、ready比例、deadline与明示fallback（自由射/保持/撤下），没收信号不全军一起扣ammo，不无穷等。

## 13. 权威武器循环、弹体和伤害

七角色民兵/盾剑/长枪/弓/弩/火绳/燧发；火炮Army实物、现有合格crew。参数为抽象游戏量，不提供制造配方或现实武器性能。WeaponCycle保存IDLE/AIMING/READY/FIRING/RECOVERING/RELOADING/BLOCKED、weapon/ammo、start/lastIntegrated、required/completed/remainder、loadedQuantity、cycleCounter和Shot receipts。

加载把实际stock转入loaded chamber/quiver，开火从loaded扣，不reload/fire各扣一发。中断保已装实物和剩工作；换目标、暂停、载入、改国家不重新满装。开火核当刻活且可执行、真实custody/fit、支承/合法船射位、真实ray/方向/射程、当前接触或显式合法区域射击、ready/loaded>0、有效权限/期限和unique ShotId；无ammo仅UNLOADED，粒子不伤人。

ProjectileRef为权威非生命实物/flight record，不是视觉粒子或移动生命Actor。保存source/owner、初pose/velocity、game gravity、消费receipt、expiry/lastIntegrated、hit账。瞄准点/方向从当刻真实自身枪姿和本人的ObservedFact或已授权区域产生，实际提交不把过期目标自动吸到敌人的隐藏新pose；敌移动后可实际射空/被挡。每tick完整swept查询，hit时间后稳定Ref排序；楼墙树干/护甲真阻，decor不阻，不因帧跳/只截16近邻漏人体。瞄准误差由实际skill/疲劳/持具稳定/距离及saved perShot RNG，重画/LOD不再roll。

伤害按有限body collider、实际部位/方向、护具coverage/material/durability进入life伤势；头盔不护脚，坏护具不假保护。近战用权威contact window/距离朝向/格挡/武器sweep/body能力，animation notify不写HP，IK不伸长武器。炮需真器材支承/回转/crew排班与工作，维修不复制已发弹。

同tick合法开火从相同snapshot准备，再按physical hit time结；同瞬间hit先聚合再唯一death，ActorId先者不能免除同时反击。完全同时打中同一护具的多个contact以同瞬间护具覆盖/耐久快照计算，再唯一聚合耗损和伤害，不以hit ID抢先磨穿护甲让后序hit白赚穿透；下一实际时刻才读取新耐久。已发projectile在射手死/停战后继续，死亡时刻后未发动作取消。geometry/control变化重验proposal，不能旧枪姿穿新墙；重复Shot/hit receipt拒二次消费/伤害。

## 14. 士气、撤退、追击与护理

个人fear/stress、pain、fatigue、sleep/energy/exposure、观察损失记忆、commanderTrust、dutyCommitment和exitBelief分开；Army页用分布/分位，不能平均morale掩盖受困者。只见/收到的损失进记忆一次，未知远方死亡不瞬降士气。STEADY→HESITATING→SEEKING_COVER/ORDERLY_RETREAT→ROUTED/REQUEST_SURRENDER；恢复靠真安全tick/休息/food/护理/可信命令，无将领回血光环。

恐惧影响选择/响应/hold；疲劳伤势已经经life影响运动/持具，不再叠低士气0.5damage。撤退核实际可达出口、body/伤员负重、威胁范围与food，分有限点再集合；无路找真实遮挡/降武器请求，不无穷穿墙。追击有origin、识别、时间/距供给界限/归程、体力余粮，失敌有限LastKnown搜索，不追隐藏真位置。救同伴/收实物/保补给与追击比较。

投降需表达和接收，不删除Actor/文化/伤势/产权；接受者有真实看管、容量、食物、安全route，可谈disarm-release而非无限收押/无牢房就自动杀人。拘留与臣籍/原Army分开，招降改籍走法或显式God。伤员护理实时间/材料，搬运核真实体重/器具/人数/净宽，不把牛塞人担架；沿已有护理不新造医院产业或免费回血。

## 15. 墙门、围困与占领

墙门实材维护/净宽/开关时间，非沿行政边界免费刷。封锁实际守住通行/射线与合法禁止，不将全地图nav设无限只放自己。围困从实际当地粮/人员消耗/路线流量产生危险，不每天抽象减10%居民。候选观察通路、谈判、实际围困、待援、合法军事墙门/器材攻击、从真破口进入、撤围。

首版不逐像素楼破碎；whole object/稳定structure section有durability、support与唯一物损/生命hazard，暴力毁损不能套Godterrain免费wipe/退款。修墙仍送料后自动工期，无人物挥锤结算。

占领需真实行政执行界面、当期主要交通入口控制、合格驻留/值守时间、组织守军已不能当地有效执行行政、真实food/治安能力；关键节点revision固定，至少实际入口+实际界面，非占所有世界道路，也非一人踩旗。commit用真实控制，未知守方反抗只反馈可观察未能维持，AI无需被赠全敌情。

广场毁只剩逻辑custody/容量0，不能当隐形占领点；DAMAGED仅可用真实建成/批准有限应急接口；LANDLESS无可占地表域，不在海里captureCityIndex。占领控制不自动改主权/私人owner；政府货依法接管也需实际custody，私人企业教会库存按战争法/征用程序，不变全城战利品。居民/租约/税/在途货/岗位/lease同笔清理，活人不注销。

## 16. 停战、和平履行与地改后重规划

政治域按实际价值/授权和战争报告比较continue/ceasefire/limited settlement/installments/withdrawal/prisoner exchange，不morale<30自动和平。停火有效tick/范围/受约者/legal事实commit，真实通知送达/ack另外保存：已知签署者停新射；未通知者可能依旧命令发生真实违令事故，不能悄延法律tick或删旧炮弹。事故独立调查，不一炮自动新世界大战。

和约签署与履行分开：领土复用全主权/行政/职位/税/租约闭包；赔偿整数分期due；有限通行；俘虏实交接运输；解封实际撤令离位。和平保伤势、欠薪、武器损耗、归还任务、旧债务。不读敌绝密财政当底价，不假第三方签署/不存在国/宗教或帝国批准。

地形typewipe按ADR0017删除命中独立ArmySupply/船、地面军械与非随身货。活Actor实际穿戴军衣、手持枪、膛内弹和携带备弹容器全部保留，保装填/冷却/Shot计数及绝对状态。只对被毁地表对象记loss，并撤失效route/slot/world anchors/support/远水网/目标预约；不能无条件撤仍存在weapon/grip，不能补发装备。生命保pose进真实hazard；高峰规则不变。

军事事务：generation/权限/WorldLaw/intent核验→现有合法work/reload/伤势积到tick一次→真人员/stock/custody/排班/geometry/cap→冻结同瞬间合法动作→唯一消费/loss/hit/damage→统一death/estate/成员/Army/City/State/crown/lease→receipt/events→发布。readset冲突整笔重prepare/拒绝，不先扣粮再征人失败。已劳动/到货/发弹保事实；Army不因主责City归档消失，按真实主体/成员另寻合法承担者，无不存在国继续新招。

## 17. 算力、失败、持久化与信息页

dueTick/overdueAge/subjectRef稳定队列，高层候选共用决策核心每tick4096 logical primitive全域配额，军事样例份额1024并由稳定公平队列分配，非另加1024；每Army128指标、16精路径；真实战斗/近碰撞/生理属于必结算执行域，与高层候选配额分开记账；不足时减低高层计划吞吐/放慢墙钟发布，不能跳必算物理或发布半tick。不是wall-clock截止/worker竞速；耗尽保存cursor、保承诺或安全等待。visibility/nav局部缓存按依赖revision，改地远水网也撤证，不能只invalid笔刷chunk。最终容量未承诺，需生产校准。

保存episodes/Belief源链/记忆信任backoff、目标授权成本、order send/receive/ack/expiry、batch/custody/预约、ActorAvailabilityLedger、formation计划及实际slots、token deadline、reload工作余数loaded、projectile/hit/death账、occupation节点时长、POW、treaty due、RNG counter/cursor；只已提交World。load不重发工资/弹/订单、不补技能/体能；新session旧playerlease失效，真实已接订单按继续取消规则。

既有War/Army/Person/City/State/Item/Contract/Brain/Task互通，assignment非新增永久页面。War显示真实因果/争议/授权/目标/成本/占领/和平；Brain保决策当时报告和贡献，后来真相不能倒填过去“已知”。Army底栏六动作移动/接敌/防守/阵形/补给/撤退，局部typed追击界限/射击纪律/撤退阈值/口粮保留/有限协同。无任意ammo999/满reload/HP0日志抹损；简中英德无框、无必要小字不加，受限why-not不泄敵私仓。

## 18. 贯穿例与验收边界

A公国收到B国截桥的片面迟报。冒险野心领袖可选威吓/动员，议会可因order偏好要求调查；实际使者发现Godterrain毁桥导致临时封路，则撤索赔。B若确实违已有通行约且拒履行，A经授权/真实首段供给可宣战。相同人格军官也因可达侧路、伤员/短粮不同而选不同方案；士兵有自己伤痛/家属/记忆，可救伤或迟疑，不全军同脑。

玩家把桥头soil刷deep ocean：桥段、地面器械/独立补给毁损；生命与随身军衣、武器、弹药和包内内容保留，按真实落水及身体危险处理。route/slot/水路/城市服务域重验，装备与装填状态不刷新；实际报告才改变战略/和平判断。

以下72项为**未来生产验收**，不是已通过游戏测试。有限参考模型只核验抽象授权、动员守恒、有限情报选方案、装填receipt、同瞬间射击与停火；不验证真3D碰撞、完整投票、物流经济或战术胜负。生产需完整World贯穿验收。

|编号|领域|未来生产验收|
|---|---|---|
|WD01|政治危机|关新战争同时阻新声明和有组织未宣战进攻，已有战争和即时有限自卫继续。|
|WD02|政治危机|关系低相邻本身不roll开战。|
|WD03|政治危机|同根转述唯一证据/怨恨，无重复归因。|
|WD04|政治危机|死亡摄政/角色修订使旧声明失效无半写。|
|WD05|政治危机|quorum与代表票真实，不刷签署者。|
|WD06|政治危机|误情报可坏战略，不能truth筛全知或赠现货。|
|WD07|政治危机|0出征威吓无免费Army或占领。|
|WD08|政治危机|虔诚不等根信仰/教会信任，无必战或伤害加成。|
|WD09|有限情报|God全图不进入AI的隐藏真相。|
|WD10|有限情报|观测与送达分开，迟报不覆盖较新直接观察。|
|WD11|有限情报|失败不泄隐藏量，重复未知探测受backoff。|
|WD12|有限情报|失踪敌有限可达域，不跟实际隐位置。|
|WD13|有限情报|门墙树干按真几何，装饰不挡视弹。|
|WD14|有限情报|未学语言/信号不瞬懂全命令。|
|WD15|有限情报|预计援军不计战场人数。|
|WD16|有限情报|未识别目标仅显式合法区域射击，不伪锁敌。|
|WD17|动员经济|现有成熟CivicActor/fit，不招普通动物或生成兵。|
|WD18|动员经济|ActorAvailabilityLedger军役农工护理互斥；ActivityTimeLedger同账别名，无双Army/双额度。|
|WD19|动员经济|保留真实工具接入/排班的粮衣照护物流，不只人数。|
|WD20|动员经济|接受招募不瞬到军营。|
|WD21|动员经济|实库存fit服役国配色，无免费制服。|
|WD22|动员经济|真实时间器材教官，不换Army满技能。|
|WD23|动员经济|多城拨款与欠薪唯一整数账/部分支付。|
|WD24|动员经济|人员库存资金任一冲突零半写。|
|WD25|补给航运|unfunded愿望不成订单quota，在途预约不算可用粮。|
|WD26|补给航运|封锁停实际路线/夺真货，不扣远库。|
|WD27|补给航运|登陆有限容器不赠广场。|
|WD28|补给航运|港楼毁保船籍在途人货。|
|WD29|补给航运|舱位mass/pose真实，船上不隔岸近战。|
|WD30|补给航运|不同食性/腐败按可食能量，不一百分比补给。|
|WD31|补给航运|未知断桥现场受阻后报告，不全知改路。|
|WD32|补给航运|军供优先明确法律后果，非无限粮。|
|WD33|战略编组|同人器械不同前线互斥，不双算Army人数。|
|WD34|战略编组|搜索耗尽保存cursor/安全等待，不默认攻。|
|WD35|战略编组|死亡取消Army撤条件后有界回退。|
|WD36|战略编组|Army存续，有效送达命令可续，无死人新令。|
|WD37|战略编组|临时编组无团营预算库存人口。|
|WD38|战略编组|平级分兵真实人员器材从source一次转。|
|WD39|战略编组|同六轴不同伤势报告责任会不同选择。|
|WD40|战略编组|军官和士兵非同脑，各自风险可救伤迟疑。|
|WD41|阵形运动|planned槽位不teleport，真楼层坡宽body核。|
|WD42|阵形运动|牛按真实体规，不缩人槽。|
|WD43|阵形运动|门桥column真token/公平批，无穿人。|
|WD44|阵形运动|cohesion影响解释/协同非基础伤害乘数。|
|WD45|阵形运动|改地撤slot/route/远水证，活人真实hazard。|
|WD46|阵形运动|高峰阻Actor进入飞越，救伤/勇气不豁免。|
|WD47|阵形运动|普通守位不暗堵旧通路，战封须法律后果。|
|WD48|阵形运动|方阵真实多向保护，无骑兵隐藏依赖。|
|WD49|战斗实物|空装填UNLOADED，粒子不伤。|
|WD50|战斗实物|stock转loaded再fire，只扣一次不双扣。|
|WD51|战斗实物|同Shot/hit receipt不可复制消费伤害。|
|WD52|战斗实物|绝对work/remainder/loaded保持，不免费重装。|
|WD53|战斗实物|完整swept防穿越，帧LOD近邻截断不漏人体。|
|WD54|战斗实物|头盔不护脚，坏护具不假保护。|
|WD55|战斗实物|同瞬间hit聚合后唯一death，Id先者无免费免反击。|
|WD56|战斗实物|species适配器具，IK/notify不能扣HP越程。|
|WD57|退却护理|士气改变选择，不增加HP基础伤害护甲。|
|WD58|退却护理|撤退真路体规/负重伤员，无瞬移。|
|WD59|退却护理|失踪有限搜，时间余粮归程约束。|
|WD60|退却护理|无路真实遮蔽/投降，不无限墙角寻路。|
|WD61|退却护理|俘虏真容量粮和身份，不新人口自动同化。|
|WD62|退却护理|签约不瞬移，实际运输交接。|
|WD63|退却护理|护理实物时间，无免费医院回血。|
|WD64|退却护理|死伤实物回收/遗产同源唯一custody。|
|WD65|城市和平|占领行政control非主权/私人货自动归属。|
|WD66|城市和平|广场逻辑capacity0非隐形占领anchor。|
|WD67|城市和平|失地共同体续，无海面capture index。|
|WD68|城市和平|围困真实流量粮债，非抽象每日死人。|
|WD69|城市和平|知停火停新shot，已发projectile继续。|
|WD70|城市和平|和平保伤欠薪装备返还实际后续。|
|WD71|城市和平|神意事件不假签约/自动lease或法物理豁免。|
|WD72|城市和平|镜头UI帧线程完成顺序不改World/RNG。|


## 19. ADR0018：装备、实际攻击与唯一生命出口

[combat_profiles_v1.json](../../data/content/combat_profiles_v1.json) 为9种武器、实际弹药、护具／盾、自然身体攻击、建筑受击和士气的数值权威。它收口范围、瞄准／接触／恢复／装填工期、抛射体初速／重力、威力／穿透、部位覆盖、实际穿戴耐久、训练、crew和真正科技 gate。所有参数是抽象游戏值，不提供真实枪炮制造或性能配方，亦不是战斗平衡完成。

原始开局只有真正采集／制作的木棍和石工具，不免费给装备或军人。传统剑／长矛／弓弩必须有 T015 实际知识、F1301—F1305 制造、现有装备适配与真实训练；火绳枪 T028/F1401、燧发枪 T029/F1402、早期炮 T030/F1403—F1404 都是后期实际研究→制造→采用→训练／crew 的关口。炮弹／装药 F1405 是真实商品。无开局枪炮、骑兵、马车、核武或现代军备。

每次攻击从本人 BeliefSnapshot 的已观察目标或显式合法区域出发，真实 aim 工作后在 profile 的接触／发射窗口核 reach／facing／真实 body sweep。瞄准误差由真实训练、握持、疲劳／睡眠和一次保存的 ShotAttempt RNG 产生；视觉弹道和动画不独立伤害。近战每循环最多3个真实接触，按实际接触分功率；抛射体真实重力扫掠到首接触，不靠帧邻域名单。装填从 stock 转一份 charge 到 loaded，开火扣 loaded 一次；空弹、无适配、未训练、工作／crew缺失不进入发射，既有加载和实际工期绝对保存。

耐久统一绝对 `durability_milli`，容量100000，显示points仅除1000。物理设备的实际 InstalledOperationVersion 单独覆盖对应未来参数：最低训练rawtick按基础1WU／实际工作tick、装填rawtick、单次wear_milli、misfire baseQ；没有实际版本才用 profile baseline，不能基线＋版本各执行一次。活动 reload 周期冻结版本／work target／已付进度，安装新工艺只影响下一周期，不凭空已装弹、回血、修复或补训练。

火器 misfire 为 `clamp(baseQ+min(2000,actualWetQ//5)+(10000-conditionQ)//5,0,6000)`；每个实际准入 ShotAttemptId 用 Runtime RNGv1 uniform10000 一次。失败也损失同一份已装真实 charge，生成 dud／损失而无 projectile／命中，之后要30实际付费清障worktick才可重新装填。重试、换靶、读档都不能重抽同一事件，预览无未来射击抽样。

护具只在真实部位／方向／覆盖交叉时减伤，已坏保护按实际 remaining/capacity，盾须真实举起并交叉迎击。先从同瞬间护具快照算保护与穿透，`transmittedPower=power×(10000−absorbedQ)//10000`，无穿透时 `finalSeverity=transmittedPower//5` 且实际kind=blunt；穿透时finalSeverity=transmittedPower按cut／pierce。只把finalSeverity传唯一 LifeApplyInjury，再由生命按部位／鲁棒转一次健康／伤口；挡住的残余不能再作为完整穿透伤出血。所有同瞬间接触从同快照算护具与wear，之后聚合一次；同瞬间真实已合法射出的弹不因射手死亡取消，之后尚未发生的发射失效。

建筑受击使用木／砌体／金属门真实 integrity／实际支承闭包；结构损坏不是减城市人口，活人和随身实物由生命与 Godterrain 统一出口承接。士气只以实际获知死伤／补给／逃生／命令事件更新恐惧和带迟滞的选择，不乘HP／基础威力。统一时钟12000tick／日、20tick／运动秒；军训、护理、送料、工作和生理共时间／能量账。有限参考模型及 CP01—CP06 是未来核验算术案例，根统一运行后才报告其有限结果，仍不代表生产仿真或 Steam 实机验收。
