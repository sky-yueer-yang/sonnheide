# 开源复用决定、许可边界与锁定计划

核验日期：2026-10-06。资料全部来自上游项目、作者或官方规格。**“选择”是工程决策，“集成”必须有实际构建与运行证据；本表没有将未下载的库写成已集成。** 本次不编造未经构建验证的 release/commit。每项版本均待接入时锁定，并将准确 commit、源归档 SHA-256 与许可证写入依赖清单。

## 1. 选择独立库，不引入游戏引擎

生产技术栈为 C++20 + SDL3 + bgfx。我们实现领域仿真、事务、调度、场景、相机、渲染通道、PBR/water 材质、LOD、工具、编辑与存档；开源库承担稳定、可明确隔离的底层任务。只有一个渲染库负责 GPU 资源和提交，同一窗口不再同时使用 SDL_Renderer、SDL_GPU 和 bgfx。

|职责|选择与上游|许可证核验|版本/集成状态|我们必须实现与主要风险|
|---|---|---|---|---|
|窗口、输入、平台设备|[SDL3](https://github.com/libsdl-org/SDL)|[zlib](https://www.libsdl.org/license.php)|待选择稳定 release 并锁 commit；尚未在此文中声称集成|SDL3 与 bgfx 原生窗口桥、HiDPI、中文 IME；不能复制 SDL2 接口|
|GPU 跨后端提交|[bgfx](https://github.com/bkaradzic/bgfx) + 所需 bx/bimg|[BSD-2-Clause 主体；内含依赖与示例资产单列](https://bkaradzic.github.io/bgfx/license.html)|三项目按兼容 commit 一起锁；toolchain 同锁|自己做 PBR、scene/LOD、frame graph、水体；caps、shader profile、Metal/Wayland 接入实测|
|glTF 资产解析|[cgltf](https://github.com/jkuhlmann/cgltf)|MIT，见该仓库 LICENSE|待锁 commit；优先离线 importer，运行时只读我们生成的 pack|白名单扩展、路径/大小/访问器验证、坐标/骨架转换；parser 支持扩展不代表我们支持其渲染|
|骨骼动画采样/混合|[ozz-animation](https://github.com/guillaumeblanc/ozz-animation)|[MIT](https://github.com/guillaumeblanc/ozz-animation/blob/master/LICENSE.md)|待锁 release + commit；仅接入动画切片|任务映射、retarget、骨骼 LOD、姿态 GPU 批次；不附赠完整人物或动作资源|
|网格优化与减面|[meshoptimizer](https://github.com/zeux/meshoptimizer)|MIT，见仓库 LICENSE.md|待锁 release + commit；离线工具优先|材质/锁边/skin 权重保护、HLOD 身份映射；不能自动完成骨骼减骨和人物穿模修复|
|生产 UI 文档与布局|[RmlUi](https://github.com/mikke89/RmlUi)|[MIT 主体，示例/font 另列](https://github.com/mikke89/RmlUi/blob/master/LICENSE.txt)|待锁 release + commit；生产 UI 切片接入|我们写 SDL3/bgfx bridge 与快照 binding；不假定已有官方 bgfx renderer|
|字体光栅化|[FreeType](https://freetype.org/)|[选择 FTL，而非其 GPLv2 替代授权](https://freetype.org/license.html)|随 UI 依赖固定|动态 atlas、fallback、字形预算；保留 FTL 及署名|
|复杂字体 shaping 候选|[HarfBuzz](https://github.com/harfbuzz/harfbuzz)|[上游 COPYING 所列 MIT 风格许可](https://github.com/harfbuzz/harfbuzz/blob/main/COPYING)|按 RmlUi 字体引擎验证后锁；未当作必需已接入|RmlUi 示例 font engine 要适配/验证；IME、换行和 shaping 是不同问题|
|开发者 UI|[Dear ImGui](https://github.com/ocornut/imgui)|MIT，见仓库 LICENSE.txt|需要开发面板时锁；复用 bgfx 内含版本优先|只用于性能/调试/资源诊断；不代替完整玩家 UI 和本地化|

运行时新增依赖必须能给出清晰边界与实际收益。bgfx 已包含或工具链已经使用 cgltf/meshoptimizer/ImGui 时，检查是否可共享同一锁定版本；不要无意链接两套实现。FreeType 和 shader 编译器的传递依赖也必须入清单，不把顶层“MIT/BSD”当作整个分发包的许可。

## 2. 渲染路线比较与拒绝理由

|路线|上游可核验事实|此项目决定|
|---|---|---|
|SDL3 + bgfx|bgfx 是自行提供框架式跨 API 渲染库；有 Metal、Vulkan、D3D 与 shaderc/texturec 工具。[官方文档](https://bkaradzic.github.io/bgfx/overview.html)|选择。符合完全自搭框架；工具/示例可复用。但不自带写实游戏、动画/LOD/编辑器。接入复杂性由我们承担|
|SDL3 + SDL_GPU|SDL 官方 GPU API 提供 3D/compute、Metal/Vulkan/D3D12；shadercross 提供离线跨编译。[官方 GPU 文档](https://wiki.libsdl.org/SDL3/CategoryGPU)、[shadercross](https://github.com/libsdl-org/SDL_shadercross)|保留唯一退路。平台层更统一，可少一组依赖；如 bgfx 平台桥阻塞才用同一场景比较，不现在同时构建两套|
|Filament|官方定位是 PBR renderer，主体 Apache-2.0，有材质/光照实现。[上游](https://github.com/google/filament)、[许可](https://github.com/google/filament/blob/main/LICENSE)|不作为初版运行时。它不是完整游戏引擎，但采用其场景/材质体系会减少我们对自写渲染骨架的控制；作为 PBR 官方知识参考，其代码须按文件许可选择性复用|
|直接 Vulkan + Metal + D3D|三种原生 API 是各自平台接口|拒绝初版同时手写三套资源/同步/shader 层。增加的是平台维护，不直接突破本游戏的因果仿真和视觉规模问题|
|Unity/Unreal/Godot|完整游戏引擎生态|用户已明确“不用游戏引擎”，不采用；设计稿 20.10 中允许引擎的旧表述被本次用户要求覆盖|
|Three.js/浏览器生产客户端|设计稿只将其作为可替换原型|生产不采用。已有规则/UI 原型若找到可作为参考；不能为迁就旧渲染器降低三维近景、LOD 与原生离线资源要求|

不将 raylib、Sokol、Diligent 等再堆成第二套候选依赖目录。改变上述选择必须由可复现的阻塞或测量支持，不能用不断换库替代水/角色/交互切片的推进。

## 3. 开源水体怎样复用

本世界只有平面水和陆地，没有动态高度场、潮汐、水动力或天然岸线编辑。初版 water 是我们自己的薄模块：共享 PBR/IBL、两层法线、Fresnel、吸收色、岸缘表现；可选低分辨率平面反射。水表现读已有 WATER 与完成填海结果，不生成海洋模拟。

**已决定的复用**：bgfx shader 基础与官方 IBL/阴影示例中许可明确的代码；合法 CC0 法线/材质/HDRI。使用其 shaderc 把同一源码变成目标后端产物，并自行测试投影/depth/RT 方向。[bgfx Shader 工具](https://bkaradzic.github.io/bgfx/tools.html)、[IBL shader 来源](https://github.com/bkaradzic/bgfx/blob/master/examples/18-ibl/fs_ibl_mesh.sc)

**不接入的资源**：Unity/Godot 专用水体插件、只实现 OpenGL framebuffer 假设的 demo、未明确许可 shader、要求真实海底高度/复杂海洋 compute 的 FFT 项目。它们可以是算法研究线索，但不能登记为“可直接使用的跨平台 water library”。没有确认合适依赖时自写本游戏的有限需求，比移植完整海洋系统更具体。

从教程得知的公式与复制教程代码不是同一授权；GPU Gems、博客、视频网站内容不能仅因免费阅读而进入 GitHub 仓库。保留公式参考链接，代码以我们的实现或明确开源许可文件为准。bgfx 示例里的 HDRI、模型与字体与代码许可证不同，不能整包复制。[bgfx 资产清单](https://bkaradzic.github.io/bgfx/license.html)

## 4. 人物、材质与字体的可用来源

|资产/工具|决定|许可、边界与进入工程方式|
|---|---|---|
|人物基体|采用 MakeHuman 随发行包附带的 CC0 数据作为基体候选；选少量男女/年龄体型，统一拓扑与骨架|官方将程序代码列为 AGPL-3.0-or-later，随包图形数据列为 CC0，输出不是程序逻辑；第三方社区资产须查各自许可。MakeHuman 是离线工具，不链接进游戏。[官方许可说明](https://github.com/makehumancommunity/makehuman/blob/master/LICENSE.md)|
|人物动作|先制作最小 idle/walk/work/carry/attack/enter/exit 套件；只有单项来源/许可清楚的外部动作才导入|ozz 提供运行能力，不意味着动作资产 MIT。动作须统一骨架、单位、loop、root motion 与事件时序；战斗/劳动事件只读仿真|
|服装|极少共享 mesh，依据设计稿 20.2 改制或自制|不得通过外部套装把服饰扩为经济装备系统。人体、服装、贴图分别记录来源，不能继承人物导出文件旁随便一项许可|
|材料/HDRI|Poly Haven 的纹理与 HDRI 候选；不采购其建筑模型|官方资产为 CC0，可再分发；页面文字、logo、渲染展示图并非同样授权。只下载明确资产原文件及许可证据，不复制预览图作为贴图。[官方许可页](https://polyhaven.com/license)|
|字体|指定 Noto Sans CJK 字体文件，覆盖中英与希腊码；不依赖玩家系统字体|指定目录 LICENSE 为 SIL OFL 1.1。保留许可及字体命名约束；文件/子集 hash 入资产清单。[Noto CJK Sans LICENSE](https://github.com/notofonts/noto-cjk/blob/main/Sans/LICENSE)|
|建模工具|Blender 作为离线创作工具|Blender 自身 GPL 与用户创作模型是不同对象；官方说明创作输出归创作者。保留脚本、`.blend`、材质、导出配置，而不是只留最终 GLB。[Blender 官方授权说明](https://www.blender.org/about/license/)|

**所有建筑的几何、模块、组合和正式外观由我们制作。** 包括 SettlementCore、CitySquare/CivicHall、住屋、森林/矿洞入口、University、Factory、Port、Chapel/Cathedral/Temple、桥和人工岸壁；不会借“开源占位”绕过该要求。外部通用石/木/砖/金属材质可作为原创建筑的表面资源。农业植物、交通资产、武器/工具等虽可寻找开源来源，当前未核验具体文件，因此不承诺已经拥有完整可用资源包。

“免费”“下载按钮”“能从 glTF 导入”不构成许可。Mixamo、Sketchfab、游戏拆包或素材市场项目不默认视为开源；不能重分发的来源不满足全部文件进入 GitHub 的要求。此处不把这些网站登记为已批准资产来源。

## 5. 版本冻结、来源证据与 GitHub 托管

每个代码依赖进入工程前生成具体记录：`name/upstream URL/tag/resolved commit/source archive SHA-256/license SPDX/license files/build flags/platforms/patches/transitive dependencies/verification evidence`。bgfx 三件套与 shaderc 的精确版本按已成功构建的组合冻结。URL 指向 main/master 的资料是研究链接，不能作为可复现构建锁。升级走单独 PR，重复兼容场景与资产重建后才合并。

每个外部资产记录：`asset ID/作者/来源页/下载原文件名/下载日期/原文件 SHA-256/许可版本/许可证据文件/允许再分发/变更记录/派生文件 hash`；自制建筑记录作者与源文件同样完整。派生文件继承适用 notices，LICENSE 不因减面、烘焙或转换 GLB 消失。许可不明的文件停在本地未批准区，不进入发布包或 GitHub 托管目录；不能用“先传以后补”完成托管。

托管划分：源代码、shader、配置、RML/RCSS、构建脚本、许可证和资产 manifest 用普通 Git；`.blend`、合法原始人物/动作/贴图与大数据包用 Git LFS；可重建发布包和平台产物进入 GitHub Releases，均引用对应 source commit 和 pack hash。工具链及原始依赖可采用准确 gitlink 或带 hash 的源码归档；需保持可获取来源和许可证，不能把一个 upstream 链接冒充已经将自身全部文件托管。

GitHub 托管不自动等于我们决定开源自己的游戏：原创代码、设计与建筑的授权策略由仓库总策略决定；第三方各自保留原许可。打包工具生成 THIRD_PARTY_NOTICES 与 asset credits；CC0 即便不强制署名，也保留来源以支持后续复核。构建 cache 不进 Git，但其所有输入、参数与生成器必须可追溯。

接入验收的顺序是：许可证/来源可分发 → 精确版本锁定 → 平台构建 → 最小真实工作负载 → 性能/生命周期验证 → 进入发布依赖。任何一环没有证据时状态仍为候选或待集成。详细视觉验收在 [RENDERING.md](../architecture/RENDERING.md)。
