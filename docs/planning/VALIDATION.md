# 当前实现和验证边界

记录日期2026-10-06；初步框架0.1。本文件的“已验证”只指下面实际执行的代码，不覆盖完整游戏规格。

## 已执行的本机验证

- Apple clang21、arm64 macOS，`-std=c++20 -Wall -Wextra -Wpedantic -Werror`编译。
- 18组内核场景：材料预约/消费；完工前水面；四邻接/禁孤岛；拒绝无半写；幂等/冲突/修订；可信Scheduler劳动入口；取消/永久完成；dirty导航；单格海峡分裂；reclaimed-only港口/泊位/重叠；4旋转；湖海/非法角度；中途checkpoint/回执续跑；损坏/版本/截断；2000命令守恒；伪造孤岛/epoch/坐标边界；伪造权限回执拒绝。
- 对已初始化运行时中的`PlanReclamation`事务逐一注入分配失败：完整checkpoint字节在失败前后相同，原commandId可重试，成功状态与正常提交完全相同；本机当前11个分配位置得到验证。该实验不覆盖所有命令或标准库启动路径。
- headless贯穿演示：2个完成造陆格、1个港口、材料100→80、reserved=0、天然mask hash不变、checkpoint相同。
- 本机`python3 tools/build.py --sanitizers`已通过：headless、全部18场景、11个allocation故障位置、目录与原创几何校验。2026-10-06，GitHub Actions的Linux/macOS Debug、Windows Release及Linux ASan+UBSan四个job均已实际通过，见[基线运行记录](https://github.com/sky-yueer-yang/sonnheide/actions/runs/37539046613)；后续提交仍以各自Checks为准。
- 内容校验包括原稿SHA-256、科技424项计数/唯一ID/前置DAG、35法律/13业务、27章追踪、3原创glTF来源/权威hash/尺寸/入口/三角绕序及本地文档链接。

## 独立审查发现并修复

不合法孤岛存档、无支撑未完成工程、极端公开坐标查询整数溢出、epoch/ID/revision耗尽处理、Accepted回执和世界分配失败分离、Observer Accepted伪回执。这些边界均有具体复现，修复没有扩大到无关领域。

## 真实地形、建址与指定水体切片

- NOAA ETOPO 2022 v1 Ice Surface真实N60E000源的TIFF头与tile 201，共199247字节，以固定SHA-256保留；官方metadata/CC0原文已保留。取出25×25原始像元中心，高程1640.096802..3981.594971m。`tools/import_etopo_sample.py --fetch`、默认离线重建、`--verify`均已实际通过；缺源/损坏/NoData不静默伪造。源bundle明确partial，不声称整幅TIFF/global package哈希。
- `site_geometry`的19组场景：全footprint中心山峰、邻楼保持旧标高/高程、后建楼侵旧坡道拒绝无半写、肩丘全宽检测、网格点之间三次曲线极值、长/短坡道、garage必需独立车辆证书、车轴坡折、门/道路正宽接触、4方向旋转、负高程LAND/NoData/WATER拒绝、未建地基不得送货、权利/预算/恶意数值/陈旧preview、远端或错侧/窄apron、默认front/socket、moved-from查询安全、North/East不同标高坡道自交。严格clang与ASan/UBSan均通过。生产曲线转弯/全车身扫掠/共享路/阶段释放/全局网络尚未实现。
- Abyssal commit `142265f5013b6f27bea4f4f819b832dec75c7bad`的LICENSE/README/index.html准确字节SHA-256已核验；three.js CDN不是原生依赖。`ocean_fft_reference`五组：尺寸2/4/8/16直接2D IDFT对照，频率方向/centered checkerboard/DC幅值，不除N²的上游约定，Hermitian实值，以及尺寸/NaN/Infinity/overflow/moved-from拒绝。最大数值误差约2.4e-14；严格clang与ASan/UBSan通过。参考只在presentation库，kernel不依赖它或GPU。
- 项目校验现在还检查上述上游文件准确hash、CC0许可hash及真实ETOPO样本的离线重建一致性；Windows checkout通过LF规则保留官方XML/HTML字节。完整GPU波浪、海岸mask、三后端画面和帧率未验证。

## 服装、正装修正与MakeHuman切片

- `clothing_economy`实际16组场景：文明/升时代仅改变需要而不赠衣；零币公共制衣仍消耗纤维/布/劳动；预约/托管款/配送/返还只结算一次；未穿备用衣与已有正装抑制重复需求；知识已采用且零雇主库存仍能启用正装规范；无正装正常工作并领取全额工资；粮房预算不被购衣抢占；外籍士兵服役国配色与国籍分离；整数损耗步长一致、较慢存放老化；交付不恢复耐久、累计修补上限；劳动/衣柜上限；在途货腐烂仍保留托管款且必须收到实际退回后退款；缺原料不自动补货；旧军装/机构在途订单先真实交接再换机构；个人不能报废机构所有的完好衣物绕过归还，初始款式与setter/生产目录采用同一上限，已交付订单清除在途标记/托管款。
- 长期场景是**100人×100游戏年，30日/月、360日/年，共36,000游戏日**。有限初始200,000纤维，不再注入资金/原料；实际生产/购买/交付/穿用34,400件，支付1,720,000，工资1,376,000，纤维剩62,400，每年均有替换成交。核对现金＋escrow、纤维/布/染料和劳时守恒。它证明该有限物料/资金规则的长期替换，未模拟供粮/住房消耗、植物种植、物价变化、市场竞争或企业利润平衡，不能当完整城市100年验证。
- MakeHuman取入14份准确源文件，共3,129,823字节；MakeHuman v1.3.0与MPFB v2.0.17的commit明确，三个衣物各自CC0证据与程序AGPL/GPL分开。默认离线、`--verify`及实际`--fetch --verify`通过；hm08 19,158顶点/18,486面、163骨/326joint helpers、57,107原始权重记录及三款衣物fit/mask/索引/材质属性经过检查。原始权重总和0.321～1.673，不冒称已归一游戏权重；独立归一检查最大误差2.22e-16。10项损坏输入实验均被拒绝，包括父图循环、缺joint、非法权重/索引、错alpha7基底、affine/遮罩与原字节破坏。
- 四张引用纹理未导入，胸罩候选许可冲突保留为pending；没有DCC导出、体型targets、GPU蒙皮、服装穿插或完整动作库证据。军装几何仍需原创制作。源件归档和oracle不能替代这些验收。

运行所有目标：`python3 tools/build.py`；事务/空间/存档修改另跑`python3 tools/build.py --sanitizers`，本轮服装与交互切片也跑sanitizer。CMake/CI运行kernel、allocation、headless、site、ocean、clothing、interaction与content八个测试；找到Node时另运行interaction_preview_contracts。[GitHub Checks](https://github.com/sky-yueer-yang/sonnheide/actions)以各提交实际结果为准。本机Python3.9/Apple clang21的正常构建与ASan/UBSan整仓检查均已通过；三平台结果在最终交付中按实际Checks报告。

## 集中信息页、世界工具与人口来源切片（本轮前的基线记录）

- [ADR 0005](../decisions/0005-unified-inspectors-and-world-tools.md)、[交互架构](../architecture/INTERACTION.md)与`data/interaction_schema.json`共同定义40类对象、251条有类型的关系路由和136项**待接入生产系统的命令意图**。目录校验检查引用、页面章节、编辑白名单、规则与清理边界；136项不是已经实现的命令。
- 独立C++20 `interaction::World`实际19组场景：40类稳定名称；固定18岁放置及无赠物/身份；禁止隐式人物导入与越权放置；地面和容量；已配对成年双亲的Scheduler出生0岁；提交时复核繁衍规则；年龄冻结但日历继续及统一成年rig/fit；规则不凭空造粮或删除战争；修订/幂等/草稿；事实记录不可自由改名；类型导航/筛选；收藏标记与世界修订分离；死亡档案保留亲缘/作者/所有者/保管者；统计去重；旧通行保护；住户/存货建筑保护；分类清理不改天然高程和已采库存；极值/跨世界/第二写者拒绝；所有本切片事务分配位置失败无半写且原命令可重试。所有公开World读取也受单写者线程约束，跨线程必须使用分离快照。
- `tools/previews/world-inspector.html`是原创地图与示例对象的离线可点击原型。Node v24.19.0仅用内建模块执行实际HTML中的模型；22组场景覆盖全部类型和引用、关系导航、视图隔离、18岁/0岁生成与事件幂等、规则、可编辑草稿/只读事实/视图别名、清理预览/陈旧拒绝/死亡档案、矿源停止与已采货保留、树木与植物子集、统计单位与去重、未知数量返回null、真实Home/库存位置/通行引用的反向保护和独立标记快照。缓存计数被故意归零后，真实引用仍阻止错误清理。
- 实际浏览器核验近景人物拾取→人物页→国籍→国家页、出生0岁档案、远观装饰道路上的国家区域选择、四项规则界面以及建筑/道路清理的允许2项/阻止3项依赖预览。修复透明拾取范围、人物绘制顺序和远观区域兜底，显示身体圆在0/18/42/44/72岁均为r8；320px宽页面无横向溢出。SVG最小高度造成留白时使用逆screenCTM，窄屏实际放置成功且为18岁、无自动国籍/文化语言。最终页面截图保存在本机忽略的`.build/previews`，不作为资产或项目源文件提交。
- 本机设置`SONNHEIDE_NODE`为已发现的Node v24.19.0运行时后，`python3 tools/build.py`与`python3 tools/build.py --sanitizers`均实际退出0；最新sanitizer整仓执行包含18内核、11分配故障、19建址、16服装、5FFT、19交互及22原型场景。原稿SHA-256、生成目录、三份原创glTF和已固定上游原始字节仍通过校验。Node为可选开发验证工具，缺失时本机明确SKIP；CI单独运行原型检查。
- 当前C++与HTML分别是有限oracle与示例状态，尚未互相连接，也没有接入原有World、生产经济/自然死亡、GPU地图、RmlUi客户端或生产存档。规则测试确认切片权限与边界，不能替代完整饥饿/战争后果的业务实现。原型比较限同类对象；生产跨类型比较必须先选择共同统计口径。

## WorldBox式底部结构、英汉德与丰富编辑（上一轮实测记录）

- [ADR 0006](../decisions/0006-bottom-toolbar-trilingual-editing.md)、[UI架构](../architecture/UI_ARCHITECTURE.md)、[编辑矩阵](../architecture/EDITING.md)、[程序结构](../architecture/CLIENT_STRUCTURE.md)及施工计划已写入。当时的10阶段/资产提前顺序现在由[ADR 0007](../decisions/0007-foundation-experience-first.md)和[现行11阶段计划](IMPLEMENTATION_PLAN.md)覆盖。参考依据、项目选择与商业游戏未实际运行的限制见[前端研究](../research/WORLDBOX_FRONTEND.md)。原稿逐字保留；没有创建空的生产目录或宣称新依赖已经集成。
- 原型实际改为地图中心、底部7个section/30工具、可关闭对象窗与按需目录。397条消息在`zh/en/de`三语言中有相同key、插值参数及准确离线镜像；Python3.9标准库校验检查重复key、空词、40类/35法/6轴/7分区覆盖。界面locale与模拟Language保持独立。
- 独立C++交互oracle现为**30场景**。新增六轴0–100、四档宗教制度、35法条有界政策记录、精确一次Scheduler授权；陈旧/错类型/未成熟/能力/条约/神授/依赖拒绝与分配失败回滚。宗教地位为单一权威，旧官方摘要被明确取代并记历史；居民信仰、国籍、财产与身体不随编辑变化。137项注册意图仍不是137项生产实现。
- 浏览器实际核验中文人物点选/六轴编辑、德语国家目录/宗教绑定预览与提交、英语法规表单，以及320×760德语布局；文档宽度320，检查器在底栏之上，工具行独立横滚。发现预览只列字段数量的问题后修成实际旧值→新值；再次核验宗教2项变化、个人所得税新增的选项/1250基点/第30日/30日过渡四项。未变化字段不计数，财政显示未接入。
- 上一轮本机正常整仓构建和ASan/UBSan整仓构建均退出0：18内核、11分配故障、19建址、16服装、5FFT、30交互。前端最后调整后另运行**39项实际HTML模型/UI场景**和项目校验，包含无变更禁提交、逐法条/税率前后差异和跨对象/页签滚动复现。原稿hash、重建目录/3原创glTF与固定上游源字节均通过；GitHub各提交Checks另行报告，不沿用旧运行冒充新提交结果。
- 生产仍缺：原生SDL3/bgfx/RmlUi窗口/GPU/IME、单一World接入、人格行动AI、真实法域审批/税款/过渡结算和全域存档。HTML批准记录始终标“待领域执行”；原生最小尺寸、200%字体/DPI与启动/创建/地图的GPU实测现在属于阶段0–2基础体验门槛。既有资产启动计划与新的重排都不代表完整建筑或人物资产已经制作。

## 基础体验优先的施工顺序（本轮文档变更）

- 采纳[ADR 0007](../decisions/0007-foundation-experience-first.md)：阶段0主菜单、阶段1创建/真实初始地表、阶段2基础界面/保存载入返回。通过原生空世界门槛后才开始阶段3建筑/人物服装作者源与建址；后续完整生活、社会、制度、军队、企业、全球覆盖与发行保留在11阶段计划中。
- [应用流程](../architecture/APPLICATION_FLOW.md)定义隔离候选World、校验/首存档后发布、取消/失败保留旧世界、继续指针更新、同WorldId的会话代际和视图隔离。只列已校验地理包；不把25×25样本冒充全球，不生成免费人口/文明/建筑。
- 本轮仅改工程合同、施工依赖和阅读入口；没有改C++/原型代码、地理包或资产，没有运行或声称完成原生主页/创建世界/地表程序。实际运行`python3 tools/validate_project.py`与`git diff --check`均通过，覆盖本地文档链接、冻结来源/目录/资产和三语合同。本轮不重复核心构建或sanitizer，之前的代码测试结论仍只覆盖各自提交和切片。

## 油画主菜单、纪念性极简重做与两种环境（2026-10-07）

- [ADR 0008](../decisions/0008-sacred-interface-and-light-ages.md)定义Light/Darkness与仿真tick/存载合同；[ADR 0009](../decisions/0009-monumental-minimal-interface.md)采纳最新视觉修正：绝不添加无必要小字，主菜单纯文字无icon、右下集中右对齐，其他文字按钮不强制icon，全部按钮无框。左上使用用户原始Sonnreich标志、静态微光与96条原创细放射线，标题大写SONNHEIDE；删除原徽章、副标题、画注、页脚和散落画控/语言控。
- 五幅用户提供JPEG、公司标志原件、本地Cinzel/OFL/metadata都有准确SHA-256与来源记录。标准库油画导入器与项目校验实际通过；标志和油画不被授予开放许可。固定Google Fonts commit的字体字节未改，普通Git离线携带这组有界小型源文件；大资产仍用原LFS规则。
- 实际主菜单JS的12组VM回归：可见DOM加载失败、decode失败、交叠中失败、迟到decode、顺序与68/12秒计时、隐藏停时、降低动态、坏首图替换、全坏图暗底、三语key/参数一致与无World依赖、初始唯一5项纯文字菜单、禁用继续与调用后真实范围说明。测试执行交付脚本，不复制另一套轮播算法。
- 世界原型现有39项模型/交互回归与9项额外展示回归全部通过。保留397条英汉德镜像和40类typed对象；去掉常驻revision/提示/调试字，页签和关系动作允许纯文字。地图人名/年龄只在hover/键盘focus出现，避免相邻标签重叠；名字和年龄仍有accessible name及档案入口。Light/Darkness演示显式auto默认关闭，其墙钟演示不改世界、人物、统计或存档，不能当权威计时实现。
- 实际浏览器核验英文/德文/中文菜单及设置、1280×800与320×760，窄屏文档宽度320且标题不碰菜单；菜单右边缘相同、computed border/outline为0，Tab跳过禁用继续，焦点字重600可见。标志/微光/放射线computed animation为none，降低动态开关生效。核验创建入口真实提示、人物键盘拾取→国家→宗教/35法条编辑、底栏环境→Darkness呈现。截图保存在忽略的`.build/previews`，不是项目资产。
- 资产重建后本机`python3 tools/build.py`退出0，包含18内核、11分配故障、19建址、16服装、5FFT、30交互，以及全部60项浏览器模型/展示回归和项目来源校验；`git diff --check`通过。本轮未改C++/事务/空间/存档，不重复sanitizer；既有sanitizer记录仍只覆盖原代码，最新GitHub Checks按当前提交另行报告。
- 仍是浏览器设计/交互原型，原生SDL3/bgfx/RmlUi、真实创建/存载、GPU油画/字体及权威环境相位尚待阶段0—2。11阶段计划继续先完成主菜单、真实初始地表、基础游戏界面及存档往返，再启动新建筑/人物服装资产。没有以这轮截图宣称原生空世界门槛已通过。

## 当前实现与生产差异表

2026-10-06建筑施工变更见[ADR 0002](../decisions/0002-building-construction.md)。本轮已检查材料前侧配送、分层围网、完整顶盖、成品显示及反向撤网的可播放示意，包含320px窄屏；角色与料箱分离，预览状态保存回声不会主动重置本次播放。原稿/提取目录/三份原创灰盒已重建校验且未变。生产建筑自动工期、真实Person搬运和GPU围网仍未实现；现有填海内核没有因此取消劳动。

|当前|生产仍需|
|---|---|
|合成6×6等小图；安全容量上限100万格|全球真实数据、投影、coast coverage、物理clearance、内存预算|
|真实25×25 ETOPO窗口/原字节离线重建|完整全球源包/固定EarthPack、datum/投影/NoData/区域精度报告|
|独立地形/建址oracle，可信level portal，四向直坡与全部保护域|完整world/产权/交通网络、车辆转弯/体积净空、阶段释放、真实物流、存档与GPU|
|MIT Abyssal源与CPU FFT参考|原生GPU谱/泡沫/折射/水下、海岸/湖水mask、三后端帧率与生命周期|
|独立两槽服装Ledger、16组场景、100人100年有限原料/资金循环|World命令/完整交易/存档/交通、公共生存闭环、批次/多层fit/二手回收/市场竞争及性能|
|MakeHuman身体/绑定和三款CC0衣物源件、离线结构核验|DCC固定环境、targets/皮肤/头发/眼睛/纹理、canonical骨架/LOD/动画/GPU与原创军装|
|40类集中页面合同、底部7分区三语示例原型、独立30场景交互oracle|正式World单一事实源、完整业务命令/统计、类型拾取、原生客户端、人格AI/法律结算、人口全生命周期与保存迁移|
|一种通用材料；Scheduler直接给5单位劳动|实名CitySquare库存、真实运输/Person劳动/施工合同、部分投入结算|
|PlaceRoad无工程成本；KernelPlacePort几何探针|科技、预算、产权、法域、建设流程和广场完整路网|
|整状态复制、同步全图BFS、map/set|分页写集、SoA、Chunk/门户动态连通、异步三态query|
|kernel文本checkpoint/FNV回归fingerprint|SHA-256块/pack、生产fsync/多代保存、版本迁移、全域L1/L2重放|
|3个原创几何灰盒|所有建筑家族、写实模块/材质、容量/三时代/文化外观、完整LOD|
|C++/Python标准库可执行|SDL3/bgfx/PBR/water/ozz/RmlUi/IME实际集成与GPU实测|
|目录提取数据与27章设计映射、原型397词英汉德资源|完整英汉德定义/flags/doctrines/naming与全领域规则实现|

没有声称10000/100000完整人口达到帧率，没有经济平衡、真实战斗、社会传播的实测结论。原稿提到199条旧追踪、独立国旗工具和TS参考未提供，本轮不声称已经运行它们。


## 2026-10-07 同心圆与密集光芒修订（前一轮浏览器原型）

- 本轮覆盖上文首版96条细线的视觉设计：标志向左上画面内部移动，固定顺时针12°；SVG原始几何为三层内圆（116/121/127）、120刻度、150—195半径无图形环带、三层外圆（200/204/210）。360条射线都从211半径开始，末端六档226—490、线宽0.3—1.5，长短粗细差异大；构图可越屏。Logo原始PNG未改，静态光晕/圆环/射线/主页字体共用#fff6df，取消上一版昏暗旧金。标题仍在右侧、主页字体缩小、菜单行距紧凑。
- 新增原创SVG与Python 3.9+标准库离线配方。实际生成与`tools/build_menu_compass.py --verify`通过；manifest记录原始PNG、SVG、配方和字体的尺寸/hash/权利，project validator核对生成字节和UI共用光色。原始油画不变，仅使用共用屏幕渐变压暗大部分画面/边缘并保留小中央亮焦点。
- 实际浏览器检查1280×800桌面和320×760窄屏；标志已向内、同心圆/空环带/外圈射线的顺序可见，字体明亮柔光。窄屏分别检查中文、英文、德文：文档宽320、所有菜单在界内、标题和菜单至少24px间隔、按钮右缘一致、border/outline为0。桌面文字24px、标题约50px，品牌/Logo/射线/柔光computed animation均none；恢复用户中文语言与正常窗口尺寸。截图保存在忽略的`.build/previews/sonnheide-menu-concentric.png`。
- 本轮`SONNHEIDE_NODE=已发现本地Node路径 python3 tools/build.py`实际退出0，包含18内核、11分配故障、19建址、16服装、5 FFT、30交互场景，以及39 Inspector交互、9呈现、12菜单（共60项）检查，项目来源/hash/三语校验通过。不修改C++/事务/空间/存档；本轮未重跑sanitizer，上文旧sanitizer证据保留其原有范围。浏览器截图与检查仍不代表原生GPU、创建/存载或权威环境状态接入。


## 2026-10-07 外圈收紧与降低射线亮度（前一轮浏览器原型）

- 用户要求外圈大幅缩小并几乎贴近内圈，同时减细外圈、降低射线亮度。内圈116/121/127保持，外圈半径从200/204/210收为137/140/144，线宽0.25/0.25/0.5；含笔触后环间净距9.55。120刻度收至108—113，保留105中心及128—136空环带；360射线起点移至145，末端各减67以保留原长短差异，opacity从0.45—0.9降为0.27—0.54，CSS辉光从2px/#fff6df55降为0.75px/#fff6df18。基色、Logo原件/柔光、字体、品牌位置与背景不变。
- SVG已实际重建，`--verify`和独立只读几何/留白/颜色审查通过；来源manifest同步SVG与配方hash。实际浏览器检查桌面1280×800、窄屏320×760，环间窄缝清楚，边缘细且射线柔和，窄屏宽度320无横向溢出；恢复正常窗口尺寸，截图在忽略的`.build/previews/sonnheide-menu-close-rings.png`。本轮未新增实现镜像测试，复用已有构建/项目/前端回归；C++与权威世界规则未改。
- 本轮正常`python3 tools/build.py`（指定已有Node运行时）实际退出0：原有18内核、11分配故障、19建址、16服装、5 FFT、30交互与60项前端检查全部通过。未重跑sanitizer；仅调整浏览器UI资产与准则，不扩大原生接入声明。


## 2026-10-07 普通字号与随机极慢背景运动（前一轮浏览器原型）

- SONNHEIDE与五项菜单在全部响应断点统一普通16px字号，右对齐整组桌面/窄屏上移20px、矮屏上移12px；保留原可点空间。360射线opacity从0.27—0.54再降至0.18—0.36，移除额外drop-shadow；圈层几何、Logo原件/柔光和小亮焦点深暗背景不改。SVG已重建，manifest更新源/配方hash。
- 旧CSS120秒来回动画改为实际JS呈现时钟：每段140—200秒，随机选实际dx/dy符号对应的左下/右上/右下/左上方向，按边界余量35—75%步长，排除连续同方向与过小余量。五次缓动保证段交界连续且起止速度/加速度平滑；平移始终在±0.55%/±0.4%内。两图层共享平移轨迹，各自以240秒时钟从1.018微放大到1.036，只有新的隐藏候选重置缩放。
- 暂停/hidden/reduced冻结当前pose与时钟，恢复首帧不追赶，初始reduced为静态基准。降低动态手动换图仍静止；加载失败回退和locale变更不重置当前运动。Math.random只用于本地呈现的方向/步长/时长选择，不接入World或模拟RNG，68秒hold和12秒交叠保持。
- 实际浏览器核对当前桌面946×691和窄屏320×760：标题及五按钮computed font-size全部16px，右缘相同，窄屏无横向溢出、标题/菜单不重叠；两层DOM transform呈现同一平移和独立微放大。新截图位于忽略的`.build/previews/sonnheide-menu-slow-drift.png`。图像为静态证据，动画方向/时间边界由真实脚本检查单独验证。
- 本轮正常构建实际退出0，原有18内核、11分配故障、19建址、16服装、5 FFT、30交互场景通过；前端为39 Inspector交互＋9呈现＋16菜单，共64项。新增4组执行真实菜单脚本的运动检查，覆盖40段四方向/边界/交界连续性、极端合法随机抽值、两层共享漂移、失败/切语不重置、暂停隐藏减弱冻结与恢复。浏览器DOM亦实际观察到共享平移与缩放持续缓慢变化。未修改C++、空间、事务或存档，未重跑sanitizer；原生接入范围不变。


## 2026-10-07 固定标题、收紧行距和字距（当前浏览器原型）

- 用户要求最上方不动、下方往上收紧：用原菜单底距/行步/标题间隙/16px行高保留独立标题top anchor，新菜单从标题下方12px开始，不因缩行后沿底部向下跑。各断点保存旧anchor参数，新菜单行步统一24px；去掉菜单垂直padding，避免18px文字行高加padding突破24px。字号全部18px，标题和菜单字距统一0.01em（18px下0.18px）；品牌/背景/运动未改。
- 实际946×691桌面对比：标题top从380.8515625变为380.8359375，仅布局舍入差0.016px；五项菜单top为414.078125/438.078125/462.078125/486.078125/510.078125，均高24px、字号18px，字距0.18px、右缘一致。原第一项435.7265625、最后一项579.7265625，确认下方实际向上收紧。截图在忽略的`.build/previews/sonnheide-menu-compact-type.png`。
- 窄屏320×760实际标题top为443.9140625（此前443.9296875），菜单同为18px/24px、右缘294.40625、文档宽320。项目来源/三语校验、现有16项菜单检查与diff check通过；只更新既有布局检查的top定位断言，不新增低风险实现镜像测试。本次排版没有资产源/目录或C++改动，未重跑C++和sanitizer；上一轮构建证据保留原范围。

## 原生主页面接入（2026-10-07）

- 本机Apple clang21/arm64，SDL3.2.28、bgfx固定commit、RmlUi5.1、FreeType2.13.3实际编译链接；独立core继续不依赖窗口/GPU。
- Metal真实窗口1280×800逻辑/2560×1600物理；120帧原生smoke执行设置、英汉德切换、创建/载入入口、返回及有序退出。创建/载入入口只显示缺包/无档，不发布World，不运行oracle样例作正式游戏。
- CUA原生窗口实际检查发现并修复ImageIO垂直翻转、Rml5.1缺默认HTML块布局和Retina字形采样放大。最终截图可见正向油画/原始Logo、密集低亮圆环射线、18dp统一字号/24dp菜单行高；中文/English/Deutsch设置全部即时切换。用户已实际查看并确认视觉没有问题。
- 实际Rml parser/font/layout/events测试覆盖1280×800及400×600、density1/2、三语言、菜单间距、面板行布局/边界、真实pointer事件和Escape返回，不只检查源码字符串。运动模型验证900ms手动/12s自动交叠、有界最后方向排队、失败保旧、10000s四向移动界限、暂停/隐藏/降低动态和偏好失败原子性。
- 完整本机CTest 12/12通过（6.18s），含core/site/clothing/interaction/CPU FFT与原生模型、真实UI布局、JPEG/PNG解码与中文资源路径、源资产合同；浏览器39+9+16原回归也曾独立全部通过。GPU与UI布局证据不代表原生世界地图、水体、全部信息页或生产存档已实现。
- 项目GitHub Release八份原始文件已实际上传并从项目下载URL逐个回读校验长度和SHA-256。原始设计基线、油画、Logo和已有Cinzel字体字节不变；本轮未修改核心世界事务。
- 发行目标Steam；本机窗口证据是开发验证，不安排App Store工作。Windows native构建与具体结果将在CI完成后单独记录，不能用headless Windows绿勾代替图形客户端构建。
- 用户最新明确暂缓真实地形包/创建World，本轮只完成接入。

### Windows 构建与跨平台检查

[原生构建 run 37581479771](https://github.com/sky-yueer-yang/sonnheide/actions/runs/37581479771) 从项目 Release 获取锁定源件，Windows MSVC 完整构建客户端和所有有限正确性目标。三项原生测试（模型、真实 Rml 布局、JPEG/PNG/中文路径）以及其余 core 测试通过；首次完整 CTest 最后一项原始许可哈希因 Windows checkout 换行转换失败。已通过 `third_party/native/** -text` 保留原字节，未改变锁定哈希。既有菜单 JS 校验另有 LF 专用正则，现支持 CRLF，UI/build 源显式固定 LF；LF 与真实 CRLF 文件分别运行16项检查通过。修复后 CI 结果另行记录。唯一发行渠道为 Steam，Windows GPU 实机/Steam Overlay 未由构建 runner 验证。

## 2026-10-07 Flat-land decision and removal of real-height dependencies

ADR 0010 replaces the real-height product route with the real land/water outline, a uniform inland platform and a frozen narrow coastal transition. Inland buildings have no independent levelling foundation; optional coastal support retains full footprint, pedestrian/garage access, current front-side delivery and old-access protection. ETOPO original bytes/notices and prior validation remain historical records, not new-world dependencies. No global source import, native map or World creation was implemented in this revision.

The retired ADR 0003 site implementation now builds only as `sonnheide_legacy_slope_oracle` under BUILD_TESTING. `site.cpp` is absent from current kernel/headless sources. The portable builder likewise compiles it only with its historical site test. The original v0.6 source SHA-256 remains `cdff70f277a46554c7d06f0078d1d77049bb88ca965d05e889d619ade949bea6`.

Normal `tools/build.py` and `tools/build.py --sanitizers` both exited 0: the existing 18 kernel, 11 allocation-failure, 16 clothing, 19 historical slope, 5 FFT and 30 interaction scenarios, native menu model, project/native-source validation and 39+9+16 browser regressions passed. Fresh CMake configured and linked headless/kernel and the separately linked historical site test; the three selected CTests passed (3.21s). `nm -C` confirmed no TerrainGrid/SiteRegistry symbols in the current kernel archive. A separate BUILD_TESTING=OFF configuration exposes neither legacy slope nor site-test targets. These checks prove isolation and unchanged finite invariants; they do not prove the planned coastal geometry or new building rules are implemented.


## 2026-10-07 Poly Haven/Hex source adoption and preview/boundary contracts

ADR 0011 now fixes Poly Haven PBR materials, the mmikk Hex-Tiling source snapshot, two Pure Sky profiles, finite authoritative sea collars/protected outer water, ocean-only presentation beyond the world, and game-equivalent maximum selection preview. The user explicitly chose irregular closure coasts; source interiors stay unchanged and the final boundary must be previewed before freezing. Active world/terrain/render/creation/simulation/plan contracts were reconciled by independent read-only reviews. This is a design/source step, not native map or GPU implementation.

Actual source evidence: four unmodified MIT files match the fixed commit and SHA-256; 24 original 2048×2048 PNGs and two original 8192×4096 Radiance HDRs (581,937,823 raw bytes) match official lengths/MD5 and acquired SHA-256. PNG IHDR CRC/dimensions and HDR format/orientation headers pass. All ten byte-stable ZIP_STORED packs match their locked hashes; an independent Python 3.9.6 review reconstructed every archive from its original members with identical lengths/hashes. HDR headers and source bytes are verified, not complete 8K visual/exposure/IBL QA. CC0 originals, per-asset authors/provenance and license text accompany each source pack.

`python3 tools/prepare_ground_sources.py --verify-cache`, `python3 tools/validate_project.py` and `python3 tools/build.py` passed. Existing finite kernel/allocation/clothing/historical-site/FFT/interaction/menu scenarios retain their earlier scope. Optional Node preview checks were skipped in this build; no core/transaction/spatial/save implementation changed, so this step adds no sanitizer claim. Separate source probes verified deterministic ZIP rebuild, rejection of corrupted/unexpected packs, and that oversize/wrong-SHA downloads never promote a partial file. The publication recipe verifies hosted bytes after both first upload and repeat publication, refusing mismatches without overwriting. Publication evidence is reported separately from local verification.

Complete global full geography, native world creation/save/load, the bgfx PBR/Hex shader, same-path maximum preview, Pure Sky/IBL processing, infinite-looking ocean and performance/image quality remain unimplemented. The immutable v0.6 design source is unchanged.
