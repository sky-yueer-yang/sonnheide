# v0.9 类型化交互与输入规格

状态：可实现规格与标准库参考验证；没有原生UI或生产handler。机器入口为`data/contracts/interaction_runtime_v1.json`。

## 表单与共享命令

70类页面的每个可编辑字段都有具体值Schema，继承页先展开字段，不显示“same_state_fields”占位。基础字符串、颜色、主题、预算、基因、政策、对象集合、实物操作、建设、研究、军令等均规定属性、单位、范围与必填项。禁止未知属性与任意JSON覆写。预算为basis points总和10000，余额/库存不在可写字段；基因逐trait bounded且不写Species；所有实际交接/建造/修补都是请求，不是直接设置完成状态。

对象草案保存`DraftId/WorldId/SessionGeneration/targetRef/page_id/targetRevision/fieldValues/localDraftRevision/dirty`。`PreviewObjectDraft`按字段所属域生成完整计划；`CommitObjectDraft`只分发该计划中的已登记命令，在单写者按当前依赖重新验证。它不是另一个世界写者，不代替法律批准、真实资源或知情资格。危险后果需相同token确认，不自动接受重算后的新损失。各页面共享同一Schema、错误和查询事实，不由UI自己猜权限。

UI传输的绝对tick、revision、货量和minor金额使用规范十进制字符串，严格checked 0..9223372036854775807，不经过JS Number/浮点；Q、坐标、有限count/duration保持有界整数。WorldId用32小写hex、stableId用16小写hex、generation/SessionGeneration/DraftGeneration用uint32≥1，递增溢出拒绝并结束该会话/草案，不回绕复用。page_id是视图，Ref alias比较前归一（person/animal→actor、empire→state、port→building、city→settlement、ship→vehicle、equipment→item、thermal_exposure→actor只读快照），skill视图用ActorRef＋view_selector.skill_id，训练值必须与所选Actor/skill_id相同；不产生第二实体；save wire的权威整数由核心解析，UI wire格式不改变世界hash。

查询返回`worldRevision/queryGeneration/targetRef/status/fieldValues/fieldMetadata/relations/historyCursor`。状态严格是live/archive/missing/permission_denied；未知数值为null而非0。查询另携page_id与可选view_selector，技能页必须给已登记skill_id，其余页面不接受该subselector。metadata返回type、unit、allowed_values、read_only_reason；草案只改本地值。已死亡对象用历史页，不把同名新Actor替换进旧页面。关系点选、后退恢复tab/scroll/filter/draft；切语言不改变world/数值。请求generation不匹配的晚结果丢弃。

## 输入状态机

主状态`Observe/BrushArmed/StrokeRecording/PreviewPending/PreviewReady/CommitPending/Meta/Possess/Modal`互斥。UI先消费指针/键盘，只有未消费的地图输入才进入当前工具。按下记录pointerId/world/session/tool；失焦、取消、隐藏、退出world结束stroke，不把按住时长补算成新的剂量。植被的可验证时间输入仍按vegetation_generator合同，而普通类型涂刷按连续扫掠命中格去重。

默认鼠标左键查看或执行当前工具，右键拖拽平移、按下未拖则取消当前草案；中键拖拽环绕（yaw全360°，pitch在10°—85°），滚轮指数缩放。WASD平移，Q/E绕目标旋转，Space暂停，1/4/16/64倍在底栏选择，Escape按modal→预览/笔触→附身释放→主菜单顺序处理。输入焦点在文本框时不触发WASD/数字控制组，中文IME composition结束前不提交。Ctrl+1..9绑定现存Ref集合、1..9选择；暂停键仅非文本焦点触发。

近景拾取取精确射线正向最近真实碰撞面；相同量化距离的优先级Actor→equipment/item→building→Plant→surface，再Ref排序。不以透明decor、水反射、发光体或LOD代理夺取点击。选中工具要求的目标类型先过滤；Alt打开当前位置合法候选列表以选择遮挡物。远景按当前选择图层查询实际地面点的国家/城市等对象，不以旗帜贴图颜色判身份；没有目标显示空地信息，不自动选择最近国家。

任何画笔/附身不能同时让同一输入直接编辑相机和World。进入元控制或观察只选择scope，不获取租约；观察不建租约。AcquireControlLease使用META_CONTROL或ACTOR_POSSESSION两种模式并原子附带可选首条指令。所有依赖既有租约的操作必须携expected_lease_revision十进制字符串；Ref.generation不能替代当前lease_revision。ExitMetaControl携所见租约与版本的完整列表，过时列表拒绝重新预览，实际退出时可信session生命周期统一释放。IssueArmyOrder明确区分法定职务与玩家租约授权，不能以null租约绕过。控制租约的独占/续期/暂停已由civic合同规定；失焦停手动输入，不伪造AI已执行指令。镜头可看外海；越界写入拒绝，不clamp到边缘。

## 无框与三语

所有按钮无border/outline/ring；键盘焦点用整块亮度、颜色与可访问名称。2026-10-10起所有前端含主页均复古街机像素化，实际像素字体、离散sprite和填充阶梯控件，旧油画网站构图只留历史源件。当前原生窗口最小640×480 logical px，底栏宽屏八列、窄屏四列两行；工具带和详情可滚动，窄窗覆盖地图仍保关闭/返回。字号按12整数倍与8dp节奏，DPI由平台实际比例换算。后续独立字号缩放/键盘重映射仍待交付；必要数值/后果/错误清楚显示，装饰小字禁止。当前像素规范及两Age配色见[前端美术](../design/FRONTEND_ART_DIRECTION.md)。

所有错误都有中英德模板和稳定code；动态对象专名不翻译。三语标签、enum和值在运行定义中，不依赖模拟Language。DPI/德文长文本/中文IME/键盘重映射属于原生阶段的实测，不因本Schema通过就称已实现。

保存的rename后directory fsync失败返回DURABILITY_UNKNOWN，保当前内存世界/Continue与旧检查点，不声明磁盘指针仍未变化，不自动发布候选；普通SAVE_FAILED只用于提交点之前的失败。界面显示实际可恢复状态，禁止把未确认持久化写成保存成功。

Ref.kind由runtime_foundation.object_ref_wire.canonical_kind_registry严格生成枚举，未知kind拒绝；页面别名在UI边界归一后再提交，batch泛称必须解析为stock_batch或fruit_batch，不能造泛称新实体。

命令Envelope必须带canonical target Ref与expected_target_revision，不能靠当前选中页面或显示名字寻找目标。所有Ref须同World并解析其generation及当前Revision；创建命令用实际既存发起者/父scope（如Actor或World）为target，新对象ID由一次提交分配。target_page只表示入口页面，并不宣称尚未创建的对象已存在。Rename针对实际被命名对象，不针对不存在的NameRecord。草案Envelope.target必须与ObjectDraft.target一致；所有授权/签署/到期票据用history_event中已登记receipt subtype，message为真实排队消息记录，不能把任意Ref当授权。
