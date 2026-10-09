# 自研三维像素客户端、场景与渲染

2026-10-08。当前设计为[ADR0012](../decisions/0012-editable-3d-pixel-world.md)。完整[像素渲染/资产合同](PIXEL_RENDERING_AND_ASSETS.md)规定作者与性能样本；本页规定接入边界。旧PBR/Hex/PureSky的真实GPU证据仅历史，当前可执行程序仍旧画风，不能称新版像素已可试玩。

## 1. 单份事实、三种代理

SimulationSnapshot消费已提交人物/物品/楼/terrain revision，不含可写World。RenderProxy、碰撞/拾取代理、政治区域代理分别从同事实生成；可重建但不可反写权威地形。屏幕射线首先得到世界/session/revision，地形工具使用真实高度列/工程表面，忽略水波、矿物曲面、旗层、云与标签。正常点选使用对象空间索引/简化碰撞体，密集歧义按需ID pass回读；GPU结果过期重新拾取，不接受上一帧已拆对象。

相机仍透视、yaw360°，低角/高角、指针锚定缩放、pan/orbit/跟随；不穿地、受有限视距预算，精细网格覆盖实际视锥地面，不因低镜头裁远陆成海。世界精度不随镜头/LOD变化，操作地形格不能通过当前低LOD三角猜实际点。

## 2. GPU通道

像素地形/建筑/植被不透明→必要阴影→背景天空与连续外海→像素水/明确透明排序→对象选择/政治/矿物/军令overlay→RmlUi。具体pass顺序依后端深度能力冻结；禁止读写同一颜色/深度attachment。色板与图集采样保持可见像素，远景mip/抗闪烁不能把所有图像退化一整块平均色。

heightQ顶面+侧壁按当前chunk revision生成，mesh合并不吞高差、门和拾取对象映射；邻块halo取同权威页，边缘锁定防裂。权威地形按CPU准备/重验与确定性屏障提交，不等待GPU_READY，也不由镜头或设备帧率决定ready tick。呈现新revision前，受影响范围必须采用同版精确差量/替代mesh，或撤掉旧mesh并显示清楚的暂不可见区域；未就绪范围暂缓客户端落笔/旧图拾取，CPU当前几何与仿真照常有效。禁止几帧旧高程仍可见、新高程已能行走的错版窗口；呈现等待不得暂停或改写权威提交。过期worker结果丢弃，资源安全退役。

材质默认原创像素图集，topMaterial、biome cover、湿润/雪层及稳定变体seed是不同参数。不依赖8套Poly Haven PBR或Hex生成所有地表，不将照片量化当最终原创资产。真实光照可用简化方向光/环境/接触阴影，像素细节主要来自作者色簇。水深来自权威地表，波动/泡沫/倒影只表现，不能造成真实淹水或开航。默认原创pixel water；Abyssal锁定数学留研究、非强制FFT移植任务。

天空/云为原创像素化画面、固定环境profile，不要求两个8K HDR随新world初始化。域外海/天空衔接不呈现矩形大平面边缘；雾辅助视距，不以遮雾代替合法权威域说明。Light/Darkness快照有真实tick余量，灯仅已有有效灯具/能源，画面不赠设施。

## 3. 人物、服装与建筑

原创cuboid硬边mesh、简约共享骨架、精细像素UV/图集、动画/LOD；不使用MakeHuman正式角色源，也不使用面向镜头的纸片最终人。近景角色/衣物细节与远景实例/HLOD分开，贴图texel不是碰撞体素。全龄共享成人body/rig/fit，不因0岁改身高。

WornItemSnapshot来自实际穿着衣物，输出cut/fit/color policy/item revision与必要condition桶；展示换国色不造衣/恢复耐久。军装按actualServingState主题色，国籍独立。服装衣柜/货物/订单不放在renderer。动作取真实任务与时间；取料/携行/放料，不新增人物砌筑动画，攻击/施工动画不结算命中/材料/完成。

建筑仍原创模块与正式作者源，固定footprint/floor/portal/权威体积；外观变体共享authorityHash，不改容量。HLOD合并保留各BuildingId映射，远景城市代理跳同CitySquare。矩形网罩消费construction generation/状态，逐层围/撤、远景降线密度，网罩不成为导航墙。

## 4. 输入、UI与资源生命周期

沿用RmlUi/FreeType和唯一InputRouter：UI/IME优先→当前工具capture→临时相机→观察。地形stroke/军令/抓取互斥，开模态/失焦/Esc由同owner取消未提交动作；UI点击不得穿透地图。底栏八分区，近远typed引用进入同InspectorController。所有按钮无边框/outline/ring，原创功能图标有三语名称，必要错误/后果正常字号。

保留已批准油画艺术主菜单/Logo/静态光环/右下纯文字；游戏像素化不把中文/法律文字强制低清点阵。三语、DPI、中文IME、德文长文本与脏草稿独立验证，UI locale不变世界语言。

资源requested→loading→decoded→upload queued→resident→retiring；hash去重、CPU/GPU/每帧上传上限、generation取消；backend引用期间禁止释放源内存。失败给实际asset/hash原因，中性fallback明确不能冒充正式完成美术。shader缓存由toolchain/目标后端/defines/输入hash绑定，未变shader不重复编译/烘焙。

## 5. 验收边界

新GPU验收需正式应用入口而非独立近景probe：Blank/Earth最高预览与实际world，同一真实height/water/材质；近中远所有yaw/lowpitch可读；连续zoom/LOD不闪、不出现假可走面；人物衣物360°、原创建筑门/车道/网罩；Light/Darkness与实际灯具；三语/IME/input捕获；编辑mesh在预算内同步；CPU/GPU p50/p95/p99、draw/骨架人数/显存/upload、真实城市经济与灾损重放。

首轮1080p60fps只是目标，具体设备档/人口/视距依实测。Windows GPU+Steam设备必须单独证据，Metal开发与CI编译不能代替。旧正式入口地表可见性修复已做真实通道消融，不证明新像素风已完成。
