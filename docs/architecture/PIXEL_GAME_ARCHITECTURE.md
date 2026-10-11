# 重置后自研程序结构与共享机制

状态：设计，未创建这些生产目录/目标，旧程序不恢复；ADR0015/0016/0017保留，ADR0018/v0.9冻结固定八档地形、全域tick和存档。总规则见[v0.9](../design/Sonnheide_Design_v0.9_Executable_Rules.md)。

## 1. 真正需要先突破的边界

|边界|不能靠什么蒙混|必须交付的事实|
|---|---|---|
|同一三维表面|2D画面/可走颜色/mesh单独坡面|平顶与ramp_patch同一权威几何供碰撞、拾取、水网、导航、建址、温度|
|地形破坏的远后果|只更新dirty chunk|通路断开能影响远港/城市供粮；同次提交撤证/停止任务/记物损且无半状态|
|遗传、生理与库存|耐寒耐饿标签/全参数拉满|species固定包络、genome有来源、绝对能量/伤害连续；食物产出有实际摄入上限|
|启智身份连续|生成复制人或瞬间“加文化”|同ActorId、身体与年龄，产权转换/任务释放/亲代意识来源同提交|
|人口与社会创生|自刷工人、动物代替人类创生|所有新生来源可追溯；人类之外不得发起文化/语言Genesis|
|社会与产权|所有关系用一个owner|国家主权/行政/占领、个人臣籍/文化、所有者/控制/保管/空间各自一致|
|批准、保存与异步|先换World再保存/晚候选覆盖|候选隔离，首档读回才发布，三generation隔离，旧Continue保留|

## 2. 目标目录及依赖

下表是施工时的目标，不先创建空目录或空类。每模块必须随对应贯穿功能落地。

```text
src/
  foundation/       IDs、整数时间/账额、Result、确定性RNG、量纲
  world/            单写者World、事务、事件、版本、实体寿命
  terrain/          固定8档column/ramp_patch、材质、无身份湿导航拓扑、矩形边界
  spatial/          body/collider、容器、通路/支承证书、步行/车/水/飞行
  life/             species、subspecies、genome、physiology、reproduction
  animals/          行为、觅食/捕食、照料/畜产、启智转换
  ecology/          tree/fruit、ForagePatch、授粉、农业与恢复
  society/          Family/Household/Dynasty、Culture/Language/Knowledge
  government/       State/City、法域、王冠/摄政/选举、边界与成员
  religion/         根/教派/章程、信仰、先知、教会任务
  economy/          ItemBatch/Assets、双账、生产线、合同/税/企业
  buildings/        原创定义、site/access联合批准、前侧送货/施工状态
  transport/        车辆/船、Shipment、人货装卸与真实路径
  military/         单层Army、阵位/战斗/补给、战争/盟约/Plot
  query/            一致快照、typed页面、统计/历史、后果预览
  application/      菜单/创建/进入/退出、控制租约、UI草案与存载
  renderer/         像素网格、实例/骨架/fit、天空水光、表现场景
  platform/         SDL候选窗口输入、GPU适配、文件与音频
  ui/               八底栏、对象窗、关系历史、zh-CN/en/de
content/            经验证的当前定义包与美术runtime包（施工后创建）
tools/              3.9+离线导入/烘焙/校验/统一构建（施工后创建）
tests/              真实不变量、贯穿场景、故障注入与性能（随功能创建）
```

核心只依赖foundation与不可变定义，不能include平台/窗口/GPU/UI头。query只读已完成revision；application不直接改Actor、钱或height。renderer/spatial都消费同一表面事实，空间派生不能反写terrain。工具离线烘焙可用独立开源库，任何额外依赖必须记录架构理由/官方来源/版本/许可/hash/可分发原件；ADR0018已重新读取官方固定commit许可证并决定复用原锁SDL3.2.28/bgfx/RmlUi5.1/FreeType2.13.3和wrapper/bx/bimg的七项版本/hash；只复用独立库和原件，不恢复旧程序。新生产链接/Windows GPU仍未验证。

## 3. ID、身体与空间

`ObjectRef={WorldId, kind, stableId, generation}`，墓碑/档案不可再分配相同身份；显示名不作外键。Actor共享物理生命记录，Human/Animal speciesRef不可在线换。CivicAgent是同Actor的能力/法律组成，不是第二个人口实体。每种固定成人身体profile、碰撞/fit/nav等所有派生引用相同BodyProfileId。

位置只能是一个：WorldPose / ContainerRef+Slot / InTransit(承运体/路线段) / DeadArchive。Building内工作容器、船/车/矿场和巢均有实际容量/支承/父容器无环；不可同时地图上走路又船内运输。人、动物、树、货有不同经济属性但共用空间/身份协议；装饰花草不是实体，也不进入导航。

裸体身体质量用于碰撞/搬运和代谢profile；可回收肉骨皮/脂肪或毛乳蛋reserve是独立真实产出账，不随成人体规自动填满。玩家放置的初始维持能量有一次有界来源，不能转化为商品收益。启智后同物种身体不变，不依据政治公职临时加人手。

## 4. 单写者事务与版本化候选

共用信封：commandId/actorPermission/WorldId/SessionGeneration/DraftGeneration/expectedWorldRevision/targetRefs/readSet/writeSet/dependencies/idempotencyKey。prepare不改World，只生成候选/资源预约方案；validate重新检查最新地形/物种/能力/财产权/法域/路线/存活；preview必须来自同候选，记录每个实体、数量、证书、风险与拒绝原因。

提交重新核验readSet/权限/候选hash，原子发布写集与一次events。大地形笔触在提交前完整准备，不能边准备边对World分块半提交；资源不足则限制一笔大小/要求分为明确独立笔触，不伪称跨百万格事务已做到。不可用“最后一笔获胜”吞掉旧债务或被破坏货物。清理、启智、王位空缺、疆界改动、工资、果实采收采用相同屏障。

撤销只支持尚未提交草案或明确可安全逆转的几何变化；绝不倒转已经发生的死亡、摄入、生产、付款、出生、启智、继位和物损。已提交地形恢复作为新事务，使用新的后果预览，不能回写历史。

## 5. 路径证书与地形热状态

AccessCertificate记录端点/用途/运动profile、表面/占用/路权版本及走廊依赖；人物/车库/送货/港口各自证书，普通建设阻塞旧证书即拒绝。Godterrain可撤证：先列受影响关系，再按实际物理后果提交，远离编辑格但依赖海峡的Shipment也必须失效。依赖索引按nav区域边/海水连通边维护，不扫描所有城市每笔重建。

水/高峰精确裁切坡面；局部mesh可异步，但提交后碰撞/拾取必须立即读取新revision，旧mesh可短暂停留只作表现且编辑交互不能假命中新height。温度以实际Actor位置/支承高程采样，室内/衣物保护有真实条件；镜头不参与生理。路径缓存key包含移动能力revision，因此改亚种能力不能继续使用旧快速路线。

## 6. 仿真时钟、事件与预算

高频运动/接触、较低频体温/能量/行为、每日实际生产/腐坏、周期工资税/项目、低频文化场/语言分化分别调度，但由同一整数tick最终提交。必须记录当期任务和积累余数：量化不能通过频繁载入或暂停多获果/乳/薪资。到期队列使用公平工作配额，落后有可解释积压而不假称全部完成。

首样时间明确分单位：20 SimTick为1个运动SimSecond，12000 tick为1游戏日，500tick为1日历小时，30日/月、12月/年（360日）。两者是固定profile转换，移动m/SimSecond不是m/日历秒；生理小时、食物、睡眠、孕期、工资与年龄按日历单位转tick，禁止消费者混用。倍率只改墙上时间推进，不改一天需要多少食物或一米多少劳动。路径距离/劳动/食物与战斗重装时长必须联测后校准此首样，保存不变，改profile须明确迁移。

动物觅食/捕食用局部空间索引与限频决策；高频移动也按真实通路推进。蜂群授粉按群/服务区有界事件；寒冷检测用活跃Actor批次，不每帧为每棵decor采气象。人类GenesisNode有限聚落级，智慧动物参与接触但不当创生发起计数；没有每格×每文化矩阵。账本不逐tick复制，历史采样单独低频且可标缺口。

## 7. 页面、草案与控制的统一协议

query按ObjectKind产生支持三语的字段metadata、关系Ref、允许编辑范围和当前权限原因，UI不猜写权限。EditDraft保targetRef/version、原值、typed patch、依赖版本、验证结果和本地locale无关的值。跳页/改语言/另选对象提示草案存在并可回到草案，绝不自动保存；关系返回历史保世界/查询generation，晚返回不覆盖当前页。

全球工具只进八底栏；对象局部菜单使用相同CommandId，不隐藏第二种写入通道。控制租约是`who controls which existing Actor set until when`，民生/紧急行为和完全手动选择边界明确；ControlGroup快捷键只是已存在Ref集合，不生成Actor。Fog是选定主体的知情观察，不是仿真裁剪。

## 8. 存档和发布

保存权威实体/绝对批次与账/亲代意识来源/峰水坡面/规则/草案以外任务/继任/随机与到期余量；mesh、LOD、装饰cache可重建。新SonnSave1的120byte header/64byte段表、规范JSON整数payload、段hash与definition/asset hash分开，加载固定包与species版本，未经迁移不可静默改包络。checkpoint写临时→flush→原子替换→读回核验→发布pointer，失败不覆盖旧有效存档。

本地多槽/自动滚动档/安全导出包；不做Workshop/Cloud、任何远程账号或隐式上传。新档和旧源码时代档不同，不承诺旧游戏运行档自动兼容。本机缓存/个人档/凭证不入Git。已发布Git历史不重写/强推。

## 9. 原创资产工作顺序与合同

按[新施工计划](../planning/IMPLEMENTATION_PLAN.md)，S00锁米制尺寸/枢轴/入口/送货区/碰撞/净空/衣层/材质像素密度/LOD/TreeRef果位/灯预算，主菜单已有hash源复用。S01–S02基础地表/创建/应用/存载成立后，S03立刻做正式代表件：一男一女固定体规、兔/牛/狼/鱼/蜂群、基础庇护及住宅/广场/工坊/衣坊/港口、八主题代表树果、衣物/正装/军装fit、樱瓣/圣树光/神光。模型制作不授予建筑功能/科技或实物衣物；其余资产随S04–S10持续扩产，不在单个阶段等待全美术齐备。

资产作者源+生成配方+manifest+版本/hash+许可进入Git/GitLFS/Releases；原创不默认开源。技术衣层可共用但不要为每位Actor复制mesh/material。地形纹理原创像素可拼块且基于世界坐标，不用已废弃的写实PBR接入制造“材质加载完成”假证据。天空/字体候选先许可核验再入正式包，旧MakeHuman与灰盒留历史来源，不能混入新asset registry。

## 10. 验证与发行边界

新工程实际存在后，统一构建一次全部targets+完整CTest；改变事务/空间/存档再统一sanitizer。文档本轮只核对合同/来源/引用/跨模块语义，不运行不存在的旧构建器。测100/1000/10000人及实际动物/经济/战争/渲染，记录设备/场景/世界规模/平均与P95/P99帧时/仿真积压/内存/保存恢复；没有测量不承诺大世界容量。

Steam唯一发行；Windows GPU/Steam安装、控制器/键鼠/DPI/音频/存档实机证据独立，Metal仅开发验证。全程序/必需可分发资产托管GitHub；许可证原件与版本/hash锁定，不用CI/NullRenderer替代实机三维试玩。

## 12. ADR0015跨域算法边界（ADR0018固定类型覆盖旧自由高度）

政府模块的NormalizeSettlement读取候选最终几何/活成员/实际服务，输出双轴状态、有效域、行政权限与State/职位后果；不另作人口删除器。City是同Settlement的CITY章程资格。TerritorialTitle与effective dry index分离，CitySquareAccount没有物理设施时容量0。growth/newcity/Reestablish使用真实TaskPlan/FoundingParty与经济预约，动作执行器推进实际路程和劳动。

行为与运动由life/animals/government任务源共同调用natural_action：感知/目标/承诺/任务、分层路径、有限加速度、局部建议、最终3D扫掠和窄口token由权威组件负责；renderer只做原创clip/脚IK/微动，不用Notify发经济提交。BodyProfile/SurfaceRef跨层同源，异步旧foot/grip/path目标在publication barrier失效。

application只产生God typed proposals；地理换类先冻结活Actor真实穿戴/携带递归保护，再从精确ChangedGeographyFootprint的support/container毁损闭包扣除；活Actor/Gestation及随身物保pose/绑定/绝对状态后结真实hazard，继任/成员/State在最终候选一次规范化。特殊God立国/改籍/转城不伪造普通支持票或资产转让。命令有唯一civic registry和独占lease，selection不是World变更。

生态生成使用独立EditorInputClock规范化剂量，不吃模拟RNG；有限GodVegetationSource记录Plant生物量而不写仓库，固定窗口/保守冠多边形/成年包络/idle EscapeConnection防堵。生成器不能按renderer消失树数补种，wipe/清理/采伐的dose epoch失效同写集。细节见[城市](SETTLEMENT_LIFECYCLE_AND_EXPANSION.md)、[动作](NATURAL_BEHAVIOR_AND_LOCOMOTION.md)、[God控制](GOD_CONTROL_AND_GEOGRAPHY_MUTATIONS.md)、[植被](VEGETATION_GENERATOR_AND_DENSITY.md)。

## 13. ADR0016决策边界

新增decision模块随真实行为功能落地：纯提案读取角色可知BeliefSnapshot/PersonalitySnapshot，不能持有World私密全图接口；政治/经济/军事/社会保独立reasoner，输出typed CandidatePlan与真实claims。world事务只做实际守恒/寿命/权限核验，不替人物免费侦察未来；受限失败结果与合法接触证据严格分开。公共ActorAvailability/库存资金预约跨所有domain，query的God真视图不注入AI。

life唯一写CurrentPersonality和长期经历额度，genome潜在倾向与当前人格分离；事件/消息给实际知情者，政府议事与Army命令各有真实传递和执行时间。DecisionEpisode/Commitment/OutcomeAttribution保存逻辑工作与证据/一次effect，不用worker墙钟或UI选择决定World。实现合同见[共享核心](DECISION_CORE_PERSONALITY_AND_INFORMATION.md)及四领域文档，不先创建空decision框架。

## 14. ADR0018可执行规则边界

flat terrainKind对应唯一−20/−8/−2/+1/+2/+16/+48/+96m，sand/soil互转真实wipe；仅CreateRamp拥有连续四角面。升降只相邻档、平整指定档、复制仅flat；无自由高度、smooth、自然边缘降高/不规则海岸。Blank/Earth用同候选，Earth冻结等距圆柱/微度/日期线/中心even-odd层级，8格deep-ocean guard之外纯表现；矩形裁切可见，不暗补坡岸。没有山海湖自然对象/命名/WaterBodyRef，水网仅当前revision可重建索引，不入实体存档。

World root按统一FREEZE→旧生理工作一次积分→due死亡标记→边界命令→真实消息/截止冻结→同snapshot运动攻击→同瞬间接触聚合→真实交接→到期执行→唯一死亡遗产/最终城市国家王冠→一次publish。暂停编辑只同tick boundaryOrdinal递增，不赠休息/劳动；inputSequence由可信dispatcher分配，worker速度不抢顺序。完整规则与实际wire字段见[确定性世界](DETERMINISTIC_WORLD_AND_RUNTIME.md)和[runtime基础合同](../../data/contracts/runtime_foundation_v1.json)。life_profiles唯一伤势函数，runtime/spatial只供HazardContactFacts/InjuryImpulse；同一健康/死亡不能两模块写。

本轮只有文档、机器定义与有限Python3.9模型，不创建空src或虚假试玩。所有本批修改结束由统一审计验算；生产阶段仍全部未开始。
