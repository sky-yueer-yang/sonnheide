# v0.8设计接续与旧稿退役

2026-10-09。用户先要求清空实现、保留旧稿，待新版完成后删除旧稿；本轮新版总设计/专门规则/机器合同/248项处置已经形成，因此删除39个旧作者文档/验收文件及1份旧UI schema。原始v0.6绝不删除或改写。旧40份完整内容仍在已发布Git历史，禁止重写/强推。

旧施工计划/验证两文件被新完整计划替换，同样可在历史取回。旧data/catalogs/requirements、许可/来源锁/资源manifests作为源材料保存，未作为当前运行数据。

历史基线：[b13f01890053bd58609af3366883c93ebef18e22](https://github.com/sky-yueer-yang/sonnheide/tree/b13f01890053bd58609af3366883c93ebef18e22)。原代码清空时49份保留文档的hash记录是当时的事实，不表示本轮接续后那些文件仍在工作树。

## 原始27章的有效规则接续

|原章|有效内容/覆盖|新落点|
|---|---|---|
|01|范围与冲突裁决|[D01](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D01)、[D02](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D02)|
|02|同Actor/空间与创建|[D03](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D03)、[D04](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D04)、[D07](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D07)、[D08](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D08)|
|03|文化场/语言/命名|[D11](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D11)|
|04|国家/八轴/继任/疆界|[D13](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D13)|
|05|建址/城市/墙门/物流|[D14](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D14)|
|06|公共经济/生存需求|[D15](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D15)、[D16](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D16)|
|07|企业/工厂/退出/停用马系|[D16](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D16)、[D17](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D17)|
|08|真实合同/税/资本/货物|[D17](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D17)|
|09|矿物实际源与库存|[D18](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D18)|
|10|科研/知识传播采用|[D18](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D18)|
|11|宗教根/先知/不可变章程|[D12](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D12)|
|12|教会经济/国家/教派|[D12](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D12)、[D17](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D17)|
|13|实际信仰传播与独立宗教建筑|[D12](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D12)、[D21](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D21)|
|14|已有Actor真实移民|[D19](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D19)|
|15|单层Army/七地面装备/停用骑乘|[D19](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D19)|
|16|空间战斗/前现代墙门|[D19](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D19)|
|17|八栏军事交互/元控制|[D19](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D19)、[D20](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D20)、[D21](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D21)|
|18|War/Plot/盟约/殖民/叛乱|[D19](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D19)|
|19|原13模板/23色/theme国旗与政治图|[D13](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D13)、[D21](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D21)|
|20|替换写实/无服装为真像素/实物衣物/三语|[D15](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D15)、[D21](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D21)、[D24](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D24)|
|21|重起自研框架/权威模块|[D02](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D02)、[D26](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D26)|
|22|新的完整施工验收与未实现边界|[D27](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D27)|
|23|贯穿历史按真实新规则运行|[D27](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D27)|
|24|typed编辑/单写者/自由地形覆盖冻结|[D02](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D02)、[D04](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D04)、[D21](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D21)|
|25|保原科技目录来源但停用16项马依赖|[D18](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D18)|
|26|法律/业务源继承与新域分开|[D13](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D13)、[D16](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D16)、[D17](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D17)、[D18](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D18)|
|27|新Actor/terrain/gene/政治事务与强异常|[D02](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D02)、[D26](../design/Sonnheide_Design_v0.8_Living_Pixel_World.md#D26)|

全部未覆盖的原经济/命名/科技/国旗具体目录从逐字原稿继承，冲突处以v0.8明确替代，不能因旧稿被移出当前规范入口遗失领域。三份新专门合同明确基因/启智/坡道与冷热/政治继任/70页/28规则；surface合同保全部木果实物细节。

## 已退役文件

|旧文件|历史完整副本|
|---|---|
|data/interaction_schema.json|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/data/interaction_schema.json)|
|docs/architecture/APPLICATION_FLOW.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/architecture/APPLICATION_FLOW.md)|
|docs/architecture/CLIENT_STRUCTURE.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/architecture/CLIENT_STRUCTURE.md)|
|docs/architecture/CLOTHING_ECONOMY.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/architecture/CLOTHING_ECONOMY.md)|
|docs/architecture/CONTENT_PIPELINE.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/architecture/CONTENT_PIPELINE.md)|
|docs/architecture/CONTRACTS.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/architecture/CONTRACTS.md)|
|docs/architecture/DOMAIN_MAP.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/architecture/DOMAIN_MAP.md)|
|docs/architecture/EDITING.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/architecture/EDITING.md)|
|docs/architecture/ENGINEERING.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/architecture/ENGINEERING.md)|
|docs/architecture/INTERACTION.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/architecture/INTERACTION.md)|
|docs/architecture/LIGHT_AGES.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/architecture/LIGHT_AGES.md)|
|docs/architecture/OVERVIEW.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/architecture/OVERVIEW.md)|
|docs/architecture/PERSISTENCE.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/architecture/PERSISTENCE.md)|
|docs/architecture/PIXEL_RENDERING_AND_ASSETS.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/architecture/PIXEL_RENDERING_AND_ASSETS.md)|
|docs/architecture/PIXEL_TERRAIN_TRANSACTIONS.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/architecture/PIXEL_TERRAIN_TRANSACTIONS.md)|
|docs/architecture/RENDERING.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/architecture/RENDERING.md)|
|docs/architecture/SIMULATION.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/architecture/SIMULATION.md)|
|docs/architecture/SURFACE_ECOLOGY_AND_TREES.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/architecture/SURFACE_ECOLOGY_AND_TREES.md)|
|docs/architecture/TERRAIN_AND_SITES.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/architecture/TERRAIN_AND_SITES.md)|
|docs/architecture/UI_ARCHITECTURE.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/architecture/UI_ARCHITECTURE.md)|
|docs/architecture/VISUAL_STYLE.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/architecture/VISUAL_STYLE.md)|
|docs/architecture/WORLD.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/architecture/WORLD.md)|
|docs/decisions/0001-foundation.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/decisions/0001-foundation.md)|
|docs/decisions/0002-building-construction.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/decisions/0002-building-construction.md)|
|docs/decisions/0003-terrain-and-site-access.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/decisions/0003-terrain-and-site-access.md)|
|docs/decisions/0004-clothing-and-makehuman.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/decisions/0004-clothing-and-makehuman.md)|
|docs/decisions/0005-unified-inspectors-and-world-tools.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/decisions/0005-unified-inspectors-and-world-tools.md)|
|docs/decisions/0006-bottom-toolbar-trilingual-editing.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/decisions/0006-bottom-toolbar-trilingual-editing.md)|
|docs/decisions/0007-foundation-experience-first.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/decisions/0007-foundation-experience-first.md)|
|docs/decisions/0008-sacred-interface-and-light-ages.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/decisions/0008-sacred-interface-and-light-ages.md)|
|docs/decisions/0009-monumental-minimal-interface.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/decisions/0009-monumental-minimal-interface.md)|
|docs/decisions/0010-flat-land-and-coastal-transition.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/decisions/0010-flat-land-and-coastal-transition.md)|
|docs/decisions/0011-ground-sky-and-bounded-world-preview.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/decisions/0011-ground-sky-and-bounded-world-preview.md)|
|docs/decisions/0012-editable-3d-pixel-world.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/decisions/0012-editable-3d-pixel-world.md)|
|docs/decisions/0013-layered-surfaces-and-fruiting-trees.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/decisions/0013-layered-surfaces-and-fruiting-trees.md)|
|docs/design/Sonnheide_Design_v0.7_Pixel_World.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/design/Sonnheide_Design_v0.7_Pixel_World.md)|
|docs/planning/PHASE1_ACCEPTANCE.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/planning/PHASE1_ACCEPTANCE.md)|
|docs/planning/PHASE1_EVIDENCE.json|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/planning/PHASE1_EVIDENCE.json)|
|docs/planning/RISKS.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/planning/RISKS.md)|
|docs/planning/ROADMAP.md|[Git历史](https://github.com/sky-yueer-yang/sonnheide/blob/b13f01890053bd58609af3366883c93ebef18e22/docs/planning/ROADMAP.md)|
