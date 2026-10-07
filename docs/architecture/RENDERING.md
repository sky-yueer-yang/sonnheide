# 原生三维客户端与表现层架构

设计基线：`Sonnheide_Complete_Design.md` v0.6；本方案日期：2026-10-06。本文是生产客户端的设计与验收契约，不表示图形依赖、PBR、水体、人物或 UI 已经集成。当前可执行能力以仓库构建目标及测试结果为准。

## 1. 已作出的选择与真正的难点

生产客户端采用 **C++20 + SDL3 + bgfx**。SDL3 负责窗口、输入、设备与平台事件；bgfx 负责跨图形 API 的资源及提交。我们的代码负责场景表示、相机、材质、渲染通道、资源流送、LOD、动画调度、拾取、UI 绑定和工具。仿真没有 SDL/bgfx 类型，也没有图形设备依赖。bgfx 官方将自身定义为需要应用自行提供框架的渲染库；不是 Unity、Unreal 或 Godot 式游戏引擎。[bgfx 官方边界说明](https://bkaradzic.github.io/bgfx/overview.html)

初版选择 macOS/Metal、Windows/Direct3D 11、Linux/Vulkan 为验证路径。Windows 的 D3D12 是后续性能验证候选；Linux 的 X11、Wayland 各须验证。支持某个平台的库不等于我们的游戏已经支持该平台。最低 OS、GPU、显存及驱动要求必须在实测后冻结。

五个问题比“把模型画出来”更关键：

1. **模拟规模与渲染规模分离**：全部 Person 持续存在，只有一小部分接受高频骨骼采样；不以隐藏对象来停止经济或战争。
2. **规则格网与曲线岸线一致**：天然矢量岸线、冻结逻辑掩码和新增矩形人工陆地必须同时可读；不能让水材质或漂亮岸壁改写可通行性。
3. **表现与命令的空间语义分离**：矿物曲面的高峰、旗面和文化波纹不得截走工具射线；一个世界位置的定义必须独立于当前特效。
4. **异步表现仍需因果一致**：LOD、网格构建、GPU 回读会滞后，必须携带版本；不能点击到已删除建筑或把未完成填海画成可建设陆地。
5. **跨后端一致**：Metal、D3D、Vulkan 的深度、反射纹理方向、shader 编译和窗口生命周期一起验证；只在 OpenGL 演示中正确的水体不能算复用成功。

SDL_GPU 同样是可行的无引擎路径，并提供 Metal/Vulkan/D3D12 与独立 shadercross 工具。选择 bgfx 是为了复用其跨后端提交、shaderc/texturec 工具和相关示例，而非认为 SDL_GPU 不能做三维。本阶段不同时维护两个图形后端框架；若 SDL3/bgfx 窗口互操作在目标系统长期阻塞，先以同一验证场景比较 SDL_GPU 再更改 ADR。[SDL_GPU 官方文档](https://wiki.libsdl.org/SDL3/CategoryGPU)

## 2. 边界、线程与数据流

```text
SDL 事件 → InputRouter → 当前 Tool / CameraController / UI
                                     ↓
                            Command 草稿 / PreviewQuery
                                     ↓
                          仿真验证与原子提交屏障
                                     ↓
不可变已提交快照 → PresentationBridge → RenderScene / Inspectors
                                           ↓
                       剔除 → LOD → 动画预算 → 批次 → bgfx
```

`PresentationBridge` 消费一致快照及变更记录，不拿可写领域对象引用。身份使用稳定实体 ID；图形对象使用带 generation 的本地句柄；两者不是同一种 ID。GPU 资源、骨骼采样缓存、相机跟随及隐藏代理都不进入游戏存档。关键数据按以下边界传递；实际字段名随快照协议统一。

|快照内容|表现使用方式|禁止行为|
|---|---|---|
|`tick/revision/simTime` 与两份已提交运动状态|仅连续移动做插值；暂停固定时间|靠帧数推进装填、矿物再生、劳动产出|
|稳定 ID、位置、朝向、容纳状态、任务与视觉变体|创建/更新/回收视觉代理；入楼后隐藏|隐藏等于删除 Person、Vehicle 或 Cargo|
|天然图层哈希、人工陆地版本与 dirty chunk|冻结天然网格；仅更新受影响人工网格|由水 shader 修改 `effectiveSurface`|
|国家旗定义、主题色、主权与占领关系|不同图层绘制法定主权和占领|占领自动覆盖主权或人物国籍|
|矿物格心值、稀疏 GenesisNode|生成只读曲面与视觉插值|插值点成为新矿格或文化仿真节点|
|建筑性能定义、外观 ID、入口与 footprint|表现外观和选取体；按领域预览放置|换皮肤增加容量或移动入口|

仿真线程发布快照后，表现线程可保留其所有权直到消费结束。首版使用简单的有界快照交换；人口增长后以共享不可变 chunk 和 dirty 列降低复制量，禁止为每个渲染帧序列化全量 JSON。过期表现快照可以跳过；命令、提交事实与历史事件不能丢弃。抓取/释放、传送、入楼/出楼、死亡带有空间不连续标记，禁止在两个端点间插值穿城飞行。

窗口事件及要求主线程的平台操作留在主线程；bgfx API 提交集中在确定线程。首轮平台验证只用单一提交入口，不提前把所有 draw 分散到工作线程。CPU 资源解析、网格生成、LOD 准备可在任务池执行；完成后通过有界上传队列交给图形线程。bgfx 自身有 API/render 线程分工，不能再建立一套不受控的多线程窗口或资源释放循环。[bgfx 线程说明](https://bkaradzic.github.io/bgfx/internals.html)

## 3. 坐标、相机、深度与平台接入

世界采用右手坐标、Y 向上、X/Z 为地图平面。领域的绝对位置与格索引保持稳定；GPU 使用相机附近原点的 float 相对坐标，原点改变不写回仿真。glTF 的 Y 向上、单位及坐标约定在资源编译时统一转换；Blender 原文件不直接作为运行时资产。[glTF 2.0 官方规格](https://github.com/KhronosGroup/glTF/blob/main/specification/2.0/Specification.adoc)

相机保存焦点、距离、yaw、pitch、FOV 和跟随目标；yaw 可连续转满 360°。pitch/距离受防穿地和可读性边界限制，但不锁成固定等角视图。缩放围绕光标命中的地面点；世界尺度 `worldScale` 不受镜头改变。窗口尺寸以逻辑 UI 点计，GPU viewport/拾取纹理以实际 drawable 像素计，HiDPI 换算在一处完成。聚焦人物/建筑/CitySquare 用其世界锚点，跟随不改变其模拟路线。

必须实现并统一使用 `ProjectionConvention`、`screenToWorldRay`、`worldToScreen` 和 `renderTargetUv` 四个辅助接口：

- 从 `bgfx::Caps::homogeneousDepth` 取 NDC 深度范围，从 `originBottomLeft` 取相应原点约定；不能在 shader 中无条件写 OpenGL 的 `depth * 2 - 1`。仅当范围是 `[-1,1]` 时作该深度转换。
- shader 重建世界位置使用与当前 GPU 投影对应的逆 VP；CPU unproject 必须使用同一投影，不能另一处生成默认 OpenGL 矩阵。
- 屏幕 UI、NDC、RT 采样方向分别转换；平面反射还涉及镜像矩阵与剔除 winding，不能用一个全局“翻 Y”掩盖全部问题。
- 初版使用常规深度并收紧 near/far；reverse-Z、无限远投影待独立验证后加入，不能混用 clear-depth、比较函数和重建公式。
- 射线近平行地面、命中位于相机后方或世界范围外时明确返回无命中；不将极大交点 clamp 到岸边后提交命令。

这些 caps 是上游公开接口；锁定代码后以该 commit 的头文件为准，在线文档不能替代实际编译接口。[bgfx Caps 源码](https://github.com/bkaradzic/bgfx/blob/master/include/bgfx/bgfx.h)

`PlatformSurface` 是我们唯一的平台接入口。Windows 用 SDL3 提供的原生 HWND；X11/Wayland 使用对应 display/window/surface 及 bgfx 原生窗口类型；macOS 验证 NSWindow 与所锁定 bgfx 的 Metal 接口所需句柄。在需要显式 CAMetalLayer 的路径使用 SDL 的 Metal view/layer 接口，并明确谁持有、谁销毁。禁止把 SDL2 的 `SDL_SysWMinfo` 代码直接复制进 SDL3。最小化、resize、Retina 比例变化、全屏、窗口销毁先跑通再进入完整渲染。[SDL3 原生窗口属性](https://wiki.libsdl.org/SDL3/SDL_GetWindowProperties)、[SDL Metal View](https://wiki.libsdl.org/SDL3/SDL_Metal_CreateView)、[SDL Metal Layer](https://wiki.libsdl.org/SDL3/SDL_Metal_GetLayer)

## 4. 我们自己的渲染通道与 PBR

初版采用简单 forward 渲染；白天主日光 + 环境光满足核心地图观察。首轮不引入 deferred、实时 GI、光追、虚拟几何或必须依赖 compute 的 GPU-driven 路径。需要更多局部灯时再依据实际可见负载决定 clustered forward。

|顺序|通道|内容与约束|
|---|---|---|
|1|资源上传与可见集|缓存资源；空间 chunk 与视锥剔除；不逐对象创建材质|
|2|阴影|日光级联阴影；近景重要人物；远景不做高成本动态阴影|
|3|不透明世界|冻结真实高程地表、人工平台、地基/坡道、道路、原创建筑、人物、树木与交通资产|
|4|水前颜色/深度准备|仅在水体质量等级需要时复制或解析采样源；禁止读写同一 attachment|
|5|水与透明世界|Abyssal谱波位移水面与独立WATER遮罩；限制透明层数量与overdraw|
|6|领域 overlay|国旗裁切、疆界、矿物曲面、文化场、命令预览；按模式只开必要层|
|7|后处理|固定曝光基线、色调映射、轻量抗锯齿；UI 之前完成|
|8|界面与标签|检查器、工具状态、标签、选择轮廓；UI 像素色与地图灯光分离|
|按需|精确对象 ID|仅点击需要时渲染候选；异步回读或 CPU 代理回退|

每个通道声明输入、输出、尺寸、格式、MSAA 和顺序，使用固定 view 分配表及显式依赖；不把 bgfx 的排序机制当作自动 frame graph。首版 frame graph 是我们的小型 pass 列表，只有实际使用的通道。透明水、矿物曲面与标签的组合要专测；不能通过打开全部透明 overlay 得到不可读地图。

材质基线是 metallic-roughness PBR：base color/normal/metallic/roughness/AO、线性空间光照、IBL、主日光阴影、受控曝光。base color 与 emissive 纹理按 sRGB 解码，数据纹理保持线性；切线、normal map 方向在导入时验证。批次 key 为 mesh/LOD/material variant/render pass；国家主题色使用实例参数，不能为每人复制材质。UI 国旗保存固定 8-bit 色表，不因渲染光照生成第 24 种可选色。

可有选择地移植 bgfx 官方 IBL/阴影示例代码并保留 BSD notices；生产材质的公式与能量响应参考 Filament 的官方 PBR 文档。代码复用需要具体文件许可，示例配套模型、HDRI、贴图、字体分别审查。不能将整份示例资产包视为 BSD。[bgfx IBL shader](https://github.com/bkaradzic/bgfx/blob/master/examples/18-ibl/fs_ibl_mesh.sc)、[Filament PBR 文档源码](https://github.com/google/filament/blob/main/docs/Filament.md.html)、[bgfx 示例资产许可](https://bkaradzic.github.io/bgfx/license.html)

## 5. 真实地形、天然海岸与人工港区

天然陆地由冻结矢量岸线与真实高程场共同生成，渲染切成空间chunk；坡度影响地面可达与建址，详见[ADR 0003](../decisions/0003-terrain-and-site-access.md)与[地形/建址合同](TERRAIN_AND_SITES.md)。近景网格和远景LOD使用同一源/基准/比例；视觉误差不能作为建造或通行依据。天然岸线附近提供 `CoastCoverageIndex`。人工造陆另有固定工程表面，生成格网平台与原创直线挡墙；地基/坡道是独立实体，不修改天然高程。挡墙、建筑与坡道几何全部由我们制作，不能平滑人工直岸或扩大规则footprint。

人工陆地完成事务发布 chunk generation。网格任务只接受该版本，并将其结果与当前 generation 比较；落后的结果直接丢弃。施工时显示桩/驳船/进度，底面继续 WATER；导航临时障碍不等于已完成陆地。提交完成后，权威可通行性立即更新；渲染若尚未重建，先用简单完成平台覆盖，禁止让 UI 呈现“可行走但仍显示水”的长窗口。

Port 预览只画四项：已完成 RECLAIMED footprint、整排 sea-facing edge、BerthWaterZone、陆侧道路入口；旋转仅 0/90/180/270°。放置是否合法、后续填海是否侵占泊位、升级能否扩 footprint 都由领域查询返回；preview mesh 不自行做另一套规则。拆港不删除人工陆地。道路/桥和入口的视觉网格不能成为唯一导航来源。

**切片 A 必须冻结的岸线决定**：v0.6 明确冻结逻辑地表与矢量天然岸线，却未完整给出“天然岸线部分覆盖格内建筑 footprint”的覆盖阈值。应明确哪些天然临海格可支撑整个 footprint，并让同一覆盖查询用于放置、鼠标预览和场景一致性检查。它不允许通过 water shader 的 alpha 或 GPU 像素决定法律边界。规则冻结前，天然临海部分格只允许展示，不应宣称完整建设交互已经验收。

## 6. 水体：Abyssal Ocean 原生移植

用户指定[abyssal-ocean](https://github.com/squall01337/abyssal-ocean)。锁定commit `142265f5013b6f27bea4f4f819b832dec75c7bad`、MIT原文与三个上游源文件的准确字节，见[来源清单](../../third_party/abyssal-ocean/UPSTREAM.json)。上游是WebGL2/three.js 0.180.0浏览器实现；我们复用其算法/shader并移植到自研C++/bgfx客户端，不把浏览器运行时链接进核心。当前已构建的部分是独立[presentation CPU FFT参考](../../presentation/src/ocean_fft.cpp)，尚无GPU水面或实机帧率证据。

原生端应依次接入：seeded谱初始化→时间演化→横/纵inverse FFT ping-pong→位移/导数合成→多cascade采样→折射/反射/泡沫/水下表现。上游默认512²×3 cascades的FFT链约63个pass/帧，仿真render targets估算约52MiB，尚未含颜色/深度/反射与窗口缓冲。此为源码预算估算，不是目标设备测量；首轮低/中/高质量冻结cascade尺寸与更新频率，逐步对照参考输出，不在游戏CPU每帧执行此参考FFT。

关键移植门槛：GLSL3改为所锁shaderc的uniform/sampler/varying和矩阵约定；每个依赖pass用有序view，显式声明RT读写/格式与floating-point renderability。当前CPU参考保留上游正指数inverse、centered谱的checkerboard修正与不除N²约定，并用直接2D IDFT核对。禁止换一套FFT归一化后仅靠调波高掩盖错误。世界位置用相机相对float，但谱相位用冻结的全局位置对cascade周期取模，浮动原点不能让波跳动。同步GPU高度回读替换为异步且只供表现；核心不消费非确定波高。

`WaterRenderer`输入：独立权威WATER mask、完成填海更新、同一datum下的平均水位/真实海底高程、可见chunk、风/谱质量profile、日光/IBL、相机与纯表现时间。高程负值不能代替mask；低于海平面的LAND必须保持干燥。上游TMA全局depth和局部浅水衰减不能宣称为真实浅水水动力、河流流动或湖面求解；内陆水域须有单独固定水位profile，不能全套用海平面。默认程序岛生成不进入真实世界。

水体输出只含表现draw/pass；视觉浪、泡沫和船尾迹不改水陆、地形、地基、船导航或材料。不加入侵蚀/潮汐/刷地形玩法。水前scene color/depth按caps处理，禁止读写同一attachment；可选低分辨率反射与水下效果以实际预算决定，基础海岸遮罩必须正确。

回归场景：真实弯岸/阿尔卑斯高程、低于海平面LAND、人工直墙与四旋转港口、窄航道、内陆湖、填海完成更新、相机贴水/跨海拔/resize、浮动原点、国旗/矿曲面叠加。必须逐后端核验FFT幅值/方向、反射/深度UV、LAND干燥、GPU资源寿命与frame CPU/GPU p95/p99。CPU数学通过不能替代这些GPU验收。

## 7. 人物、动画 LOD 与原创建筑 HLOD

人体正式采用MakeHuman/MPFB生态中经准入的CC0核心基体、rig/weights/targets，再统一拓扑、骨架、皮肤和LOD；社区资产逐文件授权，具体已取入范围见[MakeHuman记录](../research/MAKEHUMAN_ECOSYSTEM.md)。动画运行时采用 ozz-animation，其采样、混合及压缩不负责 AI、碰撞、路径或伤害。重定向器、任务到动画的映射与动作资源需要我们制作或单项筛选，不能声称引入 ozz 就获得动作库。[MakeHuman 代码/资产/输出许可边界](https://github.com/makehumancommunity/makehuman/blob/master/LICENSE.md)、[ozz-animation](https://github.com/guillaumeblanc/ozz-animation)

服装按[ADR 0004](../decisions/0004-clothing-and-makehuman.md)由真实穿着快照决定：前文明黑内裤/女性胸罩，上古国家主题色T恤+白短裤，后期已购置多样自由衣物。军装原创且各国家共用同款，颜色取实际服役国家，法定国籍另存；公司正装规范只提高员工购买优先级，缺正装仍正常工作/领薪。共享mesh/骨架/LOD/材质组合，theme binding与自由colorway分开，换国家主题色不增衣物或重置耐久。真实耐久在仿真侧，与相机/LOD无关。衣物不足用覆盖fallback显示经济缺口，不以显示皮肤奖励或绕过实际购买。

按屏幕投影尺寸和迟滞选择 LOD，同时设置动画预算；以下为首轮实验配置，**不是实测性能承诺**。

|表示|骨骼与采样|适用状态|
|---|---|---|
|近景/选中|完整简约骨架，30–60 Hz 采样|可信人物比例、手持物、入口与跟随|
|中景|离线减骨/减面版本，10–15 Hz 采样并插值|可辨人流和阵形；有限共享动作相位|
|远景|极简三维代理/点/群体标记，无逐人完整姿态|保留战略位置、数量与可选 ID|

完整动画预算建议先试 256 个可见近景人；其余按屏幕尺寸、选中/跟随、正在发生的关键事件分配降级名额，不能因“全部在附近”解除上限。动作相位从稳定 ID 与已提交任务时间派生，选中对象升级 LOD 时不重新从第 0 帧开始。攻击、劳动与装填的结果只读权威时间；重播动画不能重复结算。

GPU 蒙皮按同 mesh/材质/骨架 LOD 批次，姿态 palette 使用受设备限制的缓冲/纹理方案；必须查询 caps 并限制每批角色数。支持不足时回退 CPU 蒙皮较小近景集合。共享姿态相位是中远景优化，近景用必要的独立姿态。`meshoptimizer` 可以减面/优化顶点，**不能自动解决骨骼删减、衣服穿模、动作重定向**。[meshoptimizer 官方能力与限制](https://github.com/zeux/meshoptimizer)

建筑模块、组合规则与每个正式建筑模型全部自制，外部建筑模型不得作为占位生产资源。建筑 HLOD 以城市/街区 chunk 为单位离线或增量合并；必须保留门、入口锚点、footprint 与可选实体映射。变更生成 dirty chunk，异步合并完成前保留原实例。远景点击城市代理返回 CitySquare；近景按真实建筑选择，不把一个 HLOD mesh 误当成单个产权主体。

建筑施工遵循[ADR 0002](../decisions/0002-building-construction.md)：人物只取料/持物行走/前侧放料，不提供砌筑动作；本体自动工期由仿真推进。`ConstructionProxy`消费工程快照，显示`RAISING→ENCLOSED→LOWERING→READY`：原创长方体四面网罩逐层围起（初值3秒），顶盖闭合后保持全包围，可选预制中段clip，完成后同一采样函数反向逐层拆去（初值3秒）。网罩网格共用、按bounds实例化并批次提交；完整bounds参与剔除，远景减少线网密度。先用少量真实工地测透明overdraw/锯齿和LOD，不逐栋建立骨骼施工动画。

网罩是表现，没有铁料消耗、城防、碰撞或施工劳时；禁入来自权威工程占用。表现秒随暂停冻结，不因游戏加速成倍缩短。完成提前到达可先围完再逆序拆，但功能按已提交完成事实生效，全部已启用入口/装卸路径的网片立即让行。取消只退出当前覆盖程度，场地复用立即淘汰旧工程网片；离屏不补播，载入WAITING_DELIVERY无网罩、AUTOMATIC_BUILD直接全网罩、COMPLETE直接成品；同job/generation/completeRevision重复事件不重置。详见ADR中的材料锚点、合法工程范围和时间合同。

纹理图集/材质实例/纹理数组可按平台资源约束组合。虚拟纹理只作为后续候选，不能在初版预算里假定免费存在。所有静态 LOD 与 HLOD 都记录构建工具版本、输入 hash 和误差；道路/岸壁边缘需要锁边，避免 chunk 间裂缝。

## 8. 对象拾取、矿物笔刷与政治/军事 overlay

三种拾取通道分开：

1. `TerrainSurfacePick`：屏幕射线与冻结权威地形/已完成工程表面求交；水域工具命中固定水位。用于矿物、人工造陆、道路、阵线与抓取中心；无视矿曲面、视觉波浪、旗面、文化节点和当前GPU地形LOD。
2. `SceneObjectPick`：空间 chunk 广阶段 → AABB/简化碰撞体 → 必要时 mesh BVH。建筑、人物、CitySquare 与车辆有不同 pick mask；容纳中对象从世界 pick 集移出但仍可在检查器查询。
3. `VisibleIdPick`：密集近景需要时做小区域 ID pass；无 MSAA/色调映射/颜色混合，以每帧局部 ID 映射回稳定 ID。GPU 回读携带 frame、camera、snapshot revision 和 generation；过期结果复验/重采，不直接提交命令。首次点击可先给 CPU 预选，不能每帧同步 readback 阻塞 GPU。[bgfx 按需拾取示例](https://github.com/bkaradzic/bgfx/tree/master/examples/30-picking)

资源曲面高度为 `surfaceBaseY + offset + visualScale * rate`，格心插值非负，显示与 tooltip 的量纲不同。夷平 anchorRate 在 pointerDown 锁定原始场值；笔刷扫过的是地面投影与格心距离，不是可见三角面。曲面只在资源模式启用，地面环、曲面光标、真实速率和格数同时显示。路径段补采样确保快速拖动不断裂，preview 不改权威库存。一次 stroke 对应一次稀疏命令/撤销事务；仿真暂停与编辑锁由命令系统控制，不能只停 UI 动画。

政治旗层每个当前显示政治单元仅一张整体世界 UV；飞地共用其联合范围，精确 territory mask 裁切。范围或主权变更触发 UV/mask dirty，不随相机 yaw 旋转。法定主权与军事占领独立；国家/城市/基层政治不同时盖三张旗。旗 texture 与字段变化缓存，国旗不会在每帧重新生成。PRE_CULTURAL 仍用单色希腊字母，不提前套正式模板。

军令 overlay 消费服务返回的草稿可行性：目标、路线、瓶颈、阵线、朝向、补给与拒绝原因。松开鼠标只提交 ArmyOrder 命令；红色不可行预览不自动绕过规则。形成阵位的世界端点固定在 X/Z，镜头旋转不翻转前后排。选中 Army 后只出现相关手势；远景可选群体代理必须映射实际 Army，不能新造“渲染子军队”。

文化场按稀疏节点插值成淡波纹；镜头远近只改变表现采样，不能增加模拟节点。国家标签选择境内可见锚点，遮挡检测与屏幕占用排序让选中者优先；战略层选国家/城市，近景选具体对象，使用明确模式与优先级防止层间抢点。

## 9. UI、输入仲裁与资源生命周期

生产 UI 使用 RmlUi + FreeType；Dear ImGui 限开发者面板。RmlUi 提供保留式文档、样式、布局和输入接口，我们实现 SDL3 事件桥、bgfx RenderInterface、数据绑定与行为；不导入一个浏览器或将 DOM 对象变为领域事实。RmlUi 本体是 MIT，但字体与示例资产另有许可证。[RmlUi 官方集成说明与许可证](https://github.com/mikke89/RmlUi)

输入经过唯一 `InputRouter`：UI 焦点/捕获优先 → 当前工具 capture → 临时相机修饰 → 观察默认动作。一旦 pointerDown 开始 stroke/阵线/抓取，记录 capture owner，松键、出窗口、取消时由同一个 owner 清理；切模式先取消草稿。Esc 先取消未提交草稿，再退出当前工具。资源模式滚轮调笔刷；显式修饰键才缩放；空格临时平移，中键旋转。所有映射进入配置并在底部状态显示，不在不同 subsystem 各自读键盘。

中英文、希腊码、IME composition、粘贴与 UTF-8 名称从切片 A 开始验证。FreeType 负责字形光栅化；复杂 shaping 可启用 RmlUi 的 HarfBuzz 示例字体引擎并固化适配，不能把“能加载中文字体”误认作已支持中文输入与换行。字体采用可分发 Noto Sans CJK 的指定子集/文件，逐页加载字形 atlas，避免一次栅格化全部 CJK。字号、DPI 与 atlas 页数共同受预算限制。[FreeType 许可](https://freetype.org/license.html)、[Noto CJK 字体许可](https://github.com/notofonts/noto-cjk/blob/main/Sans/LICENSE)

检查器遵循统一结构：名称/归属 → 四项核心事实与当前任务 → 三至六个动作 → 可展开细节。可用动作与拒绝原因来自领域查询；UI disabled 不替代后端校验。异步加载未完成时使用自制中性代理；代理不得伪装为建筑最终美术或改变实体位置。

资源流程为 `requested → loading → decoded → upload queued → resident → retiring`。请求按资产 hash 去重；CPU decode 有内存上限，GPU upload 有每帧字节预算，镜头预测预取但过期请求可取消。高 LOD 常驻，细 LOD 按使用度淘汰。资源退役必须等图形后端不再引用，`makeRef` 等零拷贝数据也需持有到 backend 消费；不能在 submit 后立刻释放源 vector。资源失败记录 asset ID/路径/hash/原因，继续可观察世界但不可假称资源已加载。

## 10. shader、资产编译与验收门槛

shader 源码、varying 定义、include 树、材质 schema 都进入 Git；生成产物按 `{bgfx commit, shaderc commit, target profile, defines, input hashes}` 做缓存键。bgfx、bx、bimg 与其 shader toolchain 一起固定兼容 commit，不能运行时下载最新版 shaderc 或只 pin 运行库。Metal/D3D/Vulkan shader 的离线构建作为 CI 的独立目标；一个后端编译成功不冒充其余后端成功。[bgfx 编译工具文档](https://bkaradzic.github.io/bgfx/tools.html)

源资产采用 `.blend`/合法人物来源/原始贴图，离线转换为经过验证的 glTF 中间文件，再由我们的资源编译器生成资源包。cgltf 只解析 glTF，不自动做动画 retarget、压缩 texture transcoding 或完整渲染；未支持的扩展明确拒绝，不能静默丢材质/蒙皮。最终资源包可离线运行，不在玩家启动时依赖资产网站。

首轮 1080p、60 fps（16.7 ms）只是测量目标。先分别记录 CPU 表现/提交、CPU 仿真、GPU 各 pass、p50/p95/p99、显存、上传字节、draw 数、可见对象、完整骨架人数和资源等待时间。不能把 CPU+GPU 时间机械相加，也不能用独立人物走路 benchmark 推断完整文明性能。以 100/1,000/10,000 人的真实任务、城市、港区和交战场景测量后设定设备档。

生产客户端接入需要依次通过：

1. 三平台窗口/resize/HiDPI/input/minimize/关闭，画面与资源生命周期无崩溃；shader 目标全部可编译。
2. 一座原创 CitySquare + 原创住屋 + 一个合法人物基体；360°相机、PBR、日光阴影；基本内存稳定。
3. 天然弯岸 + 单格人工填海 + 正交原创 Port；可行域、预览、网格版本和航路更新一致。
4. 矿峰后的画笔仍命中原地面点；相机 0/90/180/270°、HiDPI 与多个后端投影/反投影一致；锚定夷平不漂移。
5. 旗整体世界投影、政治层切换、占领层、国家主题色人物批次、中英文检查器与 IME 可用。
6. 连续拉远/拉近与跟随时动画/HLOD不闪切，不重置任务；选中对象优先；异步拾取不选择已回收 ID。
7. 真实负载与长期运行记录，并证明相机/渲染开关不改变权威仿真结果。

以上门槛完成前可交付可执行 headless 核心、设计及资源约束，但不能声称已交付写实三维客户端。
