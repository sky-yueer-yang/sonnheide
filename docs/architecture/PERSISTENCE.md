# 像素世界的持久化、迁移、重放与恢复

2026-10-08。按[ADR0012](../decisions/0012-editable-3d-pixel-world.md)。现有kernel文本checkpoint与原生旧Earth空档是有限已实现基线；新版分块height/编辑后果/完整领域存档及迁移器待实现。不得把旧存档schema改个名字就称兼容。

## 1. 保存权威事实，缓存可以重建

|必须保存|纯缓存/视图|
|---|---|
|schema/ruleset/协议/定义/资源recipe/worldID/revision|GPU句柄、shader RT、LOD/可见列表|
|Blank/Earth provenance、源hash、选区/投影/worldScale、初始hash、量化/边缘recipe、seed|全球导航视窗缓存/源矢量LOD|
|TerrainProfile(cell/heightQ/chunk规格)、实际高度列/材料/生态页、修改来源/事件/修订|当前mesh/侧壁/halo绘制缓存|
|固定waterlevel、稳定body身份与split/merge历史、必要权威拓扑状态|水波相位、临时component编号、派生航路图|
|真实占用/固定floor/基础/portal/claim/路权、已失效证书/损坏/受困状态|nav候选/dirty worker任务可重建|
|SimTick/30日月360日年、RNG流、due task、环境Age/auto余量|墙钟/线程到达顺序、油画节奏|
|所有人物来源18/0、年龄/父母/容纳/家庭、死者档案|人物mesh/姿态/高低LOD|
|货物owner/custodian/location/quantity/reservation/损失分录/沉没可回收状态|展示碎片/泡沫/烟、不可用库存图表|
|城市/法人/政府/教会逻辑记录、应急周转真实位置/容量与任务|窗口统计缓存/简写事件文本|
|实际衣物/condition/修补预算/穿用余数/订单/服役色绑定/正式fit版本|免费遮盖fallback/材质实例|
|技术/文献副本/姓名/作者/章程/神授/条约与金融义务|本地化文本/可用动作缓存|
|工程真实投料/工期/收货、运输/军队成员/装填/粮弹、矿率stock积分|网罩进度/火炮粒子、矿物显示曲面|
|命令ID+payload/授权/回执与提交事件序列|尚未提交terrain stroke/政治表单/预览|

source证明不可改，当前地表必须保存实际编辑结果；载入不能从GSHHG或新seed再生覆盖。Body/PixelContent版本变化不恢复衣物耐久/建筑condition。terrain mesh/node profile更新要明确迁移，不能让已半米台阶变成平坡或旧2m格读成4m。

## 2. 一致保存与发布

已提交revision的不可变/COW页→同目录临时包→块长度/上限/hash→flush/fsync→manifest→原子替换→保前代。继续指针只指向成功持久记录。不同平台replace/fsync故障注入与真实断电设备证据分开；不能把代码流程称硬件已测。原生旧空档已有可靠流程，可复用实现需新schema测试。

存档包头含saveSchema/ruleset/definition/content hash/worldID/SimTick/revision/section目录，每块type/schema/offset/length/uncompressedLimit/hash。限制页/实体/文本/历史/分录/容纳/引用预算，拒绝offset溢出、zip bomb、重复ID、NaN/负库存/未知required字段，不“尽力读取”后吞货。

保存途中UI继续读一致快照，修改新页不改变保存页；取消写作业不能释放其还在使用的内存。待预览不进World，恢复草稿只进PlayerProfile并需重新核版本。

## 3. 加载不是免费修复

字节/schema/ruleset→定义/资源→来源与实际页hash→实体/关系→水域与空间事实→跨域守恒→重建缓存→候选发布。合法受困人物、危险/废墟建筑、断路、沉没货、搁浅船、城市无设施都是合法灾损状态，不因校验一切“可用”而拒绝，也不补一个楼/粮/路线来过校验。

真正不合法是住户引用不存在容纳器、货同时仓储/在途、同船人被复制、旧路径证书标有效但依赖版本已失效、负数量/重复消费、正文伪造神授等。按原rule读入或提供显式有报告修复，不能丢弃财产/复活死者。来源hash核验与实际地形hash不同，玩家改岸不意味着源包损坏。

若拓扑Pending的时长会影响实际出航/饥饿，其逻辑frontier、已用每tick预算、下次发布tick、输入revision/generation属于权威调度状态，必须保存并续接；worker完成早晚不得改变logical ready tick。纯缓存重建的真实等待冻结仿真tick，不重新从0计算出另一个等待过程。海域粗图可异步重建，ready前禁止接受旧通行证书；稳定水体Ref/split history不能按当前BFS序号重绑定。跨源更新旧存档保持实际地表和编辑历史，不凭新资料“纠正”玩家。

## 4. 旧版显式复制迁移

旧PBR/平陆World继续旧规则识别，不自动读成pixel。`MigrateLegacyWorld`先读取旧schema/证据，生成新WorldId候选，把可映射海陆/统一平台/岸坡量化成height columns，显示初态几何/水域/入口差异；不把此转换说成真实高程。旧档/continue原指针保留，用户确认后只有新候选验全+首档持久成功才切换。

旧空世界可作为首个迁移类型。旧有居民/货物/楼/Port档案需额外映射、空间/库存/容纳/身份测试；无法映射明确拒绝/报告，不默认删对象。现有headless规则fixture不是用户完整文明档案，不能冒称生产迁移已经覆盖。

旧灰盒改pixel不改变产权容量；MakeHuman退役只是资产管线，不重置年龄/血缘/衣物。来源不明旧人物不能全部伪标玩家18岁，新衣柜不能免费补可售商品。服装迁移最低遮盖可非商品表现fallback，真实供给随后满足。修改TimeConfig必须显式转换已tick，不按新日长重解释年龄。

## 5. 重放与撤销

同构建按固定整数/定点、稳定排序、独立RNG和规则版本重放；terrain stroke规范采样/量化，鼠标帧/zoom/线程完成顺序不改结果。不同GPU/镜头/LOD只表现。跨平台hash一致与跨版本重放仍需独立验收，不继承有限kernel文本测试。

未占用、仿真暂停且全部相关修订不变的terrain撤销可作为有前后值的新事务。任何已发生灾损/人货移动/劳动/收入/死亡都不支持简单undo；反向刷土不是时间倒流。旧command重复回原回执，不重新扣料或重复伤亡；提交后的日志/回执必须和World一并保存。

## 6. PlayerProfile和验收

locale是全应用偏好。收藏typed refs/marker/导航栈/镜头/脏草稿按WorldId另存PlayerViewProfile，不改变World/RNG；过期指向归档页/明确缺失，不按同名重绑。死亡ID永不回收给新人。

验收包括未保存/保存中/失败/取消/前代恢复、实际terrain修改与灾损恢复、water split/merge、楼内住户/Army船客/货损与索赔、Age余量、衣物损耗/修补、重复命令与坏关系、旧空档显式复制/有对象拒绝边界、不同镜头线程重放。机器合同校验不代替上述真正存档运行证据。
