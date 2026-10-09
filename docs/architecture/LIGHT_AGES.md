# Light / Darkness 世界环境与交替合同

2026-10-07。按[ADR 0008](../decisions/0008-sacred-interface-and-light-ages.md)取代太阳昼夜循环；本文是待生产接入的规则、存档与表现合同，不声称目前headless已实现这套环境。浏览器的明暗切换与计时演示仅为交互原型。阶段顺序仍按[IMPLEMENTATION_PLAN](../planning/IMPLEMENTATION_PLAN.md)。

配置意图记录在[规划示例](../../data/light_ages.example.json)，与现有`time_config.example.json`一样不是活动内核配置；不能仅因JSON存在就标环境已实现。

## 1. 三种概念必须分开

|概念|权威与作用|不得混用|
|---|---|---|
|World环境Age|Light / Darkness，世界级统一明暗和自动交替规则|不等于科技时代、宗教地位、locale、相机曝光或主菜单画作|
|模拟时间|唯一整数SimTick、固定TimeConfig、30日月/12月年及人物simulationAge|日仍计生产/税/年龄，不驱动太阳自转或每天一次昼夜|
|表现时间|画作轮播、短暂明暗过渡和GPU动画|不改到期tick、能源、人物资格或环境事件|

Light使用固定方向的主光和受控环境光；Darkness使用较弱固定环境光与已有可用灯具。主光方向是美术参数，没有依据日历转动的天体轨道。全龄成人身体、来源证明、水平worldScale和量化TerrainProfile不随环境Age改变；有效高度/海陆按[ADR 0012](../decisions/0012-editable-3d-pixel-world.md)可由地形工具改变。明暗切换本身不改高度、材料、生态、水深或建筑基础，不会产生DEM或新的通行面。本规则不附赠光照范围之外的新生存、战争或宗教机制。

## 2. 有界权威数据与不变量

生产字段名可与Schema统一，语义必须保持。所有字段在一个World已提交revision中保存，UI不保有第二份自动计时器。

|字段|语义/边界|
|---|---|
|`environmentAge`|枚举Light或Darkness，禁止任意字符串或第三环境|
|`ageEnteredTick`|当前明暗实际发生改变的tick，≤当前SimTick；重新开启自动不伪造一次AgeChanged|
|`automatic`|bool，默认false；手动选择Age会在同一事务设false|
|`lightDurationDays/darknessDurationDays`|整数1—3600，默认各30，作为调参初值；不得浮点、负数、0或无限|
|`autoCycleStartedTick`|auto开启时当前计时段起点，手动时不存在；与ageEnteredTick分开|
|`nextTransitionTick`|auto时为cycleStart + 当前环境durationDays×ticksPerDay，手动时不存在|
|`scheduleGeneration`|修改自动/时长/手动环境时更新，到期任务仅在其generation与World相同才有效|
|`environmentRuleRevision/schemaVersion`|预览/命令的expected版本、可追溯迁移与定义边界|

`ticksPerDay`沿用世界冻结的TimeConfig；存档/版本升级不能重新解释日长。duration乘法、deadline加法与generation递增使用受限整数和checked arithmetic，溢出拒绝而不是绕回到过去；到达支持的最大模拟时间边界时暂停推进并给明确原因。创建时不得把先前世界环境或用户菜单偏好带入新世界；允许以后增加明确创建参数，但默认Light/manual必须能追溯。

存档在完整提交屏障取状态。若automatic，已提交checkpoint要求nextTransitionTick>SimTick、cycleStart≤SimTick和相等的公式；若manual，两项auto计时字段均为空。环境transition在deadline被处理后才发布该tick快照，不输出“已逾期却未切换”的完整档。仅看最后一个环境枚举无法恢复余量，必须存计时锚点和deadline。

## 3. 玩家操作是一套原子状态机

入口放在游戏内底部“世界”分区的环境面板；世界规则窗口可跳至同一面板。不是顶栏第二套全局控制。面板给当前Age、自动/手动、剩余游戏日、两种时长与必要三语说明，采用无边框文字按钮或确实帮助功能辨识的图标；文字不强制附图标，禁止不必要小字，不以太阳角度表盘暗示天文模拟。主菜单纯文字布局按[ADR 0009](../decisions/0009-monumental-minimal-interface.md)，没有World时不提交世界环境命令。

|操作|原子结果|时长/历史语义|
|---|---|---|
|手动选Light或Darkness|设相应Age、automatic=false、清auto计时、更新generation|Age真正变了才写AgeChanged/ageEnteredTick；关闭自动另记规则变化；已是同Age/manual则无额外世界差异|
|开启自动|保留Age，automatic=true，从当前tick按该Age完整时长设cycleStart/deadline，更新generation|不立即换Age，不把以前手动停留时间算作已用自动时段|
|关闭自动|保留Age和ageEnteredTick，automatic=false，清计时，更新generation|当前明暗持续；再次开启从完整时段起算，UI明确说明|
|修改两种时长|先校验两个有界整数，再同一事务更新；auto时从当前tick重启当前Age完整时段，manual时只存配置|没有半个合法字段写入；不通过缩短值制造立即到期或跳Age|
|自动到期|仅有效generation且deadline准确命中：Age翻转一次，ageEnteredTick/cycleStart=deadline，下一deadline=本次deadline+新Age时长|以真实deadline作锚点，不能用迟到worker完成时刻或当前帧时间累计漂移|

启用自动、修改时长和手动环境使用带commandId/expectedRevision的typed命令与明确回执。重复commandId返回已有结果，不再次重启计时；两个基于同旧revision的编辑只有首个可批准，后一个刷新草稿/解释冲突。开关界面等待已提交结果，不先把假定批准值显示成真实World。

**同tick竞态顺序固定。** tick T 的已获准外部环境命令按既有稳定命令排序先提交，再处理T上仍有效的环境到期任务，之后其他消费者使用这一环境状态。手动选择与旧到期同tick时，手动事务更新generation使旧任务无效，不先暗再亮或反之；修改时长/重开自动同理。不以线程完成顺序、鼠标帧、窗口焦点或locale决定胜者。同一批多个命令按commandId/issuer序号既定排序和revision复核，不能由UI客户端自行决定结果。

暂停期间玩家仍可提交合法环境规则编辑，SimTick不推进；开启auto设置当前tick起的完整时段，直到恢复推进才耗时。明暗显示对新的已提交Age响应，暂停不能让手动选中的环境完全不可见。

## 4. 暂停、加速、快进与历史

自动周期以SimTick驱动，剩余量为`nextTransitionTick-SimTick`；禁止`Date.now()`、系统时区或每帧减真实秒。菜单、后台等待和离线不补算。全局应用暂停/回到菜单冻结模拟；自动保持当前phase与原deadline。正常推进和加速以相同截止事件处理，资源预算不足时降低可实现加速，不跳过明暗之间的真实劳动/供给事件。

若一次advance跨多个环境deadline，必须以每个截止点拆分时间段，并按顺序处理各段已有消费者和AgeChanged。不能只用终点奇偶计算最终Age后漏掉中间灯具用量、历史或已接入的能力变化。最低1游戏日与受限tick推进预算限制事件速率；不存在0tick自动环。不要求每次自动切换都渲染一帧，表现可直接绘制最后已提交状态，权威事件及期间资源不能丢。

事件至少含worldId、source/manual-or-auto、old/new Age或旧/新配置、effectiveTick、revision、generation、commandId（自动为可信scheduler事件ID）。AgeChanged只记录明暗真实改变；重新开始计时用配置/计时重置事件，防止历史把一次开自动误认为进入新的文明时代。完整历史保存和查询按已有分段/分页预算，不能每帧扫描全部切换。

## 5. 存载、迁移与会话隔离

首空世界checkpoint写Light/manual、默认duration配置、初始ageEnteredTick与环境schema。生产阶段2保存当前环境、规则与精确计时；按已有checkpoint协议校验并durable提交。profile只含面板布局、相机和纯表现偏好，不存第二份World Age。语言切换只换标签；暗/亮模式不是应用主题开关。

载入同WorldId保留Age/自动/时长/anchor/deadline，分配新SessionGeneration，重建一次匹配scheduleGeneration的到期任务。若统一scheduler档保存相同任务，以稳定task ID去重；禁止一份存档任务加一份环境模块新任务导致双切。保存到期前最后一个tick与载入后推进一tick必须和连续运行相同；“恢复剩余30天”不应每次载入重置成30天。

旧生产存档没有环境段时，迁移到**Light/manual，两个默认duration，ageEnteredTick=原SimTick**，显式报告“新增环境规则；未推断旧日夜”。原SimTick/TimeConfig/年龄、地理、库存与定义不重写，不反推太阳相位、不补历史用灯成本；原件保留，派生新schema档通过全部校验后才使用。已有oracle/浏览器fixture不是待迁移的完整World。新schema缺必填字段、非法时长或不一致deadline属于损坏/不兼容，拒绝并保留旧世界/原档，不能自动静默回Light。

环境查询、UI提交、GPU资源与过渡回调绑定worldId+SessionGeneration+环境revision。切World或重载同World后晚到回调全部拒绝。油画主页面独立时钟独立资源，世界自动周期不因进入菜单动画而继续。

## 6. 黑暗、灯具与实际因果

环境快照提供当前Age及其固定光照profile；渲染不能根据镜头高度、HDR自动曝光或图像亮度修改World Age。Darkness压低天空、地表和水体照度，但有受控最低环境照明，让地形、入口、可选对象与导航意图仍可观察。UI在世界后处理之后绘制，文字/图标不跟世界变黑；降低画质不影响逻辑Age或合法灯具可用性。

后续真实灯具至少分别保存“对象存在/拥有者/安装位置/合法状态/用户或系统开关请求”与已定义供给能力的可用结果。Darkness产生点亮请求，Light默认关闭装饰性自动照明；实际点亮必须已有对象且符合该灯具采用的能力合同，不发灯具或能源。emissive外观、局部light代理和真实功能分别报告：代理被LOD剔除不表示灯已关，也不能以材质发光制造库存/电量/劳动收益。相应灯具供给域未定义时，只制作开发场景表现，不伪称生产电网或真实燃料结算。

太阳能民用交通等原有技术能力继续按其业务合同执行；涉及环境输入的能力须显式读环境快照，Darkness不因视觉补光获得太阳入射。实际能量与存储接口在车辆/设施业务阶段定义，本文不扩张新电网、强制车辆燃料或停工机制。Age的两种名称不自动改变劳动、作物、生育、衰老、饥饿、信仰和战争；若未来需要实际影响，要单独给可解释领域合同，不能藏在shader或灯光开关。

表现短暂交叠只在两个固定profile间过渡，没有日出轨迹或实时天文路径。它响应最终已提交Age，快进时可丢中间视觉帧；载入直接使用当前状态，不补播全部历史。短过渡时长为纯表现参数，不延迟World事件和已定义供给；降低动态使用直接切换。主菜单独立保持小中央亮焦点/大部分渐暗的油画构图，与Light世界共存，不按World相位全幅压暗或补亮。

## 7. 原生基础与后续灯具验收

|场景|必须证明|
|---|---|
|创建空World|Light/manual，0人0楼，无虚构灯/供给，日历仍正常|
|Light→Darkness→Light，暂停中手动切换|typed提交一致；自动原子关闭，pause不挡新状态展示，年龄/地理/locale未改|
|开启auto/修改时长/关闭再开|按当前Age完整duration重启；不伪造AgeChanged；有界参数拒绝无半写|
|到期同tick手动/旧worker/重复commandId|generation拒旧任务，一次批准、无双切/重复历史或重启|
|暂停后恢复/正常与加速/跨多deadline|deadline余量、期间事实、终点hash与事件时间相同，不漏段不受帧率影响|
|deadline前一tick保存→载入→推进/反复载入|连续对照完全一致；旧Session回调拒绝；到期任务只一份|
|旧档迁移/新schema缺字段/坏时长/溢出|有报告派生版本，原件与当前World保留；新损坏不静默修复|
|两个profile与世界UI/主菜单|Darkness可辨地形与入口，UI对比独立；主菜单不会随World变亮；降低动态可关闭持续运动|
|阶段3—4真实灯具/LOD/保存|已有灯具按能力真实可用；丢light代理不改功能，没有供给不能假开灯|

阶段0验收油画与按钮，阶段1验收两个固定环境，阶段2闭合规则/时钟/存载/失败和三语入口，再允许阶段3资产工作。灯具资产/能力在阶段3—4接入。上述表是施工门槛；只有实际运行对应代码和设备/存档场景后，VALIDATION才能标已测试。
