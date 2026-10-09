# SONNHEIDE

2026-10-09，按用户要求清空旧自研实现，开始重新整理游戏。**当前仓库只有历史设计、资源来源和研究资料，没有可运行游戏、构建入口或试玩。**

本轮只全面对比WorldBox，不生成新版游戏设计，不决定新增机制。旧设计文档全部保留，待后续新版设计写好后再处理；旧验收仅说明删除前的历史状态。

|资料|入口|
|---|---|
|WorldBox功能对照：已发布/预告、旧设计覆盖与缺口|[完整研究报告](docs/research/WORLDBOX_FULL_COMPARISON_2026-10-09.md)|
|代码清空范围、保留范围与核验|[重置记录](docs/research/IMPLEMENTATION_RESET_2026-10-09.md)、[逐文件清单](docs/research/IMPLEMENTATION_RESET_2026-10-09.json)|
|用户原始完整设计，逐字保留|[v0.6原稿](docs/design/Sonnheide_Complete_Design_v0.6.md)|
|删除前像素设计与三层地表设计，作为旧设计保留|[v0.7](docs/design/Sonnheide_Design_v0.7_Pixel_World.md)、[ADR0013](docs/decisions/0013-layered-surfaces-and-fruiting-trees.md)|
|历史架构、决策、施工与验收|[docs](docs)|
|旧机器合同、目录与来源锁，非当前运行数据|[data](data)|
|原始美术、历史资产配方与许可|[assets](assets)、[第三方通知](THIRD_PARTY_NOTICES.md)|

完整旧代码仍可在[删除前GitHub快照](https://github.com/sky-yueer-yang/sonnheide/tree/b13f01890053bd58609af3366883c93ebef18e22)查阅。旧文档中的代码路径和运行命令对应此快照，不能在当前目录执行。

`third_party/`保留固定版本的第三方原件和许可，仅作为历史来源；不保留自研接入或启用它们作为当前程序。油画、公司标志、字体、开放资产和旧建筑资产均保留，不将原创内容擅自授予开放许可，见[权利策略](OWNERSHIP.md)。本机原始下载归档在忽略的`.source-cache/`，旧构建产物和预览缓存已清除；个人存档和凭证不入仓。

项目继续以用户当前要求为准：自研框架、真正360°三维像素世界、原创楼体与人物、英汉德三语、唯一Steam发行渠道。WorldBox的功能被列入对照表不意味着采纳；炸弹等毁灭工具不在本轮范围。
