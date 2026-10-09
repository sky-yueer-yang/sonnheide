# 地皮、装饰植被、功能树与果实闭环

状态：用户最新地表规则的生产设计，2026-10-08。依据 [v0.7](../design/Sonnheide_Design_v0.7_Pixel_World.md) 和 [地形事务](PIXEL_TERRAIN_TRANSACTIONS.md)；本文件覆盖此前“十类 biome”中的地表配对，原稿保持不变。这里没有交付树木、果实、恢复、采摘、砍伐或存档代码；所有配置示例都是游戏校准值，不是对现实植物、生长季或营养的生物学断言。

核心结构固定为三层：**地皮、装饰小植物、功能树**。装饰层丰富画面，真实树承担年龄、果实与木材。普通农业仍由独立农田/作物/劳动系统产生粮食，不从草、花、芦苇或 biome 色彩提取食品。

## 1. 用户配对与有效生境

### 1.1 八个主题与沙材质覆盖

|主题/生境|地皮|装饰小植物|功能树与特殊表现|
|---|---|---|---|
|snowfield / 雪原|原创雪原地皮/覆盖|用户未指定，制作配置可稀疏或为空|松树 `pine`|
|flower_meadow / 花甸|原创花甸地皮|多样花草，含用户要求的热带花|多种果树；首样本池为苹果、梨、桃、橙、芒果、石榴|
|maple_field / 枫原|原创枫原地皮|红色落叶与有界低草|枫树 `maple`，叶片及落叶全为红色系，仅分深浅红|
|cherry_field / 樱野|原创樱野地皮|仅深绿草|樱花树 `cherry_blossom`，零星落瓣|
|wetland / 湿地|原创湿地地皮|芦苇|菩提树 `bodhi`|
|savanna / 荒原|原创荒原地皮|枯草|胡杨 `huyang_poplar`|
|sonnheide_sacred / Sonnheide 圣原|原创圣原地皮|制作配置另定，不产生资源|原创圣树 `sacred_tree`，树体发光|
|volcanic / 炎地|原创火山表面、裂缝|为空|枯树配置 `dead_tree`；可砍实际余木，不生长/结果；静态少量熔岩|
|sand / 沙生境|沙材质优先|**没有装饰小植物**|椰子树 `coconut_palm`；周期长熟椰子，可采食|

八个主题对应八个 biome ID；sand 是 substrate habitat 单列，共九个 surface profile，不强行作为第九主题。中性裸泥地 `biome=None` 作为编辑态保留，不增加新主题。水果池的六种选择属于当前首样本，不增加六套经济行业或六套科技树。

雪原松树、湿地菩提树、荒原胡杨及各配对是用户设定的游戏世界。不能以真实生态分布纠正这些配对，也不能借主题名称自动引入气候学、季节、病害、动物、魔法或随机灾害。

### 1.2 先看地形，再看沙材质与主题

```text
physicalSurface = 当前权威 heightQ / waterLevel / collision

if topMaterial == SAND:
    effectiveSurfaceProfile = sand
else if biomeId != None:
    effectiveSurfaceProfile = biomeId
else:
    effectiveSurfaceProfile = neutral_soil_or_rock

treeEligibility = profile species/configuration pool
                ∩ 当前创建来源的活树/枯树资格
                ∩ 实际干湿/水深/支承允许条件
                ∩ 地权/土地用途/净空/密度/既有通道条件
```

物理 WATER 不因涂湿地、炎地或沙色变成可站立陆地；新椰树也不能仅因 SAND 在深水中生成。湿地的芦苇和树必须分别按自己的表现/真实树适用范围处理，不通过装饰反推船舶通航。炎地熔岩首先仅在适用干燥火山地皮上显示，不升高地面、不填海。

首样本所有真实树按陆生根支承 profile 准入，新树根位必须是实际干燥 LAND；湿地主题可显示湿润风格，不赋予菩提树水上支承。已有树被淹则进入明确损伤/容忍时间后果，不能因旧 habitat 标签继续认可新树种植。

沙材质优先影响当前地皮、装饰和**新树选择**；底下已有 biomeId 不删除。涂回 SOIL 时重新露出原主题的地皮/装饰和未来生成规则，但不会复活被清除的树或生成成年树林。沙地上已有松树仍是原松树，不能换成椰子树再给一套新储量。

主题切换默认不替换已有真实树。需要立即换树群时，提供明确的清理/重植组合预览：旧树、未采果、预约、损失/可回收安排与新幼树分别登记。不得把“换地表皮肤”变成不付代价的树种、年龄和资源重置。

## 2. 三层各自拥有什么事实

|层|数据与身份|表现/玩法|绝对禁止|
|---|---|---|---|
|地皮|当前列 heightQ/topMaterial，独立 biome/肥力/湿度/cover 与版本|真实地面及原创像素材质，影响明确的适宜性/行动 profile|颜色直接生成食物、木材、矿物或人口|
|装饰小植物|持久 recipe/密度/稳定 Chunk seed/clear mask 与表现版本；无独立资源实体|花、草、芦苇、枯草等小植物的视觉分布与有限风动|库存、可采量、碰撞、导航阻挡、射击遮挡、劳动任务、产权货物|
|功能树|稳定 TreeRef（现有 PlantRef 的 tree 子类语义别名）+ TreeComponent；所有权/位置/年龄/状态/储量/果批次/预约|真实可砍树、可采食果树、树干功能代理、生命周期|按 LOD 新建树、换皮补果、把树木直接计为仓库货物|

所有树都是功能树，即使暂时没有果实或可采木量。远景树/HLOD 是这些树的代理，不是另一片可以重复采伐的森林。装饰花草没有收割动作；玩家点它们返回 TerrainCell/生境信息，不产生可转卖的 Plant 或“草料”库存。农田中的真实作物属于独立农业实体/聚合记录，不能使用装饰草花资产的存在判定粮食产量。

功能树也不是一种新 EconomicActor。树的资源权利来自实际所有者/用地/合同，不能把“林地位于某国”理解成树和果实自动归国库。托管者、作业者、土地所有者、树所有者与采收权可不同。

## 3. 有界定义与树实例

下列类型是目标合同，不是已存在的 C++ API。具体字段由生产 schema 对齐现有 Plant kind，避免再创建一套平行 Tree 身份系统。

`TreeRef` 映射 `PlantRef(kind=plant, subtype=tree)`；不新增 entity kind、目录计数或独立 ID 分配器。树种 ID 固定为 `pine / apple_tree / pear_tree / peach_tree / orange_tree / mango_tree / pomegranate_tree / maple / cherry_blossom / bodhi / huyang_poplar / sacred_tree / coconut_palm / dead_tree`；食物变体 `coconut` 与树种 `coconut_palm` 分开。

```text
SurfaceEcologyDefinition
  surfaceProfileId, biomeIdOrSubstrateRule
  groundRecipeId
  decorationPool[], decorationDensity, decorationSeedVersion
  defaultTreeSpeciesPool[]
  treeDensityLimit, recruitmentRateLimit, recruitmentDelayTicks
  physicalEligibilityProfile, landUseExclusions
  definitionHash

TreeSpeciesDefinition
  speciesId, originalBodyAssetId, growthAppearanceProfiles
  physicalContactProfile, matureEnvelope, interactionSockets
  habitatToleranceProfile, growthRateProfile
  maxStandingWood, woodRecoveryProfile, fellingWorkProfile
  fruitProfile?                 // 本轮只有花甸六类和 coconut_palm 默认非空
  glowPresentationProfile?      // sacred_tree
  petalPresentationProfile?     // cherry_blossom
  definitionHash

TreeComponent
  plantId, speciesId, plantedOrInitialTick, effectiveAgeTicks
  creationSource, creationEventId, legalOwner, resourceRights
  location, supportBinding, landUseBinding
  lifeState, workState, habitatState, damageState, revision
  standingWood, woodGrowthRemainder, lastAdvancedTick
  growthStage, growthCredit, nextGrowthEventTick
  fruitCycleAnchor, cycleIndex, cycleBudgetUsed, fruitLots[]
  activeHarvestReservations[], exclusiveTreeUseReservation?
```

种类、初始来源和已发生的事件不通过通用改名/换材质编辑器覆写。阶段外观读取同一树的实际成长，不改变 TreeId。年龄是树的游戏年龄，不使用人物 aging 开关冻结植物；TimeConfig、植物生长规则和对应开关必须明确区分。

木量与年龄分别存在。一个成熟树模型不是固定满额木材凭证；受损或曾被处理的树不得因重新加载成熟 mesh 补满 standingWood。幼树可被砍/清理，但只能回收当时真实可采量，不能领取成年树配额。

树身、树干支承、交互区和成熟包络都受有限 profile 约束。根部站位、采摘/伐木等待点不从随机枝条 mesh 推断。首次实现用有界树干/必要树冠代理，不做逐枝生长、真实根系土壤求解或自由木材碎裂物理。

## 4. 生长与资源预算先守恒，再选择美术

树龄、生长 credit、木量及果实只随模拟时间/可信事件前进。镜头、RmlUi 信息页、风动、花瓣粒子、Light/Darkness、材质加载或 LOD 不发生成长。玩家关闭人物饥饿/衰老也不改树龄或果实鲜度。

木量可用有限整数/定点积分：在同一有效 habitat 区间，按明确速率增长，达到 maxStandingWood 后停止；超容量的潜在增长不储存成积压 credit。状态/适宜性变化前先更新到提交 tick，再应用新速率。死亡/砍伐不能把“最后一次积分时间”倒回去。

至少满足：

```text
木量：initialWood + actualWoodGrowth - removedForTimber - woodLoss
    = currentInPlaceWood

果实：actualFruitFormed - pickedFruit - fruitLoss
    = currentAttachedFruit

商品：openingGoods + actualHarvest/otherProduction - consumed - spoiled - lost
    = closingGoods
```

原位木量、附着果实与 Food/Wood 商品账不能相加为两份可用库存。采伐/采摘将原位量扣下，再按固定可回收/可食换算生成真实货物；残留、不可食部分和损失有明确分录，不自动产生新副产品行业。

God放置活树默认登记有限幼株，初始可采木/成熟果为零；成长后才形成原位资源。dead_tree例外从DEAD起始、木果0且不成长，具体见第15节。首版初始地图无需生成成年树林；如以后允许初始化不同年龄的自然树，必须另在新 World 最高预览中明确 INITIAL 来源与有界初始储量/果批次，不是在游戏内重涂后反复调用世界初始化器。

## 5. 果实周期：逐渐出现、长大、变色、成熟

### 5.1 果实是批次，不是每果一实体

每棵果树按自己的固定周期产生多个 formation slot，逐步出现未熟果；成熟阶段由实际成长积分决定。不同种类可有不同模型、初/熟色、周期与数量。六类花甸果树各自是一种树，一棵树不随机混出六种果实。椰树复用同一周期/采食协议，外形与食物换算由自己的 profile 指定。

```text
FruitProfile
  fruitVariantId, cyclePeriodTicks, formationSlotsPerCycle
  formationWindowTicks, maxFruitPerCycle, maxAttachedFruit
  growthRequiredCredit, growthRateProfile
  ripeLifetimeTicks, maxActiveLotsPerTree
  foodConversion, massVolumeProfile, harvestWorkPerQuantity
  unripeToRipeAppearance, visibleFruitInstanceLimit

FruitLot
  lotId = {TreeId, cycleIndex, formationSlotIndex}
  formedTick, quantity, growthCredit, growthRemainder
  lastAdvancedTick, matureTick?, expiryTick?
  state                       // UNRIPE | RIPE | RETIRED
  reservedQuantity, reservationRefs[], revision
```

lotId 是稳定原位批次引用，不为每个果子发 EntityId。FoodBatch 形成时保存 sourceLotId、原 formedTick/matureTick/expiry、数量/变体、owner/custodian/location 与食物 profile；它是采收后的真实货物，不与源 FruitLot 双计。

初样可先用固定数量的 formation slot，例如每个周期 6 次形成、最多 16 活跃 lot；具体时间/数量待生存经济校准。不同 slot 年龄不能为了省内存合并成一批“平均成熟度”，否则会提前成熟或延长保鲜。相同年龄/鲜度/物权/预约与 source 可合并的批次可按规范规则合并。

### 5.2 不把未发生增长攒成瞬间产量

每个 formation slot 有本周期数量预算，实际形成量受当时成长条件、maxAttachedFruit 和 lot 上限约束。果量或 lot 数达到 cap 时，形成信用与小数余数均不继续累计，lastAdvancedTick 仍推进；满容量区间形成贡献为零。进入满容量时丢弃未兑现的形成信用，之后只保留真正未满且正常增长区间的不足一单位余数，不能把多年满树期累计成 deferred fruit。容量释放只允许之后的有效时间形成新量，不补发满期间或错过 slot 的预算。

lot 上限不能通过合并不同 formed/mature/expiry tick 的 cohort 腾位置；否则会使未熟果提前成熟或使旧果延迟腐坏。已有不同批次继续占其槽直到真实消耗/损失/退休。合并仅在数量之外的成熟、鲜度、来源、权利与预约语义完全兼容时允许，且保留 source lot 的可追溯分配。

cycleAnchor/phase 由明确生命周期事实与固定 seed/counter 协议确定；重涂同主题、材质切换、换树皮肤、国家主题色、存载和点详情不改它。物理/生态变化可改变之后的生长条件，不能重新抽到更好果期或再执行本周期已用 slot。

肥力、湿度或种植能力改变**未来增长**。已有 FruitLot 保存绝对数量与成长 credit，禁止使用“成熟度 80% × 新的最大产量”直接放大现存果量。成熟模型和颜色也不能生成 quantity。恢复适宜性不会补发暂停期间错过的 formation slot。

### 5.3 更新至 tick 必须经过中间事件

`AdvanceTreeTo(T)` 在成熟树阶段、formation slot、果实成熟、鲜度到期、habitat/损伤变化等真实边界分段推进；在每个边界按固定规则和顺序结算。跨几十天直接只比较最终成熟度，会漏掉中间已成熟又腐失的果子，也会多发下一轮数量。

每 lot 的成熟 tick 是实际 credit 达到阈值的时刻；之后鲜度由绝对 matureTick/expiryTick 驱动。采摘可用谓词为 `matureTick <= T < expiryTick`，到期 tick 不再可采。更新到 T 先处理到期/损失，再分配当时可用量；预约不会延长鲜度。

同 tick 使用固定阶段顺序：先按旧条件积分到 T 并结算到期/自然损失；再按稳定命令序提交地形/清除等后果及树生命状态；随后各采果/伐木命令重验同一 tree revision 与互斥许可，最后分配该 tick 合法的新预约/形成事件。已在更早 tick 采下的货物不受源树后续命令回滚。同阶段以稳定事件/命令 ID 决胜，不由 worker、镜头或鼠标事件到达线程决定；在同 tick 被毁或到期的附着果不能抢跑领取。

时间边界不得执行两次：连续积分只处理上次已提交边界之后的区间；T上的到期/成熟、命令、formation各进入上述唯一阶段。AdvanceTreeTo(T)不得在旧条件下先形成T上的新果，再在最终formation阶段重复形成；每slot保已用标记、cycle/退休水位，T上的新形成只使用命令后的实际生境/存活/容量。预览仅在不可变快照上计算候选，不调用写World的advance。

长时间加速/离屏不跳过周期、重启不按电脑墙钟补果。若需要批处理许多无交互周期，只能用与逐事件执行等价的闭式/跳跃算法，并保留计数/损失/当前活跃 lot；不能用一条“树已满果”结果替代所有时间史。

### 5.4 图像读取实际成长

RenderSnapshot 按 TreeId/lot 提供有界 fruit visuals：未熟→长大→色彩渐变→成熟，数量大时用代表性实例/密度表达并有可见上限。画面中的代表果不等于逐个可点的真实 item；检查器显示绝对总量、成熟/未熟/预约与鲜度。

视觉进度可在两个已知模拟时刻间插值，不向 World 回写成熟。树上有果、篮子满了、果子换色或落下一个粒子都不能作为采收完成/进食事件。落瓣只属于樱花树的独立只读表现，不消耗或增加 FruitLot。

## 6. 采摘、装载、储存和食用

### 6.1 预约针对现存成熟量

采收任务预览实际树/lot、合法采收权、可达交互位置、人员与劳动、工具需求、载荷/目的容量、鲜度余量。初期公共/家户采集可以低效率进行，不以大学、私企或高级苗圃为基础食物门槛。

`availableRipe = existingRipe - activeReserved`。预约绑定 source lot 的明确数量、受益任务、货权条款、执行者与期限；预约只锁定这份原位量，不同时生成 FoodBatch。不把下一个周期预计果量作为当前库存或已收订单。未来采购可以有等待生产的合同节点，不能用预测数量支付完成验收。

已成熟果批次可能比路线的实际完成时间更早到期，预览应显示风险。不能为了下单自动延长 expiry 或赠送运输工具。许可/物权来源、身体安全与地点必须同时成立；神力全知看见树不代表普通居民已经取得采收权。

### 6.2 实际到树边并花劳动才采下

任务到真实交互点，逐个有界工作/装载步骤结算。每次提交先 `AdvanceTreeTo(T)`、核验树/lot/预约/权限/路线/人员/容量，再原子：

```text
sourceFruit quantity/reserved ↓
实际劳动与采收进度结算
FoodBatch quantity ↑，Location=采收者载荷或真实现场容器
owner/custodian 按合同确定
receipt/event/progress 一并发布
```

剩余采摘继续是同一真实任务。到期、人员死亡/移走、场所被淹或容量用完只能停止/取消未发生部分；已经采到篮里的果子不放回树，不因重试再生成一批。

无目的仓位时可等待或真实改道，但不能把树下抽象缓存做成无限仓库。运到广场/家庭/Army 容器按已有物流改变唯一 location。先装载后出发、验收到达后可用；场景中不渲染篮子也保留真实货物。

### 6.3 食品变体与鲜度

苹果、梨、桃、橙、芒果、石榴和椰子作为 `Food` 的有限 fruit variant/显示品类，复用现有需求、配给、货运、购买、税基与账本。不要新增七套不可替代的基本饥饿需求。种类的质量、可食比例、重量/体积、鲜度可以有定义差异，但不是外部真实营养声明。

数量换算用整数/定点和明确余数：扣除多少原位果、生成多少 Food、不可食/损失多少，不能分别四舍五入后重复增加供给。食品被吃时扣实际 batch；野树上未采的果不计家庭粮仓，不因经过树边自动饱腹。

采摘、换货主、运输、仓储、合批或重新加载保留原成熟年龄。第一版鲜度可统一沿用原成熟 tick 的到期 profile，避免在每次搬运时重置保鲜。合法处理/保存工艺若以后新增，需真实 recipe/投入/新产物来源，不能靠改 food variant 名字续命。

腐失按实际批次数量写 LOSS，已预约量同步失效，合同处理未交付部分。不存在自动把腐果转换成新肥料/种子商品的旁路；若将来添加副产品，另写配方。未采熟果到期默认退出附着库存并记自然脱落/腐失，粒子可表现，没有免费的落果 pickup 实体或双份食物。

## 7. 所有功能树可砍，采果与砍树不能互相复制

松、六类花甸果树、枫、樱花、菩提、胡杨、圣树和椰树都可砍；炎地枯树和已死树体若仍存在可用支承/可达作业空间，也可回收其实际余木，无需恢复生命状态。圣树无宗教豁免或无穷木量；砍掉圣树得到普通定义的木材，不自动获得发光燃料、魔法物品或免费照明设施。

TreeUse 规则第一版保持清楚：采摘可锁定有限 lot/作业容量；开始砍伐需要独占整棵树的使用许可。存在未完成采摘预约时，砍伐计划等待完成或经明确取消/退出后重验，不能一边锁果子一边领取整树木材。普通伐木不默认取消别人的客户果合同。

采摘与伐木复用 `HarvestPlant` 的 typed `mode=fruit / fell`，同树共享版本与 TreeUse 许可。多个采果任务可以预约互不重叠的真实 lot 数量，但 `fell` 与全部 `fruit` 使用互斥；毁树后果明确取消未发生工作，释放预约而不复制已采货物。

活树的workState进入FELLING作业后暂停新的果形成/采摘和正常成长；枯树只改变作业状态，lifeState保持DEAD且没有成长/果期。现有果鲜度继续推进，不能靠长期半砍冻结成熟果。取消伐木保留已发生劳动/损伤；恢复后沿旧 cycle/history 前进，不补冻结期间果期。

砍伐需真实人、合法权利、可达站位、劳动/工具 profile 与安全作业范围。完成时更新到 tick 并原子扣实际 inPlaceWood，按已批准回收率生成 WoodBatch 到有容量的现场/搬运位置，关闭树的生长/果周期、处理仍附着果实、转 STUMP/退休状态、更新碰撞/占用和历史。

伐木的附着果默认记真实损失，不自动生成与站立时等量的“采果奖金”。若玩家希望先摘后砍，采用有明确前置边的真实任务 DAG；其已采 Food 不再次包含在伐木输出。God 清除树不是伐木，默认核销剩余资源和取消未发生预约，不送 Wood/Food 到仓库。

stump 保留原树退役引用/来源，实际可回收木量不得再补。新树是新的 PlantId，不能从 stump 重置相同 ID 为年轻满木树。树桩清除/允许抽象自然退役有明确位置/时间/资源损失规则，不能不付劳动地当场再领一次伐木收益。

## 8. 换地皮、换主题、淹水和死亡

### 8.1 正常画生态不等于清树

改变 biome/material/肥力/湿度前，受影响真实树与果批次更新至同一提交 tick；已有绝对资源、种类/年龄/来源和预约保留。新条件作用于后续增长和生境状态。只换地皮艺术参数不对现有树发死亡或变种事件。

物理/生态容忍条件变化可使树 `SUITABLE → STRESSED → NON_GROWING → DEAD`，时间阈值和增长系数属于锁定游戏 profile。不能仅因为 maple_field 改名 flower_meadow 就立即处死所有旧枫树；需要具体基质、湿度、支承/水深等原因。反过来，不适宜树也不能无条件继续无限产果。

正常建址/道路工程若需树退出，加入有真实权利、砍伐/清场/运走安排的前置任务。临时改城市边界或所有者不会修改树的物理状态，不免费把土地上的树全部转企业库存。

### 8.2 真实地形后果继续按地形事务

抬地埋树、降低撤走根部支持、变海超过容忍水深等实际改变，按 [PIXEL_TERRAIN_TRANSACTIONS](PIXEL_TERRAIN_TRANSACTIONS.md) 的全因果事务转损伤/倒伏/不可用状态。无需逐枝刚体仿真；有限树干/根部代理和明确位置/损伤足以支持初版。

淹死/清除关闭未来木量增长与果形成，附着果按定义核销，不将其搬进仓库。剩余木量可以保存在真实残树/倒伏或沉没位置等待可达回收，或按明确损失比例核销；不能同时成为可回收原位量与已输出 WoodBatch。死亡后不再恢复成长/结果，重新造陆只恢复地点条件，不复活原树。

已经采下并在篮子/车/船/仓库里的 Food/Wood 保留其唯一位置，分别按地形/容器损伤和鲜度结算；不会因为原树死亡被回收回树，也不因为树得救恢复已腐食品。树/场地修复与货物恢复是不同流程。

重新刷适宜主题可让尚存且只是受胁迫的树恢复未来成长；死亡、砍伐、果腐失与已发生损伤不撤销。连续换皮/换沙土/沉海再露陆不能重置成长 anchor、木量、lotId 或周期 budget。

## 9. 种植、成长占用与城市安全

### 9.1 三种运行中新树来源

|来源|准入|起始状态|经济边界|
|---|---|---|---|
|divine 神力植树|可信玩家能力和当前 God rule、有限数量/范围、实际生境/站位/claims、明确预览|活树为幼株；dead_tree为DEAD；均无初始可采木果|树创建记录 divine 子来源，不伪装公共生产或已支付植树|
|civic 文明植树|真实公共/家户/林业 actor、苗种/传播来源、权利、劳动、真实路径与有限培育能力|仅活树幼株，不含dead_tree；记录实际投入|高级苗圃改善产率/规模，不阻断初期基础植树|
|wild 自然萌发|可信有界恢复调度器、当前恢复开关/rule revision、物理适宜性、空生长槽、密度/每日额度/冷却|仅活树幼株，不含dead_tree；无可采量|是明确自然成长来源，不生成商品、不占用人类劳动|

这三种是 `PlantVegetation`/可信恢复器的运行时 authority mode；树创建子来源不替换地形合同的 `INITIAL / DIVINE_EDIT / CIVIC_EARTHWORK` mutation provenance。INITIAL 仅属于新 World 一次性初始状态，首版不强加成年树目标；如另设初始化植被，保存实际状态/seed/来源且禁止游戏内重用初始化器。

文明繁殖来源可先用受限现存种植材料或已登记培育/繁殖能力与实际 labor 输入，不借画笔隐式补苗。若没有现有种苗商品定义，不为此擅造完整种子市场；生产采用的具体传播来源/预算须在 recipe 中明确，不能一面声称全部实物，一面凭空写一千个幼树。

默认/自然树种由有效生境池确定，新生境没有树池就不生成树；炎地树体配置为dead_tree，但其不进入自然萌发或文明苗种池。玩家明确选择非默认种类时仍需相应种植能力/授权、物理适用与完整风险预览，不能给普通施工隐藏的无限强制通过权限。

### 9.2 成熟包络必须在幼树时就检查

树干站位、成长所需净空、有限成熟包络、交互等待点以及必要资源采收通路，在植树批准时就检查。仅按幼苗小模型塞进车库口，会在成长后破坏已经承诺的通道。大树冠是否允许越过道路由明确 profile/净空规则决定，不因叶片风动或 LOD 改变。

LandUse 将自然地、农田、道路、建筑、已批准工地/通道、庭院等分别登记。自然树不会在路面、屋内、前侧接货区、泊位、农田或永久出入口中长出；植物恢复读当前 claims/用途与 terrain revision。已有合法庭院植树可允许，但不能用“绿化”绕过同一占用检查。

新建筑对已有真实树按法律/清场流程处理；新树成长不能自动摧毁先建房或推走汽车。后期普通建址与种植都保护旧必要通行。God 明确破坏可另走组合预览，保留权利、货物和历史，不隐藏在自然萌发中。

首版可采用保守有界 matureEnvelope/间距与树干代理；不要为多种树引入自由三维园艺编辑器、每片叶碰撞或全森林根系模拟。共享树冠/树干几何配方不改变实例的资源和所有权。

## 10. 有限自然恢复与开关的准确含义

`natural_vegetation_regrowth` 仅控制**新的野生树萌发/扩散**。关闭后不删除既有树，不冻结其年龄/木量/果实，不停止农田作物，不停神力/文明合法种植，不暂停鲜度/死亡或树桩退出。

打开只从当前 tick 继续以后合格的候选，不补发关闭期植物；排队的自然萌发在提交时检查当前 rule revision。不得根据关停期墙钟建立一片成年树林。装饰层按地皮/seed 生成，不依赖这个开关来复制经济树。

每生境区域使用有限招聘式恢复槽：当前真实树计数、密度上限、每游戏日最大新幼株、最后萌发/清除 tick、冷却、slotGeneration/counter、稳定候选序。候选只在生态/空位/规则变化或到期时唤醒，不每 tick 扫每个格和全部树。

清除/砍伐后需要实际槽空闲和恢复延迟；成熟占用仍存在的 stump 不立即提供新树空间。重复涂同主题、切沙土、刷新 UI 或恢复存档不重置 counter/冷却，也不重新抽整区候选。候选身份在合法提交时才成为 PlantId，失败不伪造另一棵存在的树。

有界密度还不够：幼株初始木/果必须为零，成熟需模拟时间，日萌发额度不积压。因此反复“恢复→砍幼树→重涂”不会得到每次一份成熟木材。真实经过许多周期后森林提供可再生木材是明确成长模型，不是免费瞬时生产。

自然萌发提交与人工植树争同一真实空位/claims；稳定 writer 顺序决定谁先占，后者重验而不是重叠长树。原生种类选择用独立 seed/counter，不消费人物/文化/战斗 RNG，不由 worker 先完成或渲染可见性决定。

## 11. 装饰、花瓣、圣树光与炎地熔岩

装饰层为读取持久 `recipe + density + Chunk seed + clear mask + revision` 的有界实例/纹理系统。清除小植被只更新 mask，不删除真实树；重载和缓存重建必须应用原 mask，不能把已清草花补回。主题切换可以明确改变装饰 recipe/密度，但默认保留清除 mask；若玩家主动重铺装饰，预览/提交显式重置选定 mask，不能以缓存失效代替该动作。换地皮可以立即改变小花、小草、芦苇、枯草数量/色彩；这些没有经济库存，所以无需假劳动结算。对采收动作、角色/车船通行、射击、住房、食物/木材统计和剧情任务均无作用。装饰实例不获得可写 PlantRef，不与功能树统计相加。

沙生境装饰小植物池固定为空，不能为了补美术覆盖随机撒草/花/芦苇。已有樱花树的零星落瓣属于该树的有界粒子表现，落地不生成有库存的小植物、货物、种子或永久碰撞。粒子 seed 与生长 RNG 分开，暂停/低动态/LOD 可以减少表现而不改变树的时间。

圣树发光来自原创树体/叶材质与只读 glow profile，并实际让近旁地面与人物材质接收有界局部柔光；不能只做一张光晕贴片。每树半径/强度、相邻灯聚合与每视图光预算有明确上限，低设置保留树体自发光与代表性近旁柔光；光源代理与 bloom 只改变画面。它不会增加信仰、知识、肥力、食物、战力或再生速率，不创造电/燃料商品，不自动承担城市路灯服务。建筑灯具与能源规则保持独立。

life/felling 权威状态是发光门控的第一条件。树死亡或权威伐倒时，树体自发光、近旁柔光及聚合灯贡献从下一份已发布 RenderSnapshot 同步退出；这个 death gate 优先于 LOD 滞回、淡出、灯缓存和聚合，不得因远景代理继续照亮地面/人物。砍伐过程未伐倒的存活树仍按真实状态显示；普通 WoodBatch 不继承免费灯具功能。

炎地少量熔岩是干燥火山地皮上的原创**静态有界片区/裂隙**，读取当前实际面并受密度/视觉预算限制；首版不加熔岩流动动效。首版不喷发、不流体扩散、不灼伤、不毁楼、不改变物理水深或经济资源。只有用户以后明确要求玩法扩展，才新增危险/流体/产物合同，不能仅因颜色像熔岩把文明伤亡偷偷接上。

地表视觉可以丰富，但真树主干、果实阶段与材料/水深必须能区分。远景允许共享 atlas/实例化/HLOD，近景的显示代表数需与真实量解释一致；不可把一片装饰密林伪称有一千棵可采树。

## 12. 事件调度、性能与存档

### 12.1 按到期与接触推进

树木实例使用有限热列（位置/阶段/下个 due）与稀疏冷记录（权利、定义、fruit lots/预约、历史）。生命周期共享定义，成长/成熟/到期进入稳定 due 队列；进入信息页只读快照，不触发生长、补果或消费 RNG。

采摘、伐木、转权、生态/地形影响及保存准备在需要时把相关树更新到指定 tick。状态未变的长区间可 lazy 积分/批处理，但所有交互点和跨越事件须等价。被改生态的大区域需有界受影响树索引/准备写集，预算耗尽 Pending/可取消，不静默少处理一部分老树。

每树活跃 lot、预约、交互工作者、可见果实例都有上限；lot 上限满时停止新 formation，不抹掉已有 lot。历史按汇总/归档保存守恒事件，不每帧扫描全文或每果新建任务。规模样本必须含成熟果树、在途/腐失/采摘预约和密集城市绿化，不能只画万棵无资源树称完整性能。

退休 fruit lot 不永远占活跃槽，也不要求每树保存无界 retired-ID 集合。以单调 cycleIndex、当前周期已用 slot 标记、守恒累计和已归档周期水位证明旧 slot 已执行；在途食品/未结任务保留必要 sourceLotId 与不可变来源摘要。只有不再被活跃货物、预约、任务或重试回执引用的历史明细才能按存档 checkpoint 归档压缩，压缩不清空幂等水位或重新开放旧 command/slot。将来源摘要复制进 FoodBatch 不复制源量。

### 12.2 保存恢复同一棵树和同一批果

必须保存 surface/生态层、profile/定义版本、装饰 recipe/密度/seed/clear mask；所有真实 TreeId/种类/来源/权利/位置/支持/状态/年龄/木量/积分余数/last tick、fruit anchor/cycle/used budget、每 lot 的绝对数量/年龄/credit/成熟到期/预约、采摘/伐木/运输状态、stump/死亡档案；并保存恢复槽密度/counter/冷却、rule revision 和逻辑 due 队列。

GPU mesh、风动、装饰实例、花瓣、树光/熔岩 RT、果实可见代表是可重建缓存，不能在载入时调用生态初始化器发树/发果。食品批次成熟年龄不会因从 TreeLot 变成仓库批次而丢失，已退休 lotId 不复用给同周期另一批。

合法零果、已枯树、被淹树、未完成伐木、到期/失效预约与未卸食品必须可恢复。恢复成完整成熟模型不恢复其资源；旧 save 无果实字段时不默认“补满一树”。显式迁移保留原件、来源与损失报告，不把旧装饰树林都转换成有满额木/果的经济树。

存档续跑与未存档直跑应在树龄、木/果守恒、lot 年龄、损失/产物、任务/预约、恢复计数和人口食物实际消费上相同；不同 worker、相机、LOD、仿真加速分段也需独立验证。同一 commandId 重试不二次植树/采摘/砍伐；强异常失败前不留下已少果但没有货物/回执的状态。

## 13. 不变量、反例与验收

以下是待实施门槛，不是当前通过测试报告。

|贯穿场景|必须验证|
|---|---|
|九 profile/三层|八 biome 加 sand substrate；各树/装饰精确按用户配对，volcanic 仅枯树且无小植物，sand 没有小植物|
|同花甸重复刷百次|已有 TreeId/种类/年龄/木量/fruit anchor/lot 不重置，无额外成熟树/果；装饰可重建|
|花甸三层近中远|多样花/热带花只装饰，六类真实果树可区分；隐藏不暂停成长|
|果子逐渐形成/成熟|绝对数量/lot 年龄与模型长大/渐变色一致；未熟不可采，图片不结算食物|
|树已80%成长时提高肥力|只增加后续 rate，不按新最大产量瞬间放大已有果量/木量|
|满果树/满lot无人采跨多周期|last tick照常推进，满期间形成信用为零；旧批到期损失，新量按当前预算；不合并不同成熟/到期批次，无多年积压补果|
|到期 tick 完成采摘|旧成熟 lot 已不可采，真实已采部分保留；预约不延长鲜度|
|采果预约后砍树|独占冲突/明确取消后重验；不能取得同果双份食品与完整木量奖金|
|先采一部分再伐木|篮中 Food 只保留一次，剩余附着量损失，木材来自真实 standingWood|
|取消半砍/重载半砍|保真实劳动/损伤/果鲜度，不补年龄/果期；旧回执不再次出木|
|鲜果转手/运送/合批|保原 mature/expiry；唯一 location 与数量守恒，不通过搬运续鲜|
|雪原/樱野改沙再改土|现有松/樱 TreeId 不换椰树；沙装饰空；原主题保留，旧资源/损伤不重置|
|樱花瓣/圣光/熔岩|无库存/碰撞/导航/任务/魔法加成；炎地普通角色不被未授权灼伤|
|圣树 death gate/装饰重载|死亡/伐倒使自发光、近旁地面/人物柔光与远景聚合灯同时退出；已清花草 mask 经存载/LOD不补回|
|淹死果树再填回|原树死亡不复活；已采食品独立结算，附着损失不补；新树另ID|
|幼苗长大靠近车库|成熟包络事前防侵占，既有必需入口保持；自然树不偷偷封旧路|
|城市/农田刷森林|LandUse 覆层防野树插入道路/房/农田；农业产出由实际作物/劳动，不收装饰草花|
|恢复关闭跨百日再打开|老树/农作物照常长，果会过期；不开关补树，不补发关闭期萌发|
|恢复→砍幼苗→重涂|零初始可采量、日额度/冷却/counter 保留，不得每stroke重抽成年树|
|人工/自然同时争空槽|稳定顺序只有一株成功，失败不留半ID/占用/收费|
|采摘中改地/死亡/满仓|到达/劳动/lot/容量二验，在途食品保唯一位置，取消只未发生部分|
|保存任意生命周期|formed/ripe/expired、stressed/dead/stump、采摘/砍伐/运输、恢复候选续跑等价|
|坏存档/分配失败|负果量/重复lot/预约超量/未知profile/重复位置拒绝；失败不半少资源/多商品|

第一条完整历史应是：玩家画花甸→形成装饰，联合明确 PlantVegetation 预览生成幼树→树逐渐成熟/多批长果→公共人物到树边采食/运粮→存载继续→先摘后砍→木材入实际物流→幼株有限恢复。第二条是：花甸果树与仓储任务运作→改沙/淹水→已有树保种类并失效/损失→运输/预约正确退出→重新植树而非复活。圣原和炎地用同一空间/树账本验证其丰富表现，不另起魔法或灾害引擎。

## 14. 与其他合同的接入关系

主任务需同步 v0.7 的八主题/九 surface 配对、机器 surface/species/fruit/decoration 定义、WORLD 的生态/物理来源、CONTENT/像素资产的三层与纯表现边界、INTERACTION 的 Tree/fruit 实物信息与合法动作、PERSISTENCE 的果 lot/恢复队列；不得覆写原稿和历史来源。

已有 `CommitBiomeStroke / PlantVegetation / ClearTargets` 接入同一预览与单写者。树检查器使用 `TreeRef = PlantRef(kind=plant, subtype=tree)`，没有新实体 kind/平行目录计数。采摘/伐木复用 `HarvestPlant(mode=fruit / fell)` 与供货/搬货/劳动合同执行器，不由渲染直接发货；typed payload、稳定 commandId、实际 TreeRef/lot 预约、版本二验、幂等/失败恢复和同 tick 排序必须落地。清除装饰使用 mask，不混成 `fell`。当前不存在这些树经济实现，不在 UI 假装 Accepted。

先做一棵真实果树从幼株→逐步果批→成熟→实际采收/到达/食用→有限替换/存载的贯穿 oracle，再接九 surface/三层样本和多种类表现；同时用独占伐木与淹水反例证明守恒。只做漂亮草地或树上果子变色不足以宣称新地表完成。

## 15. 炎地枯树的终止状态与资源回收（2026-10-09）

dead_tree是枯树放置archetype，不代表可遗传的生物树种。初态lifeState=DEAD，growth/fruit永久关闭，零果量；与“新活树为young”规则分开。生物死亡和作业状态分别检查：已死但尚未清除、可达且有真实余木的树仍可HarvestPlant(fell)，无需复活到ALIVE/FELLING生命状态。劳动/作业可处于FELLING，lifeState仍为DEAD；完成后扣实际余木，生成有容量位置的正量WoodBatch并退役，0木量仅清场不生成空货批。

神力放置枯树由可信player/current god_rule审批，初始木果量0，不因枯枝模型大就发成年木量。INITIAL仅在新World一次性初始化可有明确有界枯木量，必须入最高预览/首档与资源来源记录。自然萌发只生成活幼株，排除dead_tree；文明种植不可将枯树当苗种，也不新增枯木种子/苗圃市场。普通树真实死亡保留原speciesId/TreeId/来源、已发生损耗和剩余木量，不变种成dead_tree来重置库存。

主题涂抹到炎地不自动杀死原树，不自动生成带木量枯树；枯树摆放走显式树体配置预览。所有支承/占用/成熟或固定枝体包络/伐木站位/旧接入仍核验，模板不复制实体和余木。存载、LOD、改回适宜土层和恢复开关不重新激活已死树，也不增长枯木。枯树几何可保留，不进入圣光/樱瓣/结果效果；若既有圣树死亡，其原种类保留，发光按死亡门控关闭。

验收应覆盖神力0木枯树砍后不产货、INITIAL余木→部分/完整回收→重载守恒、原树死亡不换种/补量、重复刷炎地不杀旧树/添木、野生恢复不萌发枯树、零生长/零果及旧车库通道保持。该配置仍属设计，尚无生产枯树资产或执行器。
