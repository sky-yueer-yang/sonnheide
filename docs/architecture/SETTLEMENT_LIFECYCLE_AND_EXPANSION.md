# 城市连续性、扩张、新城与地形灾损算法

状态：2026-10-09设计补全；未实现生产程序。本文件和[机器合同](../../data/contracts/settlement_lifecycle_v1.json)专门接续[v0.8](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md)城市缺口；算法修订号`ADR0015-2026-10-09`，design_version仍为0.8。具体新规则覆盖旧稿中尚不充分的城市灾损/创城描述。原始v0.6不改。身体运动见后续动作合同；地理换类的完整对象清除、神意成员/城市转国见同批神意合同。这里不把调查闭源行为等同拥有WorldBox源算法。

## 1. 调查得到的事实与无法得到的东西

WorldBox官方历史更新记录曾明确：扩地依赖建筑区和治理条件；断开的城域会弃置；水山隔离参与新城选址，殖民点倾向离旧边界更远；弃城衰减需跨存档继续。记录也出现过沙土笔刷保楼的规则，和我们此次换类清除要求不同。[官方变更记录](https://www.superworldbox.com/changelog)

公开材料不能证明现版内部权重、城市最大面积公式、每次迁移概率或所有废城计时。以下均为Sonnheide自己的确定性方案，数字是待联测的工程样例，不宣称还原WorldBox。一般非负权路径搜索可参考[Boost官方Dijkstra文档](https://www.boost.org/doc/libs/latest/libs/graph/doc/html/graph/algorithms/shortest_paths/dijkstra_shortest_paths.html)；区域门点/局部缓存的研究依据可参考[HPA*原论文](https://webdocs.cs.ualberta.ca/~mmueller/ps/2004/hpastar.pdf)。这是算法依据，不是本轮新添或锁定Boost依赖，也不继承论文设备性能。

## 2. 真正必须拆开的六件事

|事实|权威记录|不能拿它替代什么|
|---|---|---|
|土地是否当前存在、是否干、是否可支承|SurfaceRevision、精确干面、峰禁区|城市颜色、地理主题|
|谁对该空间有主权/私产|TerritorialTitle、LandTitle|城市服务、人在何处、楼里有什么|
|城市是否有连续的共同体|SettlementId、CityMembership|当前站在城里的人数、楼数|
|城市服务还能覆盖哪里|AdministrativeAssignment、ServiceCertificate|任意半径圈、全国主权|
|真实居民能否居住/生产|住宅/公共广场/食物/道路/衣物真实能力|有Culture、画出来的边界|
|政治共同体是否存续|StateId、实际臣籍/章程共同体|国王存活、旧旗帜、船的owner|

最核心不变量是：**同一批地理/清楼/死亡/迁居变更，以最终几何、最终存活集合和最终正式成员集合一起结算，不在循环中先删一座楼就判断国家死亡。**

## 3. 身份与地图域

`SettlementId`从真实聚居登记开始终身稳定；`City`是同一Settlement取得正式城市章程后的资格视图，`CityRef`携同一stableId及generation，不制造第二套居民或人口计数。前文化Settlement可以成立State；普通正式City需真实居民、合法干地锚点、真实可用CitySquare/公共服务、官方文化及合法章程。

建筑仍在但损坏时，容量取结构/入口/设施/真实服务可用性，不按楼数或HP百分比线性推断；全城楼还在但均不能工作同样DAMAGED/STRANDED。未被完全wipe的存货可以是真实受困/危险可抢救物，须任务和道路取出；不能看到废墟就给修复材料退款。

每城市恰好一个持久`CitySquareAccountRef`，这是逻辑仓储/托管登记，不是无限仓库。物理广场被删后该记录状态`FACILITY_LOST`，实际容量为0；内部物品按此次清除模式处理，旧预约失效，不能让account保存物理物品的另一份可用副本。重建新物理广场绑定原account，既有已损失物资不复原；历史可出现多个先后BuildingId，但同时最多一个有效广场设施。财政货币账、账债、合同及所有权来源另存，不因仓库楼删除消失。

四个面积必须分别显示及比较：

- `current_dry_sovereign_area_mm2`：存续国家有效TerritorialTitle与当前精确干面交集的水平投影面积；坡面不因斜面面积更大增税。
- `current_assigned_city_land_mm2`：当前有效城市行政assignment覆盖的干面；峰政治claim可以存在，但不能冒充可服务/可建地。
- `currently_served_land_mm2`：存在适用真实服务通路/路权/维护能力的干面。ServiceCertificate采用用途profile，不能让会飞的蜂或游泳者替全市证明普通步行接入。
- `built_and_committed_footprint_mm2`：现存建筑/设施/已批准项目完整占地的并集面积；不把存储逻辑account算作建筑。

格域面积采用固定点精确三角面裁切和水平投影，边界以tile span加必要polygon覆盖表示；不将2m格统一误算成4m²干地。一个含水坡格可以只有部分可行政干域。统计使用同一revision。

`HomeCityMembership`、`StateCitizenship`、`legal_domicile`、`CurrentPhysicalJurisdiction`、工作/军籍/雇主分别存；旅行者经过不会自动入籍。PhysicalJurisdictionQuery只能从当前合法WorldPose或实际容器出口锚点取域，船上人物用实际承运体位置及明示水域政策，不按旧出生点或摄像头猜。非法峰占据有hazard，不构成合法干地驻点或创城锚点。

## 4. 法律主权与改地后地图颜色

`TerritorialTitle`是State的法律空间权利，`effective_dry_sovereignty_index = continuing_state_titles ∩ current_dry_domain`。普通改地不将title转给邻国，不转让私产/国籍/继承/债。土地淹没使有效陆地主权面积立即缩小；仍存续且未依法清算的State保留该位置title，后来再次出陆可恢复陆地主权索引。已归档State的title只在历史中，恢复陆地不能复活它。

城市行政assignment采用较严格规则：被淹或变成无法维护的割裂域时，撤销有效服务并将不再合格的assignment转历史/`UNASSIGNED_ADMIN`。重新涂成soil不会自动恢复行政域、广场、CityId活动资格、人口或货物，必须显式Reestablish/Expand。**行政清除不等于State主权让渡。**峰仍可有国家政治title，合法城市的未服务山区可作为行政附属背景显示；峰不得充当anchor/住房/人口容量/导航。

没有有效国家title但有存续前文化Settlement的合法集体占用区域，显示Settlement而非凭空国家；地理来源与产权证明保留，即使其有效地表已不同。自然变化、城市服务剪分、主权神意转移有各自事件，不伪造和约。

## 5. 两条状态轴，不能只用一个destroyed布尔值

`physical_status`与`community_status`正交：

|物理状态|精确条件|直接后果|
|---|---|---|
|OPERABLE|有效anchor、完整最低公共服务、当前服务可达、有实际居民共同体|可依实际需求提起正常建设/扩城|
|DAMAGED|仍有合法可支承anchor和可维护干域，但广场/住房/设施不足|保身份/成员/账债；真实重建优先，不冒充新城|
|STRANDED|仍有部分合法干域，但原anchor/主服务链不可用，居民或设施分割|停旧接入/预约；实际重选anchor/救援/迁居，不自动分城|
|LANDLESS|无任何合格可维护干地锚点；旧域全水/全峰/失权|有效城市服务域为0；现有成员形成displaced community，不能在海中建设|

|共同体状态|精确条件|行为|
|---|---|---|
|CONTINUING|至少一个活的合法CityMember，或一个有效在途Reestablish/入城party契约有活参与者|身份连续，可显示灾损/失地但不假扩张|
|DEPOPULATED|上述集合为空，首次记录depopulation_start_tick|旧楼不删除，停止新扩张；等待真实接收/入住恢复|
|WINDING_DOWN|明确解散/所有成员合法退出，或无人共同体到期|不可发新政治/行政命令，进入真实资产和预约清算|
|ARCHIVED|有效成员/在途party为空，行政assignment=0，职位/审批撤销、清算义务已迁交可读档案|stableId永久只读，名称可复用但ID不复用|

在途party不能以空计划无限续命：需保存真实活参与者、同意、资源预约、实际路线、到达deadline、最近实质行进/休整依据；未出发/失活/撤销/过期即不计continuation。进度取沿既定承诺路线的单调净完成段与必要有界休整，不以每tick位移、绕圈、往返或反复换目的地续命；绝对deadline不因进展滚动延长，UI查看/载入不刷新。居民在别国但仍合法保HomeCity，城市可以保持“失地共同体”身份；它不获得建设/征税/领土扩张能力。真正选择永久离开或转居后，成员集合变空便走归档流程；不能拿30天到期杀人或强清国籍。

外部活Gestation/蛋不计现有人口，也不单独让死国/死城运转。只有已有合法活steward、真实照料和保存的合法continuation dependency候选可维持相关待决登记；没有此条件，最后成员死/退出仍归档。未来出生保真实亲代/启智受孕来源，出生时只解析仍存续的合法居城/臣籍目标，否则真实无国籍/未登记并安排现有实际照料；旧城市/国家origin留历史，不向ARCHIVED写live membership、复活ID或自动刷养父母。

失地超过样例30游戏日，显示`RELOCATION_REQUIRED`并撤销一切尚依赖旧物理广场的行政执行业务；这个计时是迁居提醒/权限降级，不是无限保留可操作政府的grace。未能复建的连续共同体仍保法律身份、财债、历史及自愿加入其他城市的权利。玩家可显式安排合法复建或解散，不会涂回自动治好。

同一个City发生临时撤离/迁居仍保CityId；新独立的居民共同体要新ID。正式解散后的旧成员不能用同名复建复活旧ID。

## 6. 事件闭包与地理换类删除

`changed_canonical_geography_cells`只含权威geography_kind真正变更的格域；同值重复涂不触发删除，油画/主题换皮不伪装geography换类。地形height/slope几何变化而kind未变也要按真实支承/危险结算，但不凭空触发“换类删物”事件。具体分类与wipe合同由同批God控制合同定义。

换类触发的对象集通过SpatialIndex和容器/支承反向索引求闭包：命中楼完整占地/必要支承即删**整座楼**，不裁半栋；依赖设施、树Plant、路段、农田、物品、穿戴/携带物、亡者遗体等非生命按完整协议删除/核销。命中活Actor保其身体/基因/知识/记忆/年龄，活孕体及外部Gestation按生命来源保护；窝/房/船/衣物被删不保护其容器。动物不是Plant；Tree是机制Plant仍删。

不得把免直接erase解读成免死：落水、失支承、削坡、压埋和非法峰占据按真实hazard推进。峰覆到Actor保真实pose并标`InvalidOccupancyHazard`，不抬到峰顶，不算成功走进峰，不规划穿峰逃跑；真实削地/可行救援或生理损害处理。危险后果和当笔对象删除在提交预览清楚呈现，直接生命清除另走死亡事件。

**普通“清理建筑”不同**：在有真实安全出口/仓储/路线情况下先疏散保管人货，然后删除楼；无法保证则拒绝/给具体必须先搬运项。玩家明确选择强制灾损/换类wipe时，命中的非生命真实物损一次记账。建设损毁造成结构废墟与可抢救存货的可能性取决于损毁事件，不给完全wipe自动掉落木石退款。

## 7. NormalizeSettlement：唯一城市结算入口

输入不是“刚删的是第几座楼”，而是候选World最终状态及affected closure。

```text
PrepareMutation(command)
  freeze before revisions and typed target sets
  compute changed geometry + whole object/container/support closure
  compute tentative losses, hazards, movement/access and membership effects
  compute all affected settlements/states (including remote dependencies)
  for each settlement by SettlementId:
      collect final alive members and valid live inbound parties
      clip assignments to final dry/valid-title domain
      invalidate affected service certificates and classify components
      preserve history; zero every lost facility actual capacity
      choose/reject actual anchor relocation; compute physical status
      compute community status and absolute due ticks
      stage memberships/task/housing/tax/claims/rank/office consequences
  for each state by StateId:
      compute final real constituency + current effective titles/direct cities
      classify governance/displacement/wind-down; resolve vacancies once
  preview candidate facts; no old resource or live Actor restoration
Validate all read revisions and full dependency closure
Commit once -> one event batch -> one consistent query snapshot
```

同一个Actor/ItemBatch/TreeRef只处理一次，损失key为`(commandId, stableId, resourceSubBatchId)`；组件重算不会再扣一次。先计算final alive/成员集合，再继任、选举、城市/国家状态，避免楼遍历和死亡顺序影响结果。`NormalizeSettlement`是权威候选阶段纯函数，不能自行更新人口、生成新的AI任务Actor或重放过去的损失。

## 8. 连通性、割裂和anchor选择

城市行政干域连通性是精确共享正长度陆边的4连通；点接触不算。服务连通性是实际nav/道路/路权的图，和陆域连通不同。跨江桥/合法航线能证明通信或迁居，但不能让两个陆域自动变成一个连续城域；首版默认独立岛域需另建Settlement/City。既有明确批准的离岸行政district可作为独立district记录，需自己的公共服务/人口/实际通信证书，且在预览中列出，不偷偷开放所有enclave。

主成分先保有效原anchor所在成分；若原anchor损失，找现有有效公共节点及**活成员实际到场可达**候选。评分为：保留真实可用住房人数降序、现有公共广场功能降序、成员真实路径总cost升序、新anchor占地重建投入升序、候选固定坐标升序。不取最大面积当万能规则；无人到场的远岛和峰不合格。

`SelectAnchor`只生成待到场/待建计划，不遥控迁出被困者或把旧stock移到新楼。原anchor地块变水且尚无真实到场替代时就是STRANDED/LANDLESS。可在本笔事务已真实站在合法替代点的负责人明确提交`DesignateTemporaryAnchor`，不刷公共设施；缺广场仍DAMAGED，不算可用正式City。

断开且无法继续服务的域撤为`UNASSIGNED_ADMIN`，Statetitle和私产保持；当地还活的人继续原成员身份直到真实迁居/神意成员编辑；楼若未wipe仍留下真实原主的无人/受困设施，不能因为行政断开自动删楼。如果当地居民独立组织且满足新城流程，另建新CityId及显式分离成员；单纯改地不会替他们自动签字。

## 9. 自然扩城的具体算法

没有基于“每人固定多少土地”直接涂圈的算法。每游戏日以及真实housing/service/resource事件触发一次低频规划，暂停不会跑。`ai_border_claims=false`阻止新的自主claim，不撤旧域；`ai_public_construction=false`阻止新的公共项目，已合法项目按原资源继续。

1. 从实际Household、人口合法增长、住房床位、实际生产/送货/道路拥堵取得需求。按可到达和合法服务覆盖算缺口，空楼不以0人口虚构需求。
2. 为需求生成项目候选（住宅/衣坊/农田/路/广场重建等），先试有效域内闲置地、复用/修复既有楼及不增域的合法加密。保护基础食物、衣物与运转余量，不从装饰植物估食物产量。
3. 必须向外时，从**存在实际有效服务/已占用设施的边界**提取候选前沿，不从裸空painted claim一直链式吞世界。候选包含完整项目占地、入口/前侧送料、实际路廊和必要地基；普通扩城不毁树楼私产而不结算。
4. 新域必须当前干、非峰可建目标、有实际对应运动/物流路线、权属许可、全占地、施工真实stock/劳动预算、任务源与目的双端可达。普通扩张仅在本国或合法未主张地，不能顺手越邻国；村落无State时记真实集体占用，不赠State。
5. 服务catchment以多源非负整数cost Dijkstra算：公共广场/供粮/实际道路接口为源，边cost含运动profile、坡/宽/阶差、门/路权、真实装卸及拥堵预测，不能颜色=导航。暂时排队/人物堵门是非负ETA软cost，不是永久图断边，不能使全城行政域每tick缩放；拆桥、失支承、峰水硬障碍、合法路权取消才撤硬证。sample最大单程服务路径为180运动SimSecond（3600tick），但不同服务type有自己的期限，食物耐久/货运容量可收紧；不是180日历秒。
6. 候选逐个验证原城市及邻居的旧AccessCertificate，普通后建不得堵路。先reserved项目成功再将对应必要域claim；不为了远期可能建设拿整片无用荒地。
7. 用本文件第11节共同PlanComparator比较原域/扩城/新城方案；成功提交`ExpandSettlement`只能分配域与实际合法项目审批/预算预约，建筑仍真实送料建设。
8. 项目取消后无永久抢地：未开工无公共用途的新增域在明确期限后可被依法释放，基于claim来源/当前用途/居民和私产检查，不回退过去产权。已有人住/设施建成的域不因原项目ID结束取消。

每次自动扩张最多sample16个项目候选、每城市最多一个竞争性growth plan、每天新增行政域sample64个2m格等效面积；这是防爆发和公平调度的初样预算，不是正式容量上限。用户画城市边界可提出更大范围，但必要业务验证不豁免。管理能力决定真实文书/服务负载/审批周期，不给领导属性一个无依据的加地倍数。

## 10. 新城候选与真实创城状态机

需要新城的原因必须对应真实需求：原城catchment不能服务目标资源/港口/住房，有独立陆域或路程成本过高，存续失地共同体选择迁居，或真实已存在居民有自组织/分立诉求。不能仅城市超过固定人口就刷城，也不能为了国家升大公国拆两个空广场。

`FoundingParty`字段：PlanId、sourceSettlement、existingActorRefs、各真实同意/合法代表、Household安排、originRemainingServiceBudget、destination、food/clothes/tools/material reservations、transport/access certificates、deadline、actualProgressRevision。人类或已启智且成熟Actor均可发起，Culture可空时建前文化Settlement；有文化启智动物可建正式City，不能在过程中创生新Culture/Language。样例自主殖民party至少2个独立Household和3个成熟CivicActor，仅为风险策略；**登记与正式City资格不硬要求3人**，玩家显式单人立国/真实独人建设不被这个AI策略阻止。

候选搜索采用明确人口/需求来源：先从已探索、真实可达region catalog中抽取最多sample64个固定排序/独立保存规划RNG候选；不在没地图知识处全知找神奇资源。已探索不是镜头看过，不改变全知God视图。粗筛几何/catchment/权属后，精筛完整地形、粮运90游戏日预测、实际准备和建筑site oracle；粗筛结果不能批准施工。

必须留够源城真实基本服务与自身口粮。离开可减少源城拥挤，但若移走唯一制衣/供粮成员，需先安排真实替代/供货，不能“全source精英搬走后发默认工人”。供给预测不信用无限decor、未来野生恢复、还没长出来果或玩家将来供货，最多用已确认生产能力保守下界，失败明确缺口。

空间距离不用直线radius。若同一个可服务catchment且扩城可满足需求，应优先原城/扩城；被山海/峰/禁路隔开、实际单程服务超上限才成为新城候选。邻城隔山很近可合法独立，平原很近则拒重复规划或并入现城。邻国claimed地拒自主殖民，除非现成条约/法律明确授权；授权不转居民国籍。

```text
PROPOSED -> QUALIFIED -> RESERVED -> DEPARTING -> TRAVELLING
  -> ARRIVED_REAL -> PROVISIONAL_CAMP -> SETTLEMENT_REGISTERED
  -> SQUARE_AND_BASIC_HOUSING_BUILT -> CITY_CHARTERED
任何中间态 -> PAUSED | CANCELLED | LOST
```

`SettlementRegistered`只能由至少一个合法成熟实际到场者、可支承干地anchor、合法集体用地与当前有效自愿成员触发；不产生Building/CitySquare物理库存/国家/文化。临时营地若只露宿没有住宅，寒冷/安全需求仍真实存在。

正式City需：至少一个活CivicMember；该成员组织具有有效官方Culture关系；合法章程和直接行政关系；实际广场设施/逻辑account绑定；至少一处真实可使用的居住设施（容量足供当前正式居住成员，否则拒承诺新成员）；基础食物/衣物来源与运输/公共服務具有现实可执行预算；没有未解决峰/支承/送货/旧接入冲突。贫穷不能免费免除这些条件；可以长期仍是Settlement。

不把到达和建成压成一步：人在途时source成员按实际迁移契约保留/过渡，目的地不提前算人口；中途死/回头/货失各自一次结算。住进后新HomeCity另明确成员迁移事务，source/target计数同提交；不能同一人让两座城市保资格。

## 11. 扩城还是新城：同一个规划器做比较

先输出`RepairInside`, `BuildInside`, `ExpandExisting`, `FoundNew`, `ReestablishSameCommunity`, `AdmitToExisting`, `WaitOrRequestAid`七种明确方案。硬门控不通过的方案只有拒绝原因，不能用漂亮分数压过洪水/财产权/新人口来源。

比较目标是固定排序tuple，避免随线程/RNG乱变：

```text
(
 unmet_basic_need_person_days_ASC,
 unacceptable_risk_count_ASC,
 disrupted_origin_essential_services_ASC,
 relocation_household_count_ASC,
 estimated_nonrefundable_real_effort_ticks_ASC,
 actual_delivery_cost_accounting_units_ASC,
 irreversible_ecological_biomass_loss_grams_ASC,
 nonrefundable_asset_loss_accounting_units_ASC,
 plan_kind_preference_ASC,
 stable_candidate_coordinate_ASC,
 plan_id_ASC
)
```

人日缺口仅由实际现有/已合法孕期新增群体×冻结日历预测窗口计算，每个个体/Household只计一次；劳动tick、运输费用、不可逆生态生物量损失克数与资产损失核算单位分别比较，不相加成单位错乱的“总分”。同目标同预算时`RepairInside<BuildInside<ExpandExisting<Reestablish<AdmitExisting<FoundNew<Wait`是默认tie break，不强迫在更危险原址修复。估计没有确定能力/仓储/路线时标UNKNOWN并按保守最坏值；不得unknown=0。

自主扩城相对新城：sample预测窗90日，候选new城连续3次日评估占优且至少改善基本缺口20%才发party，防每次暂堵路就移城。实际灾难/玩家显式命令可跳这个**决策迟滞**，不能跳硬物理/资源门控。正在进行的方案保到履约/严重失效，不每tick撤全部预约。候选cache含versions与明确过期；UI查询不增加“连续3次”计数。

若原地无再建可行方案，`ReestablishSameCommunity`保CityId优先于刷一个同名新城；如果仅部分自愿居民分立并批准新共同体，那是`FoundNew`，保原城存续成员，给新ID。迁居身份选择在预览清楚，不能按哪种便于升rank偷换。

## 12. 国家存续、无地政府与归档

State另存`governance_status = OPERATING / DISPLACED / WINDING_DOWN / ARCHIVED`；OPERATING同时需要真实活共同体和实际合法可支承非峰干地治理驻点，或真实有效成员State治理；仅drytitle面积尚存、但所有行政共同体离开/受困，也转DISPLACED，按同一绝对30日迁居/失驻点计时降权，不无限沿空地征税。`living_constituency`是活的法定臣籍成员、依法认定参与政治共同体的成员或帝国实际有效成员State的真实共同体。普通路过外国人、楼owner、地图flag、未执行许诺、空CityId、亡者、无实际主体的船不计。

- 城毁/都城毁/君主死：先真实城市灾损和王冠空位结算，不等于国家灭亡；选已有真实其他直辖城迁都，不自动刷新首都人口/宫殿/仓储。
- 全有效陆地消失但living constituency不空：DISPLACED；保法律身份/私人财产/实际公共account/外交历史，不在海上征陆税、批楼或发殖民计划。自愿迁居/接受本国尚可行实体城/复建恢复，不把投靠他国当瞬间治好旧国。
- 首样30日landless grace后仅保连续共同体/资产/既有合同和防卫/救援的受限治理权限；旧地扩张、征税、新战略动员/战争或帝国授予不继续。无地并不清除臣籍；以实际可操作功能限制解决无限幽灵政府问题。
- `living_constituency=empty`且没有有效合法在途continuation，立即WINDING_DOWN，撤销新的主权/公务authority、实际title变历史清算待决，暂停未履行政治计划；公共债/他人托管/契约有wind-down executor，不因为立刻归档删除欠款。绝不能拿“有一艘owner=State的船”续国。
- 有真实剩余estate/法人债不是国家活人口。清算记录可独立长期存在， State archive不得等待所有债永远清完：转入独立`PublicEstateAccountRef`和明确债务/托管执行Ref后即可ARCHIVED，实体不刷钱赎债。

无有效私人/国家土地的荒地不会因为archive自动赠给邻国。主权空缺转`UNCLAIMED`；私产title仍有合法后继/estate即保留，神意立国/扩城需分别处理私产许可。政府死亡档案和产权注销不是同一操作。

正式直辖City当前资格按活共同体＋真实公共服务/章程而非永久historical CITY charter count；DAMAGED无广场/住房不能凑“第二个真实City”。`direct_qualifying_city_count`在同批重算，DUCHY仅在第二个实际合格直辖City取得时晋级GRAND_DUCHY；默认既得GRAND_DUCHY法定rank保留，缺城只降当前实际能力，不按单帧计数抖动自动降爵。只有有效章程明确的降级程序或显式神意编辑才可降级并留后果事件。已授王冠/帝国的法定称号和宪制合法性另按旧合同处理，不凭少一栋楼解除神授/私有继承；不能把荣誉历史title当运转加成。

## 13. 弃城衰减、再占用和恢复

DEPOPULATED样例30游戏日deadline；保存绝对start/due和原domain snapshot，不从载入当天重计。每游戏日剥离边缘`max(1,ceil(original_assigned_area_cell_equivalents/30))`等效格的unused行政assignment，直到deadline全部撤销。边缘包括真实domain外界和有限权威域边，内洞边；全世界都是城域仍有世界权威边可剥，不能死城占满世界不衰减。每格按距外缘层数/坐标确定次序，实际完整建筑及私产不随剥界物理删除。

持续接收有真实到场居民并登记合法成员，可取消未完成wind-down、回CONTINUING，恢复**当前还合法的**域/设施；已撤销域要另申请，毁物绝不恢复。已进入ARCHIVED只能给新SettlementId；相同名字/地块/文化不是同一城。归档后所有搜索/收藏打开原档案并指明新实体关系，不能指向同名城。

迁居deadline和弃城deadline不同；前者对有成员失地community降权限，后者针对无成员归档。到期规则关闭AI扩张/新城也照常结算既有灾损/契约/归档；不能用rule off冻住账债和非法占用。

## 14. 神意元控制接口

`GodFoundStateFromActor`：真实有Culture且成熟CivicActor，实际合法干地支承/非峰/无人主权、私产单独许可；可同事务登记PROVISIONAL_CAMP＋DUCHY State＋本人第一有效成员/职位。初claim仅其真实支承/身体净空所需且合法的干面primitive交域，样例最多16格；不按半径画一大圈，含他国title/未经授权私产/水/峰或身体支承不足则拒，后续域仍按真实项目扩张。事件为明确GodFoundationEvent，无须伪造不存在居民支持，但不发正式City、CitySquare、住宅、库货/文化/语言。若其他人被纳入须单独显式成员授权；单人不能签别人票。

`CommitLocalMembershipStroke`：几何笔刷只枚举当前实际命中的活CivicActor；每目标PhysicalCity/State从同一候选snapshot查，Scope=仅城市/仅国籍/两者明确；只能加入存续合法目标，空地/已归档/峰hazard无City则skip或明示清空选择，默认skip不无意制造无国籍。不改Culture/Language/私产/雇主/位置。家庭/未成熟者照料与岗位条约预览，不伪造自愿迁居；GodMembershipEvent标明强制神意。

`GodTransferCityToState`：选城→目标国；整体城市及明确core territory主权/行政、真实publicaccount authority、首都/等级/选举席/条约/税法/在途合同/armyaccess一次预览。默认不改居民citizenship/私产owner/文化语言；若另选居民入籍toggle则列具体Actor，与转城同一candidate，否则另画brush。不能只有city.ownerState字段换了而其土地还着另一国；不能转别国跨域district而不列冲突。

同批城市archive检查必须读取神意最终成员集合：先wipe再成员加入/转城和相反顺序只有明确不同执行tick才可以不同结果；同一命令batch声明final intent时normalize一次，不在中间发布消失的City再复活。完整God权限/控制租约/命令route在另一合同，城市接口不能绕过有效Lease作Actor命令。

## 15. 事务read/write与增量执行

ReadSet：changed surface/kind、湿域拓扑/峰裁域、support/nav/water/access versions、整楼及dependency closure、life/death/gestation、actual memberships/Household/charter/permissions、titles/claims/assignment、公私stocks/reservations、在途货与foundingparty、publicaccount/debt、position/container、rank/election/treaties、clock/rules及candidateRNG cursor。

WriteSet：必要几何和整对象删除/物损、hazards/实际物理状态、撤证/暂停任务、城市域/anchor/status/capacity、真实成员迁居、city/state counters/职位/继位、公共清算托管Ref/法域税责、持久绝对due、历史events/损失idempotency receipts。天然palette/rendererdecor cache不作World库存；sourceprovenance/过去events不覆写。

Single-writer即使多线程prepare仍要提交重新validate所有读版本；新准备期间旁边坡道/投票/死孕/成员被改，整个相关candidate失效后重新预览，不只patch一个计数。并行prepared两城同时claim同域，按writer合法command order先提交者获域，后者重算，不双城同辖。

DirtyGrid→reverse spatial/support/container→affected nav edges/regions→reverse ServiceCertificate/Shipment/FoundingParty→City/State index；桥/海峡变化可能dirty远端城，必须按dependency而非编辑格周边radius闭包。存续法律title索引与当前dry-index分别维护。原稳定City域不每笔全图扫描；删除连接点要重验证整个受影响分量，简单union-find不能正确处理删除。

未来目标：chunk局部mask＋region portalgraph＋保存service certificates；candidate先粗筛后exactverification。大闭包不能在后台慢慢同时允许旧证书继续走；需准备到完整候选后一次切换或明确拒绝本笔规模。容量/最长帧/内存本轮无测量承诺。批量统计来自一次committed snapshot，renderer/LOD/query不得触发扩城/衰减/RNG。

## 16. 存载及三语交互

保存SettlementId/generation、CITY charterhistory、双状态/anchor、active/withdrawn assignment、title/provenance、公私logicaccount和实际facilityRefs、membership/charterevents、due ticks/decaycursor、GrowthPlan/Party realprogress/资源与route版本、候选比较输入与独立RNG cursor、失效service证书、statewind-down executorRefs、完整事件idempotencyreceipt。可重建mesh/空间cache，但加载必须校验权威域、实际对象与reserve，不再次产生居民/城/国家/审批/物资。

所需typed字段有中英德label，全部是业务事实：存续/受损/受困/失地、服务面积/行政面积/主权陆地面积、居城成员/实际在场/无家可归、广场已毁/可用库容、待迁居/新城party/来源城/方案拒绝原因、当前国籍/当前物理法域、退役/后继城。初始页面不塞算法权重与debug小字；后果窗口明确失物/受困/成员/主权改变。地图边界不同深浅可显示有效服务/历史claim，绝不将危险峰涂成可走。

按钮继续无框，全部全局工具底栏相应section；信息页只局部关系/编辑/关闭。选择一人、城市、国家即可跨页；前名和档案链接保StableRef，不用textname寻址。

## 17. 参数与性能证据边界

样例：二维格2m、普通500mm/ramp125mm按冻结profile；20tick/运动秒、1200tick/游戏日、30日/月360日/年；日评估、3次确认、20%改善、90日forecast、180运动秒catchment、30日失地降权/无人弃城、16项目/64选址候选。保存实际参数hash。时间加速/暂停/存载不能使累计3次或30日变成渲染帧；locale/相机不影响方案排序。所有样例需在真实粮/衣/劳动/路程/寒冷/生产对齐后联测调整，不在旧World静默换profile。

## 18. 必须贯穿的反例

下表50个场景是未来production验收合同，不代表游戏已实现。[有限参考模型](../research/models/settlement_reference_model.py)只覆盖其中身份/连通/区域/事务收敛的抽象子集，不能代替坡几何、真实生理、建址、物流、GPU或Steam实机。

|ID|场景|应有结果|
|---|---|---|
|SC01|全楼普通清理但居民/干地在|先真实人货疏散，DAMAGED保ID，重建无免费楼|
|SC02|全楼强制毁损，原广场有第三方货|容量0，一次物损/托管结算，不把货变城市owner|
|SC03|全陆涂海，居民仍活|LANDLESS，真实落水/救援，国籍/成员不随色消失|
|SC04|全陆涂海且无存活成员|无中间假继位，进入城市/国家wind-down档案|
|SC05|所有楼毁但有另一真实直辖城|现国保留，实际迁都候选而不刷人宫殿|
|SC06|所有人死但楼全在|DEPOPULATED停止扩张，30日边缘衰减，楼产权仍真实|
|SC07|城域占满整个权威世界无人|域边可剥，30日内撤完，不无边界永存|
|SC08|只少一座楼|不能人口/行政域随机骤缩|
|SC09|桥拆导致半城断接|证书撤，孤域UNASSIGNED_ADMIN，Statetitle不转邻国|
|SC10|水切城，一侧没被wipe的楼|不自动删该侧楼，不自动另刷新城|
|SC11|原anchor淹，最大干片无人且远|不以面积最大空岛当新中心|
|SC12|原anchor淹，但负责人已实际在合法次中心|可显式临时anchor，没广场仍DAMAGED|
|SC13|干片仅角相接|不是行政/服务连通|
|SC14|水中坡格两湿域切出小干岛|按精确正长度陆边/actualnav，不按color格bool|
|SC15|峰覆盖居民|保pose hazard，不抬峰、不强制导航逃离、没有合法创城点|
|SC16|soil改hill楼跨三个格，只扫一个|整栋一次删；相关楼内goods/道路预约闭包|
|SC17|同geography kind重复涂|无第二次wipe，无重复损失|
|SC18|主题换皮|非kind改变，不删楼/补果/自动改城籍|
|SC19|改height但kind未变导致支承失效|仍真实结构/通行灾损，不以no wipe漏伤|
|SC20|同批死亡和转籍顺序反转|final合法集合唯一，城市/国家状态一致|
|SC21|弃城第29日存载|deadline不刷新，既有财债/旧成员不复生|
|SC22|归档后重涂soil再造同名楼|新城需新ID，收藏仍旧档案|
|SC23|失地成员真实迁居30天未建成|仅共同体/财债保留，不能从旧海域税/发新战|
|SC24|失地所有成员永久入别城并退出|旧城可归档，无强保幽灵城|
|SC25|无地国仅剩有owner国籍字段的空船|不能作为活政治共同体续国|
|SC26|国王死，合法儿童臣籍仍活|国家存续，合法摄政/空位，不刷继承人|
|SC27|state存续，旧title海变土|有效dry主权可恢复，City行政/楼货不恢复|
|SC28|state已archive，旧title海变土|仍无主或当前合法权利，不复活国家|
|SC29|原城有闲置可用住宅|先真实复用，不为了人口图表刷新区|
|SC30|荒地离近城20m但隔不可越山|按path/catchment，可合理独立候选|
|SC31|荒地500m但合法快路仍可服务|比较cost，不能只按distance建新城|
|SC32|新城party带走唯一粮衣工|origin余量不够拒计划，不给替代工人|
|SC33|party途中有人死/货毁|目标人口/库存不提前计，取消/改计划一次|
|SC34|候选地已有邻国私产/主权|自主found拒，神意模式需显式title/私产后果|
|SC35|无人paint边界成两个城市|拒空city/stock/升rank|
|SC36|party无Culture|可Settlement，不假CITY或创Culture|
|SC37|有culture启智牛到场|可担创城政治资格，搬运/工具仍真实身体门控|
|SC38|玩家有文化成熟一人神意立国|真实camp+DUCHY本人，不免费formalCity/广场/人口|
|SC39|篡改其trait增加治理|不能给地/物资/另生边界；服务负载实际校准|
|SC40|AI扩张关着毁城|灾损/债/撤证照常，不能冻结历史|
|SC41|两城并行candidate抢同域|单写先者commit后者revalidate，无双辖|
|SC42|换locale/镜头/LOD查城市千次|不推进growth/streak/abandon/RNG|
|SC43|海峡变deep，远城食物航线受阻|closure含远依赖City/Shipment，不只本格dirty|
|SC44|转城到另一国后居民仍原国籍|行政/主权变，个人身份不暗改；另明确toggle|
|SC45|物理成员brush扫旅行者|真实当前法域入籍事件，位置和文化/货权不改|
|SC46|同人参与两个新城party|独占/真实预约冲突，不能双计延续/人口|
|SC47|有财债但无任何活国家主体|statearchive可交PublicEstate，欠债仍在，不造钱|
|SC48|有入城party却30天不行进反复读档|due/progress不刷新，失效后不阻止归档|
|SC49|最后成员死/退出，外部启智孕体/蛋仍活且后出生|孕体非现有人口；无实际合法steward不得维持政府，旧ID归档后出生解析合法存续目标或无国籍、不复活|
|SC50|所有楼还在但结构/入口/屋顶/设备损坏令服务停摆|按实际capacity为0而非楼数进入DAMAGED/STRANDED，真实可抢救货不退款，优先实际修复而非刷城|
