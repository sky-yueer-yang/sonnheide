# 自然行为、真实移动与原创像素动作算法

2026-10-09。状态：**算法设计，尚无生产实现、测量或运行验收**。对应 [natural_action_v1.json](../../data/contracts/natural_action_v1.json)。前置规范为 [v0.8设计](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md)、[生命合同](GENETICS_ANIMALS_AND_UPLIFT.md)、[真实坡道](ALTITUDE_RAMPS_AND_THERMAL_EXPOSURE.md)及[控制租约](CIVIC_CONTROL_AND_SUCCESSION.md)。此文落实用户要求的自然、灵动、非机器人动作；用户要求的“平时不能一直站着”不解释成所有人永不休息、无目的地来回绕圈。

## 1. 真正要解决的五个边界

|非平凡缺口|必须同时成立的结果|
|---|---|
|任务正确但人看起来像插值机器人|移动有起步、有限加速度、制动、朝向准备、步态和互动预备；真实任务进度不由动画帧回调结算。|
|路径合法但人群堵死窄门|全局路径、局部避让、门口队列和离开口容量分别解决；局部算法无法解决时出现可解释等待/改路，而非穿人或抖动。|
|坡道与高分辨率身体不相容|权威表面支承/碰撞和纯表现脚部IK共用SurfaceRef/revision；脚部贴地不能给Actor获得原来不存在的道路。|
|自由动作破坏真实劳动与物品守恒|休息、社交、掉头、抢占、控制租约中断全部保存真实custody和劳动ledger；眨眼、抬头、衣物摆动只改变表现。|
|镜头、LOD、worker快慢决定行为|同SimTick、相同输入得到相同移动、冷饿、任务及接触；只降表现细节，不因远景让生命少吃饭、免碰撞或瞬间到货。|

选择的不是一个“万能AI算法”，而是具备明确因果边界的六层：感知与需求 → 目标选择 → 有限任务计划 → 权威路径/移动 → 实际交互事务 → 动作表现。每层错误有类型与原因，不把寻路失败伪装成“正在闲逛”。

## 2. 调查结论和来源边界

WorldBox官方更新记录说明其曾修复区域连接、不可达殖民、冻结仍走路等问题；Brain和Meta Control存在官方公开介绍，但没有公开足够材料证明其具体寻路、效用、步态公式。本设计不称复刻内部算法。[官方记录](https://www.superworldbox.com/changelog)、[Meta Control预告](https://steamcommunity.com/games/1206560/announcements/detail/669491979075191190)。

以下技术来源提供算法思想，并不作为新增运行依赖或美术授权：

|来源|核实的思想|本项目采纳边界|
|---|---|---|
|[Reynolds 1999，作者原文](https://www.red3d.com/cwr/steer/gdc99/)|目标、steering、locomotion分层；seek/arrive/避障等局部行为|采用分层与到达制动，不能以任意加权steering代替碰撞、峰禁区和预约。|
|[Botea等2004 HPA*，作者机构原文](https://webdocs.cs.ualberta.ca/~mmueller/ps/2004/hpastar.pdf)|cluster/入口抽象、局部路径缓存降低大图搜索量|自主设计带地形/权利/身体版本的portal层次；论文格图结果不是本项目真3D容量或最优性证明。|
|[van den Berg等ORCA，研究组原文](https://gamma.cs.unc.edu/ORCA/publications/ORCA.pdf)|以相对速度约束和低维优化做互惠避碰|只用于局部速度建议。实际加速度、窄道和异形身体破坏直接套用其假设，最终扫掠验证与队列不可省。|
|[Sharon等2015 CBS，作者原文](https://webdocs.cs.ualberta.ca/~nathanst/papers/sharon2015cbsjournal.pdf)|根据冲突添加时空约束再规划|参考局部冲突约束，不对全世界运行最优CBS；我们有界窗口不承诺完整或全局最优。|
|[Kevin Dill 2016，讲者公开GDC资料](https://media.gdcvault.com/gdc2016/Presentations/Dill_Kevin_Nuts_and_Bolts.pdf)|效用考虑项与行为规划结构|采用可解释评分、合法门控及稳定承诺，具体需求系数和生命周期全部为Sonnheide自主设计。|
|[Ubisoft La Forge官方Motion Matching研究](https://www.ubisoft.com/en-us/studio/laforge/news/6xXL85Q3bF2vEj76xmnmIu/introducing-learned-motion-matching)|以未来轨迹/姿态特征选择动画片段，并平滑过渡|初版选择小型原创动作库＋相位匹配，不引入ML训练或大型第三方动捕库；足够原创数据后可比较motion matching。|
|[Kovar等2002 Footskate Cleanup，作者实验室](https://graphics.cs.wisc.edu/Papers/2002/KSG02/)|通过脚部接触约束减少足滑|采用接触锁定和有界IK；不复制其库、不宣称通用IK能使非法坡变合法。|

论文/网页提供理论和观察，下面的参数、状态机、门控和守恒政策均为本项目设计选择。第三方代码今后如果实际复用，仍需单独固定官方版本/hash/许可/notices；研究引用不构成依赖锁定。

## 3. 权威状态与纯表现的严格划分

`ActorRef`保持生命身份唯一。权威移动组件保存毫米定点位置/速度/朝向、`SurfaceRef`或水/飞行域、身体profile、实际负重、路径走廊/目的地、任务与控制来源。`SurfaceRef={terrain_triangle|building_floor|bridge_deck, objectRef, localSurfaceId, revision}`；地形仍是单值height columns，真实建筑楼板/桥面是独立真实结构，不偷加天然悬空层。人类及各物种全龄固定成人身体、骨架与fit；年龄只通过生命合同的真实能力门控影响动作能力。

`ActionIntent`表示当前目的，如送货、逃逸、休息、交谈；`ActionEpisode`记录该目的的阶段及不可重复结算量。`MotionState`记录真实运动；`PresentationSnapshot`只读这些事实，驱动原创像素骨架、衣物和音效。

下面是权威事实：行走/跑步发生的真实距离、身体碰撞与支承、正在吃真实哪一批食物、已送料数量、实际休息时间、与谁发生真实社交接触。下面是表现：眨眼、呼吸幅度、头部看向、头发衣角摆动、手指微动、脚IK修正、步行片段选择、脚步声音。动画不能写位置、导航、库存、寿命、关系、文化、工作进度、控制租约或伤害。

任务需要接触时，权威执行器验证距离、身体可达接口、视线/遮挡、目标版本、实际工具/预约和当前权限；不得只验证中心点接近，也不得从“播放了拿取动作”推断拿到了物品。表现手臂触点在真实允许的工作站接口内，不通过IK伸臂穿墙。

## 4. 固定时间、确定性与随机流

继承当前冻结profile：20 SimTick/运动SimSecond，1200tick/游戏日，30日/月、12月/年。米/运动秒只进入运动；睡眠小时、饥饿、日常作息按日历→tick命名换算。不能把8日历小时睡眠直接当8运动小时；首样加速日历本身仍需实际路线/任务/睡眠联测校准，不宣称已真实。

运动与近碰撞每tick更新，需求与行为以保存的到期队列错相更新，事件可使其提前到期。队列key为 `(dueTick ASC, overdueWaitAge DESC, ActorId ASC)`，统一有界逻辑工作配额；worker只计算同一快照的提案，单写者按冻结顺序发布。不得以谁先算完、GPU是否可见、wall-clock截止选择路径或行为。预算耗尽保存明确积压；不得静默跳过危险、生理账或已到期不可分割交互。

行为随机使用 `WorldSeed/ActorId/behaviorDomain/decisionCounter`的保存counter，进行有限候选抽样，禁止每frame重抽目的地。视觉变体用只读hash或独立本地presentation随机流，不消耗模拟RNG。角色不共享一个“全体每五秒换动作”的计时器，错相来自保存的个人日程/阶段。Pause停止SimTick与真动作；镜头、Locale、暂停菜单、降低动态都不能变成免费休息或额外劳动。

## 5. 感知、目标评分与承诺

感知候选从真实局部索引查询：实际可见/可闻Actor、可达食物源、住处/工作点、社交伙伴、休息支承和危险。限制远候选数不能漏掉近距离致命碰撞；感知范围不等于整张地图全知。默认神视野仍不赠Actor全知。

先处理硬门控：活着/非受困/实际能力、Species运动方式、峰禁区、真实库存/工具/民事权限、目标来源、有效控制租约。当前能力从`PhenotypeCapacities`读取，不在AI另读allele重算。随后计算定点效用：

```
U(a) = clamp(needRelief(a) + dutyBenefit(a) + personalPreference(a)
             + actualSocialBenefit(a) + futureShelterBenefit(a)
             - routeTimeCost(a) - metabolicCost(a) - exposureRisk(a)
             - expectedQueueCost(a) - switchLoss(a), scoreRange)
```

需求曲线是有界分段线性/查表，禁止无上限exp或零除。人格六轴、真实偏好、疼痛信号、关系、技能只影响`personalPreference`/风险选择，不能替代门控或生成商品。“最高分”之外保留有限同类动作变体；从近最优、合法候选中按保存随机流选择，避免所有人都同刻冲去同一点。

普通换目标要求`U(new) >= U(current)+hysteresisMargin`且当前承诺达到最小持续量；真正危险/目标消失/无能量/许可失效可立即打断。当前任务剩余真实工作、带货、预约成本被计入`switchLoss`。Brain页展示候选门控、各评分贡献、正在承诺的目标和预计重考虑tick；不是任意改分就绕过库存、成年和道路。

## 6. 闲暇也有目的，休息不是错误

人和启智动物的非工作活动包括：回到熟悉安全点休息、散步/观看实际景观、拜访真实朋友、实际语言交谈、照看家属、去真实商铺看选购、学习/练习、等候已预约服务、调整已经穿着的衣服、真实饮食、露宿/睡眠。具有实际资源或关系后果的动作必须通过相应世界事务。不存在的椅子/床/饮食/书/器材不生成；无屋者可找可达地面坐卧，舒适度更差但可实际恢复。

“散步”选有限距离安全环路或两个实际景观点，计算完整回路/归程能量、曝冷和路权；保存起点/目的/截止与候选计数。找不到合适路线时改为站着看景/坐下休息，而不是把所有Actor永远排进路径队列。低收入者不会为了填满动画而持续随机消费，工作的搬运者不会每几米停止看景。

真实站立等待可以有轻微换重心、看向下一位、衣角调整和呼吸；真实坐卧需要足够空间/支承，睡眠要安全和连续实际tick。肩膀/头微动不打断“正在休息”，但离开支承/开始跑步会结束恢复积分。睡眠个体日程与其睡眠债/岗位/安全点有关，与Age of Light/Darkness无强制同步；不增加太阳昼夜系统。

普通动物按Species实际身体和食性选择觅食、休息、警戒、社群/照幼、求偶及逃敌；没有手的启智牛仍使用牛的骨架、物理运动与真实适配设备。动物启智切换高层目标集，不把位置/步态/能量重置成人类默认。蜜蜂每个实际Actor独立生命，LOD蜂群代理只显示现有成员；装饰粒子不参加行为。花草摆设不成为免费食物。

## 7. 任务计划、抢占和连续真实劳动

采用小型有界任务图/HTN式分解，节点是有明确precondition/effect的动作：`LocateSource → ReserveQuantity → Travel → ApproachInterface → AcquireActualBatch → TravelLoaded → QueueForReceiver → DeliverActualQuantity`。不是每帧运行任意长全局GOAP。具体子系统定义食品、果木、教学、护理等模板；运输、站位和交互执行器共用。

每个动作经历`PROPOSED → RESERVED → TRAVELLING → APPROACHING → ACTING → COMPLETED`，可进入`WAITING/PAUSED/BLOCKED/CANCELLED/FAILED`。`ActionCommitLedger`保存目标revision、预约来源、完成劳动量、已消费批次、已经产生的world effect事件ID、lastIntegratedTick与整数余数。一个目标可以在多个动作episode完成，但同一实际effect不能重复。

计划中断政策：

|情况|处理|
|---|---|
|即时真实危险/地形换type|当前tick之前合法劳动先结算；中断旧route/contact；未被本笔wipe的存续实物保持custody，被wipe物先唯一loss核销后撤握持，危险支承进入生命hazard。|
|短期休息/排队|保留明确剩余工作和可保持的有界预约；停止产出积分，恢复时重新核版本。|
|取消任务/释放元控制|消耗与完成效果保留；未取货预约释放，已拿货仍由真实Actor保管，按法律规则退送或保管，不瞬移回库。|
|同一人受两个任务请求|一个主动作episode；兼容纯表现层可以叠加，两个实物操作不能同时各算完整劳动。|
|工作点/家损毁|接`LostWork/LostHome/RelocationPlan`，实际找可达替代；不因此删除居民或直接换户籍。|

建筑材料送到前侧实际接货区才记交付；送齐后本体按工期自动建造。人物可以离开去休息/下一任务；没有人物原地挥锤砌墙动画，也不把铁丝网围起/拆掉回调记为完成。

## 8. 版本化真3D全局寻路

每个合法运动域生成身体profile对应的可走面和portal。输入包括SurfaceRef及revision、身体净空/固定尺寸、实际负重、最大坡/台阶/转弯、峰裁区、水深/运动能力、桥门/路权。普通人不可自动爬hill的真实直壁；真实坡道提供连续面后才可跨越。飞行搜索也绕峰棱柱，水生搜索跟当前WetRegion/连通/实际水深，不偷用陆上路径。

分两层：cluster间portal A*，cluster内实际triangle/doorway corridor A*；稳定key`(fCost,hCost,nodeId)`，缓存绑World/profile/surface/right/destination versions。高层近似路仅是候选，局部不通过就不能承诺可达。路径平滑只在已验证通行走廊内shortcut，不能跨竖壁、峰、失效桥、台阶、私有门或两湿区间的干脊。不同楼板高度的Actor不能在同一平面避让网中相互堵路。

路线cost将几何长度、坡/负载成本、当前可知危险和预计队列分开。热风险是`ExposureEstimate`，不是永久阻塞墙；仍允许具备真实能力的救助或玩家明确危险命令，命令不免伤害。资源/门禁/支承变了就撤相应证书，新站位仍合法者在该真实位置有界制动再重规划。正在受破坏支承承托者走真实危险流程，不把他传到最后旧路径节点。

起点/目标必须真实存在。近似目标替代应预览“到这处可达入口/岸边等候”，不能将无路目标默默钳制到另一城市。全图不可达要有reasoncode与受阻位置/版本；等待worker不是已找到道路。

## 9. 到达、起跑、制动与转身

每tick从合法走廊取look-ahead目标，计算本项目自主控制：

```
v_cap = phenotypeModeCap * injuryFactor * fatigueFactor * loadFactor
v_arrive = min(v_cap, sqrt_floor(2 * allowedBrake * remainingSafeDistance))
v_turn = curvatureCap(turnRadius, agility, currentLoad)
v_preferred = corridorTangent * min(v_arrive, v_turn, surfaceCap)
v_next is inside reachableAccelerationSet(v_current, a_cap, tickDuration)
```

根朝向以定点角度、有限转角更新；躯干朝向与视线可小范围不同，但负载、坡道和窄门要求身体真正转正。大转弯可减速、停下、原地转身再走，不能以插值瞬转180°。起步需要实际推动/加速；跑步有加速和减速，不靠“把clip播放两倍”伪造更远运动。

首样人类变体`slow_walk/walk/brisk_walk/run/short_sprint`，上限来自已有`ground_speed`species envelope，再乘冻结的分档比例；最高档不能超过该能力上限。跑步准入由实际动机、体力/能量、负重、地面/净空和安全决定。普通任务偏好walk/brisk，紧急逃逸/救助可run；sprint有真实疲劳/能量/恢复窗口，跑停切换带hysteresis。比兔更快的动画片段不让牛突破自己的species速度。首版不添加自由攀岩、跳上hill、高峰跳越、魔法冲刺或骑乘。

sample参数只是待测：人类walk比例0.45、brisk0.62、run0.85、sprint1.0；正常最大加速度2000mm/运动秒²、制动3000，必须结合species/agility/伤势和冻结profile限制。若坡面/低能量要求更低则取更低。动画时间和移动成本按实际位移计算，停止在门口不能继续循环跑步并扣出虚假劳动。

## 10. 局部避让：ORCA思想不是最终碰撞证明

开放可走面按当前局部面/实体高度相容邻域产生ORCA式速度约束，加入静态边界/走廊/门token，以及有限加速度的可达速度集。相邻者按ActorId排序；双方基于相同tick快照求提案，不顺序挪前一人后再算后一人。

标准ORCA常假定独立圆盘和即时速度选择；四足长体、抱货的人、转弯车和陡坡不能直接继承该保证。第一版可使用冻结定点候选集最小化“偏离preferred＋加速/转向代价＋预测冲突”，ORCA半平面用来裁减建议。完全满足约束则选择最小cost/稳定candidateId；无解进入减速/等待/窄道协调。未来经测试的定点LP可以替换候选搜索，但不能删除权威碰撞最后防线。浮点库结果未经确定性验证不写World。

每个提案需经过真实3D身体包络及实际载荷的`SweptContactValidate`；穿门看净空/转向，穿Actor看双方同tick扫掠时空，而不只看终点。全局收集潜在接触对，以稳定event/pair key裁短移动到合法接触前，做有界共同重求；仍冲突的局部组件按确定规则保持/制动或记录真实碰撞危险，不做随机挤开、多人位置交换或传送。紧急物理约束可使运动被接触打断，它不是“无成本瞬间刹车”能力；真实事故/伤势走生命事务。

远邻候选上限可取16/Actor，但**即时安全扫掠邻居不能截断为16**。空间hash/BVH查询所有与真实swept body相交的近对；异常密度超过冻结工作准入时先拒绝新拥挤命令/暂停该组件运动更新的推进（同tick旧位置，不继续别人的穿越），发诊断/等待，不忽略第17人。冻结运动必须扩展到所有可能与该组件扫掠相交的参与者并在同一运动屏障发布，不能仅停弱者而继续别人的穿越。已验证原位合法的等待生命仍按同SimTick结算生理，真实支承/hazard不能暂停；若连完整安全闭包都无法计算，本tick不得部分发布而进入明确统一积压。不能以camera culling减少碰撞。

## 11. 窄门、单道、交叉口与死锁

门/窄桥/坡口是`BottleneckRef`，由真实geometry/width/deck/rights生成，有实际入口、通行方向、出口清空区与旁候等待点。通行token由`requestTick ASC → ageCredit DESC → stableActorId ASC`公平队列发出，真实救助等有限高优先级不能永久饿死普通者；冻结priority class累计服务预算，超预算回普通队列。

获得token只是保留时空通行机会，不是拥有路，也不驱赶已经在里面的人。出口容量和既有occupant先验；无合法等待点则上游停止进队/改路，不把100人叠在门前。单道方向分批开放，另一边waitAge到阈值后翻向，当前批清空后才放对向。站队位置用真实净空和队长信息推进，各自起步有错相，不全群同tick挤到收货口。实际待办多人送货保持各自真实批次。

监测`lastProgressTick、distanceRemaining、blockingActorRefs`，持续无进展进入局部wait-for graph。发现cycle时，对有合法侧退空间者按公平序请求真实退让路径；只能在合法已预览的身体能力/路权内移动。必要时仅对这个小组件在有限时空窗口做预约搜索，保存前沿；超限/不可解标`DEADLOCK_NO_SAFE_YIELD`并停止/请求玩家改路，不能永远左右摆头或将弱者穿过强者。不要声称有界CBS式方案保证所有动态世界必有解。

formation/groupmove只为现有成员生成稳定目的slot与到达窗口，slot先checkbody/support；不能生成士兵、让最后排直接滑到队头、让整组速度统一成最快者或把同门叫作多个平行容量。不同species自主步态与速度保留，组leader候补只从实际成员选，不刷Actor。

## 12. 原创高分辨率像素步态与动作过渡

正式美术阶段前置制作少量高质量代表rig：人类、兔/牛/狼代表四足、鸡/鹰、鱼和蜂，再扩到19species定义。所有clip、骨架、contact markers、carry grip、chair/bed/interface anchors、衣层fit、LOD均原创可复建；没有MakeHuman身体或第三方动作库的默认生产依赖。

每个BodyProfile具有限定动作词汇：人类起步/各速度步行与跑步/停步/左转右转/原地转身/抱持/取放/食饮/坐起/躺卧/真实劳动；四足walk/trot/gallop/站卧/觅食警戒，兔hop，鸟walk/起飞/稳定飞行/降落，鱼游泳，蜂飞行。缺适配clip不得回退到human双足，也不得假装具有不存在的手。跳跃式兔步态只在已经合法的地面运动包络内表现，不允许跳越绝对峰或导航直壁。

基础采用in-place原创clip的相位匹配blend graph：选片基于实际速度/加速度/turn/slope/load/injury；步幅由实际速度匹配，脚触点先建立后锁定。相位随真实水平行程和冻结的strideLength推进，静止不继续走路循环。起/停/turn片段接相容左右脚phase；小幅transition采用有界姿态差衰减/短blend，避免全骨瞬间换帧。结构动作如坐下/起立按真实ActionEpisode阶段推进，动画播放停止不多算恢复。

可加入有限motion-matching检索：原创clip未来trajectory/velocity/facing/pose/footcontact特征做版本化确定检索，仅影响片段选择。它不是初版硬依赖，不引入神经网络或无限动捕数据。没有足够原创动作覆盖时先增加关键起停转身/持物clip和接触质量，而不是堆更多同质walk片段。

像素风是几何/材质清晰而高分辨率，不要求骨骼每帧硬跳离散角造成机器人。关节可以连续平滑，cuboid表面保持硬朗；不用把渲染低分辨率滤镜当像素资产。衣服的骨架fit跟本species固定成人body共用，真实穿戴层、库存/耐久与显示绑定，不在动画里生成衣服。

## 13. 脚部接触、坡面IK与持物

对将落地的脚从未来真实落脚SurfaceRef中取点/法线/净空，并记录surface revision。支撑相阶段把表现脚锁在该点；摆腿阶段沿原创曲线向下个目标移动。人类two-bone IK用固定肢段长度和knee pole；目标过远时限幅，不能拉伸身体或改腿长。骨盆偏移、脚踝法线角度、躯干倾斜都在BodyProfile表现包络内。四足多接触相位按本species定义，不能假定所有动物两脚交替。

真实root支承高度由权威地面/controller提供，IK不能把root挪上坡顶。坡越陡、载荷越大时步幅缩短/速度降低/躯干倾斜/脚抬高由动作适配，只在已通过能力门控的坡上生效。左右脚不可跨分别不同高台造成假支承；落点越出合法body走廊则取消这个IK目标并选择相容fallback，不把合法性反馈成新路径。

地理换type/楼板摧毁/桥消失在**同一发布屏障**使旧foot/contact/grip anchors失效，立即停止旧动作参考。不能在异步旧mesh上站着/取货或把手继续伸向已经删除的箱子。新表现snapshot可较晚重建，但必须显示非互动fallback/真实fall/水中/受困姿态，并禁止旧拾取目标。fall/swim/flight核心与体湿/伤害由权威生命物理负责，clip只表现。

持物位置用实际ItemBatch/容器及Actor custody生成。handoff真实提交前看见抓取预备，提交后使用提交事件驱动在相容短窗口附着；表现迟到不会形成两份物品或延迟第二次扣账。破坏删除实物可出现短消散/松手动画，但没有可拾的第二份货。不同species有真实carry接口/工具，没有手时不能用人类手grip悬浮货箱；需要实际适配器或不同合法任务。

## 14. 地形抹除与城市损毁的同tick接口

用户新增规则是改`GeographyType`时删除触及的非生命对象；它比普通换生态主题更强。动作消费者按发布事件区分`TerrainGeometryChanged/GeographyWipe/BuildingDestroyed/TreeDestroyed/ContainerDestroyed/CityStatusChanged`，不擅自把主题色变化当同一wipe。

事件处理顺序：旧状态合法积分至mutationTick → 核销/转移实物由上游事务完成 → 撤route/access/foot/grip/receiver refs → 计算当前生命真实支承/水/净空/hazard → 保存恢复任务/重新规划到期。Actor不能被动作层直接delete。车辆/箱子/床若被wipe删除，里面的活人保持ActorId、真实旧位置和生命状态，解除不存在容器关系；命中的穿戴衣物、携带工具与货也属于非生命实物，由上游wipe loss ledger核销；同屏障撤衣物保温/equipment能力与grip anchors，动作层不能免费吐出被核销货物。体内孕体及真正活胚胎/卵的GestationRef不直接erase，商品食用蛋不冒充生命；窝/保育器被删时保护孕体身份并按真实支承/冷热处理危险。

同位置仍有合法支承者进入`GROUNDED_PENDING_REPLAN`；失去支承者`FALLING/IN_WATER/STRANDED/TRAPPED_IN_DAMAGED_CONTAINER`；土体抬高包埋或峰包络覆盖者进入明确`InvalidOccupancyHazard(TERRAIN_ENTRAPPED/PEAK_ENCROACHED)`。这是神力改变世界造成的不合法受困状态，**不是合法站在峰内、导航节点或允许主动入峰**。不把Actor瞬移到新地面/岸边，不让IK重新把双脚钉在新土面；真实可行逃逸/外界救援/再改地才可恢复正常。损伤按有cause/dueTick的有界生命规则，不因重建nav自动死亡、删除、复活。

城市全楼毁坏、全失地、行政转让本身不改变Actor的当前物理位置。`HomeCityRef/citizenship/PhysicalJurisdictionQuery`相互独立；收到`LostHome/LostWork/RelocationPlan`后从真实当前点走路/找照护/避难/迁居。当前脚下国家查询仅供操作方案使用，不让一脚跨界自动更国籍，正式加入/迁居由typed civic事务结算。

## 15. 元控制、附身和自然动作

`IssueMetaOrder`给合法租约目标设置typed高层意图：move/deliver/learn/assist等；附身输入转换为有界期望方向/速度/动作请求，仍经过同一个移动执行器。玩家不能通过每frame写ActorPos、播放run clip、改Brain排名绕过支承、碰撞、峰、能量、私产权限或社会资格。

租约有效性在命令和后续关键动作接触处检查。过期/失格/换Session同时撤外部来源，当前motion不瞬间回原位置、不免费停止所有惯性、不重置能量；恢复自主规划并在正常控制范围内制动/接续。grouporder新成员不自动加入租约；scope变化使旧目标版本失效。国家/城市转让指令只是civic变更，不自动取得居民动作租约或拿他们货物。

`Stop`含义是停止后续意图并在真实可行范围制动，当前已取得货还在手里。`CancelTask`与`EmergencyEvacuate`有独立typed语义；任意元控制不得把已耗食物退回仓库、放弃货就免费传库或结束任务重复领工资。无路目标可出具替代入口的明确方案，批准只约束该方案，不绕旧route版本。

## 16. 保存、恢复与缓存

必须保存的动作权威状态：Actor motion fixedpoint pos/vel/yaw/support/bodyProfile；mode/kinematic状态/负重与体伤版本；主目标/ActionEpisode阶段、TaskRef、真实工作量/lastIntegratedTick/余数/effect receipts；reservation/custody引用；routeRequestId/current segment/access dependency revisions（派生path可重建）、待决前沿与固定工作预算/fair cursor；`lastProgressTick/waitAge`、bottleneck请求/token/exit容量；生理rest/sleep episode及其生效位置；行为dueTick/目标承诺/候选生成counter；行为类别开关；归属改变/危险/搬迁事件已处理eventId；group slots；控制命令序列与来源。

生命合同中的绝对双能源、伤势、体湿、疲劳/冷热/睡眠债保存，动作层不另造一份百分比库存。租约保历史但读档新Session废旧控制权，真实未完成任务仍在；自主planner恢复不是重抽genes、衣服、伙伴、货物或归属。退出/读档必须结束旧session输入缓存。

可以重建：nav mesh/走廊候选缓存、foot IK目标、clipblend/音效/视觉微动。为了画面连续可保存presentation checkpoint(clipid/phase/contact显示marker)，它不得影响World hash/路径/能量；旧定义/Surface版本不合时丢弃并从当前权威motion重建。纯表现字段不成为必需游戏事实，删除该checkpoint两次加载得到相同World结果。

## 17. 性能与可验证预算

首个贯穿场景用512真实Actor（含人/兔/牛/鱼/蜂/一组启智动物），真实货物/果木/食物/冷热/两城/窄门/坡道，之后扩1000与2000，**这只是测试阶梯，不是容量承诺**。speed1单tick50ms，候选目标：权威移动/碰撞≤8ms、行为/计划≤4ms、path工作≤4ms；其余预算留生命、经济、地形、单写者、渲染与操作系统。数值必须在声明CPU/GPU/地图/负载/倍率条件下测p50/p95/p99，不引用论文千人数据当本项目实测。

逻辑准入样例：单次个人行为候选≤24，常规目标计划节点≤32；每tick全体path扩展总8192节点、单请求单份工作256节点；局部死锁组件≤12真实Actor/200tick时空窗口/2048搜索节点；这些值冻结且可迁移，不让线程快者多拿预算。安全sweep候选不按感知cap截断；超密组件进入明确预算处理。需求/行为到期错相，不是每Frame每Actor全图搜索。

表现LOD按屏幕影响选择：近景≤256完整骨架/脚IK目标、远景instance+低频姿态或精简rig；该256也是待测渲染预算。skinning可GPU，近景IK可CPU，仅生成pose；GPU回读/遮挡查询不写World。所有Actor无论可见与否仍由同权威运动/生命/任务时钟推进；不可把远人物改成到点瞬间出现的经济代理。纯休息/等待也计真实支承、风险与能量，可事件积分但不能按镜头跳结算。

## 18. 实施顺序与验收证据

先在阶段1–2的地表/坡道/占用fixture建立SurfaceRef、真实碰撞支承和access invalidation，不扩经济。阶段3正式原创美术同步交付动作词汇、骨架、contact/fit与代表clip，不能等全经济完成才发现门小于牛身体。阶段4实现需求/生命/自然行为/真正步态，阶段5送料交接与公共食衣，之后接城市迁居和MetaControl；每批完成再统一构建/完整CTest/事务空间存档sanitizer，不反复小改编译。

验收同时看事实和画面：World hash/ledger/任务记录证明守恒与确定性；可重复录屏近景展示起步、急停、转身、坡脚、持物、排队、工作与休闲的相位自然性。只看录屏不能证明正确，只看headless正确不能证明自然。未来契约的42个失败场景须能独立定位，当前没有声称这些已经通过。

## 19. 权威移动伪码

```text
step(tick, root):
  integrate_due_physiology_and_actual_work_to(tick)   # fixed order, unique ledger
  apply_committed_topology_hazards_and_lease_changes(root.events)
  due = stable_fair_due_queue(root.behavior_due, frozen_work_budget)
  for actor in due:
    sensed = bounded_actual_candidates(actor, root)
    goal = select_legal_goal_with_hysteresis_and_saved_counter(actor, sensed)
    prepare_plan_or_resume_saved_episode(actor, goal)
  advance_path_frontiers_by_logical_node_budget(root) # worker finish time irrelevant
  for actor in all_physical_movers_sorted_by_id:
    if invalid_occupancy_hazard: prepare_hazard_or_real_rescue_only()
    else: revalidate_next_segment_and_make_acceleration_reachable_proposal()
  proposals = solve_local_avoidance_and_bottleneck_tokens_same_snapshot()
  motions = resolve_all_actual_swept_contact_pairs_deterministically(proposals)
  interaction = validate_reached_interface_and_prepare_real_effects(motions)
  validate_single_writer_write_set(motions, interaction, ledgers, hazards)
  publish_once()                                    # no partial delivery or crossing
  emit_readonly_presentation_snapshot()              # no authority on animation notify
```

高速倍率只增加执行的同样tick数量。若单tick必需工作无法完成，不推进部分Actor而丢掉其余人；冻结或明确一致的有界模拟积压。身体、路权、库存、tasks全部新root可见后才发动作事件，旧异步动画/路径worker带版本拒收。
