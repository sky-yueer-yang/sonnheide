# 初步程序结构：自研客户端、统一对象页与生产接入

2026-10-06。本文是生产工程的目标结构与接入顺序；第 1 节列出当前真实实现和验证范围。最新顺序是主页面与应用启动→创建世界与真实初始地表→基础游戏界面、镜头、保存载入和返回→再开始原创建筑与人物服装资产、建设和生活。前三阶段先形成一个可可靠进入、离开和恢复的真实区域空世界。计划目录在首次实现对应功能时才创建，不通过空目录或接口数量表示完成。应用状态与世界交接见 [APPLICATION_FLOW](APPLICATION_FLOW.md)，阶段和退出条件见 [完整施工计划](../planning/IMPLEMENTATION_PLAN.md)。

视觉最新按[ADR 0009](../decisions/0009-monumental-minimal-interface.md)，环境按[ADR 0008](../decisions/0008-sacred-interface-and-light-ages.md)：[油画/极简无框交互](VISUAL_STYLE.md)贯穿菜单、底栏、对象和动态表单；核心只发布[Light/Darkness环境状态](LIGHT_AGES.md)，呈现层消费其快照，不计算自转公转/太阳昼夜。主菜单轮播属于应用可见时间，自动光暗属于仿真tick，不能共用计时器。

## 1. 现有工程与生产目标的区别

|当前真实文件/目标|已有证据|生产接入还缺什么|
|---|---|---|
|`engine/src/world.cpp`、`simulation.cpp`、`persistence.cpp` / `sonnheide_kernel`|材料、填海、有限港口与 checkpoint 不变量|真实 Person、共用时钟、统一货物/资金/任务/法律、全域存档|
|`engine/src/site.cpp`|独立坡地建址与入口保护 oracle|生产 World、现状物流、全交通网络、全车身转弯扫掠、阶段占用释放|
|`engine/src/clothing.cpp`|独立有界衣物 Ledger 和有限长期替换场景|真实城市原料再生产、统一劳时/账户/物流/存档|
|`engine/src/interaction.cpp` / `sonnheide_interaction`|独立 40 类对象引用、人口/规则/清理 oracle|生产 World 查询和领域命令；不能再造第二份 Person/State 事实|
|`presentation/src/ocean_fft.cpp` / `sonnheide_ocean_math`|Abyssal CPU FFT 正确性参考|bgfx GPU 频谱、波面材质、海岸遮罩、三后端画面与预算|
|`apps/headless/main.cpp`|有限造陆→港口→存档贯穿|生产世界的全域 headless 场景运行器|
|`data/ui_locales.json`|英汉德统一消息目录与离线镜像校验|完整生产字段/定义文本、字体、IME和原生消息cooker|
|`tools/previews/world-inspector.html`|浏览器示例状态的可点击交互原型|SDL3/bgfx/RmlUi 原生客户端；原型不嵌入发行游戏，不作为生产事实源|
|`tools/previews/main-menu.html`、五幅用户油画|浏览器主菜单/三语设置与环境小场景；固定原字节来源|原生应用状态、真实新建/存载、World环境计时与GPU灯光；按钮不能假成功|
|`assets/source`、`generated`、`manifests`|3 份原创灰盒、选定 MakeHuman 源件、来源与部分验证|正式建模/材质、统一成人 rig、衣物适配、动作、LOD、完整 pack|

运行时库选择沿用 [开源栈决定](../research/OPEN_SOURCE_STACK.md)：C++20 + SDL3 + bgfx/bx/bimg + RmlUi + FreeType；cgltf/meshoptimizer 优先离线使用；ozz-animation 用于角色动画。它们当前仍未集成。`data/dependencies.json` 是选择清单，不是生产 lock。接入成功后才记录兼容 commit、源归档 SHA-256、编译器/参数、传递依赖、许可文件和实测平台。

## 2. 要先解决的四个工程断点

**先证明世界能被安全创建、进入和恢复，再接生活领域。** 主菜单不是已经存在 World 的另一张页面。启动、创建草稿、地理预览、生成候选、读取存档、提交新世界和返回菜单必须有明确状态；世界未创建时没有 EntityRef、人口统计或可提交领域命令。旧世界保留到新候选完整校验和持久化成功，取消、缺包、损坏和失败不会先把旧世界清掉。加载同一 worldId 也换 SessionGeneration，拒绝旧查询和 GPU 拾取回调。详细合同见 [APPLICATION_FLOW](APPLICATION_FLOW.md)。

**独立 oracle 不能并排组成正式世界。** 目前内核、Site、服装和交互切片有自己的局部事实。生产接入必须将稳定 ID、人员、物品唯一位置、账户、任务、时钟、通路和存档收归一个权威 World；局部算法成为只读规划器或事务内计算器。跨域变更先准备写集，再一次提交。不得靠四套状态同步脚本解决同一人或同一批衣物的多份真相。

**集中对象页的共用项是引用、投影和编辑协议。** 国家、人物、货物和法律不是一张任意 JSON 表。共用导航/章节框架，具体页面向领域查询取得有含义的事实；可编辑项绑定明确命令，制度变更显示程序前置、受影响对象和失败主因。页面不持有可写 World 指针。

**初始地表与界面先做，建筑资产在基础门槛后尽早做。** 阶段 0—2 优先主页面、创建、地表、水体与基础 UI 的原创美术及真实数据接入；建筑、成人角色和服装仅核对已有约束与来源，不开始新的作者源制作。阶段 2 的空世界贯穿通过后，阶段 3 才启动建筑代表件、统一成人角色/衣物和坡地工程。footprint、门位、车库净空、前侧接货区、坡道/台阶和围网占用先落实到代表件合同，批量扩充在真实地形验证后进行。已有灰盒保留为有限 fixture，不能冒充这一阶段已完成。

## 3. 初步目标目录树

下列 `planned` 表示责任归属，不表示目录已存在。第一版保持少量构建目标；领域按源文件组织，出现明确编译/依赖收益时再拆独立库。

```text
Sonnheide/
├─ engine/                            现有；无 SDL/GPU/DCC 依赖
│  ├─ include/sonnheide/
│  └─ src/
│     ├─ foundation/                  planned：ID/时钟/定义/RNG/事务信封
│     ├─ world/                       planned：创建候选/冻结地理/地块/空间/通路
│     ├─ people/                      planned：生命史/家庭/需求/任务资格
│     ├─ economy/                     planned：物品/账户/预约/合同/物流
│     ├─ construction/                planned：地基/接入/建设/拆除/填海
│     ├─ society/                     planned：文化/语言/命名/迁移
│     ├─ institutions/                planned：国家/法律/企业/宗教/教会
│     ├─ knowledge/                   planned：研究/文献/许可/生产采用
│     ├─ military/                    planned：Army/补给/战斗/战争/和约
│     ├─ queries/                     planned：对象页/图谱/统计只读投影
│     └─ persistence/                 planned：checkpoint/迁移/历史/重放
├─ platform/                          planned：SDL3 窗口/IME/文件/设备桥
├─ presentation/                      现有 CPU FFT；其余 planned
│  ├─ renderer/                       bgfx 资源/通道/PBR/水体/阴影
│  ├─ scene/                          视觉代理/LOD/HLOD/动画/空间拾取
│  └─ overlays/                       国家/文化/矿物/施工/军令只读图层
├─ client/                            planned：玩家交互
│  ├─ application/                    主菜单/应用状态/创建草稿/存档选择/世界交接
│  ├─ shell/                          底部 section 工具栏/窗口宿主/通知/时钟
│  ├─ tools/                          工具会话/选择/放置/笔刷/军令/清理预览
│  ├─ inspectors/                     页面注册/类型路由/关系/历史/比较
│  ├─ forms/                          字段/草稿/权限/预览/提交/冲突恢复
│  ├─ localization/                   en/zh/de 消息/格式/字体/换行
│  ├─ view_state/                     收藏/标记/窗口/镜头/导航/偏好
│  └─ bridges/                        RmlUi ↔ SDL3/bgfx；快照 binding
├─ apps/
│  ├─ headless/                       现有；以后增加真实贯穿场景参数
│  └─ game/                           planned：生命周期/候选发布/装配与有序关闭
├─ assets/
│  ├─ source/                         作者源与可分发原始源；大文件 LFS
│  │  ├─ buildings/                   全原创楼体/构件/地基/道路/港口
│  │  ├─ characters/                  planned：统一成人规格、变体/衣物
│  │  ├─ animations/                  planned：rig/重定向/源动作
│  │  ├─ ui/                          partial：油画/企业标志/Cinzel源与许可；native UI planned
│  │  └─ materials/                   planned：自制及逐项准入表面素材
│  ├─ generated/                     有意义 fixtures 与派生产物
│  └─ manifests/                     原文件/许可/配方/hash/几何/LOD
├─ data/
│  ├─ catalogs/                       现有：424 科技定义、35 法律族、13 业务
│  ├─ interaction_schema.json         现有：40 类型、关系/编辑合同
│  ├─ localization/                   planned：en/zh/de 消息目录
│  └─ geo/                            现有 ETOPO 窗口；完整 EarthPack planned
├─ ui/                               planned：RML/RCSS 与原创图标引用
│  ├─ application/                    主页面/创建/进度/存档/恢复/三语设置
│  ├─ shell/                          section 栏、工具带、窗口、toast
│  ├─ inspectors/                     类型专属页面模板
│  └─ forms/                          字段、对象选择器、预览、比较
├─ tools/
│  ├─ content_cooker/                 planned：glTF/动作/LOD/材质/pack
│  ├─ localization/                   planned：覆盖/占位符/术语/伪本地化
│  ├─ geo/                            planned：源→投影/EarthPack/精度报告
│  └─ previews/                       现有：可审阅交互/施工原型
├─ tests/                            切片与真实贯穿/迁移/故障注入
├─ third_party/                      固定源、许可、补丁；现有 Abyssal
└─ docs/                             来源、ADR、工程合同、施工与验证
```

`source` 和 `generated` 不共用编辑入口；shader/RML/RCSS/配置属于文本源码。renderer 不导入网页原型脚本。正式 UI 按我们的主题原创，不复制 WorldBox 的图标、画框、像素画或品牌。

## 4. 构建目标与依赖方向

|目标|责任与依赖|禁止项|
|---|---|---|
|`sonnheide_kernel`|创建候选校验、冻结地理、权威 World、事务、调度、查询、存档；标准 C++20|窗口/GPU/字体/网页；按镜头执行不同规则|
|`sonnheide_interaction`、`sonnheide_ocean_math`|当前独立 oracle/数学参考，保留作回归对照|独立生产 World；GPU 倒灌核心|
|`sonnheide_platform` planned|SDL3 生命周期、输入/IME、设备与原子持久化文件桥|经营人物/经济；与 bgfx 同窗口提交第二套 GPU|
|`sonnheide_presentation` planned|bgfx/ozz、场景、只读快照/内容 pack|结算材料/劳动/伤害/死亡；动画回调判完工|
|`sonnheide_client` planned|应用状态/菜单/创建与存档反馈；窗口/工具/检查器/编辑/locale/view profile|直接写 World；自由覆写 JSON；缓存作业务真相|
|`sonnheide_game` planned|拥有线程、活动会话和私有候选；校验后交接、装配和有序关闭|第二份领域实现；先销毁旧世界再试着加载新世界|
|`sonnheide_headless`|核心与场景输入；生产规则/保存/重放测试|运行时要求 GPU/DCC/系统字体|
|`sonnheide_content_cooker` planned|固定离线 importer/LOD/动作/pack；DCC 外部离线步骤|取互联网浮动版本；编辑工具链接发行游戏|

UI 数据类型可位于核心公开查询协议，但 RML 元素、翻译字符串和 GPU 句柄不进权威存档。开发性能面板如使用 ImGui，仍与玩家 RmlUi 页面分开。候选 World 只在私有准备区校验，不接命令、不运行仿真；活动会话始终只有一个。它是安全切换的暂存对象，不是第二份并行经营的生产事实。

环境状态由核心单写者持有相位、自动开关、有界期限与generation，存入世界checkpoint；界面只发typed切换意图，renderer只插值显示。日历不导出日照角，油画轮播不写环境。无框按钮在RmlUi中同样禁止outline/ring，主菜单纯文字，其余按实际识别需要使用icon；保持三语标签和可见焦点状态，不到发行时再换一套样式。

### 4.1 应用、创建与存档的责任划分

|组件|职责|依赖与边界|
|---|---|---|
|`ApplicationFlow`|Boot/MainMenu/WorldSetup/PreparingWorld/InWorld/Returning/LoadFailure 状态与可用动作|客户端状态；不创造模拟实体；统一处理取消和退出|
|`WorldCreationController`|名称、固定 GeoPack、合法选区、统一比例的创建草稿；显示真实覆盖、精度和预算|调用核心只读验证；地理预览不是最后天然 mask/height 的生成器|
|`PreparedWorld` / 创建服务|离线源校验、投影、同 XYZ 比例、冻结自然层、初始空世界|核心私有候选；缺源拒绝，不以随机山地或免费人房补齐|
|`SaveLoadPresenter`|继续/加载列表、兼容和缺包主因、保存进度、明确恢复版本选择|列表索引不是 World；加载失败不改原存档；完整格式见 PERSISTENCE|
|`WorldSessionCoordinator`|候选全验、首次 checkpoint、SessionGeneration 与安全发布/释放|应用装配责任；旧世界到发布前可恢复，Continue 指针最后更新|
|`ViewProfileStore`|按 worldId 的镜头/收藏/窗口偏好；全局 UI locale/输入偏好分离|切换同世界也撤旧订阅；profile 损坏可重置视图，不重写 World|

首次程序只需少量有实际调用的文件完成这条链；上表不要求立即创建六个库或独立 class。原生地表/GPU/IME、完整持久化和这些状态当前仍待接入。

## 5. 从点选到提交的真实路径

```text
鼠标/触控/键盘/IME
 ↓ SDL3 → InputRouter（文本框/窗口/当前工具/地图仲裁）
 ↓ ToolSession 或 WindowHost
地图 PickResult{worldId, sessionGeneration, kind, id, snapshotRevision, surfacePoint}
 ↓ InspectorRouter.resolve → InspectorRegistry + QuerySnapshot
 ↓ RmlUi 页面（关系用 EntityRef；未知量显示未接入）
编辑草稿{targetRef, sessionGeneration, baseRevision, typedValues}
 ↓ CommandPreview（前置/代价/影响/主因）
 ↓ CommandGateway：可信权限 + 幂等 commandId + expectedRevision
World 单写者：校验 → 准备写集/事件/回执 → 原子发布
 ↓ Receipt + 新 detached Snapshot + DomainEvents
窗口增量更新、通知、资产/动画只读表现
```

焦点内点选不穿透到地图。拖窗口、滚检查器、编辑文字不触发放人或生命清理。所有全局工具只在底部栏提供固定入口，地图旁只显示当前工具的上下文参数/原因，不复制第二套全域工具。Esc 先交给 IME/文本控件，再关闭最上层弹窗或取消地图笔划，最后退选择；离开未保存对象表单先提供继续编辑/丢弃选择，不静默删除草稿。快捷键/按钮走同一动作注册表。

### 5.1 客户端初步组件

|组件|最小职责|输入/输出|
|---|---|---|
|`ToolRegistry`|section、工具定义、图标、三语 key、模式、权限/条件|稳定 ToolId；语言不产生新 ID|
|`ToolSession`|Idle/Preview/Drawing/Ready/Submitted；本次草稿|只读预览、显式提交；hover 不执行命令|
|`WindowHost`|对象页/统计/法规/设置，位置/焦点/关闭/返回/比较|窗口实例与 EntityRef，不另持可变实体|
|`InspectorRegistry`|40 页面、章节/字段/关系/动作映射|typed projection；关系可点并保留历史|
|`InspectorRouter`|近景 Person/Building、远景 State、收藏/搜索统一路由|活跃实体/死亡注销档案；跨世界拒绝|
|`FormController`|草稿、验证、权限、程序型变更、预览/冲突恢复|typed payload；草稿不改世界|
|`ObjectPicker`|按类型/权限/前置过滤候选，显示名/摘要|EntityRef；玩家不手填 ID|
|`StatisticsPresenter`|单位/期间/分母/覆盖/stock-flow 区分|StatisticsProjection；未知量非 0|
|`LocaleService`|en/zh/de 消息/数字/日期/复数/字体/切换|显示文本；不改仿真 Language/存档 ID|
|`ViewProfileStore`|按世界收藏/标记/窗口/导航/镜头；全局 locale/快捷键分离|独立视图版本；不推进 World/RNG|
|`CommandGateway`|可信来源/修订/幂等信封/回执/原因映射|只发单写者信封；payload 不可自称 Scheduler|

这些是责任边界，不要求马上为每项写空 class。真实人物页先用少量源文件跑通，再按复杂度拆分。

### 5.2 编辑对应实际因果

- 人物姓名、允许特性/性格使用受限字段表；范围、相互排斥、成年/服役资格和进行中任务由领域复核。改特性不自动补库存、抹伤或造新人。
- 国家分自由资料、制度状态、历史事实。官方宗教/宗教绑定不是 checkbox 任意写 `religionId`；预览宗教资格、宪制、帝国/Grant、教会关系和影响，再专用事务变更。关闭绑定不清除所有人的信仰。
- 法规用 35 法律族结构化参数、授权/前置、施行日/过渡；历史税款、存量合同、冻结 charter 不因改法回写。不合法存量要处理方案或拒绝，不能偷删人货企业。
- 文化/语言/宗教共用导航但保留领域命令与只读谱系/来源。locale 与仿真语言分开；切语不生成派生语言。
- 六类清理先给选区、可执行/阻止项、原因/补救；实际住户/货物/旧通路引用决定保护，不能信 UI 缓存。生命清理产生死亡档案。

目录以 [交互合同](INTERACTION.md)和机器 schema 为准；原型编辑不等于生产命令实现。

## 6. 单写者、快照与规模

模拟线程拥有 World；渲染/输入只持有已发布 detached snapshot。后台 worker 读取带 sourceRevision 的不可变数据，返回导航/统计/内容准备结果；单写者复核版本才发布。线程数量按测量确定，正确性不依赖并行任务先后。

快照分轻量世界摘要/地图块、可见视觉代理、订阅对象页、统计查询、档案分页。开一人页不复制整个世界；远景隐藏动画不减少人口。移动可在两已提交状态间插值，年龄/衣损/材料/军令结算不插值写回。

拾取带 `snapshotRevision`；提交再验目标活跃/地点/规则/通路/权限。死亡注销同 ref 保留，旧收藏不跳到复用 ID 新人。新建世界分配新 worldId；加载同一世界保持其稳定 ID，但每次进入都更换 SessionGeneration 并撤旧订阅。查询、创建作业、GPU 回读及表单提交必须匹配各自会话/草稿 generation，不能只检查 worldId。view profile 按 worldId 独立，回菜单时不留可提交旧页。切换和失败恢复见 [APPLICATION_FLOW](APPLICATION_FLOW.md)。

统计不求可见代理：活跃 Person ID 去重，国籍/居住/现场/雇员分别；owner/custodian 不可相加，衣物件数/纤维单位分开。跨类型比较选共同 metric profile，列口径/缺项，不对不同量强画排行。

## 7. 英汉德三语的结构要求

locale 的唯一消息键为 `en`、`zh`、`de`；平台输入 `zh-Hans`/`zh-CN` 可归一为 `zh`，不创建第二份中文消息。三语同一 ActionId/ToolId/EntityKind/payload，原姓名/备注不翻译作身份。菜单显示 English / 中文 / Deutsch，自称切换即时重排，不重载 World/选中对象/草稿。

本次原型的消息契约位于 `data/ui_locales.json`，生产继续用同一语义 key；只有文本分包产生实际收益才迁入目标 `data/localization`，不维护两套独立翻译源。消息含 section、工具、种类、章节、字段、enum、单位、可用/拒绝原因、通知、指引、法规/教义/科技解释及 credits。采用占位符/数量形式，不拼接中文语序套英文。domain 返回 reason code + typed arguments，客户端译成主因/下一步，不暴露异常或 enum。数字编辑器按当前 locale 解析，再生成固定量纲 typed payload；德文小数逗号不得被静默当成整数或千位分隔。

字体沿 Noto/SIL OFL 路线逐项固定文件/hash/许可证，验汉字、ä/ö/ü/ß、英文、希腊前文化码和姓名；FreeType atlas/fallback/cache 预算单列。光栅化不自动解决 shaping、IME、换行；HarfBuzz 按 RmlUi 实测再决定/锁定，不能假称接入。

SDL3 IME composition 与 committed text 分开；候选框跟 caret/DPI，组合未提交不发重命名。验证中文输入、德语死键/组合重音、复制/粘贴/选择/撤销/多行、字符验证和转义。长德语允许换行/扩宽，不盖地图或截关键词；必要图标+tooltip仍需读得懂风险/选择状态。

验收：三语 key/占位符完整；德国长词/扩展伪本地化；100%/150%/200% DPI；焦点/tooltip；320px 原型无溢出，原生最低尺寸实测；切语不变 World hash/RNG/payload。正式范围只含英汉德，不提前扩语言数量。

## 8. 基础世界门槛后的建筑、人物与美术

阶段 0—2 的可见工作是主页面、创建界面、真实地表与水体、三语底栏、相机和保存载入；不依赖放人或已有建筑。进入世界时人物、国家、城市、建筑和建设任务全为空。世界未发布前底栏和信息页不获得 World 引用，发布后只有接入功能可执行；未接入项给出明确状态而非假成功。

基础空世界门槛通过后，阶段 3 的第一批资产包含原创住宅、CitySquare、工坊/Port 代表件、道路/桥/岸壁、长方体地基、台阶/坡道、接货区/逐层矩形网；同时统一成人角色、内衣/T恤/短裤、idle/walk/carry/enter/exit。其他楼族、原创军装、车辆按后续领域批次启动，不把阶段 3 扩成全资产交付。初始地表和 UI 的制作在前，已有建筑灰盒来源与约束继续保留。

住宅同一合同定义 footprint/门/garage/全宽接入/前侧接货/网罩层与包围尺寸；约数秒围起、工期全包围、约数秒反向拆。人物实际送货，本体工期 World 结算。港口不借水上基绕 reclaimed-only；美术不刷平天然地形。

准入记录作者/许可原文、原件 hash、坐标/尺寸、recipe、离线工具、派生 hash、材质/LOD/权重、权威几何 hash、场景。胸罩来源不明用原创替代，不上传冲突文件。0/18 岁共成人尺寸/骨架/绑定/fit，不造儿童 rig/成长缩放。

普通 Git：代码/recipe/RML/RCSS/语言/shader/manifest/notices/fixtures；Git LFS：合法 `.blend`/人物/衣物/纹理/动作大原件；GitHub Releases：固定 source commit/SHA 的可重建 pack/平台发行包。个人存档/cache/工具安装/凭证不入 Git。原创建筑/UI 不自动获第三方开源许可。

## 9. 第一可玩空世界的退出条件（阶段 0—2）

1. 原生启动到主页面；新建、继续、加载、英汉德设置和退出各有真实状态。无可载存档时继续禁用并说明；未创建世界不存在对象引用或伪统计。
2. 创建从一个已准入、离线齐全的真实区域 GeoPack 选择合法含陆地范围；显示来源/覆盖/分辨率/比例/预算。最终自然 height/mask/worldScale 固定，三轴同一比例，不自动生人、国家或房屋。现有 25×25 ETOPO 窗口不是完整 GeoPack 或全球覆盖证据。
3. 原生地表/水体、旋转平移缩放、边界/拾取、底部七组入口、中文/英文/德文与设置可使用；无对象时点地面给地理反馈，空目录明确为空。尚未接入人物/建设/制度命令不可伪造执行。
4. 创建→进入空图→保存→返回主页面→继续/加载同图可重复；冻结自然层与 core checkpoint 同摘要，镜头/locale 单独恢复，不加速离线时间、不添人房。
5. 缺地理块、非法选区、损坏或不兼容存档、创建中取消、保存失败、异步旧结果、退出/切换时 dirty 表单均有验证。失败留旧世界与最后有效存档，成功回菜单后撤旧订阅，不存在半个已进入世界。

这些是建筑资产和建设施工的硬前提；当前浏览器原型和 headless 切片不能替代原生链路通过。具体失败矩阵与验收故事见 [APPLICATION_FLOW](APPLICATION_FLOW.md)。

## 10. 第一生活切片的生产接入退出条件（阶段 4）

1. 阶段 0—2 空世界基础门槛和阶段 3 原创代表件/成人资产/坡地工程通过；干净 clone 可构建既有 headless，原稿/目录/有限规则仍通过，无 planned 依赖假标 integrated。
2. 原生底部 section→放人→人物页→实际已形成的关联对象；国籍/国家尚无时明确显示未形成，不赠国家填页面。三语/IME/DPI可用，窗口点击不穿地图。
3. 同一权威 World 真正完成 18/0 岁、坡地接入、人物前侧送货/自动施工围网、基本制衣/穿用、规则/收藏/清理/保存；不能四套 oracle 拼截图。
4. 核心脱离窗口也可跑同场景；镜头/locale/收藏不改结果；陈旧/异常失败不分裂事实。
5. 按 [施工计划](../planning/IMPLEMENTATION_PLAN.md) 门槛推进，未完域明确待接入，不填伪数据。
