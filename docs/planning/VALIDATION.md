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

运行所有目标：`python3 tools/build.py`；事务/空间/存档修改另跑`python3 tools/build.py --sanitizers`，本轮服装切片也跑sanitizer。CMake/CI分别运行kernel、allocation、headless、site、ocean、clothing与content七个测试；[GitHub Checks](https://github.com/sky-yueer-yang/sonnheide/actions)以各提交实际结果为准。本机Python3.9/Apple clang21的正常构建与ASan/UBSan整仓检查均已通过，包含上述16组服装场景；三平台结果在最终交付中按实际Checks报告。

## 当前实现与生产差异

2026-10-06建筑施工变更见[ADR 0002](../decisions/0002-building-construction.md)。本轮已检查材料前侧配送、分层围网、完整顶盖、成品显示及反向撤网的可播放示意，包含320px窄屏；角色与料箱分离，预览状态保存回声不会主动重置本次播放。原稿/提取目录/三份原创灰盒已重建校验且未变。生产建筑自动工期、真实Person搬运和GPU围网仍未实现；现有填海内核没有因此取消劳动。

|当前|生产仍需|
|---|---|
|合成6×6等小图；安全容量上限100万格|全球真实数据、投影、coast coverage、物理clearance、内存预算|
|真实25×25 ETOPO窗口/原字节离线重建|完整全球源包/固定EarthPack、datum/投影/NoData/区域精度报告|
|独立地形/建址oracle，可信level portal，四向直坡与全部保护域|完整world/产权/交通网络、车辆转弯/体积净空、阶段释放、真实物流、存档与GPU|
|MIT Abyssal源与CPU FFT参考|原生GPU谱/泡沫/折射/水下、海岸/湖水mask、三后端帧率与生命周期|
|独立两槽服装Ledger、16组场景、100人100年有限原料/资金循环|World命令/完整交易/存档/交通、公共生存闭环、批次/多层fit/二手回收/市场竞争及性能|
|MakeHuman身体/绑定和三款CC0衣物源件、离线结构核验|DCC固定环境、targets/皮肤/头发/眼睛/纹理、canonical骨架/LOD/动画/GPU与原创军装|
|一种通用材料；Scheduler直接给5单位劳动|实名CitySquare库存、真实运输/Person劳动/施工合同、部分投入结算|
|PlaceRoad无工程成本；KernelPlacePort几何探针|科技、预算、产权、法域、建设流程和广场完整路网|
|整状态复制、同步全图BFS、map/set|分页写集、SoA、Chunk/门户动态连通、异步三态query|
|kernel文本checkpoint/FNV回归fingerprint|SHA-256块/pack、生产fsync/多代保存、版本迁移、全域L1/L2重放|
|3个原创几何灰盒|所有建筑家族、写实模块/材质、容量/三时代/文化外观、完整LOD|
|C++/Python标准库可执行|SDL3/bgfx/PBR/water/ozz/RmlUi/IME实际集成与GPU实测|
|目录提取数据与27章设计映射|完整中英定义/flags/doctrines/naming与全领域规则实现|

没有声称10000/100000完整人口达到帧率，没有经济平衡、真实战斗、社会传播的实测结论。原稿提到199条旧追踪、独立国旗工具和TS参考未提供，本轮不声称已经运行它们。
