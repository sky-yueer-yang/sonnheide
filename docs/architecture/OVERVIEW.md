# 当前工程总纲：三维像素文明与可编辑地形

2026-10-08。[v0.7完整设计](../design/Sonnheide_Design_v0.7_Pixel_World.md)/[ADR0012](../decisions/0012-editable-3d-pixel-world.md)/[机器合同](../../data/contracts/pixel_world.json)为当前设计，旧PBR客户端是已实现基线，新pixel实现未完成。

## 1. 真正的突破点

|边界|必须解决的实际问题|当前设计|
|---|---|---|
|地形三维与规模|高分辨率贴图不意味着世界满体素；terrain/nav/入口尺度混用会制造假可达|分块单顶面高度列+立面；独立资产texel密度/导航净空，统一权威surface revision|
|海峡全局拓扑|删一格影响远港，只重算刷子附近会留下假航路|chunk门户+受影响粗图split/merge；dirty即证书失效，Pending不得复用旧路径|
|Godterrain全因果|只改格会让楼/住户/货物/旧路悬空，永保通又使工具不可用|占用依赖索引、后果预览、单写者强异常发布；保护/破坏两模式，普通施工仍保通|
|不同来源同世界|Blank与Earth两渲染器/两存档会反复分叉|统一TerrainWorldDraft/PreviewDescriptor/候选发布；source只作初始证明|
|高精像素人/衣物|免费换色消灭服装行业，贴图像素=方块造成几何爆炸|原创cuboid+精细图集+fit/动作LOD，WornItemSnapshot读取真实实物|
|城市灾损恢复|删楼不等于删法人/货权/国家，免费应急仓库会复制资源|逻辑实体/物理空间分离、有限应急容纳、货损分录/继承/真实迁移|
|可靠旧新规则|加schema数字不代表能把旧档转换|explicit复制迁移、新WorldId、差异/损失预览、原档保留、新首档成功才发布|

身份关系、实物/货币账本、知识许可、任务合同、空间运输仍是五共享基础，所有模拟修改单写者。镜头、UI locale、LOD和工作线程完成顺序不决定仿真RNG/收益/死亡。

## 2. 进程与依赖方向

原创作者源/已核验资源→离线Cooker→PixelContentPack/定义；输入→只读预览→CommandDispatcher→单写者World→不可变快照/查询→RmlUi/场景proxy/bgfx。后台读快照生成mesh/nav/IO候选，写者依据revision接纳，不直接修改实体。生产核心不依赖SDL/GPU/DCC。

沿用现有C++20、SDL3、bgfx/bx/bimg、RmlUi、FreeType；cgltf/ozz/mesh优化仅按真实资产需要核锁后接入，不为换画风先换库。Steam唯一商店，Metal只开发验证。不开23个微服务，不用Unity/Unreal/完整游戏引擎。

## 3. 目标工程结构和增量顺序

下面是职责/未来落点，不创建空目录冒充实现。已有engine/world_creation与earth_world/nav/native页在迁移适配完成前保持旧契约；新入口明确版本，不把类重命名就算重构。

|落点（按需新增/改造）|所有权/职责|首接入|
|---|---|---|
|engine/world/terrain_columns、terrain_profile、water_bodies|当前surface/水位/生态/来源、页与revision|P1|
|engine/world/terrain_commands、terrain_effect_plan|typed stroke/preview、依赖读写集、占用/损失Adapter|P2|
|engine/world/spatial_dependencies、navigation|地面/水/道路门户、支承、旧证书、split/merge|P1–P4|
|engine/world_creation、geography importer|Blank/Earth统一草稿、量化、边缘/同路径候选与首档|P1|
|engine/people、ledger、city、mobility|真实居民容纳/灾损/衣物与库存、应急/修复|P4|
|engine/politics、religion、science、military|原稿领域通过共享基础，不另造地形账本|P5–P8|
|presentation/native/pixel_terrain、pixel_water、pixel_scene|块阶面/侧面、原创像素材质/水/天空、阴影/LOD/拾取|P1–P3|
|client/native/terrain_tool、inspector、input_router|底部八组、当前tool capture、正常字后果预览/typed关系|P2起|
|persistence/pixel_sections、migration_registry|提交态页/坏档拒绝、legacy复制、恢复/重放|P1起|
|assets/source/pixel、tools/pixel_content_cook|原创人物/衣物/房屋作者源、hash/fit/LOD/recipe验证|P0合同/P3正式资产|
|tests/terrain_transactions、scenarios、native/GPU验收|拓扑/强异常/人货守恒/真实画面，非实现镜像单测|每个贯穿门槛|

模块路径是规划候选，不保证此刻实际存在；细分只在有功能/热点时创建。地形后果Adapter先有小fixture证明，再生产接真实人货/城市，避免先上线工具后补删除善后。

## 4. 性能与不变量

热人物SoA、冷政治/机构/历史分页、不可变定义共享、terrain按chunk copy-on-write。保存一致revision页，网格/导航按dirty与预算，图集/实例/骨架/HLOD按投影尺寸和迟滞。不能每笔复制全世界，也不能每帧全图flood/扫全人口或Person²文化传播。

权威整数/定点、右手Y上游戏米，GPU相机相对float；网格profile每世界冻结，当前height/材质可变。先真实100/1000/10000人生产/物流/衣物/城市灾损/战争负载，再探索更大容量；1080p60fps与10万人均非已测承诺。稳定ID不回收，货物owner/custodian/location、法定主权/实际控制、Culture/Language分别存储。

## 5. 当前完成度

已实现的原生窗口/菜单/三语、真实GSHHG导航与旧PBR空世界创建、有限headless事务/存档与独立衣物/交互oracle保留。新像素地表、可编辑真实高度、原创像素人楼服装、占用因果生产接入、完整文明与迁移器尚未实现。旧MakeHuman/PolyHaven/Hex/Abyssal资源保留许可/哈希和历史，不自动成为新runtime必要输入。

后续按[IMPLEMENTATION_PLAN](../planning/IMPLEMENTATION_PLAN.md)：先新terrain/create/3D界面+存载完整门槛，随即原创人/屋/广场/服装，再居民生活/文明制度/宗教/战争/企业。已批准艺术主菜单复用，资产合同与试样质量准则P0先定。每批先全部修改/审查后一次统一编译验收。
