# 命令、查询、事件与跨领域合同

这是生产接口规范。当前C++内核只实现5种演示负载；[原稿第27章](../design/Sonnheide_Complete_Design_v0.6.md#27--最小命令协议与跨系统事务)列出的23个核心命令不是已经完成的API。

## 1. 可信入口与预览

UI/输入工具提交`CommandEnvelope{protocolVersion, commandId, commandKind, targetIds, expectedRevisions, payload, previewToken}`。`actor/capabilities/source`由本进程可信Dispatcher提供；payload里无`isGod`和客户端自报授权。玩家全知观察不能给AI玩家权限。单机不是安全服务器，但这一隔离可以防止按钮/UI绕过规则。

预览返回成本分项、材料来源/预约候选、执行者、目标范围、规则依赖、主要阻碍与替代。预览不预约、不扣款、不创建实体。PreviewToken绑定定义hash、目标/相关产权/法律/空间/市场容量版本、payload hash和授权主体；提交时读取最新权威状态二验。不能只检查建筑version而漏掉海侧造陆、国家法律或已批准市场项目。

结果为Accepted / Rejected / RequiresRepreview，含稳定reasonKey、参数、事件ID、改变实体修订、createdIDs与可选retryHint。界面先显示一个主因，可展开全部。预览缓存过期是重预览，不等于永久禁止。多选先知等原子批量操作必须完整校验再统一提交。

## 2. 幂等与提交屏障

以`commandId + canonicalPayloadHash + authorityIdentity`登记回执；相同ID/内容/身份重复返回原结果，不重新扣钱。改负载、改期望版本、改权限主体都属于ID冲突；重预览后的真正新尝试使用新ID。当前内核canonical包含expectedRevision与序列化负载，按此规则工作。

生产事务步骤：解析形状/数值/文本→权限→所有相关实体版本→领域硬约束→预留写集/ID/事件/回执容量→暂存账户/货物/劳动/空间写集→跨对象不变量→一次publish→索引失效与快照。**回执和事件属于同一提交，不可世界先变、随后回执分配失败。** 写线程是唯一提交者；并发候选只读且按稳定system/entity/sequence排序。

当前内核copy-on-commit并将Accepted回执插入staged，验证后使用noexcept move提交；独立分配失败注入覆盖全部分配点。Rejected回执失败可导致请求重试，但不改经济事实。生产写集不得破坏这个强异常保证。

## 3. 五类共享数据入口

|基础|权威接口|关键区分|
|身份与关系|Person/Household/Actor/Ownership/PoliticalRelation/ReligionMembership查询与关系事务|国籍、住所、位置、雇主、文化、语言、信仰、Army不是同一个Affiliation|
|资产与账本|Accounts/StockBatches/AssetRegistry/Reservation/Posting|钱≠货；legalOwner≠custodian≠location；库存available/reserved/inTransit互斥|
|知识与许可|Awareness/Access/Competence/License/Adoption|发现/读到/会用/合法用/实际安装分别成立|
|任务与合同|DAG、阶段、到期、投料/劳动、承诺、取消/违约|每个进度有真实执行和成本；不能只涨进度条|
|空间与运输|Land/Water、roads/bridge/entrances/containers、path certificates|实体不可同时在地图和容纳器可用；临时抢占不可改变法定物权|

## 4. 六种合同动作

提供货物、搬运货物、运送人员、施工/维护、研究/授权、出资/偿付。复合合同为DAG，不新增“教会远征专用引擎”。每节点含执行主体、所有者/保管角色、输入输出、依赖、窗口/超时、预算承诺、验收谓词、取消处理和违约责任。

预约将真实批次/资金/槽位/劳动从available改为reserved；不制造供给。转在途后源Storage不得仍计available。到达验收改变location/custodian，物权是否同时转移由交付条款决定；付款不是到货，卸货不是所有权自动归城。目的满仓要等待/合法改道/退回，不吞货。

劳动按同一Person可用时间分配，服役、通勤、受伤、学习、施工不能多处算全时产能。工具动画不发劳动，军火粒子不决定伤亡。港口租船只转航程使用权，Ship仍归Port登记。

建筑本体例外按[ADR 0002](../decisions/0002-building-construction.md)：真实前侧送货占搬运时间，所有必需批次到货后一次投入并启动Scheduler自动工期，现场砌筑劳动为零；预览明确显示接货区、材料到达条件和自动工期。填海等其他劳动不因此取消。网罩逐层围/拆是只读表现，不承担材料交付、工程完成或开放验收。

## 5. 典型事务及失败恢复

|事务|同屏障核心写集|失败/部分完成的恢复|
|Primary Genesis|RootLanguage→正式姓名→Culture→成员→State文化化→正式City/旗|任何中间失败无半语言/文化；节点不会二次触发；姓名历史保留|
|Secondary Culture|新Culture/父源/成员/历史|绝不包含CreateLanguage；独立语言分化另走命令|
|逐格造陆（含港口地基）|材料/劳动预约、任务、地表来源、空间禁占、路径版本|未完工保持水；取消只退未发生投入；完成不可挖回；港口本体另走建筑工程|
|建筑本体开始/完成|实际SiteStorage批次、配方投入、自动工期/到期任务、Building容量/入口、事件/回执|无砌筑工人；到货未齐不启动；取消已投入不全退；完成不等待撤网，动画不结算|
|生产线|市场容量/资金/材料/工厂槽/任务|tooling一次性；取消释放未投入；已消耗不全退；换家族另建线|
|教会分裂|新同根教派/章程、冻结争议资产、限制性捐赠、债务/员工、合法转移|成员改派不等于财产自动迁移；旧charterHash不改|
|主权和约|城市主权、过渡法律/税/权益、驻军补给、无环依附图|占领者≠owner；附属保留原StateId；不直接覆盖city.stateId|
|进入建筑/船|地图位置释放、InteriorContainer引用、容量预约、人/车/货状态|显示隐藏但实体和货物仍存在；出口无空位等待/恢复|
|法定改旗|13模板/23色、themeColor有效、历史|主题色若移除同事务重选；不修改人物的国籍/服饰经济|

## 6. 查询与快照

RenderSnapshot只含视觉所需EntityId、变换、任务表达、外观/旗色、选择proxy和revision；不含可写World指针。InspectSnapshot包含四项摘要、合法动作、主要原因与展开数据；账本按需分页查询，不能每帧复制全世界货物/知识/历史。

快照带simTick、committedRevision、definitionHash与空间epoch。渲染插值只用于图像。双/三缓冲保持生命周期；收到较旧picker结果必须校验EntityId仍有效，不能因GPU迟一帧选择已拆楼。地图工具草稿在presentation，不进正式存档。

## 7. 边界输入

ID永不复用；数值先验证非负、有界、有限，整数加减检查溢出。NaN/Infinity、超大数组/footprint、非法旋转、未知required extension、无源资源路径均拒绝。固定文本走zh-CN/en key，错误参数是结构化事实；玩家名字不是可执行脚本。存档读入还要重验关系、不变量和定义兼容，JSON Schema只能检查形状。

开发Debug命令和测试Scheduler的劳动注入不会暴露给生产UI；玩家Divine Hand只能搬位置、保留身份/财产/服役，不成为任意World改写入口。
