# SONNHEIDE

2026-10-10：当前 **v0.9 / ADR0023**。自研C++20框架，真正360°三维像素世界，唯一Steam发行。普通地形八档固定高度；计划坡道连接不同平台；取消自然山海湖身份/命名和岸线/外缘过渡。冷兵器起步，后期真实研究、制造和装备枪械。

**S00/S01实物像素界面本批开发验收通过。** 全套深色实物界面、41原创透明像素物件图标、9实际加载材质、大字饱满按钮已接入；高分辨率图标采用粗暗轮廓与厚刻纹，鲜艳颜色压暗。完整CTest3/3（130.21秒）、71,101核心检查、ASan/UBSan和66操作/32实际GPU帧通过；[本批报告](docs/planning/PHYSICAL_PIXEL_DELIVERY.md)保存记录和截图，[ADR0023](docs/decisions/0023-physical-pixel-interface.md)是当前视觉规范。居中空白创建与地表沿ADR0022/0021。[上批报告](docs/planning/HEAVY_ARCADE_DELIVERY.md)与其他交付保留历史结果。Windows GPU与Steam安装仍待实测。人物、建筑、编辑笔刷和后续机制不属于当前试玩。

|入口|内容|
|---|---|
|[v0.9总设计](docs/design/Sonnheide_Design_v0.9_Executable_Rules.md)、[ADR0018](docs/decisions/0018-fixed-terrain-and-executable-game-specification.md)|最新规则与完整领域接续|
|[运行基础](docs/architecture/DETERMINISTIC_WORLD_AND_RUNTIME.md)|Earth栅格/固定档/坡道/全域tick/存档与库采用|
|[文化语言遗传群](docs/architecture/EMERGENT_CULTURE_LANGUAGE_AND_LINEAGES.md)|共同实践成核、实际交流收敛/隔离分化、繁育群与持久迟滞|
|[商品科技与开局](docs/architecture/EXECUTABLE_CONTENT_AND_BOOTSTRAP.md)|商品/配方/建筑功能/408科技、冷启动与首次货币化|
|[交互规格](docs/architecture/TYPED_INTERACTION_AND_INPUT.md)|70页面具体字段/命令、拾取、输入、草案和三语错误|
|[机器总入口](data/contracts/game_v0_9.json)|全部权威合同与四份运行定义|
|[程序结构](docs/architecture/PIXEL_GAME_ARCHITECTURE.md)、[新施工计划](docs/planning/IMPLEMENTATION_PLAN.md)|S00—S12：基础舞台→立即原创资产→真实生存/公共建造→文明制度/产业战争；信息编辑随域交付|
|[验收](docs/planning/VALIDATION.md)、[本批报告](docs/planning/DESIGN_AUDIT_ADR0018.json)|统一规格与有限模型核验；生产验收待实际施工|

统一命令：`python3 tools/validate_design.py`，Python3.9+标准库。只运行本批新增参考模型，复用未改历史模型的明确结果，不运行已删除的旧游戏。

人类仅玩家18岁放置或真实0岁出生，全龄成人体规；启智动物保原Actor，只能学习维护人类文化语言。八soil主题、机制树果、纯装饰植物、真实衣物耐久需求、缺正装仍工作领薪、前侧实物送料和铁丝网自动施工、八底栏/中英德继续有效。最新UI为原创实物像素风：深裂纹大理石、搭接石牌、珐琅与黄铜按钮、暗木账册、皮革记录板、羊皮纸提示、黑曜石输入。常用执行字24dp、48²饱满透明像素物件配48dp槽/60dp按钮，三语按真实字宽重排。星点闪烁和流星是原生GPU背景，不创建菜单World。世界用细像素材质、方向光影、漂移薄云和风动装饰花草，水面为细像素水纹，去掉写实反光。

所有楼、人、动物和衣物原创像素；自然地理无身份，机制树木、生物、实物仍有真实Ref。改地保生命及实际随身递归物品，植物建筑等按变化闭包清除，生命后续实际危险继续。

不做程序模板库、Workshop/Cloud、私家马/马车/骑兵、随机灾害或现代毁灭武器。环境只有Light/Darkness两Age。

[原v0.6](docs/design/Sonnheide_Complete_Design_v0.6.md)逐字保留；[历史v0.8](docs/design/Sonnheide_Design_v0.8_Living_Pixel_World.md)已被覆盖。`data/catalogs`是不可变来源，`data/content`是版本化运行定义。95份源件、许可、油画/Logo/hash保留，不启用MakeHuman/PBR为新生产路线。原创权利见[OWNERSHIP](OWNERSHIP.md)和[第三方通知](THIRD_PARTY_NOTICES.md)。必需源件托管GitHub，本机缓存、个人档与凭证不提交。

## 新原生程序

统一入口 `python3 tools/build.py --sanitize --native-smoke --bundle`：一次准备完整资源、构建全部目标、完整 CTest 与真实开发设备 GPU 场景。Python3.9+ 默认仅标准库；官方源依当前锁与已有缓存复用。Windows 安装 CMake3.20+ 与 Visual Studio C++工具链后使用同一入口。Metal 只用于本机开发验证。

运行 `python3 tools/play.py`；本开发设备也可双击 `Play_Sonnheide.command`。完整便携包位于 `.build/package/Sonnheide`，可直接启动其中的程序；Windows CI会保留exe和完整资源。默认独立用户存档目录，可用 `--saves` 指定测试目录。所有页面遵守[前端美术规范](docs/design/FRONTEND_ART_DIRECTION.md)：实际像素字体、48格原创实物像素图标、搭接石牌和9原创深色材质。Age改变世界环境，不锁UI配色。隐藏、降低动态与世界暂停冻结表现时间，恢复不跳时。

新profile2用2m管理格与64²独立微表面（31.25mm），均匀区RLE、稀疏岸线packed微片。五档512m至8.192km，默认2.048km；长宽独立；新建只提供平土或全海，移除世界地图和地球选区。soil/sand与水面同高0m，湿干按真实kind/微覆盖，海床深度独立。材质64px/m。真实预览、支承、拾取、填海微片和存档同源；填海只补水，保已有陆kind/theme/revision。历史profile1的250/2000mm保持原解释，不自动迁移不同包hash。

右键拖拽平移，中键拖拽360°旋转，滚轮缩放，WASD/QE移动旋转，Space暂停；文本焦点和模态窗口隔离镜头输入。平土/全海创建先完成首存档读回再发布。生产启动与便携包不读取GSHHG；历史源和几何回归保留在测试资源。后续分区明确禁用，不提供假成功按钮。
