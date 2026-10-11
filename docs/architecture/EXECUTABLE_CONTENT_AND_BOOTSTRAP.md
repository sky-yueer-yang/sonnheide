# 运行内容、科研实验与文明起步账

版本v0.9 / ADR0018，2026-10-09。本文和机器定义规定待实现运行内容；`planned_only=true`、`production_implemented=false`。当前没有C++游戏、可试玩入口或实际商品/建筑资产。完整定义与有限可执行参考模型不等于生产实现。

两份运行输入是[economy_content_v1.json](../../data/content/economy_content_v1.json)和[technology_runtime_v1.json](../../data/content/technology_runtime_v1.json)。只读来源仍是原technology/businesses目录和v0.6逐字文档；运行文件不覆盖来源。运行科技为40 capability、92 family、276 slot，共408项；停用F0701/F0702/F0703/F1306及其12slot，共16项。运行商品116项、配方165项、建筑功能24项、行业13项、运行target132项。数量是本版定义数量，实际生产完成数为0。

## 1. 此次补齐的真实断点

原有经济与科研算法可以描述“计划、研究、制造”，却缺少能被运行程序读取的材料、单位、工时、效果目标和初次起步条件。缺口会引发三个硬死锁：没有粮食就无法产生劳动、未会制造机器就不能做科研、货币尚未建立就要求先领货币工资。仅增加科技名称或一条万能效率加成不能解决。

本版用有限生态来源→实际劳动与代谢→具体配方→实物设施与知识→合法采用与交易的因果链收口。新增两个必要的资源依赖修复：早期书写墨料可用原始小型制炭，不能先要求冶金T008；T024机械件可在公共工坊少量制造，不能要求先建一个本身消耗机械件的T025工厂。专业矿场仍需真实铁材与支承，最初冶金实验可消耗有限裸露矿样，不赠矿场或开采库存。

## 2. 唯一时钟、质量与量纲

首样冻结20tick/运动秒、12000tick/游戏日、500tick/日历时、30日/月、360日/年。1x一个游戏日相当于600运动秒；倍率只改墙钟发布速度。睡8日历时为4000tick，实际劳动、照护、研究、搬运、通勤和军役共用一份ActorAvailabilityLedger。2m格、实际坡道和三维Actor由空间合同定义，本文不把格数或镜头单位当路程。

散装物均以整数g保存，实物工具/衣服/载具以piece保存并带每件质量g。energy_milli使用life唯一整数能源账，Q为0…10000。原始徒手代表样例1WU=1实际接触工作tick；技能、身体、工具、伤势和能源门控改变实际获得的工作量时必须保存余数。劳动时段不能因为制作速度提高被重复分配。

配方40energy_milli/WU是life劳动扣账的定义引用，经济WorkReceipt是扣账证据，不另扣一遍。真实负重移动采用life公式：按实际mm距离、当前递归携带质量、species最大负重和运动方式扣能量。300000的日粮能源是基代、负重行走和实际工作共同预算，不能同时承诺4000WU再无界旅行。

商品quality_q是实际批次品质；卖方声称、消费者观察与真值分开。科研只可使用本项目实际接触的批次品质和实测证据，不通过校验反馈读竞争者隐藏库存或技能。

## 3. 商品是有来源和到期的真实批次

每个商品定义quantity_unit_id、mass_g_per_quantity_unit、营养和食性、耐久、保质期、实际仓储空间及source/owner/custodian/location/madeTick/expiryTick/reservation身份。不同owner、品质、来源或到期的批不能无条件合并。in_transit、已预约和预期产出不算仓库可售余额。

| 商品 | 单位 | energy_milli/单位 | digestibleQ | 绝对保存的保质期 |
|---|---|---:|---:|---:|
| fruit | g | 250 | 8000 | 3游戏日 |
| fruit_pulp | g | 250 | 8500 | 2日 |
| grain | g | 600 | 9000 | 60日 |
| prepared_food | g | 350 | 9500 | 2日 |
| milk | g | 150 | 9500 | 2日 |
| meat | g | 500 | 9000 | 2日 |
| egg | g | 400 | 9000 | 7日 |
| honey | g | 1000 | 9500 | 180日 |
| forage/feed/dry_feed | g | 80 | 8000 | 3/15/60日 |
| seeds | g | 100 | 5000 | 180日，并保存真实viability |

最终吸收是`quantity × energyPerUnit × digestibleQ × nutrient_absorption_q / 10^8`，整数余数由life消费receipt保存。各species真实食性继续门控。prepared_food保存实际食材组成，谷物水煮型是plant-only，燃料木材不是食物；不能把meat改名混合熟食让草食动物食用。

seeds保存parent_plant_species、crop_profile、viability和原batch。果种只能生长同类果株，不能拿果种冒充谷种；播种、成熟、采种和交易均保存variant。装饰花草不是forage，也不能拿God幼树、拷贝树、成年动物体重或非可回收维持能量制造食物。

raw_hide/bone/wool不是现成皮革或无限动物材料。只有普通非启智动物的保存productive recoverable reserve可在真实死亡后接触回收meat/raw_hide/bone；人类和启智动物死亡只按遗产处理。milk/wool/未受精食用egg/honey须life形成真实reserve，Gestation/Egg受精记录不作为蛋商品。生皮再以真实盐、水、纤维和两日等待鞣制为leather；wool须梳理为纤维。转换工、输入和loss都有独立唯一receipt。

## 4. 配方和源资源账

配方有具体inputs、outputs、work_units、contact_ticks、minimum_elapsed、设施、工具、知识门控、source_debit及loss。所有材料是实际批次；每个输出满足：

`输入质量 + 真实源扣账质量 = 输出质量 + 已嵌入结构质量 + 显式损耗质量`。

散石、落木、矿样、果树、机制纤维Plant和Crop不是万能仓库。source_quantity=0或设施/工具/路径/接触/实际能源缺失时不产货。每次取用按sourceRef及唯一receipt扣实际数量，多方抢最后一份时只能一方获得该份；验证失败不能出现先扣料再返双份或半成品照样完成。

| 起步操作 | 真输入或源 | 真输出 | 接触工作tick | 关键门控 |
|---|---|---|---:|---|
| 采果 | 成熟果reserve1500g | fruit1500g | 100 | 可达成熟batch，真实路程 |
| 捡石 | 散石2000g | stone2000g | 120 | 现有有限GroundResource |
| 拾木 | 落木3000g | wood3000g | 140 | 有限落木/可徒手细枝，不伪造大树砍伐 |
| 取纤维 | 机制纤维reserve600g | raw_fiber600g | 180 | 非decor |
| 打石手具 | stone1000g | 800g手具一件 | 300 | 余200g碎屑 |
| 石斧 | stone1000+wood500+fiber100 | 石斧1200g一件 | 450 | 已有真石手具 |
| 手纺/手织 | fiber600→thread500→cloth450 | 实际线/布批 | 300+400 | loss100+50g |
| 内裤/胸罩 | cloth60/90g | 50/80g一件 | 180/180 | 实际成人body fit |
| 小型制炭 | wood1000g | charcoal250g | 500 | 消耗真木，余750g燃耗/残料 |
| 谷种播小田 | grain species seeds200g | living Crop结构 | 500 | 200g嵌入，非免费消失/库存种子 |
| 谷物收割 | 真成熟Cropreserve5500g | grain5000+同种seeds250 | 650 | 余250g损耗，至少7日实际生长 |

设施的配方不是默认无限生产。门控验证后投入进入真实WIP，work和elapsed分别积分；实际场址/WIP/源被毁时结真实loss，已赚劳动权不删，却不能从过去progress补出已经被毁的材料。只能在实际完成、一次commit时产货；切画面、加载、改主题、同样receipt重放均不补库存。

效率改良若降低未来loss，必须在同一实物产出receipt内重新计算usable/loss，output受真实source、质量与能量上限约束；不能在固定配方输出之后再乘加一份科技奖励。已发生损耗或劳动不退款。

## 5. 设施功能与建筑施工

24项定义是功能规格，原创美术几何尚未制作。building_function含居民床位、仓储g、真实工作位、教学座位、泊位/车位、最小占地、入口、前侧送料容量、材料、等待工期、知识门控和损伤后功能规则。building ID不自动等于企业，广场法律身份/账户亦不等于当前尚存楼体。

| 功能 | 代表容量 | 实际施工材料 | 到齐后工期 |
|---|---|---|---:|
| basic_shelter | 4床/80000g | 木30000、石8000、纤维2000g | 1200tick |
| house | 6床/150000g | 木60000、石30000、布5000g | 2400 |
| public_workshop | 4工位/200000g | 木45000、石25000、纤维3000g | 1800 |
| textile_workshop | 4工位/180000g | 木50000、石20000、纤维4000g | 2000 |
| farm_plot | 2工位/30000g临时容量 | 木2000、石1000、纤维300g | 到齐功能建立，真实setup300 |
| arms_workshop | 4工位/200000g | 木60000、石40000、铁10000g | 3000，需T015 |
| port / shipyard | 8/6工位，港2泊位 | 各实际木/石/铁/纤维清单 | 7000/9000，需T010 |
| university / research_institute | 12/6工位，12/4座 | 各实材清单，无教师生成 | 8000/6000，需T007 |

其余city_square、animal_care_site、mine、furnace、technology_factory、warehouse、office、chapel/cathedral/temple、road_section/bridge/defense_wall/gate均有相同完整字段。办公室是可承载真实服务的功能，不把Investment、Bank等虚构成新建筑。

JointSiteApproval联合验证全占地真实支承、绝对floor、必要矩形地基的付费实材、实际人入口、车库真实车道、前侧送料区、claims和旧接入。所有初始八地形固定高度按runtime合同；本版无海岸渐变。港口只可在真正湿面与可入泊接触处工作，不拿普通chunk边当岸，不免费填海。标准人入口1200×2200mm、车道口3000×3500mm只是首样minimum；大species或真实载具必须能通过实际几何，不靠clip缩身体。

材料由真实人、合法实际载具运到front，不能远仓瞬间变楼体。到齐后才开始自动建造，无砌筑人，onsite_masonry_actor_work=0。长方体铁丝网逐层围上、全包、反向拆是表现，动画不结工期或材料。若再加工/安装有实际接触工，独立WorkReceipt占实际日程。

open_work_site/community_work_site可在合法净空dry非高峰地面工作，无需先建公共工坊；是保存的真实ground cache与contact interface，每处30000g、4位、setup300tick。起步fixture用四处cache，合计120000g，但每处仍受真实容量和非重叠位置约束；这不是赠四座仓库或16份劳动额度。

## 6. 13行业如何运行

行业定义映射源13角色到真实facility、recipe或transaction_handler。公众共同体先办农业、制衣、储运；成熟货币、私企门控和入市容量条件才允许对应私人法主体。

| 行业 | 真实运行入口 |
|---|---|
| Farm | 匹配谷种/Crop、实际浇护、采粮/草料、实际育苗设备 |
| Forestry | 有限落木、石斧采伐、树果互斥、合法育苗/培育 |
| Mine | 有限已存在矿reserve、专业支承/工具、grade观察、真实采出 |
| Port | 实际泊位、湿通路、船舱/装卸、真货custody |
| University | social实际教学/ResearchProject、座位/老师/学生真实工时 |
| ResearchInstitute | 同一社会研究owner、真实实验/许可交换，无第二进度 |
| TechnologyFactory | 真配方/原料/工具/line/install版本；公共工坊低量先行 |
| Logistics | 徒步、手推车、真实船、后期非马太阳民车的领取/移动/交货 |
| Construction | JointSiteApproval→前侧真送料→结构嵌入→自动工期 |
| Investment | 真实现金转款、唯一DemandQuota、压力情境和应付债后分红 |
| Bank | 存款真实现金保管、有限loan、真实还本息，不贷出空气 |
| ConsumerGoods | 真衣服/食物/乐器/家具及可达实际服务时段 |
| ResidentialLetting | 已functional可居住单位、真租约、搬家/押金/实际租金 |

public_operation不需要先有私企或银行。投资不自动给设备，银行利息应收不算已收到现金，施工/大学不凭industry名字刷新工人。农业和护理劳动与制衣/军役通过同一日程取舍；普通动物成熟年龄不能冒充成人工人。

## 7. 408项科技的真实定义和采用

每node有唯一id、原source_line、真实三语labels、source_prerequisites与有效prerequisites、消耗材料、工具设施、接触工、elapsed、experiment_rule_ref、typed effects、采用培训/安装成本和作用范围。来源40+96+288=424原样保留。源内没有活节点依赖已停马family；新source若引入这种边必须显式定义合法替代准入，loader不能把停用项默认视为已知或删除存活节点凑数。

知识状态AWARENESS/ACCESS/COMPETENCE/LICENSE/ADOPTION分开。经济做出资、采购、许可、真实设备安装；社会ResearchProject唯一积工/耗料/样本/知识receipt。发现可知不等于任何国/企业已会制造；授权知识、身体工具适配与训练仍实际完成。prototype只成为非可售实验WIP/残料，研究本身不赠可卖机器、枪炮、衣服或角色。

| 方向 | family实际操作与slot作用对象 |
|---|---|
| 农业F01 | 犁耕深/工作、播种散失/行数、切幅、真灌水、脱粒loss、种床面积 |
| 林业F02 | 实际锯切/支承、可用材loss、干燥等待、绳载荷、苗木照护 |
| 采掘F03 | 实际采工、支承载荷、提升量、矿筛loss、排水/通风与检查 |
| 冶材F04 | 炉壳热耗、真实残料/杂质、模具磨损、边角检验、处理loss |
| 机械F05 | 定位误差、传动/摩擦能耗、装配/换线真实工、返工 |
| 建筑F06 | paid kit接头loss、实际span/load、养护/送货/检修；无施工人动画 |
| 陆运F07 | 仅路面、太阳货车、太阳私人汽车；没有私马/骑乘/马车 |
| 水运F08 | 真舱载、装卸工、壳损、坞承重、实际乘员座位 |
| 知识F09 | 实际书写量/校核、查档工、监督训练、翻译实际review |
| 光学F10 | 真观测误差/校准/缺陷可见阈值；不授全知 |
| 纺织F11 | 引导/作业/质量三slot、实际纺织工/loss、裁剪fit、未来wear |
| 消费F12 | 原创乐器制作/误差/实际session、家具承重、真观众位置 |
| 冷兵F13 | 剑盾枪弓弩真实训练、未来磨损、付费保养；骑兵停用 |
| 黑粉F14 | 真已装弹药misfire、装填工、未来磨损、弹药实际检验/修理 |
| 账务F15 | 单位实际记账工作可处理entries、校核/关账，不能修改钱 |
| 仓储F16 | 实际protected capacity、取放工、future decay；不刷新到期 |

family三slot按原“连接/作业/承重”等不同意义指定具体参数，不是三个等级重复`efficiency+5%`。例如F0101三slot分别将joint_wear40→25points/1000WU、till_contact80→60tick/m²、till_depth120→180mm；F1602改变实际容量200000→280000g、取货35→24tick/kg、future_damage25→15points/日。容量不生成货，深耕不豁免坡道、高峰、真实水和种子。

40capability效果是命名FeatureGate，只登记对应有条件行为；T031允许评估私企准入但不创建企业，T018允许真实货币化命令但不发行直到单独合法init，T039允许制度流程但不改现有国家，T040允许真实圣所工程但Religion仍需玩家创建。

## 8. 确定实验规则，避免“成功bool”空洞

`economy_content.experiment_rules`用408个experiment_id键登记每项真实实验，公共工料/时长规则只引用一份experiment_common_profile。每rule关联node、最少不同samples、必须已有的receipt类别、测量字段/单位、确定EQ/GE/LE/BETWEEN/IN_DECLARED_RANGE谓词、失败残料与保存状态。缺experiment/target/参数/单位/unknown opcode时拒绝内容加载，不能silent no-op。

capability使用40个具体受控活动：T001检查真种200g、照护≥420tick、成熟粮≥5000g与同种种子≥250g；T002使用既有真实语言24个编码符号、独立读者正确≥22；T004三次长/质量对照及误差≤10mm/20g；T018用1000minor单位试验记录检查平衡/重放，却不实际发行；T028五次实际装填试验≥3次有效；T034真实现金custody测试reserveCoverage≥10000Q且createdCash=0。其余逐node具体predicate在JSON，不由工程自由填一个EXPERIMENT_VALIDATED。

family实验测它自己的三类实际参数，至少三个不同sample，每项在声明单位/范围内。slot实验至少五组真实baseline/variant测量，以明确variant±tolerance、baseline±tolerance和paired improvement≥半个声明差值共同准入。poor input/skill可能使实验失败，求知或冒险人格不改变成功谓词。

actual prototype参数由实际关键材料最低quality_q、实际合资格research skill_q、实际设计proposal、真实接触sample计算：

`slotActual = base + trunc((variant-base)×materialQualityQ×qualifiedSkillQ / 10^8)`，再限declared bounds。

family baseline在低品质/技能时按该参数保存的worse_direction产生有限偏差。观测加入`((savedCursor % 5)-2) × min(tolerance,1)`的已保存接触误差；actual sample与observed measurement分开。误差序列不因打开页、LOD、线程或retry校验重抽。模型中的完美Q fixture能达到声明variant，低Q fixture不得把variant名字当实际量。

N个sample将本项目材料/工作预算分成N份，余数按sample cursor一次分配；同批材料不能在五个sample各称独占全批。新试验须新sampleId、真实料/工/等待和资金。失败不退款，不授知识，残料有真实custody和数量；有些材料可依法后续回收，但不是再次成为未经扣账原料。

## 9. 参数解释器与采用守恒

effect只有三种：UNLOCK_TYPED_FLAG、ENABLE_FAMILY_OPERATION、SELECT_SLOT_PARAMETER_VERSION。每个效果指operation或capability target；每个integer参数有单位、base/min/max、唯一slot_owner、实际consumer_binding和interpreter_operation。target仅接受实际installedTarget、revision、input/source、workreceipt、quantity/unit、body/interface、route/support、receiverCapacity及savedRemainder。

解释器分别计算实际训练工、接触工、elapsed due、绝对损伤、观测误差、真实loss、已存在材料回收、实际能源扣账、实际throughput、几何面积/座位/承重capacity、最大湿度和实际已装药misfire。所有公式在consumer_operations；产出还受真源和receiving capacity限制。没有万能综合分或纯展示的“科技效果”。

实际采用参数为`base + trunc((variant-base)×adoptionShareQ×conditionQ/10^8)`，限界且每slot只有一个installed版本。每个参数独立处理；同参数多来源只允许去重有界加法，不指数叠乘。坏设备/未安装培训只能保base/部分采用，不能研究瞬间全世界提升。

安装需实际设备或method kit、真实培训/接触工、停机时段、材料与合法许可。已售旧物不自动升级。绝对damage、alreadyCompletedWork、实际expiry/source量、remainder及已加载弹药均保留；新参数只影响下一个合法阶段，不能让正在半途装弹突然“已经完成”，也不能旧damage按新cap重置为满。

## 10. 真实冷兵与后期火器

初始无装备赠品。徒手、实际木棍/石具先行；传统剑/盾/长枪/弓/弩为T015及F1301…5知识、实际制造、可适配手具、真实训练和实际弹药。训练最低baseline300实际tick，slot220不是赠熟练；仍需realSkillGate。皮甲/金属甲也是实料制造和真fit。

火绳T028/F1401、燧发T029/F1402、轻/野战炮T030/F1403/F1404在后期。black_powder还须F1405，gun shot_charge和cannon_charge是各一包真粉/弹/包装实物。loading从实际stock转入武器chamber custody，fire/misfire唯一ShotAttemptId扣一次；既有loaded东西不再从仓扣。

| 设备 | baseline装填tick→slot | baseline misfireQ→slot | 每发磨损milli→slot |
|---|---:|---:|---:|
| matchlock | 110→85 | 1200→800 | 30→20 |
| flintlock | 90→70 | 600→400 | 30→20 |
| light_cannon | 240→180 | 800→550 | 30→20 |
| field_cannon | 300→225 | 800→550 | 30→20 |

武器/甲唯一durability_milli capacity100000；points100只是派生展示。剑/长枪接触wear10→7milli，弓/弩30→20milli。InstalledOperationVersion是未来对应参数唯一出口，与combat baseline一致，不再同时扣base与tech。

`misfireQ=clamp(installedBase + min(2000,weaponWetQ/5) + (10000-conditionQ)/5,0,6000)`。保存draw与ShotAttemptId，失败同样损失已装一包药，无projectile/伤害，BLOCKED后须30实际付费清障tick再装。tech维护减少未来真实维护工/材料损耗，不免费修好枪。后期太阳民车/传感只civilian，不解锁坦克、现代枪或马驱系统。

## 11. 四名真实人的启动成功场景

成功fixture明确配置四名玩家放置18岁的人、24°C温和可达真实地面，初始工具/衣服/仓库货/现金=0。不是每张Blank都保证该生态条件。现有成熟树果初24kg、14日最多6kg/日的真实新生果reserve；落木180kg、散石60kg、机制纤维12kg、同种野谷种2kg、真实可取饮水100kg均为有界source，所有取用记receipt。

成熟果来自明确CreationEcology budget或已经真实长成；God树初始幼株fruit=0时必须实际等长果并具生长source，再放人或提供另一真实食物源。6kg新果/日不是仓库补货：必须有活成熟树、保存的cycle/remainder、有限生态water与biomass budget；fixture各84000g最多14次，明确6000g biomass+6000g生态water→6000g果+6000g蒸发残料。十四日后该fixture不再额外保证供果，未配置长期生态预算不得声称永久自给。

源距cache≤30m，基础负重步速1500mm/运动秒，一次来回800tick。四处ground cache各30kg，不赠建筑。每人实际携带上限至少12kg（human代表profile15kg），每次真source动作和运送占一人的实际interval。

第一步走到source，直接吃真成熟果100g，吸收20000energy；不是赋初始可回收食物。有限nonrecoverable maintenance可付本阶段存活/抵达/direct survival采食，但之后非采食商品劳动只能用真productive food。各人实际采1500g日粮，吸收300000，供基代120000、实际负重移动和实际工作，不因拥有1日口粮而授整日任意劳动。

前期材料账：13次落木=39kg、7次散石=14kg、7次纤维=4.2kg、2次同种谷种=1kg。由真实人分配采集/运送，无同人并行；逐项用于一件手具、一件石斧、600g纤维→500g线→450g布、4内裤+2胸罩、4床住屋、四个真实farm_plot及播种。其余是有来源的剩料，不能假设所有初始source已经进仓。

4内裤+2胸罩共消耗cloth420g，输出衣物360g，缝料loss60g；原600fiber纺织loss150g，布剩30g。全部衣服满足实际成年body fit，不变为儿童身材。住屋需40kg实料由真实carrier送front，到齐后自动1200tick，无砌筑工/动画结算。

农田每块另有木2kg+石1kg+纤维300g建址/前侧送料，setup300tick、播匹配谷种200g/500tick。每日实际浇护60tick并取真water1000g；最早sowTick+84000tick才成熟。四块首次实际成熟收粮合20kg、同种种子1kg，采收每块650tick，粮并非播种后初日赠品。

有限模型排四个人的真实不重叠日程、每天4000tick睡眠、真实负重路程、接触工与基代/两能源账；前3日采集、随后制作/送料/播种/浇护、14日窗口核首粮。实际结果由统一模型运行报告，本文不预先宣称验收通过。真实大地图、复杂坡道/不同species/家庭与军事竞争需要后续生产联测。

## 12. 启动失败是正式结果

空海无合法地、裸地无成熟食物、幼果0、被高峰隔断、无matching grain seed、材料不足、负重/身体接口缺失、实际water不足、寒湿额外热耗、通道被撤、途中死亡或已有照护/军役占时均可使启动失败。NeedEnvelope保真实缺口，允许玩家改地/种植等待/放置生态/减少初始人口，或已有真实社会选择迁移救济；不会自动刷人、补粮、传送、退款、赠衣房。

第一口饭后的有限能源只证明一条可运行起步链，不证明任意出生位置能活或所有population永不饥饿。农田未成熟前依靠明确现有果reserve；其先期工量、路程和果到期共同核验，不能拿future crop货作今天食物。获得知识但缺资金/设备/匹配body亦是合法等待状态。

## 13. 首次货币化没有价格循环或重复补偿

前货币时期真实公共劳动可以取得in-kind compensation rights、实际口粮、居住/保护安排；分别保存earned/settled。已经领到或消耗的口粮不再作为“未发工资”重复兑换，真正未结劳动权益不因角色死亡消失，可在合法estate继续。

T018和实际法规/权限到位后，一次初始化冻结monetary_scope、currencyId、成员/债权snapshot、定义hash和eventId。固定issue ceiling1000000minor单位，与最近报价无关；公共base600000、实际居民等额pool200000、真实未结劳动pool200000。未结劳动固定10minor/WU，只读已赚且未被实物/工资结清的WorkReceipt权益。

每pool整数floor比例分配，再按持久旋转cursor分余数；居民不是普通动物，已经启智并实际Civic/身份满足者依法计。无居民或无有效劳动债的pool余量入命名public reserve。劳动权额超过pool时仅实际分到的货币结清同值权益，剩下真实outstandingRightValue仍债，不记录“全发”。equal grant是居民发行分配，不伪装成第二次工资。

所有recipient cash账户之和含public reserve必须等于固定issue，issuer counterpart同一TxId记相反总额。先完整prepare/validate，再原子commitNO_CURRENCY→INITIALIZED。prepare失败0发行；sameevent重放返回旧receipt；同scope另一event并发拒绝；晚加入、时代变化、读档、默认语种均不remint。

保存scopeId/currencyId/event/profileHash/freezeTick/eligibleSnapshot/issuedTotal/recipientEntries/publicReserve/laborConversionReceipts/paidRightIds/rightValueOutstanding/fairCursor/issuerCounterpart/committedFlag。后续普通税、借贷、工资、救济只转真钱；政策预算权限、ArmySupplyRequest、未来税或未到账银行利息均不当余额。

## 14. 衣服持续更换，不封锁工作

所有衣物是实物和绝对损伤，穿戴tick按1point/100tick及湿度/load profile累计余数，卸下不继续穿戴wear。内裤cap18000points，样例120points/日，约150日实际穿戴后需要真实修换；这不是墙钟定时毁衣。

最多三次修补，每次恢复不超过cap20%，终生future recovery≤cap60%，须真布料/接触工与实际fit。不能售回、载入、换theme/owner/外观刷新cap、damage、expiry或repair_count。新布重新制衣才是更换；修补无法让行业一次生产永久无需求。

家庭先保基本生存预算，再按缺衣/实际保温/耐久近坏/岗位正装期待排购买。没有衣服本身不直接死亡；真冷热身体照常。没有正装仍工作、领薪、保岗位，不罚款或禁clock-in，只把合身正装置于更高的有限购买优先。军装原创同款按服役国theme，不用国籍换色复制物品。

## 15. 持久化、载入和计算预算

本版新增内容hash/clockHash、有效technology IDs、Knowledge/安装/slot revision、实验input/work/sample/residue/余数、真实source批绝对quantity/maturity/expiry、Cropseed/water/maturity、建筑front与embedded mass/auto_due、商品custody/owner/claims、衣物wear/repair终生额度、货币唯一event和counterpart。Wire schema由runtime_foundation_v1提供，经济不创建另一存档格式。

内容定义加载应逐项校验sourcehash/408全集/DAG/三语/单位、未知target/参数/opcode、recipe质量/营养守恒、effect与实验ref、industry合法binding、facility/tool存在及真实tech gate。拒绝缺失不会变成“科技已知但效果没发生”。检查和版本化不调用AI planner、不重抽price/参数或改变World。

规划预算沿decision core同一4096全域primitives/tick；内容加载是创建/载入gate，不每Actor每frame重读408节点。科研按due/真实sample事件，生产按WIP接触/elapsed事件，source按保存growth due，wear按实际装备区间一次积分。mandatory安全/账收口不因规划预算跳过。

## 16. 有限模型与未来生产验收

[content_bootstrap_reference_model.py](../research/models/content_bootstrap_reference_model.py)只用Python3.9标准库。其run返回实际通过的范围/节点数/配方数/typed参数执行数/实验谓词案例/货币情境数与明确production=false，尚需统一执行。模型覆盖408全集及DAG、所有effect真实参数与typed算式、工料/样本/测量predicate失败、质量/营养守恒、真实资源有限与receipt重放、四人近源日程/首粮、首次货币的并发/replay/余数/权益、有限修补和更换需求。抽象仪器观测fixture不宣称实现真实Research AI、语义读写、碰撞、GPU或Steam。

以下为40个新增未来生产场景CB01–CB40，均待C++联测；已有EC01–EC72继续有效：

| ID | 必须验证的真正后果 |
|---|---|
| CB01–04 | 缺source/同source最后一份并发/错误单位/recipe loss不平，各失败无半写 |
| CB05–08 | fruit源与批到期/输入过期/一份食物多域抢吃/动物diet拒绝 |
| CB09–12 | 果种不能谷种/God幼果0/负重真实路线/公共工坊先于私企 |
| CB13–16 | 木炭墨料起步/机械件工厂起步/矿样有限/成熟Crop要求真water与七日 |
| CB17–20 | 四人14日真日程/已有照护冲突/寒湿需热耗/资源不足NeedEnvelope |
| CB21–24 | 原424不改/408全部active DAG/未知op拒绝/各slot改变实际特定操作 |
| CB25–28 | 无工料实验不成功/低quality不能name-as-variant/样本重放/实失败残料 |
| CB29–32 | researched无设备无作用/安装不清damage/半装填不提前完/湿misfire真扣装药 |
| CB33–36 | 货币prepare失败/同event重放/并发init与晚成员/已领口粮不再劳动兑换 |
| CB37–40 | cloth有限修三次/长期真实换衣/无正装有工资/楼front/源/WIP损毁结真实账 |

这些场景数量不是生产完成度；最终验收仍要求Windows GPU、实际360°3D像素渲染、真实地形/身体/界面、原创建筑和Steam平台验证。
