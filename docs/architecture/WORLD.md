# 世界、初始来源与可编辑三维地表

2026-10-08。[ADR0012](../decisions/0012-editable-3d-pixel-world.md)与[v0.7](../design/Sonnheide_Design_v0.7_Pixel_World.md)取代平陆/天然不可改合同。当前原生Earth/PBR客户端与headless LAND/WATER kernel仍执行旧规则；以下是新版生产设计，不能称为已实现。详细算法/后果见[地形事务](PIXEL_TERRAIN_TRANSACTIONS.md)。

## 1. 权威与来源

`SourceProvenance`记录Blank或Earth来源、原始包hash、选区/投影/worldScale、量化与边缘闭合配方、seed、初始地形hash。它不可改写，是历史证明，不是以后地形的只读锁。当前`TerrainWorld`持有可编辑heightQ、topMaterial、biome/fertility/moisture/cover、修改来源与修订，水深/海陆、岸线与导航由它派生。新增土不会改变原始GSHHG哈希，旧包升级不会覆盖玩家改过的海岸。

矿物rate/stock独立；涂岩石不产矿，地形变水不把矿石送到仓库。道路、桥、植物、建筑/地基、产权与领土均是另一领域，不塞进material枚举。普通平陆允许无地基；实际高差需要完整建址，不按“内陆/沿海”标签猜支承。

## 2. 两种初态，一种生成器

Blank输入世界尺寸/profile/seed、全海或平陆预设、名称，输出0人0楼的候选。全陆内域有明确预览的边缘自然化和海缓冲；全海不依赖Earth包。

Earth输入固定GSHHG2.3.7全精度闭合岸/岛湖嵌套、WGS84选区、投影与唯一水平worldScale。继续使用已有数据许可、五LOD全球导航与full来源读取；最终像素格按明确覆盖分类/量化策略生成。不能把低LOD矢量用作最终细岛事实，不能保留一条视觉矢量窄河而导航已经把它填陆。量化后的窄海峡/岛损失作为差异报告，最终预览展示真实结果。

源数据仅海陆，默认陆台/按距离派生海床不声称真实海拔。可选风格化seed山地是显式开关/来源记录；未选则不偷偷加山。投影跨日期线按中心展开/双窗口，极区按投影多边形，近对跖非法域拒绝。X/Z统一缩放、人物建筑游戏单位不跟着任意轴拉伸。

创建候选流程共用：Draft→Validate→Quantize→Surface/Water→SamePathPreview→FirstCheckpointReadback→Publish。草稿改尺寸/区域/seed/profile产生新DraftGeneration，旧异步结果失效。正式WorldId只在有效候选中分配并持久化。

## 3. 地表列与尺度

首样本2m格、0.5m高度量化、64²chunk，规格写入world/profile/recipe；不能在载入时改成新默认。下层高度以整数Q表示，GPU生成同revision顶面/侧壁；实际步高、车坡、建筑模块可更细，独立单位/碰撞形状必须一致。每列一个表面，不存满XYZ，不支持自然洞穴/悬空块。

空白均匀海/陆chunk用default+稀疏页，编辑时copy-on-write；必要时dense页，转换不能丢值。页、矿源、占用和nav缓存分开，`TerrainRevision`变化不必复制整World。世界格、可见mesh、导航节点、人口、历史、CPU/GPU内存都设预算。超过预算在创建/预览前给可读主因，不靠运行时OOM。

高度、材料、生态分别可涂；八类terrain preset只规定候选组合。其幅度、中心高度/水深和边缘profile按世界允许的参数校验。Hill/Mountain/Peak有真实体积和侧壁，窄peak可以不可通行；不自动拓宽刷子假装符合用户指向。

## 4. 水体与世界边缘

首版海/湖固定静水位Q0，heightQ<0为湿，depthQ=-heightQ。四连通至外沿权威海源为SEA，否则LAKE；对角触水不连。水深档与bodyKind分开：内陆deep preset得到深湖，不能说是通外海。隔断海峡不会删水，开通后更新海/湖关系。

局部chunk面/门户更新加全局受影响粗图split/merge；禁止只更新刷子内近海标签。WaterBodyRef稳定身份及split/merge历史，临时component编号不能持久当对象ID。几何/导航dirty时查询ReadyPath/ReadyNoPath/Pending，旧证书不跨dirty段；异步候选附world/session/terrain revision，过期丢弃。详细权威水域与航路图分离见地形事务。

有限可玩域加预算内海缓冲；远海外沿保护带不能刷、填、生成产权或港口，预览显示工具可编辑范围。域外只有持续表现海/天空，镜头可望向它，不能有船/人物/航路/存档对象或隐形clamp。没有矩形海面尽头或围墙；海域外观连续不意味着无限模拟。普通chunk边不生成海岸，世界外沿处理由显式边缘配方决定。

## 5. 普通工程与Godterrain

Godterrain允许造岛、改山、挖海，免费只是不用文明材料，不意味着免费商品或退款。来源DIVINE_EDIT，地形及直接物件影响一次发布，真实经济后果由后续任务发展。保护模式也需检查海峡导致的远方必需通路；破坏模式显示这些影响后可批准。无安全替代时仍允许明确破坏，而不是永久保通规则拒绝所有工具。

CivicReclamation/平整是合同工程，必须材料/实际劳动/现有可达施工范围，未完保持旧表面。完工写CIVIC_EARTHWORK来源，不覆盖provenance历史，后续神力可毁，不退已消耗材料。普通新建/改路保旧接入；神力导致的失效撤销证书但保留权利/责任/历史。

Port不再RECLAIMED-only；所有干陆来源均核验真实支承、正交核心/海侧、水深/净空/航道、陆路接货。湖港/海港按实际连通；地变浅或陆可能搁浅船并停运营，不能删船籍、乘客、货物。

## 6. 矿物、生态与政治

矿物曲面相对当前真实地面高度显示，但射线命中terrain而非资源overlay。抬矿率/压率/归零/锚定夷平保旧量纲与库存规则；地形与矿物stroke独立命令，不相互改库存。水下矿默认停采，恢复干陆也不刷新stock。

生态种类/地表cover决定种植适宜性、有限产能/成本，不即时产实体或商品。植物种植与死亡/采伐单独写事务；神力森林/树木来源可记录，任何资源回收仍需真实可采储量与劳动，不能把涂同一块重复当无限成熟木。Light/Darkness不改变height/biome/水。

法定领土主张与可用干陆、海域使用/实际控制分开。淹城不自动割让，Godland不自动赠邻国；同一坐标的旧地权/法律状态保留可追溯，使用资格由当前表面核验。城市/国家/企业实体与物理楼分别处理，完整后果不靠删除格里的对象解决。

## 7. 证据与验收

旧阶段1真实GSHHG/PBR/空档门槛见[历史验收](../planning/PHASE1_ACCEPTANCE.md)，不证明新高度/生态/地形编辑已实现。新版核验空白/Earth同路径、格/立面/拾取/碰撞一致、同初态同stroke与刷速无关、海峡全局split/merge、occupied灾损守恒、边界拒绝、存载/取消隔离及真实GPU视角。大图全图flood oracle只做正确性对照，不作为生产每笔全图复制方案。
