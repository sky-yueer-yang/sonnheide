# 固定高度、真实坡道与冷热/危险边界

2026-10-09，v0.9 / [ADR0018](../decisions/0018-fixed-terrain-and-executable-game-specification.md)。**设计未生产实现**。配对[terrain_access_v1.json](../../data/contracts/terrain_access_v1.json)，统一生成、数值、tick与存档见[确定性世界](DETERMINISTIC_WORLD_AND_RUNTIME.md)。原v0.6逐字保留作来源，不是执行指令；旧自由height、渐降岸线和自然水体身份已被覆盖。

## 1. 必须共同证明的边界

|边界|权威结果|
|---|---|
|八地理类型|各自唯一flat高程；普通升降也只移动固定档，不产生自由高度|
|坡道唯一例外|真实连续ramp_patch共供渲染/碰撞/拾取/支承/导航/水/温度|
|地形破坏|ADR0017地表对象闭包删除减去活Actor随身递归保护；无半写|
|高峰|实际≥80m区域精确裁禁；所有移动生命不得合法进入/飞越|
|远端路径|海峡/桥/坡断开可影响远港和供粮，同提交撤证/停任务|
|寒冷和危险|实际height/pose、species/Phenotype、衣物/体湿/能量作用于life唯一函数|
|身份/保存|无山海湖自然对象；仅当前revision的可重建导航组件|

镜头、LOD、拖动帧、线程完成速度不能改变世界。单写者原子提交是必要条件，真实完整因果读写集仍须证明。

## 2. 八档flat与水

|类型|唯一顶面mm|
|---|---:|
|deep_ocean|-20000|
|close_ocean|-8000|
|shallow_water|-2000|
|sand|1000|
|soil|2000|
|hill|16000|
|mountain|48000|
|high_peak|96000|

首样2m水平格、500mm flat量子、125mm坡角、64²chunk。flat保存`terrainKind`，高程由固定映射派生，不开放合法范围内任意heightQ。sand↔soil从1m↔2m，同时确有kind变化，清地表并保生命随身；soil生态主题只影响地皮/装饰/未来候选，不改高度或wipe。高度参数为游戏定义，不宣称真实DEM。

水面首版统一0。没有天然岸坡、浅深水自动过渡、地貌噪声、高位湖/瀑布/流体体积/潮汐/侵蚀。普通chunk接缝不是世界裁切。Core选区采用明确矩形裁切，外围8格固定deep_ocean保护域；再外纯表现海面。矩形断岸可见是明确取舍，不能暗加不规则岸线。Earth只锁GSHHG2.3.7海陆；创建投影/源嵌套/格中心采样详见确定性世界§3。

## 3. 没有自然对象身份的精确湿网

flat按固定bed<0为湿；ramp每个三角面按h<0裁精确湿多边形。共享边只在相通湿区间有**正长度**时四连通；点/对角触碰不通。一格NW/SE干、NE/SW湿时可有两个不相连LocalWetRegion，必须分别做查询和船型净宽/吃水验证。

临时节点key=`(CellCoord,inputTerrainRevision,minimumTriangleIndex)`；连通component key为当前区域最小节点，可重建、可随每笔改地改变。SEA是连到guard源的分类，其余LAKE；它们只是导航/显示分类，无WaterBodyRef、名称、实体编辑页、分裂继承/合并ID或永久自然档案。船/鱼/港口依赖实际坐标门户及水深、路径证书和revision，不依赖临时component身份证。拓扑搜索未完成时保存输入revision和排序前沿，不保存永久湖。

## 4. column与ramp_patch

```text
TerrainColumn
  surfaceShape=flat | ramp_patch
  terrainKind?                       # flat唯一字段，固定表派生height
  cornerHeightR[4]?                  # ramp NW,NE,SE,SW ×125mm
  triangulation=NW_SE
  topMaterial, ecologyRef
  rampOwnerRef?, mutationSource, mutationEventId, revision
RampRecord
  RampRef, origin=DIVINE_EDIT | CIVIC_EARTHWORK
  orderedControlPoints, widthMm, intendedMovementProfiles
  touchedCellRefs, geometryBeforeImages, ownedCellRevisions
  state=ACTIVE | FRAGMENTED | REMOVED
  createdEventId,lastEditEventId,geometryHash
```

shape字段互斥。面内h使用规范有理数插值，不再取整成500mm台阶；面下真实填实体。每列独占四角，相邻列边高不同形成实际竖壁。接边分段交点使用同一精确有理数；通行验两侧高差是否匹配或在真实台阶能力内，不能由mesh法线或颜色准许过墙。

坡道低地实际h<8m按顶材质分sand/soil，8…32m为hill，32…80m为mountain，≥80m为high_peak；水深分3m、12m阈值。该分类只用于连续坡面的后果裁切，不准flat保存这些区间任意高度。

## 5. 画笔和有限坡道求解

八flat绘制、升/降一档、统一指定档平整、仅flat复制、创建/移除坡道和主题绘制为全部几何入口。无Smooth或自由height滑杆。普通笔刷以正面积覆盖选整格；圆/方/线/带的拖画是世界空间扫掠并集，按(y,x)去重，不因刷新多升一档。选坡道区域升降需明确flat目标。命中格转flat并撤旧Ramp cell ownership，未命中格不改。

坡道先点实际下端/上端flat落脚区，保端高；峰目标拒绝。默认宽4m，步行/车/特定species profile分别显示。首版中心线是按2m格中心ties-to-even量化的正交折线；非正交拖点枚举X-Y/Y-X接法。控制点≤16、直段≤32、长度≤512格、候选≤64；先直接拼接，再对最长不够段按轴/方向/偏移1…8格枚举一次矩形回头绕行，不无限搜索。

折角生成真实平转弯平台：行人4×4m，车按ceil_to_cell(2radius+bodyWidth+2clearance)；6m半径、3m车、每侧0.25m余量得到16×16m。转弯仍需整车真实扫掠，不能凭平台图标通过。直坡起止以平台边界算；总爬升按各直段长度比例累积分配、125mm ties-to-even，直段横向同高，平台四角同公共端高。非相邻正面积自交或角高冲突直接拒绝。外围不自然降高，仅候选完整格的真实面。

每个三角纵/横坡、所有接边、两端、全宽/全身扫掠、转弯、峰、支承/权利/旧接入都验。`L≥abs(rise)/grade`只是必要条件。soil2m上hill16m升14m：行人1:3理论42m，量子后可用48m直坡；车1:8可用112m每2m升250mm连续直坡。不得拿平均坡度掩盖单段超限。

通过候选按 `(positiveFill+excavation,changedCellCount,centerlineLength,canonicalCornerArray)`精确升序取首个。预览冻结角数组和consequenceHash；commit不偷偷换更优路线。没有候选返回真实缺长度/净空/峰/受保护接入原因，用户可改控制点/目标，不能私降平台。

## 6. 身体通行profile

|首样profile|纵坡|横坡|台阶|净宽/净高|其它|
|---|---|---|---|---|---|
|无负重步行|1:3|1:4|≤250mm|≥800/2100mm|脚底支承、真实入口/路权|
|小型轮车|1:8|1:12|≤50mm|≥3000/2800mm|6m转弯、整车载荷扫掠/牵引制动|
|动物|Species+Phenotype有界能力|同左|同左|实际固定成人body与余量|陆行/攀/泳/飞独立掩码|

profile为待小场景校准的游戏参数，不是现实安全标准。负重/疲劳/受伤/湿滑读life正式能力输出并收紧，不直接使用trait flags跳过空间约束。力量不放大入口、桥净空或身体，也不产生私家马/骑兵/马车。

## 7. 高峰和危险占用

实际三角h≥80000mm裁精确PeakExclusionPrism，从峰阈值平面至生命飞行上限256m；实际身体投影/扫掠接触即可失败，不只测中心。所有移动Actor含人/启智动物/飞鸟/蜂不能合法站/放/出生/神手进入/飞越。gene、坡道、附身、衣物和关闭冷热都无豁免；Plant树按真实生境，不因nav禁止直接删；相机/光/装饰粒子不是Actor。

God破坏在旧Actor脚下升峰或填高实体时不抬人/瞬移/直接erase；保真实pose和随身绑定，标PEAK_ENCROACHED/TERRAIN_ENTRAPPED等InvalidOccupancyHazard。它撤正常路线/生产/站位资格，不是高峰通行漏洞。真实削低到固定mountain/其它档可解禁，但这是一笔新改地，仍预览支承/物损。生命危险由life_profiles.hazard_physics推进，runtime只报告事实/冲量，不能自行清HP。

## 8. 坡道移除与真实工程

before-image只保存合法原flat类型或可验证旧坡几何，不含库存、果量、食物、伤势或预约。移除恢复仍属该Ramp且revision匹配格；别人的后编辑保留，Ramp变FRAGMENTED。若B覆盖A，恢复A还需A为ACTIVE/FRAGMENTED且geometryHash验证，在本笔事件更新A当前归属/版本，不重播创建或旧证书；A已移除/无源则RAMP_SOURCE_CHANGED。

删除、恢复和同档重画都是真实新事务。类型足迹变化执行ADR0017地表整对象闭包减随身保护；同kind仍核真实几何危险。人保pose、有效平台仍站但可能STRANDED，失支承则fall/water/entrap；车船可毁而活人随身工具/第三方货保Ref和真实状态，独立货舱不保护。无免费回滚死亡/货损/材料/退款。

God坡道免费只改几何，记录DIVINE_EDIT；文明坡/填海需实际权限、配方建材、劳动/运输，完工前不提前通路。削/填体积按新旧三角差正/负区域分别积分，不能只核net让等量削填免费。普通土体不自动成商品矿物，可回收真实构件另有实物来源。

## 9. 同事务完整后果

1. 校验World/Session/Draft/权限、固定profile/枚举/量纲/checked范围；guard/域外拒绝不clamp。
2. 在单快照规范几何，求真实changed-kind正面积区域、全占地/支承/空间/container和活体携行readsets。
3. 冻结实际活Actor递归穿戴/手持/携带保护，扣地表整对象destroy closure；未采树果/地面库存核唯一loss，携行已采商品保留来源Ref但不源树级联。
4. 旧生理/合法工作/食物reserve生产积到边界一次；准备地基/支承/生命危险/任务/预约/施工SUPERSEDED。
5. 局部与远端人车水路依赖撤证；保护模式必须证明旧必要接入仍在或真实已有替代，未重证不称安全。
6. God允许破坏的具体后果列整对象扩展范围/立即物损/未来风险；死亡/交接/到货/入住/定义变化使旧preview失败重算。
7. final alive/geometry/member集合统一结Settlement/State/职位/王冠。只撤有效dry行政图，不把法律title当货删。
8. 预分配完整write set，单写者一次publish，事件/receipt唯一；worker只读重建候选不能分配身份、签证、补生产或处死。

NAV_UNVERIFIED只表示已撤旧证等待可达性；当前身体碰撞/水/峰同步验，不能冻结落水人为安全站立。建筑绝对楼板/地基不跟地变高低；同kind改高可以埋楼/露基础，按真实支承/collision/hazard核，不能强行跟随移楼。

## 10. 温度、生理与开关

权威环境温度使用整数mC样例`clamp(24000-floor(250*max(exposureAltitudeMm,0)/1000)+habitatContactCorrection,-50000,60000)`；exposureAltitude为真实Actor支承/楼板/飞行pose，水中按水面热环境采样，水下床面不产生无限增温，主题贴图/相机/Age不改温度。源函数/生态接触修正、衣物coverage/保温/体湿、疲劳/活动代谢/食物双能量账、冷/热/缺氧/病伤恢复均由life_profiles的版本化函数唯一消费；terrain只提供环境事实，不另累计health。规则参数是游戏量，不宣称真实地球气象。

energy_milli绝对保存，maintenance与productive来源分开；初始维持能量不能转肉/乳/蛋/孕体。health_milli最大基线100000；thermal/hunger/oxygen/fatigue/sleep/infection/wound债不同量，不直接混合相加。个体gene/亚种容量改变前先把旧状态积到tick，改变上限不回满、不清债、不重新孕期。

hunger_consequences和thermal_consequences独立。关闭只抑制对应直接health loss，真实能量/体湿/疲劳/暴露债继续；复开仅当前状态新tick限幅，不补历史伤亡，不给食物。峰、碰撞、坠落、溺水和压埋不是该两开关的豁免对象。体湿/恢复需真实场所/劳动/材料条件，不因关菜单或载入变干。

## 11. 保存、工作配额与验收

采用SonnSave1真实binary header/table/canonical payload、definition hash和故障屏障，见确定性世界§9–10。权威存flat kind/ramp角和所有权/实际身体与随身绑定/资源绝对量/亏欠/任务/控制/receipt，临时wet组件/mesh可重建，不保存自然湖身份证。加载不能改变profile或补货；含非法第九flat高程的包拒绝。

12000tick/日、20tick/运动秒和500tick/日历小时已冻结。拓扑每tick逻辑预算8192、总candidate1048576；stroke4096格、直接实体8192、preview4MiB为样例准入，超限拒绝或显式分独立笔触，不静默截断。大准备保存逻辑前沿，必算单tick闭包不足则统一积压/放慢墙钟，不半发布；相机/LOD不减少真实危险。

未来验收包括：固定8档；source中心采样/日期线；坡道真实人车转弯与移除；水点触/同格双湿区；末口粮竞争/同tick地改和携行交接；old证失效远港；升峰包旧生命；坏存档/缺定义/磁盘满保原Continue。有限参考模型验证算术和边界，不代表真实GPU、动物、经济或Steam已经运行。
