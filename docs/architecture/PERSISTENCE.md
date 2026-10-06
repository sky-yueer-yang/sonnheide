# 持久化、重放与恢复

目标是长期文明历史可继续运行、版本升级有明确边界、保存不留下半笔交易。当前有界文本checkpoint只覆盖headless施工/港口切片；生产分块存档和跨平台重放尚未实现。

## 1. 权威数据和可重建数据

|必须保存|可重建/不保存为权威|
|---|---|
|schema/协议/定义集/内容包/GeoSourceManifest哈希与版本|GPU句柄、视锥列表、LOD当前级别、阴影缓存|
|投影、EarthMapSelection、cellSize、唯一worldScale、冻结natural mask/hash|地球全球浏览视窗缓存、表现岸线LOD|
|reclaimed、未完工工程、真实材料/劳动投入、空间约束|effectiveSurface、landOrigin派生值、导航索引与mesh|
|建筑本体等待/自动施工/完成状态、实际到货/投入、startedTick/durationTicks/completedTick、工程定义版本|围网层数采样、表现时钟、pendingComplete、临时网罩GPU资源|
|SimTick、TimeConfig、独立RNG流/计数器、due tasks|渲染frameDelta、线程完成顺序、墙钟时间|
|身份/关系、产权/保管/位置、所有预约和在途状态|检查器筛选、未提交草稿、刷矿/军令预览|
|日月账本、批次质量/版本、合同/生产线/资本/知识|价格图显示采样、可重算的总结统计|
|名字原始码/别名/作者快照/ordinal、语言父源、文化节点状态|本地化后的完整自然语言事件文本|
|不可变章程、帝国授权、法律/和约/依附关系、先知根绑定|UI按钮enabled缓存|
|Army命令/装填/弹药/伤员/士气/补给、移民容纳旅程|炮弹粒子、走路动画时间|
|矿物rate/stock/lastIntegratedTick、车辆/船/容纳器|视觉矿物曲面三角缓存|
|命令回执去重状态、已提交事件序列/历史引用|程序内部指针/虚表/`std::hash`值|

死者压成保留稳定ID的档案，亲属、发明、文化、先知和历史引用不断链。ID不回收给新人。名称历史不依赖当前显示名。

建筑本体采用[ADR 0002](../decisions/0002-building-construction.md)的材料实送与自动工期，没有现场砌筑人物预约；填海真实劳动仍保存。载入WAITING_DELIVERY只恢复工地/配送且无网罩，载入AUTOMATIC_BUILD直接重建完整网罩，载入已完成/已取消时直接成品/无网罩，不重播围/拆动画、材料交付或完成事务。现场货物和在途货物继续保留唯一真实位置。

## 2. 生产存档包

设计包头：magic、saveSchema、protocolVersion、endian、sourceBuild、definitionSetHash、contentPackVersions、worldID、SimTick、committedRevision、sectionDirectory、checksum。每个区块有类型/schema/offset/length/uncompressedLimit/hash；按Chunk与冷域分块压缩，加载先校验目录和解压大小。

保存只读某个完整已提交revision的页快照：写临时同目录文件→逐块校验→flush＋平台fsync→写manifest→原子替换→保留前一个可恢复版本。使用全量checkpoint＋受限命令/事件增量日志，不将无限事件溯源作为唯一存档。失败留上一个版本，取消后台保存不会释放仍被读取的页。

当前`save_atomic`只提供同目录rename与stream flush，**没有生产fsync/日志/多代备份**；Windows覆盖已存在目标文件的替换语义需要专用平台实现。不能据此声称断电恢复已保证。

## 3. 读入顺序与拒绝

字节/大小检查→magic/schema/端序→包/定义版本→自然底图hash→实体记录→重建关系索引→跨域不变量→可重建缓存→发布World。整个流程读到staging world，完成前不替换运行世界。

验证：正式City唯一CitySquare；CULTURAL有文化/语言；Empire有Religion/DivineGrant；股权/政治/语言图无环；章程冻结；批次位置与available/reserved/inTransit互斥；双账守恒；工人时间不重复；每船归Port；泊位保护；人工陆地天然WATER并四连通天然陆地；待施工有真实支撑；港口核心人工陆地与水区；旗模板/色/主题色合法；姓名/事件引用稳定。

格式checksum检查传输损坏，不证明玩家没恶意修改；核心不变量仍需执行。用FNV regression fingerprint不能称为密码学防篡改。当前kernel拒绝不可能Observer Accepted回执、悬空创建结果、历史/提交/accepted revision不一致，不能自动获得生产数字签名或完整事件因果重建。

## 4. 迁移

每个schema迁移为显式`old→new`纯转换，保留原存档、输出迁移报告、重新检查。兼容材质压缩不改变权威建筑定义；改容量/footprint/入口需要显式版本迁移。升级TimeConfig不能把已有tick重解释成别的日长。

原稿提到的旧WorldBox/TypeScript参考和旧存档并未提供，本轮没有导入器。之后只导入可映射的人类、基础空间/建筑/关系；不能从单ID文化字段伪造多文化历史、凭空帝国授权或神权，先显示丢弃/转换清单。

未知较新schema、缺GeoPack/定义集、hash不符、无法修复关系时拒绝并说明所缺版本。不要“尽力读入”后静默清空货物、宗教或公司。

## 5. 重放级别

|级别|承诺和验证|
|---|---|
|L0 内核|同命令/授权/初始mask/stock产生相同文本checkpoint；中途保存/载入与直接续跑一致；当前场景已验证|
|L1 同构建|同机器族/构建/TimeConfig/定义/RNG/任务排序，不同镜头与线程完成顺序产生相同权威hash；待全领域实现后验收|
|L2 跨平台|macOS/Windows/Linux权威hash相同；需固定整数/定点和浮点数学、迭代/归约、编译参数及随机变换；研究目标，不能现在宣称已达到|
|L3 升级重放|只有迁移明确接受的版本组合；旧日志不能无条件解释成新平衡规则|

随机流按worldSeed、system、entity/cohort、逻辑样本counter分离，禁止renderer调用共享RNG。文化动态的不同dt需要同一底层采样路径或经验证的桥接，不能仅每次调用相同Noise API就声称自适应结果一样。工人并行归约用稳定顺序，unordered容器不得决定税/战斗/发明获胜者。

## 6. 恢复与观测

每阶段snapshot写摘要hash，出现首次分歧可定位tick/system/entity。发布时附性能元数据、seed、source commit、定义hash和可裁剪复现存档。玩家日志避免记录操作系统用户名/绝对下载路径。历史按世界分段索引，检查器分页，长期性能不能依赖每帧扫描几十年的全文。

验收包括运行→中途保存→载入→继续的相等性；各施工/物流/装填状态；大图按块故障、CRC错误、缺pack、读较新schema；写失败恢复前一代；不同相机/速度/任务线程数量；版本迁移前后守恒。当前只覆盖表述清楚的kernel子集。
