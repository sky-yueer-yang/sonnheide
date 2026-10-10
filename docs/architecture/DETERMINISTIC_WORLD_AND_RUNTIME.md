# 固定地形、确定性世界与运行合同

2026-10-10，v0.9 / [ADR0019](../decisions/0019-fine-terrain-and-colorful-arcade.md)。状态：S00/S01原生切片已有构建、250mm地形、GPU、创建存载；坡道、导航、生物、经济及下文完整运行阶段仍为详细设计和有限参考模型，不能称完整生产游戏或Steam已发行。实际证据见[最新报告](../planning/ARCADE_V2_DELIVERY.md)。本文件与[runtime_foundation_v1.json](../../data/contracts/runtime_foundation_v1.json)配对；覆盖旧连续自由高程、天然岸坡、不规则选区海岸及自然对象命名/身份。人物随身保护仍按ADR0017。伤势/危险函数唯一来源为[life_profiles_v1.json](../../data/content/life_profiles_v1.json)，经济内容唯一来源为[economy_content_v1.json](../../data/content/economy_content_v1.json)。

## 1. 八档高度及唯一例外

|flat类型|唯一顶高，mm|笔刷效果|
|---|---:|---|
|deep_ocean|-20000|深水床面|
|close_ocean|-8000|近海深度床面|
|shallow_water|-2000|浅水床面|
|sand|1000|沙地|
|soil|2000|土地，另选八soil生态主题|
|hill|16000|丘陵平台|
|mountain|48000|山地平台|
|high_peak|96000|高峰平台，移动生命禁止进入|

每个flat列必须同时满足类型、高度和材质映射，不能保存合法范围内的任意第九高度。sand与soil是不同档，互转确有地理kind变化，因此按ADR0017清地表、保生命和实际随身子树；这不是只换主题。首版水面统一0，不做岸边渐降、地貌噪声、自然侵蚀或半随机海床。普通chunk接边只呈现相邻真实表面，绝不创造岸线。

ADR0019新世界采用250mm格，默认512²即128×128m；真实`ramp_patch`仍是唯一连续高程例外，计划四角1mm整数，NW-SE固定剖分、精确有理数插值（尚待S02实装）。其合法范围受profile、两端、身体与峰禁约束。不能暗加岸坡或第二套平滑碰撞面；旧2m/125mm场景只保留历史。

准入算子为`PaintPreset`、`RaiseOneTier`、`LowerOneTier`、`FlattenToPreset`、`CopyFlatTerrain`、`CreateRamp`、`RemoveRamp`、`PaintEcologyTheme`。升降沿上表八档移动一档，顶/底返回`AT_TIER_LIMIT`；对含坡道区域先选明确目标类型，不猜平均高度是哪档。平整把命中整格变成同一指定flat档，不取任意平均高度。移除Smooth和自由target-height滑杆。复制只接收flat类型/主题；源含坡道返回`COPY_RAMP_REQUIRES_NEW_PLAN`，不复制RampRef、before image、人口或物资。

普通类型笔刷命中格会将其完整顶面变成指定flat档，撤销其原坡道几何归属；未命中格保留。主题笔刷不改变高度、flat/ramp形状或地理kind。所有算子都比较精确实际前后区域，类型变化触发地表对象毁损闭包减去活Actor穿戴/握持/携行递归保护；同kind形状变化仍核支承、坠落、压埋、水、冷热、路线，不额外type-wipe。坡道扫掠也遵循同一规则。

## 2. 没有山海湖对象

本版没有MountainRef、OceanRef、LakeRef、WaterBodyRef、自然命名/收藏实体、分裂继承或合并胜出ID。地形查询返回`SurfaceRef`、精确表面和局部湿区；SEA/LAKE只为导航和显示派生分类。海/湖不是可拥有法人，旧湖名不需要持续。

每格最多两个湿节点仍保留：三角面按h<0裁湿，多边形共享**正长度**湿边才四连通。局部节点key=`(CellCoord, terrainRevision, minimumTriangleIndex)`；连通component使用重建当前结果的最小局部节点作临时key。key可以在一次改地后改变，不写入权威实体表、不接受历史外键。任务/港口依赖的是具体坐标门户、净宽/吃水和地形revision，重算后重新验证；不能把旧component key当永久湖身份证。

wet/nav缓存不入必需存档，正在推进的拓扑工作只存输入revision、排序前沿和依赖坐标。无法证明旧水路时撤证/等待，即时身体碰撞和落水危险同步结算。湖中岛/岛中湖仍按源嵌套初始化，只是不产生自然身份。

## 3. Blank与Earth的冻结生成算法

`CoreRect`是用户实际选区；外加8格不可编辑的深水guard形成`AuthorityRect`。Core以外guard全部deep_ocean −20m，Authority以外只有表现海面，没有Actor、导航、产权或资源。初始Blank平陆Core全部soil +2m，Blank全海Core全部close_ocean −8m。边缘不降高；矩形陆块与guard之间可以出现真实竖壁。人车船完整身体和扫掠须留在Authority中；路线终点越界拒绝，既有移动者在真实可制动距离内停止，不瞬移/删船/隐形撞墙。外域命令不夹回边格。相机可看外海，允许看到矩形地貌。

Earth使用**游戏用等距圆柱、标准纬线0**：投影x为展开经度、y为纬度；两轴用同一`gameMmPerMicrodegree`有理比例，不独立拉伸高度/宽度。不声称局部现实距离保真。输入为整数微度west/eastUnwrapped/south/north；纬度仅[-80°,80°]，经度跨度(0,60°]、纬度跨度(0,60°]；本版不允许选南北极。框选跨日期线时east显式展开到west之后，源环沿边顺次以最小经差展开（差值范围[-180°,180°)，相等取负向）；环的整360°副本与选区相交时保留，经日期线做精确裁环而非逐顶点独立夹坐标，拆两个源窗口但只生成一个Core。点击无法合法展开/零尺寸/超范围返回原因，不偷偷变另一选区。

创建指定Nx，`Ny=ceil(Nx*(north-south)/(eastUnwrapped-west))`，不拉伸最后一排。令源微度跨度W，比例=`Nx*cellSizeMm/W`，第(i,j)格中心源坐标为`west+(i+1/2)*W/Nx`、`south+(j+1/2)*W/Nx`，以有理数保持精确；中心在north以外的末排格为海。所有坐标映射计算用checked整数/有理数；预览、生成和读回使用同一候选。初轮Core每轴32…1008格，Authority每轴≤1024格、总格≤1048576是准入上限而非性能承诺。

固定GSHHG2.3.7，南极源仍保原件但极区不准入；native/shapefile离线导入只做源格式解析和日期线展开，不按当前LOD简化。canonical环顶点保存源微度整数，层级L1陆/L2水/L3陆/L4水。对每格**中心点**做exact even-odd：边上用半开射线`(ay>py)!=(by>py)`并以有理叉乘判断右向交点；点恰在边上以当前格中心作为一个唯一判定样本，`point_on_segment`优先归该ring。取命中的最深层级，层级相同取源polygon id升序的第一个（分类同层一致）；没有L1命中是水。water统一close_ocean −8m，land统一soil +2m，无按海岸距离生成沙带或浅海。源嵌套不完整、环不闭合/非法自交、缺必需父环则拒绝数据包，不能猜大陆。窄于一格且不覆盖中心的真实岛可以在本量化结果中消失，最高预览准确显示；禁止承诺所有窄岛永远保留。

保存源包hash、选区、比例、环版本、guard和最终flat候选hash作为不可变provenance；之后编辑只改有效地形。初始生态/矿储通过各领域显式源入口生成，地图栅格算法不送人口、衣服或货币。

## 4. 画笔的几何命中和量化

普通画笔使用整格覆盖规则：命中区域与格内正面积相交即选此格，精确零面积接边不选；圆用中心到格AABB的有理平方距离严格小于r²，方/带用精确区间交。拖画是世界空间折线扫掠的区域并集，按(y,x)排序去重，不按帧数重复修改。默认半径2格，可选1…32格；单stroke≤4096格。强度不再代表自由高差，升降一笔每格只一档。

`canonicalKind(p)`在flat处直接读映射；在坡道处用实际h裁区域：水深≤3m shallow、≤12m close、>12m deep；干地h<8m以sand/soil顶材质区分，8≤h<32 hill、32≤h<80 mountain、≥80 high_peak。这是坡道地理后果分类，不准flat另存这些区间中的高度。所有changed footprint采用正面积真实多边形，不以整格分类标签漏掉坡脚。

## 5. 有限、唯一的坡道求解

输入控制点先按实际profile列网格中心进行ties-to-even量化，新世界格距250mm；起止点必须在原合法flat落脚区，端高采用原固定档，禁止高峰目标。首版路径为可解释的网格正交折线，控制点顺序保留；非正交段尝试X后Y与Y后X两种折线，不把用户拖动帧变成多个提升。直线斜坡、回头坡可用；UI显示真实折线和转弯平台。

控制点≤16，路段≤32，中心线长度≤512格，候选≤64。候选枚举按：两种直接拼接；随后对最长不够长度的段尝试左右平行偏移1…8格的一次矩形绕行（轴、方向、距离顺序冻结）。不递归搜索无限蛇形；不够则`RAMP_ROUTE_NO_FEASIBLE_CANDIDATE`，列出实际需延长/缺净空/峰/受保护通路原因。用户可增加控制点，不偷降目标。

步行直段默认净宽4m，车行默认4m但需实际≥3m。折点设真实水平平台：步行4m×4m，车用`ceil_to_cell(2*turnRadius+bodyWidth+2*clearance)`，首样radius6m、width3m、每侧clearance0.25m，故16m×16m；进入/离开平台处直段长度从平台边算。平台本身进行整车转弯扫掠，不用中心线90°假称车可转。不能用空间不足的小平台放宽profile。

剩余直段按实际水平长度L分配总爬升R：累积端高`r_i=r0+round_even(R*prefixLength/totalStraightLength)`，新profile单位1mm；转弯平台高度为相邻段公共端高。直段矩形内corner高按沿轴参数线性计算再round_even，横向同高；平台四角同高。外围坡肩只使用候选矩形和平台的完整命中格，不做自然过渡。所有格角由定义区解析；同角收到不一致端高则拒绝，不last-writer-wins。非相邻直段/平台正面积自交或重叠一律拒绝，邻接只允许相容平台交接。

对每个候选验证所有三角面纵/横坡、接边台阶、完整身体扫掠、峰、宽高、转弯平台、支承及旧必要接入；平均长度够不代表通过。端点精确匹配旧平台。通过者按`(positiveFillVolume+excavationVolume, changedCellCount, centerlineLength, canonicalCornerArray)`升序选择首个，volume精确有理数比较。预览冻结选中的角数组，不在commit再换另一路。fill/excavation分开存；文明工程实际材料劳动来自经济配方，不因净体积0而免费。

RampRecord拥有实际写格和对应revision、before-image只包含合法flat类型或可验证现存坡道几何。移除只恢复尚归本Ramp且版本一致的格；恢复flat必须回到原固定档。覆盖前坡道记录须仍ACTIVE/FRAGMENTED且geometry hash匹配，恢复是本次新事件，不恢复旧物资/证书；否则明确`RAMP_SOURCE_CHANGED`。移除或覆盖导致kind变化仍按ADR0017清地表而保随身。

## 6. 数字、时钟和输入准入

权威整数采用signed64 checked mm、tick、最小货币和库存单位；几何比较使用规范分母>0/gcd约分的checked128中间积，溢出拒绝整个candidate。坐标取整、量子取整均ties-to-even；数量分配用向下整除及明确余数账。浮点只在GPU、视觉/速度建议中使用，建议提交前转固定点并真实扫掠，不能改变权威分支。

本版冻结20tick/运动SimSecond、**12000tick/游戏日**、500tick/日历小时、30日/月、360日/年。覆盖旧1200及讨论中的4800方案。8小时睡眠=4000tick，8小时劳动=4000tick；剩余4000tick供真实通勤/进食/照护。跨日行程保食物/露宿，不能一天走完任意远距。1/4/16/64倍仅每墙钟推进同样tick数，消耗、候选和随机不变。

玩家输入在可信dispatcher获得`inputSequence`，同会话单调增长并保存；只读查询无序列。提交命令记录`requestedTick, applyTick, authorityClass, actorRef, inputSequence, commandId, idempotencyKey`。已完整准备且获必要批准的命令在下一个未冻结边界准入；尚未完成worker只推进保存的logical cursor，不按谁先算完抢位置。UI暂停下God编辑可以执行零时长boundary transaction，记录同tick递增`boundaryOrdinal`，不推进生理/劳动/截止时间。超时/坏预览不偷偷补执行。

## 7. 一个全域tick的唯一pipeline

所有phase在隔离candidate上执行，最后一次publish；各phase无对外半状态。若必算闭包不足，不推进此tick的部分人，以明确积压放慢墙钟。

|phase|处理|关键口径|
|---|---|---|
|0 FREEZE|冻结当前root/规则/合法输入队列|记录tick、boundaryOrdinal、sort keys，拒旧Session/World|
|1 INTEGRATE_OLD|把旧合法生理、工作、孕体、资源产出积到边界t一次|只积`lastIntegrated<t`部分，暂停/重试无双积|
|2 MARK_DUE_DEATH|life依据phase1当前状态标真实死亡，撤身体/公职新行动资格|死亡receipt唯一；现有法律/已合法承诺不因签署者死自动失效|
|3 APPLY_BOUNDARY_COMMANDS|规则/玩家神意/typed已批准即时编辑|按冻结inputSequence后commandId；每笔重验candidate readset，冲突后者拒/重预览，不强行相容|
|4 DELIVER_AND_FREEZE_DUE|真实到达消息/交接票据及法定截止输入集|receivedTick≤deadline的已合法到达票计入；迟到不因计算慢补入；God删代表后的实际资格重新核|
|5 PREPARE_PHYSICAL|同一个phase4快照产生动作/射击/移动/危险提案|已标死亡不新发；同瞬间合法发射都冻结后再求伤害|
|6 RESOLVE_PHYSICAL|实际扫掠时间τ升序，完全同τ接触聚合|同τ护具/生命快照；LifeApplyInjury唯一写伤势；死后τ'>τ尚未发动作取消，已发弹继续|
|7 ACTUAL_INTERACTIONS|到达真实接口的取食/送料/交接/救援|按真实contactτ、已锁不可分割交接、安全/照护/已承诺期限、waitAge、Ref；真实一份资源只一次claim|
|8 DUE_EXECUTION|真实完成生产、出生、工资税/合同、合法政策结果|重验最终幸存资源/角色；已赚劳动不因死擦除，死后未发生交接不补；孕体出生由life当前合法支持验，不凭父母名单刷人|
|9 FINAL_LIFECYCLE|唯一死亡/遗产、Family/Army/City/State/职位/王冠最终集合结算|继任读整批final alive；死亡前所得入真实遗产；无候选不刷新人|
|10 PUBLISH|检查守恒/Refs/唯一receipt，发布WorldRevision+1|只读表现/查询收到同revision；mesh/navigation候选不能反写事实|

跨领域due效果准入排序为`(dueTick, irreversibleExistingTransferPriority, legalPriorityClass, -waitAge, subjectRef, effectId)`；每领域明确priorityClass，普通到期工资/税/贷款按合法财务rank，不能靠字符串command名选择先谁。合法投票以receivedTick≤deadlineTick计入，deadline边界phase4冻结后新输入最早下一tick准入；实际deadline输入集在phase4一次冻结，后续分片扫描不延长资格。两笔合法命令冲同域，先序提交者得真实claim，后序整笔重验失败；它不同于同时物理命中的聚合。

原born/paid/delivered/loss、Shot/hit、estate/crown/election的effectId都保存；同tick重复回执拒第二次事实。全域社会共同体规范化仅在phase9对最终集合做一次，phase2/6死亡可立即撤新身体/签署权限但不逐个创建临时国王。阶段9产生的新继任者下一合法边界才能提交新命令，不回填自己“早已批准”的法令。

## 8. 危险事实和唯一生命入口

runtime/spatial报告`HazardContactFacts{ActorRef, sourceEventId, actualTick, contactFraction, supportRef, worldPose, velocity, submersionQ, headCovered, medium, compressiveContact, entrapment, rescueSupport}`，保持实际穿戴/携行和完整body/load。自由下落、入水、船楼祖先删除和救援都使用同一真实扫掠；不把人体中心在水面当全身呼吸可用。Species/Phenotype正式出口提供身体质量/置换体积/水线/呼吸介质和合法能力，不直接读alleles。

重力/浮力/安全落地速度、fall severity、缺氧/压埋债和状态函数由`life_profiles_v1.hazard_physics`唯一规定。权威物理产生versioned`InjuryImpulse{ActorRef,sourceEventId,kind,bodyRegion,severity_milli,actualTick}`；只有`LifeApplyInjury`改health/wounds和标死亡。health_milli最大基线100000，energy_milli独立，fatigue/sleep/thermal/infection/wound/oxygen独立debt，不能相互直接相加。冷/饥饿后果开关不关闭碰撞/坠落/压埋；它们只按life相应直接损害边界生效。

### 8.1 唯一的定点counter RNG

所有仿真随机用`runtime_foundation_v1.counter_rng`的`SONNRNG1_SHA256_UINT64LE_REJECTION`，不把“versioned RNG”留给实现者任选。WorldSeed固定32原始bytes。哈希preimage严格按以下顺序连接：ASCII `SONNRNG1`八bytes、seed32、domain的u16LE字节长度及ASCII正文、subject世界UUID16原始bytes、canonical kind的u16LE长度及ASCII正文、stable_id从16位小写hex解析后的u64LE、generation u32LE、definition hash32、eventCounter u64LE、drawIndex u64LE、rejectionAttempt u64LE。domain只准`[a-z][a-z0-9_.]{0,63}`；kind来自现行canonical注册表。person/animal、city、empire、port、ship、equipment视图分别先归一成actor、settlement、state、building、vehicle、item，不能给同主体造第二随机流。thermal_exposure只是Actor只读snapshot，使用同actor Ref而没有独立身份或随机流；不存在自然地理Ref。

kind还必须是`object_ref_wire.canonical_kind_registry`的精确key；合法正则的`bogus`照样拒绝。登记表冻结77种真实记录，每种有owner合同、实际required_fields和scope。skill页面是同ActorRef加已登记skill_id的子选择器，学习请求actor/skill_id必须一致，不分配SkillRef或第二流。culture_node候选统一genesis_node，不能给同核再造身份。local_profile、trusted_session、immutable_definition scope不拥有仿真随机流；真实物种/权限/保存槽查询Ref不等于可以拿它掷仿真骰。

SHA256 digest前8bytes按u64LE读取raw。`uniform(n)`只接收整数1…2^63−1，checked128计算`limit=floor(2^64/n)*n`；raw≥limit丢弃，只把attempt加1后重新哈希，raw<limit返回raw%n。成功才把drawIndex加1；拒绝不吃下一draw。每次draw attempt从0开始，最多4096次；达到上限整笔返回`RNG_REJECTION_LIMIT`，不重新seed、改domain或无条件取模。区间[a,b]先checked算n=b−a+1，再加a；counter/index溢出拒绝，不回绕。

每个真实事件在单写者隔离候选中冻结`EventFreezeReceipt`，保存algorithm/domain/eventCounter/definitionHash、按定义稳定排序的draw用途及nextDrawIndex。成功publish该真实冻结事件时，主体/domain的nextEventCounter只增加一次；DecisionEpisode首次候选冻结本身也是有回执的真实事件，之后取消实际行动不抹去此前已提交的冻结。查询、UI预览、render、worker分片、失败候选和未提交取消不产生draw、不推进counter。同回执重试直接返回已保存值。daily文化递推每个到期日、受孕、真实发声等分别使用自己的已登记domain和有限draw序列；所有领域共用这个bit协议。

参考模型包含已知SHA256 `abc`原语向量、138byte固定canonical输入、n=10的拒绝边界（2^64−1拒，下一raw17得到7）、保存counter/index重播和别名拒绝。模型首次统一验收将报告此输入真实计算出的digest/raw/uniform10000；在执行前不预填或宣称一个未计算的随机结果。这些有限向量不代替C++实现的字节一致性验收。

## 9. 新存档wire格式

格式名`SonnSave1`，major1/minor0，文件little-endian，首版**不压缩**，没有脚本/动态库。Header恰120bytes：magic8=`SONNSAV1`；u16 major、u16 minor、u32 flags(0)、u32 segmentCount、u32 reserved(0)、u64 tick、u64 WorldRevision、16bytes WorldUUID、32bytes definitionManifestSha256、32bytes segmentTableSha256。随后每段64bytes：u32 kind、u32 schemaVersion、u64 offset、u64 length、u64 recordCount、32bytes payloadSha256。table按kind严格升序、offset紧接table且各段连续无重叠/洞/尾随，segment≤32。

必需段：1 MANIFEST、2 TERRAIN、3 ENTITIES、4 LEDGERS、5 TASKS、6 DECISIONS、7 PROVENANCE、8 EVENTS_AND_RECEIPTS；可选0x80000001 PRESENTATION。payload为规范UTF-8 JSON：重复键拒绝、只允许bool/null/string/int/list/object，禁float/NaN；字串NFC，object key按Unicode codepoint升序，无空白，数组保持定义顺序，记录表按稳定Ref排序。JSON depth≤64、单string≤65536bytes、单segment≤128MiB、总file≤256MiB、recordCount≤1000000、所有算术先checked；这是初轮导入准入，不是最终容量宣传。

MANIFEST保存schema表、WorldProfile、固定definitions及asset manifest hashes、时钟转换、边界序列/公平cursor和计数。TERRAIN保存每格flat kind或ramp角数组/归属、八主题、版本；不存自然对象ID。ENTITIES保存活体/孕体/法人/实物/容器/空间绑定和墓碑；所有引用采用`{world_id:32位小写hex,kind:canonical enum,stable_id:16位小写hex,generation:uint32≥1}`；C++内部ID是uint64但Wire不走JSON Number。页面别名按`runtime_foundation.object_ref_wire.view_aliases_to_canonical_kind`归一，person/animal/thermal_exposure指actor、city指settlement、empire指state、port指building、ship指vehicle、equipment指item；不保存第二份同主体，thermal快照不建独立实体。LEDGERS保存绝对量、双账/托管/claims/税差额/currency initialization及唯一兑付receipts；TASKS保存真实route坐标依赖、工作余数/到期/载荷/cycle；DECISIONS保存人格/知识源/消息/承诺/随机counter/搜索frontier；PROVENANCE只读源/来源events；EVENTS保存已发生事实/唯一loss/death/estate/Shot/hit等receipts。asset/GPU/mesh/decor/nav cache不作权威源。

definition hash必须有完整包且每个segment schema均受当前注册表支持；缺必需definition/未知major/未来minor/不支持schema返回明确拒绝，绝不用默认species/价格/高度自动替换。首版只接受新major1/minor0；没有历史自动迁移。以后迁移需白名单`fromHash→toHash+migratorHash`，在隔离候选转换并重核Refs/守恒/来源，原件留档；没有迁移器就拒绝。unknown必需segment拒，unknown optional段仍验证长度/hash后可跳过；绝不执行其中代码。相机/纯表现段删除不改变World deterministic hash。

ENTITY/任务/决策段的canonical Ref必须命中同一77项登记表，并由owner合同的typed记录校验实际必存字段、单位和来源；未知kind或缺字段不造空壳。队列中未送达的message有真实source/recipient/issued_tick/delivery_task/payload_hash，不因为尚未出现在信息页就丢失；gestation有受孕快照、carrier/nest、真实营养进度和唯一出生回执。consent/authorization/due_event_receipt等用history_event的已登记immutable receipt subtype核证明，不把调用者给的Ref或successbit当授权。permission为trusted_session记录，map_marker/control_group/save_slot为local_profile，Species为固定definition；不得从World ENTITY段加载伪session权限或第二份物种定义。TreeRef/BuyerRef等角色称谓最终指实际canonical kind；Cell/Surface/LocalWetRegion只坐标与revision键而非新的StableRef。所有跨段引用在完整隔离候选加载后一次核闭包，缺真实目标拒载。

## 10. checkpoint故障协议

保存仅取已完成WorldRevision，写同卷临时唯一文件→flush真实文件→关闭并完整持久读回/parser/世界不变量核验→原子改名为不可变checkpoint文件→持久更新槽位/Continue临时pointer→atomic replace。pointer替换后，还须directory fsync或已验证native durable-publish成功，才达到commitPoint并发布新World/Continue或删除最旧滚动档。原checkpoint不原地覆盖。commitPoint之前明确失败保旧内存World/Continue及上一有效slot，绝不删除旧档。pointer rename已经成功但目录flush失败属于`DURABILITY_UNKNOWN`：不能保证磁盘旧指针字节未变；保旧内存World、全部旧checkpoint，不发布新World，不自动认新Continue。启动不能知道上次fsync是否返回，只读取验证上一confirmedslot或让玩家明确恢复新的完整candidate；attempt metadata只供恢复界面，不凭客户端success bit伪造持久提交确认。未指向的完好新文件可列出恢复，坏temp不repair；这是异常恢复入口，不给日常AI/保存新增审批。

Continue是本地pointer，包含WorldUUID/revision/checkpoint相对固定slot-root路径/fileSha256；拒`..`、绝对路径、symlink出root和网络URL。导入先只读解析到隔离候选，完整校验后写自己新checkpoint，不直接信外包路径。磁盘满、flush失败、short write、断电各屏障、恶意length、重复segment/Ref、引用已毁祖先、hash错、缺definition、非法flat高程或凭空currency receipt必须有故障案例。准备候选失败不动原World，晚结果带World/Session/Draft/Candidate generation拒收。

## 11. 自研平台固定依赖及边界

本批重新读取官方固定commit许可后，决定重采用[data/native_dependencies.lock.json](../../data/native_dependencies.lock.json)中的**原始七项版本/归档hash**，不复制旧程序实现。SDL3.2.28提供窗口/输入/音频；bgfx固定commit提供Windows D3D11与开发Metal；RmlUi5.1提供本地原生页面三角形；FreeType2.13.3按FTL生成字体；bgfx.cmake/bx/bimg各有独立许可，不把整个third_party统一标开源。新程序只依赖这些独立库，不用游戏引擎、浏览器UI或旧shader成品当新地表证据。

官方固定许可：[SDL](https://raw.githubusercontent.com/libsdl-org/SDL/7f3ae3d57459e59943a4ecfefc8f6277ec6bf540/LICENSE.txt)、[RmlUi](https://raw.githubusercontent.com/mikke89/RmlUi/40edf1acfa7f13f0c9b2af91d6f09ed47aa2c2c9/LICENSE.txt)、[FreeType](https://raw.githubusercontent.com/freetype/freetype/42608f77f20749dd6ddc9e0536788eaad70ea4b5/LICENSE.TXT)、[bgfx](https://raw.githubusercontent.com/bkaradzic/bgfx/cca91681c953d2de9531197b0f580c866ffaa775/LICENSE)、[wrapper](https://raw.githubusercontent.com/bkaradzic/bgfx.cmake/f2ea8fb0438721d754aefa22b14fe161f948a383/LICENSE)、[bx](https://raw.githubusercontent.com/bkaradzic/bx/d86e4ea9d9da6e832a3ff41398587d82b772c69b/LICENSE)、[bimg](https://raw.githubusercontent.com/bkaradzic/bimg/101b5b5fd4670f82cfdec8e98aa1ab9ee93bb2a1/LICENSE)。完整原声明/模块notices和已有项目Release原始包继续保留；未重新下载/重复烘焙原件。没有声称本批新工程已经链接这些版本。

foundation/world/terrain/life/economy核心不include SDL/bgfx/RmlUi；platform adapter拥有窗口、GPU和本地文件故障出口，renderer消费同revision只读表现。输入窗口坐标到framebuffer只映射一次，按真实DPI更新，不能固定2倍；UI截获pointer时地图不另触发笔刷。World暂停不停止菜单/相机；隐藏/降低动态冻结星空、云、水纹和装饰植物表现时间，恢复不跳时，不赠睡眠。Steam唯一发行；Windows GPU与Steam安装证据独立待实测。

## 12. 验算边界和生产准入

[有限参考模型](../research/models/world_runtime_reference_model.py)实现实际整数/有理数运算：八档及命中、日期线/中心even-odd、局部湿组件无身份、直坡量化/自交拒绝、事件排序/同瞬间互击/唯一receipt、真实binary段/hash/破损拒绝。它不是完整GSHHG导入、一般坡道空间求解、3D碰撞、完整生命/经济、GPU或Steam验收。

先在64²小场景证明：Blank/Earth最高预览同候选读回；不同框/日期线稳定；一条真实车坡和删除后的人货随身危险；最后一口粮竞争/同时互击/死亡继任/同tick改地与交接；commitPoint前文件故障保持旧Continue、post-rename不明进入明确恢复而不假称已回滚；再扩大真实居民生态/生产/战争。性能只测真实因果/内存/积压/P95/P99与保存恢复，不先承诺万人规模。所有本批修改和审查完成后由统一入口验算；不要每个小改动重复编译。
