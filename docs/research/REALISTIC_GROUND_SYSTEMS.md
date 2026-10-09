# 写实陆地地表的开源系统比较与接入建议

2026-10-08版本裁决：本文旧平陆不可编辑、七分区、RECLAIMED-only港口或MakeHuman生产条款由[ADR0012](../decisions/0012-editable-3d-pixel-world.md)覆盖；未冲突的身份/经济/菜单/许可规则继续有效。当前像素路线见[v0.7](../design/Sonnheide_Design_v0.7_Pixel_World.md)，本文既有实测/源锁仅代表其原范围。

核验日期：2026年10月7日。范围是普通陆地的材质、自然分区、地被和实时表现；水体继续使用已选定的 Abyssal Ocean。地形几何遵循 [ADR 0010](../decisions/0010-flat-land-and-coastal-transition.md)：内陆统一平坦，天然水边窄带下降入水。本次是研究与接入建议，没有安装完整创作环境、导入正式地面资产或实现原生地表渲染。

推荐路线是 **真实扫描 PBR 底材 + 离线程序化变体 + 自研多尺度材质混合 + 有预算的地被实例**。扫描来源优先 Poly Haven 和 ambientCG；生成工具优先 Material Maker 与 Blender；抗重复算法优先评估作者的 hextile 实现。Infinigen 是有价值的离线自然资产候选，需要单独解决导出与减面。已核验候选中，没有可以直接嵌入当前 bgfx 客户端、同时完成这些职责的整套系统。

## 地表真实感需要突破的边界

### 平坦几何与真实材料分布

普通陆地没有海拔和坡度差异，常见山地系统的自动材质规则会失去区分能力。我们需要独立的空间权重场，让裸土、草、砂、碎石、落叶形成连续、成片而有细节的分布。地区底色、局部覆盖率和近景纹理是三个尺度，不能用同一张高频噪声同时代替。

材质分区从固定配置、独立视觉种子和世界位置生成。天然岸边参数只来自冻结天然岸线；填海的 `effectiveSurface` 只限制现有可见支撑，不能重新推导岸坡、移动旧地面或抬高房屋。以后道路、建筑清场、人工填筑等表现覆盖层读取已提交的领域状态，不反向改变可建设性。

### 足够真实的近景与不重复的远景

扫描材质能提供土粒、细砂、泥裂和叶片的细节，却不能自动解决几百米地面上的规则重复。先建立以物理覆盖尺寸为依据的 UV，再叠加低频色差、覆盖率变化和抗重复采样。随机旋转必须同步应用于颜色、粗糙度、法线及相关贴图；否则光照会与砂石方向不一致。

普通地面使用水平投影；岸坡沿同一世界坐标连续采样。初版无需给全部平面使用成本更高的三向投影。法线负责细小凹凸的光照，草簇、小石子和落叶几何负责少量真实轮廓。材质内部的 displacement/height 是微表面输入，首版用于法线烘焙和混合权重，**不驱动地面顶点、碰撞、入口高度或找平地基**。近景视差以后单独验证；首版避免用强视差造成脚底漂浮和道路穿插。

### 烘焙输出与实时资产

离线 Cycles 图片或研究系统生成的大场景，不等于实时 GPU 场景。网格需要明确的材质通道、法线约定、实例锚点和 LOD；地被还要控制透明像素覆盖、阴影和远景退化。缺少这些步骤时，免费高模也会成为性能障碍。

## 可复用系统的选择

|系统|官方能力与许可|接入方式|本项目判断|
|---|---|---|---|
|[Poly Haven](https://polyhaven.com/textures)|原始纹理资产 CC0，提供扫描地面 PBR；[许可](https://polyhaven.com/license)|离线取得选定原图，编译材质包|首批真实底材来源；不是地表生成器|
|[ambientCG](https://ambientcg.com/)|原始资产和每项材质预览 CC0；[许可](https://docs.ambientcg.com/license/)|离线选择摄影测量地面，统一通道与尺寸|第二扫描来源；不把所有资产都称为扫描|
|[Material Maker](https://github.com/RodZill4/material-maker/releases/tag/1.7)|MIT，节点式程序材质，PNG/EXR 出口；[导出](https://rodzill4.github.io/material-maker/doc/export.html)|离线 `.ptex` 配方生成变体，只导入贴图|程序材质首选；Godot 是其工具依赖，不进入游戏运行时|
|[Blender](https://www.blender.org/releases/4-5/)|GPL 创作工具，烘焙和 Geometry Nodes；自创输出许可与软件分开；[官方说明](https://www.blender.org/about/license/)|独立离线环境，处理扫描贴图、烘焙、生成地被和 LOD|整体资产生产基础；不把 bpy 链接核心|
|[Infinigen Nature](https://github.com/princeton-vl/infinigen/tree/nature-stable)|程序化自然场景和资产；指定自然稳定快照为 BSD-3-Clause|只生成选定自然素材，再烘焙、减面、转游戏包|地被扩展候选；不能把完整研究场景直接当实时地图|
|[hextile-demo](https://github.com/mmikk/hextile-demo)|Morten Mikkelsen 作者实现，MIT；原图采样的六边形抗重复混合|选择性移植着色器算法，保留通知|最有价值的独立运行时算法候选；不导入 DXUT/D3D11 演示框架|
|[Terrain3D](https://github.com/TokisanGames/Terrain3D)|MIT，Godot GDExtension，地形材质、去重复、植被和 LOD|参考其材质与远近采样设计，适配选定代码|不能整体接入；Godot 节点、材质语言和高度地图体系不适合本客户端|
|[MaterialX](https://github.com/AcademySoftwareFoundation/MaterialX)|Apache-2.0，材质表示与代码生成；stdlib 包含 hextile 节点|参考 GLSL 算法；材质图可作为离线交换候选|不引入整套生成器作为初版运行时；不自动输出可直接用的 bgfx 地表|
|[FastNoiseLite](https://github.com/Auburn/FastNoiseLite)|MIT，C++ 单头和多语言噪声实现|仅作为材质权重/变体的候选输入|不是写实材质系统；C++ 有符号溢出实测边界须先处理|
|[Filament](https://github.com/google/filament)|Apache-2.0 的 PBR 渲染器；[理论](https://google.github.io/filament/Filament.html)|借鉴 PBR、色彩和法线处理；代码逐文件审查|维持既有 bgfx 路线，不增加第二渲染器|

Material Maker 1.7 提供命令行材质导出和纯图像出口，但源码中 `--size` 写入 `texture_size`，最终导出却使用默认 `image_size=2048`。这是静态源码发现，尚未运行工具确认：准入时先修正参数传递或固定经过验证的图内尺寸，并检查真实输出宽高；不能承诺现成 CLI 已能任意批量输出 4K/8K。[固定源码](https://github.com/RodZill4/material-maker/blob/4c6cea67b659e1eb472f91590e06b2b1c5245916/parse_args.gd)

Material Maker 的社区配方、输入照片、画笔和环境分别保留许可；软件 MIT 不覆盖所有社区内容。Blender 的 GPL 同样不自动覆盖作者自创的图片、网格或 `.blend`，也不免除第三方输入资产的原许可。[Material Maker 资产说明](https://www.materialmaker.org/doc)、[Blender 输出边界](https://www.blender.org/about/license/)

## 其他候选与暂不采用的原因

|候选|核验结果|判断|
|---|---|---|
|[ArmorPaint](https://github.com/armory3d/armorpaint)|当前源码主体 zlib/libpng，PBR 绘制、烘焙及批量出口；AI 模型独立许可；[官方手册](https://armorpaint.org/manual)|后期绘制备选。旧 ArmorLab 仓库已归档迁移，不按旧教程引入第二工具体系|
|[VFX Texture Lab](https://github.com/MattyGWS/VFXTextureLab)|MIT，扫描处理、无缝化和多通道出口；Python/PySide6/wgpu 等离线依赖；2026 年 beta|功能贴近需求，值得样本比较；图格式与无界面生产流程验证后再决定|
|[TextureLab](https://github.com/njbrown/texturelab)|当前源码已改成 Qt6/C++；LICENSE 为 GPLv3，发行页另称 LGPL，正式 release 与源码改写有差距|暂不作为资产基线；不能照旧 Electron 文档建立流程|
|[Materialize](https://github.com/BoundingBoxSoftware/Materialize)|GPLv3 的 Unity 项目；照片推断材质，长期未更新|历史离线备选；单张受光照片无法保证准确恢复真实 PBR 参数|
|[AwesomeBump](https://github.com/kmkolasinski/AwesomeBump)|README 声明 LGPLv3+，Qt/OpenGL，公开版本较旧|历史备选，首版不建立新生产依赖|
|[ngPlant](https://github.com/stager13/ngplant)|核心 ngpcore/ngput/pywrapper 为 BSD3，界面工具为 GPL；独立植物建模与 OBJ 导出|小型植物生成备选；库可复用但维护较旧，数据库植物和纹理逐项许可，不是全站 CC0|
|[TexGraph Public](https://github.com/galloscript/TexGraph-Public)|公开的是节点 JSON/GLSL，未找到该快照的完整工具源码和项目许可|不具备完整开源系统准入条件|
|[HighMap](https://github.com/ottolink-dev/HighMap)|C++ 高度图与侵蚀库；当前 LICENSE 是 LGPL2.1，README 仍称 GPLv3；依赖 OpenCL/OpenCV/GSL 等|需求集中于高程，且许可声明需澄清；本次不接入|
|[TerrainGen](https://github.com/Ono-Sendai/terraingen)|MIT 主体，GPU 高程/侵蚀工具，依赖 SDL2 和 glare-core 等|主要能力是山地/侵蚀；不把它作为平坦陆地材质系统引入|
|[Gaea](https://www.quadspinner.com/Legal)|主产品按商业 EULA；公开文档和 Unreal 桥不等于完整生成器开源|不归入可托管全部源件的开源方案|

生成照片、生成高程、生成材质图、实时地表渲染是不同能力。尤其照片转材质输出六张图，并不能证明 albedo 已正确去光、粗糙度准确或法线对应真实几何；这是选择扫描底材优先的技术判断。

## 首批地面材质与地被

以下是已核验官方目录元数据的候选，尚未取得原始材质包。首轮选其中六至八种，覆盖普通陆地和水边所需变化；不把每张贴图直接等同于新的玩法生态区。

|表现用途|Poly Haven 候选|官方元数据中的物理覆盖尺寸|
|---|---|---|
|普通泥土与湿泥|[brown_mud](https://polyhaven.com/a/brown_mud)|约 1.30 m|
|草地底层|[grass_ground](https://polyhaven.com/a/grass_ground)|约 2.51 m|
|草与裸土过渡|[sparse_grass](https://polyhaven.com/a/sparse_grass)|约 2 m|
|森林落叶底层|[forest_leaves_02](https://polyhaven.com/a/forest_leaves_02)|约 3.001 m|
|干燥地面|[dry_ground_01](https://polyhaven.com/a/dry_ground_01)|约 4 m|
|砂砾|[sandy_gravel_02](https://polyhaven.com/a/sandy_gravel_02)|约 2.53 m|
|细砂|[sand_03](https://polyhaven.com/a/sand_03)|约 2 m|
|潮湿岸边砂|[damp_beach_sand](https://polyhaven.com/a/damp_beach_sand)|约 2 m|

尺寸是扫描覆盖范围，不是游戏世界大小；导入时换算到统一世界单位。`sand_03` 的官方文件目录包含 Diffuse、AO、Rough、OpenGL/DirectX 法线、Displacement 和打包通道等，因此可以取得分离的真实 PBR 输入，而不是使用网页渲染图。[官方文件元数据](https://api.polyhaven.com/files/sand_03)

ambientCG 的 [Ground037](https://ambientcg.com/a/Ground037) 可作为第二来源的地面样本。官方元数据标记摄影测量、约 2.1 m 覆盖；4K PNG 包约 280 MB、8K PNG 包约 1.055 GB。源文件体积与最终游戏显存需求是不同问题：合法源包托管在项目 Git LFS/Releases，运行时使用重新编译的压缩与 mip 资源。[官方元数据](https://ambientcg.com/api/v3/assets?id=Ground037&include=dimensions,technique,downloads,maps)

近景使用有限的草簇、苔藓、叶片和小石子，远处逐步退到低模/卡片，再退为地面材质。扫描草模型不能仅凭“CC0”通过实时预算：Poly Haven `grass_medium_01`、`grass_medium_02` 的官方 polycount 都超过一百万，此字段不是经过我们解析的三角形数。必须离线烘焙、减面并核验实际网格与透明覆盖。[候选一](https://polyhaven.com/a/grass_medium_01)、[候选二](https://polyhaven.com/a/grass_medium_02)

装饰草叶不产生木材、库存或可收获植物。树木、矿物及可采集植物的可见模型必须对应真实领域对象。装饰层受天然水陆、已完成填筑、道路、建筑占地和通行保护域约束；清理或建设后重建对应有界分块，不能靠视觉贴图掩盖仍然存在的领域对象。

Poly Haven 的原始资产许可与在线 API 服务条款分别适用。当前官方 API 页注明从 2026年7月18日起商业 API 使用也免费；服务仍要求识别请求来源，实时展示 API 内容时标明来源。游戏采用离线取得、项目自托管的 CC0 原图，不必依赖运行时在线 API。Poly Haven 的网页文字、Logo、展示 render 与原始资产不能混为同一授权；ambientCG 明确将每项材质预览 render 也纳入 CC0，两站规则不同。[Poly Haven API](https://polyhaven.com/our-api)、[服务条款](https://github.com/Poly-Haven/Public-API/blob/master/ToS.md)、[ambientCG 许可](https://docs.ambientcg.com/license/)

## Infinigen 的实际适用范围

Nature 稳定标签 `nature-stable` 的指定快照 `f5bcba8de47623da9b715348cf95822445e0e9a7` 的 LICENSE 为 BSD-3-Clause。当前 2.0 alpha 与这个旧自然资产快照是不同基线，不能混用其安装、配置和导出说明，也不能向所有历史版本泛化许可证。[指定许可](https://github.com/princeton-vl/infinigen/blob/f5bcba8de47623da9b715348cf95822445e0e9a7/LICENSE)

它适合离线产生一小组有变体的自然资产，然后进入我们自己的游戏资产编译器。其官方 exporter 会烘焙程序内容；自然稳定版本只处理 albedo、roughness、metallicity 等有限材质参数，大场景实例导 OBJ/FBX 会展开并消耗大量内存。因此不要导整片森林；提取单个地被原型，另做法线/AO/植物透光烘焙和 LOD。这里的自然资产范围不包括建筑、桥或人工岸壁。[官方导出限制](https://github.com/princeton-vl/infinigen/blob/f5bcba8de47623da9b715348cf95822445e0e9a7/docs/ExportingToExternalFileFormats.md)

该 Nature 快照要求 Python 3.10 和 `bpy==3.6.0`，不能假定与推荐的 Blender 4.5 LTS 共用环境。采用时建立单独固定的离线生成环境，导出标准资产后再进入主资产处理管线；不改变项目 Python3.9+ 标准库工具的默认要求，不把 NumPy/SciPy/OpenCV 或 Blender 带入核心。[固定环境声明](https://github.com/princeton-vl/infinigen/blob/f5bcba8de47623da9b715348cf95822445e0e9a7/pyproject.toml)

## 可选的真实地表覆盖数据

[ESA WorldCover 2021 v200](https://esa-worldcover.org/en/data-access) 提供约 10 m 的全球覆盖分类、11 个类别，CC BY 4.0；可用于未来地区覆盖率的离线参考。它不是 PBR 材质，也不是无人时代自然生态重建。现代建成区和农田不能在创建人口零、建筑零的世界时自动生成城市或农场；自然水陆仍以本项目冻结底图为准。冲突类别需采用明确的回退与掩码策略，不能由覆盖分类重画岸线。

建议首版以少量地区配置和程序分布完成表现验证，WorldCover 保留为可选增强，不增加全球下载门槛。它的 land-cover 分类地图与年度卫星合成图是不同数据；本次不采集高程或全球 RGB 影像。若以后采用覆盖数据，需保留 attribution、年份/版本、转换规则与源 hash；展示真实覆盖的依据和准确性范围也要清楚，不称为完整真实生态仿真。

## 自研地表接入结构建议

```text
经核验的扫描原图与许可       固定离线工具和有种子的材质配方
             \                          /
              → 地面资产编译 → GroundMaterialPack
                                      ↓
冻结天然水陆及岸坡 → 地面几何       多尺度材质权重
已提交建设与清理状态 → 覆盖掩码       ↓
                       → 分块地表 → bgfx 地面 PBR
                                      ↓
                        有界地被实例和远近 LOD
```

这是职责建议，尚未创建对应的空目录或占位接口。

|职责|输入和输出|关键约束|
|---|---|---|
|地面资产编译|选定原图/配方 → 材质包与 manifest|hash、单位、通道、法线方向、工具版本、来源完整；不在游戏里执行 DCC|
|地面几何|冻结水陆/岸坡 → 内陆平面和水边网格|与领域支持一致；材质不修改高度|
|材质分布|地区配置、视觉种子、绝对位置 → 有界权重图|不依赖相机、加载次序或仿真 RNG；相邻块共享边缘取样|
|材质采样|权重、贴图、世界位置 → PBR 参数|统一随机变换、线性色彩混合、正确法线和 mip；低频变化不涂成噪点|
|地被实例|地面与提交后的排除掩码 → 可见实例批次|独立视觉随机、实例上限、LOD、透明/阴影预算；不自动创造资源|
|流送缓存|世界身份、会话代数、分块版本 → GPU 资源|迟到结果不能进入另一个世界；缓存可重建，不改权威仿真|

图形坐标可用浮动原点，材质相位和分布位置却必须保持绝对世界稳定，否则镜头移动会让地面纹理滑动。生成需要携带版本；代码/目录变更不静默改变旧存档外观。地表质量设置只能降低采样与可见细节，不能改变可走区域、建筑支撑或世界模拟。

## 色彩和采样的准入条件

Base Color 按 sRGB 输入并在线性空间混合；roughness、AO、metallic、normal、height 是数据贴图，不能当 sRGB。普通土、草、砂石原则上不含金属反射，输入若有特殊矿物也要按具体材质核验。AO 只用于适当的间接光遮蔽，不将它或拍摄阴影永久乘进基础颜色。

OpenGL 与 DirectX 法线方向必须在编译步骤统一，并用已知斜面样本验证；不要只按文件名猜测。每个材质共用空间变换，法线还要转回共同的切线/世界基底。抗重复造成 UV 跳变后，使用连续原坐标导数做显式梯度采样，避免远处 mip 错选和接缝。地面权重按像素限制活跃材料，首轮最多四种，先验证两种，不能每个像素都采全部材料库。

hextile 的大坐标变体与正常贴图混合可以解决部分问题，仍需要 shaderc 适配和图形后端验证。深色泥、明亮细砂和法线幅值差很大的材料不能盲目共享按颜色反差生成的全部混合权重；在固定照明下分别核验混合结果、接缝与微表面参数。初版普通平面不采用自由顶点位移。

作者特别指出：可平铺的 normal 不一定来自可平铺 height；平均法线偏斜时，随机旋转会造成异常明暗。因此扫描法线也需要验证平均方向与可平铺性。选择性复用 `hextiling.h`、`hextiling_rws.h` 及所需的 `surfgrad_framework.h` 函数，不能只复制顶层调用遗漏法线梯度依赖。[作者说明](https://github.com/mmikk/hextile-demo)、[固定实现](https://github.com/mmikk/hextile-demo/blob/43c3ed7e18e1e4539fa9323f72d5e2a65147ebb3/hextile-demo/hextiling.h)

Light/Darkness 使用既有两套固定环境表现；不增加太阳移动、天气、四季或昼夜计算。扫描贴图的准确色彩、粗糙度和材质比例先在受控光照下验收，再判断整体画面。背景油画的亮度规则不代替三维场景的材质曝光基线。

## 性能预算与验收

下列是首轮比较预算，不是已有帧率或最低配置承诺。先用六至八组 2K 运行时材质；原始高分辨率输入继续保存。若三张 2K 图分别按 BC7/BC5/BC7 的 8 bit/像素格式存储，含完整 mip 链约为每材料 16 MiB，八组约 128 MiB；4K 是其四倍，约 512 MiB。此估算不含高度图、地被、权重图、阴影或水体，格式须以目标设备 caps 实测决定。所有源包都用最高分辨率常驻显存会无谓挤占人物与建筑预算。

地被按可见分块实例提交，不逐草叶建独立 draw；同时测像素 overdraw、材质采样次数、CPU 生成/上传时间、峰值显存和资源撤销。图形负载必须包含近景低角度、普通俯视、远观三个镜头；只画一个地面色块或跑无窗口提交不能作为写实地表性能证明。

|验收场景|通过条件|
|---|---|
|同块反复加载、不同块次序、不同镜头|材质分布、地被锚点、相位不改变；不消耗仿真 RNG|
|相邻块与浮动原点切换|无几何裂缝、权重断线、纹理滑动或法线接缝|
|泥土、草、落叶、砂砾近看和远看|尺寸可信、无明显规则重复；远景不闪、不糊成一层颜色|
|两种及四种材料交界|颜色、法线和粗糙度连续；不产生旋转光照方向错误|
|岸线与人工填筑的前后对比|天然岸坡不动，湿润表现不创造陆地；未完工填海保持 WATER 支撑规则|
|临时建筑/道路占地和保护通道|草叶/小石子退出相应显示区域；领域住户、货物、资源和通行不被视觉清场删除|
|Light/Darkness、暂停和相机移动|按已提交环境表现，禁止材质或帧数推进世界|
|会话切换、取消和迟到的上传结果|旧世界资源不进入新世界；GPU 缓存回收有界|

后续施工建议先做一个独立原生平面地表样本，包含泥土、稀草、砂砾和窄岸坡，验证材质、抗重复与有限地被；这不需要全球包、创建世界或建筑玩法。通过视觉和性能门槛后，再把同一地表管线接进原生空世界流程。新建筑、人物与服装制作仍按既定空世界门槛之后的顺序推进。

## 已复现的噪声实现边界

FastNoiseLite v1.1.1 原始 C++ 单头可编译，但本机 Apple Clang21/arm64 的 UBSan 探针在 seed42、OpenSimplex2、`GetNoise(-1370.0f, -2510.0f)` 报告有符号整数溢出：

```text
FastNoiseLite.h:1006:11: runtime error: signed integer overflow:
-28 * 501125321 cannot be represented in type 'int'
```

最小复现为：

```cpp
#include "FastNoiseLite.h"
int main() {
    FastNoiseLite noise(42);
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    volatile float sample = noise.GetNoise(-1370.0f, -2510.0f);
    (void)sample;
}
```

将官方头文件放在探针目录，以 `clang++ -std=c++20 -O1 -fsanitize=undefined -fno-sanitize-recover=undefined probe.cpp -o probe` 编译并运行。研究中的多坐标探针得到了上述输出；添加 `-fwrapv` 后该小探针通过，不代表全部算法、全部坐标或跨平台确定性已验证。[指定头文件](https://github.com/Auburn/FastNoiseLite/blob/7ccfbc16eb1c932568f177d63a9ba51d89bbe516/Cpp/FastNoiseLite.h)、[上游同类问题](https://github.com/Auburn/FastNoiseLite/issues/140)

这不是随机分布不佳的证据。采用时可先隔离离线用途；若进入运行时，要审查并定义固定宽度 hash 的环绕语义，再测试。不能关闭整个权威核心的 sanitizer，也不能把表现侧的未定义行为当成可靠的跨平台分区算法。本轮未链接该库到项目。

## 版本与证据快照

这些快照仅用于可复核研究，**不是已批准的构建依赖锁**。正式选取还需固定源码归档 hash、传递依赖、补丁、可分发资产原件以及实际生成/构建结果。

|候选|核验基线|许可或源码依据|
|---|---|---|
|Material Maker|1.7，`4c6cea67b659e1eb472f91590e06b2b1c5245916`|[MIT](https://github.com/RodZill4/material-maker/blob/4c6cea67b659e1eb472f91590e06b2b1c5245916/LICENSE.md)|
|Blender|4.5.14 LTS|[官方稳定线](https://www.blender.org/releases/4-5/)|
|hextile-demo|`43c3ed7e18e1e4539fa9323f72d5e2a65147ebb3`|[MIT](https://github.com/mmikk/hextile-demo/blob/43c3ed7e18e1e4539fa9323f72d5e2a65147ebb3/LICENSE)|
|Terrain3D|v1.0.2-stable，`0077405b52e353c5e5dc3a094e7ede49833ba6fe`|[指定源码](https://github.com/TokisanGames/Terrain3D/tree/0077405b52e353c5e5dc3a094e7ede49833ba6fe)|
|MaterialX|v1.39.5，`7b64921ef1d42f2d57871e9d2c43dc11f041f26b`|[指定源码](https://github.com/AcademySoftwareFoundation/MaterialX/tree/7b64921ef1d42f2d57871e9d2c43dc11f041f26b)|
|FastNoiseLite|v1.1.1，`7ccfbc16eb1c932568f177d63a9ba51d89bbe516`|[MIT](https://github.com/Auburn/FastNoiseLite/blob/7ccfbc16eb1c932568f177d63a9ba51d89bbe516/LICENSE)|
|Filament|v1.77.2，`1da46b0a2940db9b1286ec3a8910954c191c010c`|[Apache2](https://github.com/google/filament/blob/1da46b0a2940db9b1286ec3a8910954c191c010c/LICENSE)|
|Infinigen Nature|`f5bcba8de47623da9b715348cf95822445e0e9a7`|[BSD3](https://github.com/princeton-vl/infinigen/blob/f5bcba8de47623da9b715348cf95822445e0e9a7/LICENSE)，此文件 SHA256 `e610a367a4cb695282366ffc5b669ed28e8b728ceac45e98f77aa77a7498559a`|
|ngPlant|`8fb1fe41323ef20f92e12b9f71726a77190ab0d7`|[指定源码](https://github.com/stager13/ngplant/tree/8fb1fe41323ef20f92e12b9f71726a77190ab0d7)，核心与 UI 许可分开|
|ArmorPaint|26.09 release；检查源码 `d38a8bb659504fbfec69934a3525b1abbdd3317a`|[主体许可](https://github.com/armory3d/armorpaint/blob/d38a8bb659504fbfec69934a3525b1abbdd3317a/license.md)|
|VFX Texture Lab|v0.53.0.4，`008aeed179a3c7d6bc44e7aacd92e4bc91d00af8`|[出口规范](https://github.com/MattyGWS/VFXTextureLab/blob/008aeed179a3c7d6bc44e7aacd92e4bc91d00af8/docs/EXPORTING.md)|
|TextureLab|`c3c0420282182640130ffe9810fb56c93afc286a`|[许可证](https://github.com/njbrown/texturelab/blob/c3c0420282182640130ffe9810fb56c93afc286a/LICENSE)|
|TexGraph Public|`42018291a189ee12f1dd2ffe6f85da054cb2d4da`|[公开范围](https://github.com/galloscript/TexGraph-Public/blob/42018291a189ee12f1dd2ffe6f85da054cb2d4da/README.md)|
|HighMap|`4fb63ae6ccd793b6e1e3bfa86a2c1a124c7a43dd`|[LGPL2.1 正文](https://github.com/ottolink-dev/HighMap/blob/4fb63ae6ccd793b6e1e3bfa86a2c1a124c7a43dd/LICENSE)；[README 的冲突声明](https://github.com/ottolink-dev/HighMap/blob/4fb63ae6ccd793b6e1e3bfa86a2c1a124c7a43dd/README.md)|
|TerrainGen|`50c4288b954f8a45d0a91b98f76b37e4e578dae3`|[MIT 主体](https://github.com/Ono-Sendai/terraingen/blob/50c4288b954f8a45d0a91b98f76b37e4e578dae3/LICENCE)，含第三方清单；SDL2/glare 依赖另审|
|ESA WorldCover|2021 v200，DOI `10.5281/zenodo.7254221`|[官方数据与署名要求](https://esa-worldcover.org/en/data-access)|

hextile 选定源码研究快照的 SHA256：`LICENSE` 为 `9999660d22a7791aec03fc66058621ecd76153a6dd43c0b2c4814a775ec7ed5f`；`hextile-demo/hextiling.h` 为 `2d810fb34e8bc8a0cab3ab6e593686f263d036bcda48a21aa33a7bc6c2e373f5`；`hextile-demo/hextiling_rws.h` 为 `4b154346a0fc50f64ecebd961ae835b0a9f8af7043475f67a7bdb03d415c38fa`；`hextile-demo/surfgrad_framework.h` 为 `86adb79694be13131b085c4d78eafa76b98d7072451dd3e6868301ff0cc177f1`。这些记录用于未来逐文件选取，不表示代码已经进入第三方分发目录。

代码复用通知与工具/社区资产授权分别归档。候选源码检查缓存不是正式分发源包；只在实际选取后把合法原件、生成配方和 manifest 放入项目 Git/Git LFS/Releases，不下载整站、不把本机安装或工具缓存提交到仓库。现有原生菜单依赖的集成状态见 [NATIVE_SOURCE_AUDIT](NATIVE_SOURCE_AUDIT.md)，不能从早期总览表推断当前仍未接入 SDL3/bgfx。
