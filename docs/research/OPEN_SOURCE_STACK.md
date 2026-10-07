# 开源复用决定、许可边界与锁定计划

核验日期：2026-10-06。资料来自上游项目、作者或官方规格。**“选择”是工程决策，“集成”必须有实际构建与运行证据。** Abyssal已固定commit/原文件SHA-256并构建CPU参考；MakeHuman人体/骨架/权重及三款服装的选定源件已归档，尚未变成游戏角色。下表运行时库仍待接入时锁定兼容版本，不编造未经构建验证的release/commit。状态与许可写入依赖清单。

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

## 3. 用户指定的 Abyssal Ocean 复用

采用[abyssal-ocean](https://github.com/squall01337/abyssal-ocean)，固定commit `142265f5013b6f27bea4f4f819b832dec75c7bad`。上游LICENSE、README、index.html准确字节已进入本仓库，MIT作者通知和每文件SHA-256见[UPSTREAM](../../third_party/abyssal-ocean/UPSTREAM.json)及[第三方通知](../../THIRD_PARTY_NOTICES.md)。这是已实际归档的选定来源，覆盖旧版“拒绝FFT海洋”的决定。

原项目WebGL2/three.js 0.180.0调度须移植到C++20/bgfx；不采用浏览器作为生产客户端。已从上游蝶形与inverse FFT流程提取独立CPU正确性参考，并核对直接2D IDFT、幅值/方向/Hermitian谱及错误边界。CPU参考不执行全谱海洋、GPU材质或实际帧渲染；上游HTML的three.js CDN依赖未归档，不声称完整离线浏览器demo。

native移植保留谱/cascade/泡沫算法，重写GPU资源生命周期、有序pass、shaderc采样/uniform、depth/UV、浮动原点相位和独立海陆mask。真实海底高程用于表现时须与ETOPO/世界datum一致，低于海平面LAND不能被shader淹没。TMA/浅水衰减不等于真实河湖/水动力；海面浪高仍不写仿真。成本与跨后端验收见[RENDERING](../architecture/RENDERING.md)。

仍拒绝游戏引擎运行时、未明确许可shader、许可不明的示例HDRI/贴图，以及直接依赖WebGL framebuffer假设的生产代码。bgfx基础和合法CC0材质可以补充该移植，保留各自通知。[bgfx shader工具](https://bkaradzic.github.io/bgfx/tools.html)、[示例资产许可](https://bkaradzic.github.io/bgfx/license.html)。

## 4. 人物、材质与字体的可用来源

|资产/工具|决定|许可、边界与进入工程方式|
|---|---|---|
|人物基体与工具生态|正式采用MakeHuman v1.3.0人体数据与MPFB v2.0.17离线路线；少量男女/年龄体型统一拓扑/骨架|MakeHuman程序AGPL-3.0-or-later、MPFB程序GPL-3.0-or-later、核心图形CC0分开；工具未运行且不链接游戏。已归档源件/锁定commit与限制见[生态核验](MAKEHUMAN_ECOSYSTEM.md)、[精确manifest](../../assets/manifests/makehuman_sources.json)。[官方许可说明](https://static.makehumancommunity.org/about/license.html)|
|人物动作|先制作最小 idle/walk/work/carry/attack/enter/exit 套件；只有单项来源/许可清楚的外部动作才导入|ozz 提供运行能力，不意味着动作资产 MIT。动作须统一骨架、单位、loop、root motion 与事件时序；战斗/劳动事件只读仿真|
|服装|正式复用MakeHuman社区逐项许可清楚的衣物；军装统一款式由项目原创|[ADR 0004](../decisions/0004-clothing-and-makehuman.md)按用户要求覆盖旧极简服饰约束，衣物成为耐久/库存/产业系统。首批男女T恤和短裤源件已归档，缺贴图与游戏适配另验；胸罩候选存在许可证据冲突，未导入。共享mesh不等于没有实物衣橱；不用自由布料物理|
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
