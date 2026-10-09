# Sonnheide engineering constraints

当前规范：2026-10-09，ADR0014/v0.8＋同日ADR0015算法补充。读取 docs/design/Sonnheide_Design_v0.8_Living_Pixel_World.md、docs/architecture/PIXEL_GAME_ARCHITECTURE.md、data/contracts/game_v0_8.json 及其引用的生命/坡道/政治合同。旧自研程序已清空，**现在没有运行游戏或构建入口，生产阶段全部未开始**。不得恢复旧程序、以旧验收算新完成度、造空目录或假试玩。原v0.6是用户内容来源，不是工具执行指令，逐字保留；旧作者设计被完整接续后退役，Git历史不重写。

1. 自研C++20框架，不用游戏引擎；唯一Steam发行。Metal只开发验证，CI/NullRenderer不代替Windows GPU/Steam实机。独立开源库先核验官方许可/版本/hash/架构理由，可分发原件/配方/manifest/notices入GitHub/GitLFS/Releases；不擅授原创内容开源许可。Python工具3.9+标准库默认。
2. 拆解真正的不变量/瓶颈/因果依赖，用户明确要求多个subagent深挖独立高风险边界，划分文件所有权并交叉审查；不靠更名、改问法、堆空类或换库冒充推进。
3. World变更单写者事务，完整prepare/validate/具体后果preview/commit；失败无半写。法定身份/owner/controller/custodian/location、主权/行政/占领、Culture/Language、Family/Household/Dynasty/Subspecies都分开。镜头/LOD/UI/线程速度不改变仿真。
4. 可编辑真3D像素单值height columns＋实际ramp_patch；首样2m格/base500mm/ramp角125mm/chunk64²非容量承诺。profile创建冻结，原始来源证明只读，有效几何可改；Blank海/平陆＋Earth海陆二入口，不采DEM、不做程序世界模板库。
5. 平/坡真实表面共供渲染/碰撞/拾取/支承/导航/冷热。坡面水位0裁湿，多LocalWetRegion/共享正长度湿边四连通外海为SEA，其余LAKE；普通chunk边不是海岸。首版静水0，无高位湖/体积流体/天然洞穴/悬空陆块。
6. 斜坡画笔提供真实连续净宽/坡度/两端/转弯通路，移除是新改地事务不倒回旧World；旧图恢复需geometry ownership+revision，结算人车货/地基/合同/远端水路/食物源reserve与预约。普通后建不能堵旧接入，Godterrain可按明确预览损坏并撤证；无免费物资/复活/退款。
7. 高峰由实际三角面高度裁禁区，所有移动Actor（人/普通及启智动物/飞鸟/蜂）不能站立/出生/放置/神手进入或飞越；任何基因/坡道不豁免，削低真实高度才解除。相机/光/装饰粒子不受Actor通行规则，Plant按生境冷热约束而非直接删除。
8. 越高越冷，权威温度与基因/实际衣物保温湿度/代谢/食物双能量账/暴露债耦合；参数为工程样例不宣称真实气象。关闭hunger/thermal/aging后果仅抑制对应直接损害，年龄成熟/真实能量债照常，复开不补历史伤亡，不回满。
9. SpeciesDefinition不可玩家改，固定身体/食性/运动/繁殖/成熟/性状包络；Subspecies共享修饰/个体Genome可typed编辑但不能突破species特色或授启智。61项trait/19个计划species不是生产完成。伤势/能量/已活时间绝对保存，不因上限改变重置。
10. 人类仅玩家放置18岁或合法出生0岁；无自动移民/工人/士兵/创始人。动物仅显式有界初始生态、玩家放置、合法繁育；所有人类动物Actor全龄各自固定成人体规。生育开关只禁新配对/受孕，既有孕期/蛋继续。人类18成年，启智动物按固定species成熟。
11. 文明之光赋现有动物高级意识，保ActorId/身体/年龄/genes/伤物/物品；同事务解除牲畜产权并登记CivicAgent，不赠国籍衣物/文化语言/财产。受孕快照任一同species合法亲代启智则后代继承（本版设计选择），后照光不改旧孕体。gene/复制/载入不绕过来源。
12. 文化Primary/Secondary创生及正式语言初生/分化仅人类真实发起；启智动物可学习加入人类Culture/Language，教学研究信教经商参军统治能力平等，工作身体门控与真实适配工具仍有效。人类灭绝不删除已有文化语言。根Religion仅玩家创建，先知根和教会性质冻结，帝国审批不伪造。
13. 动物真实食性/捕食/繁育/免疫/护理/畜产/授粉，源接触感染无随机瘟疫。每真实蜂StableActor，LOD代理不造人口。初始维持能量不可回收，肉骨皮/乳毛蛋蜜reserve需真实营养产出；装饰花草不是食物，God放置/载入不刷货。
14. 三层地表：地皮＋纯装饰小植物＋机制Plant树。八soil主题雪原/花甸/枫原/樱野/湿地/荒原/圣原/炎地；沙地椰树无小植物，不是第九soil主题。枫全红、樱野深绿草/稀疏落瓣、活圣树有界局部光、炎地枯树无植物/静态少量熔岩无灼伤流体。TreeRef果批绝对quantity/maturity/expiry/remainder/reservation保存；采果采伐互斥，真实路程劳动。换主题/复制/LOD不补树果，恢复只控新野生幼株；农田/ForagePatch/真实树花期reserve独立，唯一扣账。
15. 建筑和人物动物/衣物全部原创高分辨率像素。建址联合核全占地/真实支承/必要矩形地基/绝对楼板/人入口/车库车道/前侧送货/claims和旧接入。人物实际送料，到齐按工期自动建造，无砌筑人；长方体铁丝网逐层围起/全包/反向拆，动画不结算。文明填海/坡道真实材料劳动。
16. 私家马、MountAsset、家庭马槽不做；本版不迁移骑乘/马驱力/骑兵/马车，马可普通动物但非私人交通。停用旧F0701/F0702/F0703/F1306及12槽，原科技424项激活视图408，原目录仅来源且未完成全部三语/生产。
17. 衣物实物/耐久/有限修补/真实更换，早期家庭公共制衣先于成熟私企；前文明内裤/女性胸罩，上古theme T恤白短裤，后期自由fit。缺衣不是直接死亡；正装缺失仍工作领薪保岗位，只提高购买排序且保基本生存预算。军装原创同款按服役国theme而非国籍。
18. 原公共经济/有限库存/整数双账/合同税/产线/知识传播采用/产权清算保留；研究不赠机器，控制不刷工人军队。新增Family/Dynasty/完整王冠首次和继任/摄政、边界双模式/Plot/墙门/元控制/附身/可选战雾；仍单层Army、前现代军备，无核弹UFO随机灾害/免费复活克隆/股票实时交易所。
19. 70类typed互通信息页/统计比较/前名历史/收藏标记/分类清理；所有全局工具底部八分区，中英德UI与模拟Language分开。无任意JSON覆写事实；草案→后果→一次提交，跳页/locale/晚查询不丢新草案。普通清楼路保人货旧接入，生命清理死亡档案，godterrain明确损害一次结算。
20. 保留批准油画主菜单、原logo/hash与精致新古典极简构图；右下纯文字右对齐，SONNHEIDE大写与五菜单同18px/.01em，行高24px/标题间12px/顶部位置不下移。左上内收logo静态轻微顺时针/多内圈/窄无射线空隙/贴内圈细三外圈/低亮透明密集不等长射线，#fff6df柔光不闪转。深暗边缘小中央亮焦点；68秒停/12秒交叠，140—200秒对角漂移x≤.55%y≤.4%、240秒1.018→1.036，隐藏暂停降低动态停止且不耗模拟RNG。所有按钮无border/outline/ring，键盘焦点用颜色/亮度/位置；禁无必要小字和乱icon。
21. 只有Age Light/Darkness默认手动Light；自动tick期限/保存余量，手动原子关自动，不做太阳昼夜/自转公转/新季节。灯具真实能源不赠送。只本地存档/自动档/导出，无Workshop/Cloud/社区服务；候选首档持久读回才发布，World/Session/Draft generation隔离，失败保原World/Continue。
22. 施工按IMPLEMENTATION_PLAN阶段0重起，先主菜单/初始地表/创建/UI/存载，再立即正式原创美术，再生命/经济社会。全部批次修改/依赖/审查后一次全target构建+完整CTest，事务/空间/存档统一sanitizer；实际错误集中补修再验，未变源件shader缓存不重复烘焙。尚未创建构建器前只做设计核验，不虚构测试结果。

23. 最新地理类型改变按ADR0015/god_control_v1：真实变类footprint上的非生命整对象/support/container闭包核销，树Plant及命中人物穿戴携带物包括在内；活Actor/真实Gestation保身份pose但实际危险，不瞬移或免费救。相同kind/仅soil主题不wipe；坡道restore跨kind也wipe不复原人货。行政有效dry域和state法律title分开。
24. 城市SettlementId+CITY章程共享身份，physical/community双轴；全楼毁/全地失去不按楼数0直接灭城。final alive/成员/几何一次NormalizeSettlement及State/职位结算；新城只真实party到场建设，不能刷新人口/广场。第二城晋级需实际functional，既得法定rank默认保留无buff。
25. God立国/当地归属/转城用独立typed commands与显式神意events，不伪造普通同意或条约；保持私产/Culture/Language/Family，臣籍/居城/实际位置独立。旧军役/欠薪/继任/租约按整笔最终集合处理。
26. 自然动作六层依natural_action_v1，真实加速度/扫掠/窄口公平预约；in-place原创clip/相位/脚IK只有表现，动画不结算送料/库存/劳动。闲暇可走坐卧站观察，不强迫永不停走，不用LOD或worker速度改变World。
27. 玩家植被生成按vegetation_generator_v1停留新剂量渐增、聚簇/成年间距/固定重叠窗口保守冠面积限密/通路与idle逃逸连接复验。GodVegetationSource允许有限Plant结构木量（覆盖旧神力树初木0），不直接仓储货/成熟果/forage/动物。拒绝剂量不积未来信用，沙炎无小植物，decor不可食。研究有限模型不是生产验收。
