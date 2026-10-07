# 真实地形、建筑地基与入口接入合同

本文件落实 [ADR 0003](../decisions/0003-terrain-and-site-access.md)，区分生产空间合同与有限 oracle。原稿无高度/平地条款已经被用户新要求覆盖；天然地表仍不可在游戏内编辑。建筑、水体、高程数据与性能的实际实施状态分别记录，不把数学样例当作完整游戏客户端。

## 1. 需要真正攻克的边界

|非平凡点|容易误判的方案|必须得到的证据|
|地理真实度|有全球 DEM 就有房屋尺度真实地面|来源、datum、原分辨率、投影/缩放、缺口与重采样误差可追溯|
|地基完整支持|只取四角/中心最高点|完整 footprint 的冻结三角面极值；楼板全域高于地表|
|接入可行性|门口贴一块坡道或 ROAD 格|从合法现状接点到 socket 的连续、全宽、全净空有效路径|
|坡折与车身|平均坡度小于上限就可入库|最大局部纵坡、横坡、曲率/底盘、转弯扫掠与入库朝向|
|后续邻楼|等邻楼建好后重新寻路|批准前检查永久/临时占用不破坏旧接入；替代路径先承诺后换路|
|施工因果|让人沿未来坡道把材料送上平台|当前地面配送与完工永久接入分别验收，材料与人物真实退出|

## 2. 冻结地形与地理管线

### 2.1 全球基底、现有样本与后续精度

选择 NOAA [ETOPO 2022](https://www.ncei.noaa.gov/products/etopo-global-relief-model) 15 arc-second 全球基底，具体本轮源为 v1 Ice Surface。当前仓库保留的 [25×25 阿尔卑斯原始像元](../../data/geo/etopo2022_alps_sample.json) 来自 N60E000 源 TIFF，原始 byte ranges 与 [官方许可/基准 metadata](../../data/geo/sources/etopo2022_noaa_metadata.xml) 可离线核验；没有保留整套全球高程包。原字节窗口清单明确 `partial_source=true`、`full_file_sha256=null`，不能把窗口哈希冒充整幅或全球数据哈希。

样本是经纬像元中心，15 arc-second 的南北间距约数百米，经向间距还随纬度变化。重采样到 4m 或更细游戏格只增加计算节点，不增加现实细节。更细区域 DTM 与其他开放 DEM/DSM 是下一阶段来源；逐项核验地表含义、植被/建筑残余、垂直基准和许可后，才能覆盖全球基底。当前文明不继承现实房屋、道路、人口或国界，不能把 DSM 上真实城市屋顶直接当文明初始地面。

源高程采用 WGS84 水平位置、EGM2008 正高米制。新源若是椭球高或其他 geoid，先作有版本、有误差记录的 datum 转换，不把“都是米”当兼容。陆地高程不能通过正负号代替独立 NaturalLandWaterMask；湖泊、低于海平面陆地和岸线误差各有语义。人工陆地完成后增加独立的工程表面标高/边缘，不回写天然 heightfield。

### 2.2 冻结的世界空间

```text
NaturalTerrainDefinition
  sourceManifest, sourceHashes, conversionRecipeHash
  horizontalProjection, horizontalDatum, verticalDatum
  worldScale                         X/Y/Z 共用
  verticalOrigin                     仅坐标平移
  sourceResolution, missingDataPolicy, errorReport
  terrainGrid/triangulationVersion, frozenTerrainHash
  naturalLandWaterHash, coastlineCoverageVersion
```

先按冻结投影把经纬变成局部米，再同一 `worldScale` 缩放投影 X/Z 与正高 Y。`gameY=s×(height−verticalOrigin)`；人物、车辆、建筑米制资产不跟地图缩放。任何 shader 夸张高度或 GPU LOD 都不得成为权威地面。投影形变与选区边界继续受 [WORLD](WORLD.md) 的有效域约束；统一 XYZ 比例不能消除投影本身的形变。

缺失值、NaN/Infinity、越界坐标、破损 tile 与预算超限应拒绝建址/世界创建或返回待数据，不能补成平地。相邻块共享边/halo、三角化方向、边界 tie-break 与误差阈值固定，不能让同一块在相机两侧产生不同的可站立高度。

旧世界继续引用自己的 profile 和冻结包；数据更新只影响新世界，显式迁移另行设计。浮动渲染原点只改变相对 GPU 坐标，不改变高程、入口或存档事实。

### 2.3 表面和实体分层

|层|可修改者|导航用途|
|NaturalTerrain|创建世界的可信导入器，之后只读|未被实体阻挡的天然地面|
|NaturalLandWaterMask/CoastCoverage|冻结独立来源|支持与水陆合法性，不能由高度符号替代|
|ReclaimedSurface|逐格造陆完成事务|已完成工程陆地和明确标高；未完工仍 WATER|
|FoundationSolid|批准的 Building 项目|占据体积；内部/下方禁通行，顶面可作为平台|
|ApproachSurface|批准的专用接入或公共道路工程|坡道、台阶、平台与明确连接器|
|BuildingCollision/Interior|建筑完成、入口容纳事务|阻挡代理、socket 与容纳状态|
|RenderTerrain/Ocean|只读快照的 GPU 管线|仅表现，LOD/波峰不改变权威导航|

这是受限的 2.5D 多表面与显式连接器，不新增自由三维地形编辑器。自然地面仍存在于源事实中，但若被地基实体遮挡，就不得生成穿地基路线。首轮地基与接入不支持可通行的下层；桥梁如需上下分层，仍按其专用合同处理。

## 3. 长方体地基的确定性几何

建筑 footprint `F` 为合法矩形；只允许本轮四向离散旋转。顶面是单一绝对 `H`，底面是单一 `B`：

```text
hMax = max h(x,z), (x,z) in F
hMin = min h(x,z), (x,z) in F
H = roundUpToPlacementPrecision(hMax + floorClearance)
B = roundDownToPlacementPrecision(hMin - embedDepth)
FoundationSolid = F × [B,H]
BuildingWorldY = H + BuildingLocalY
```

`floorClearance`、`embedDepth`、最大外露高度、基础材料/价格算法、平台精度属于固定的定义/profile，不由模型 AABB 临时推导。固定权威地面为分片线性三角网时，在 `F` 裁切各三角形，极值位于裁切顶点。若 `F` 边界与规则高度网格完全对齐，检查所有覆盖网格顶点即可包含完整极值；**只检查矩形四角或中心不成立**。不对齐任意 footprint 的裁切实现属于生产扩展，不能套用对齐网格 shortcut。

长方体可与天然地表相交来表达基础埋入；这不产生抬高、削低地形的事实。只绘制可见外侧/顶部，导航使用明确实体与顶面。原创建模应包含坡地可见基础墙、顶沿与接入构件；资产外观不能扩大占地、吞掉通道或改变楼板。Foundation 的实例几何 hash 独立于原始建筑外观 hash。

基础量与成本不得被房屋固定配方漏掉。生产配方必须明确采用实体基础体积还是经冻结地面积分得到的填充量，并锁定计量算法与埋入假设；首轮 oracle 不实现材料/货币结算。陡坡上基础过高、过大或接入用地超预算均是合法拒绝原因，不以无限石料免除可行性问题。

## 4. 完整 SitePlan 与证书

```text
SitePlan
  planId, actor, expectedRevisions, terrainHash, definitionHash
  footprint, rotation, foundationBounds, absoluteFloorY
  entranceBindings[]                 人/货/车，各自用途和 socket
  permanentApproaches[]              地面接点 → 平台/入口
  deliveryFront, groundDeliverySocket
  deliveryApron, deliveryRoute, exitPoint
  waitingAndTurningAreas[]
  constructionEnvelope, cageParts[]
  rightsClaims[], surfaceReservations[], clearanceReservations[]
  accessCertificates[], estimatedCosts, state

AccessCertificate
  entranceId, movementProfileHash, terrainHash
  sourcePortalId, sourceNetworkId, sourcePortalRevision
  orderedSurfaces/segments, endpointBindings
  sweptClearanceVolume, rightsReferences
  surface/obstacle/permissionRevisions, proofVersion
```

生产 `GroundPortal` 是可信空间系统发布的正宽度区域，不是任意客户端写出的“这里可达”。它有合法 LAND 支持、已验证表面标高/坡度、通行权、进出方向、净空与既有交通网络绑定。本轮 [site.hpp](../../engine/include/sonnheide/site.hpp) 将这一输入命名为 `RoadPortal{id, area, absoluteHeight, connectedNetworkId}`，只核验 portal 在冻结天然平地 LAND 上，假定网络 ID 由可信调用者发布。私家接入终点必须接到入口完整 socket/landing，不能只在地图上碰一个点。沿途必须连续、没有高度缝、没有不可穿越坡折。

公共道路接点绑定一个经验证的 profile 网络。单栋 house 无邻居也可独立批准，因为它自己包含完整接入；它仍必须具备合法外部地面/道路源点和实际配送来源。不能以“未来邻居会修路”作为本栋完成条件。完全孤立且无合法来源时应拒绝/等待，不能自报一个空 network ID 绕过。

certificate 是几何与权利的证据，不预留无限交通容量；现实拥堵、等待、路线封锁继续使用原运输合同。入口容纳不是把人或车删除；世界列表→InteriorContainer 的 ID/货物关系仍沿用 [CONTRACTS](CONTRACTS.md)。

## 5. 行人、车库与道路运动 profile

### 5.1 共用检查

每条路线按完整通行条带验证 surface、最大纵坡/横坡、净宽、净高、边缘安全余量、方向、合法权利、支持、障碍与入口对齐。检查整条条带和扫掠体积，不能只检查端点或每米中心线样本。Nav 表面、碰撞代理和视觉构件需对齐；换外观/LOD 不改变任何阈值。

人行 profile 可以允许有真实踏步的 stairs 及 landing，但要约束踏步/单步高差、宽度与等待点。车辆 profile 不能继承这种许可，需平顺路面和本车型尺寸。所有初始数字是待实车/人物切片验证的游戏参数，不是现实建筑法规认证。

### 5.2 车辆独立要求

- 最大局部纵坡与横坡；平均高度差/长度仅作早期下界。
- 车身宽高、轮距、轴距、离地间距、坡折处底盘/前后悬的保守扫掠。
- 转弯半径与转向上限；弯道处内外轮和车身扫掠，不只检查路线中心。
- 进入车库的末端方向、平缓 landing 与门宽净高；车库模型 socket 必须对位。
- 合法退出/倒车/等待路径；公共道路上的转向权与等待空间不能借邻楼空地。

有车库的建筑必须同时取得行人必需入口与车辆入口证书。两种模式可在许可明确时共享表面，不能因车辆 route 存在就省略与另一侧人门的连接。

### 5.3 本轮直线 C1 oracle

第一轮只支持四向直线、横向无高差的 smoothstep 接入。从合法平地起点高 `h0` 到平台高 `H`，水平长 `L`，令 `u∈[0,1]`：

```text
y(u) = h0 + (H-h0) × (3u² - 2u³)
max |dy/ds| = 1.5 × |H-h0| / L
max |d²y/ds²| <= 6 × |H-h0| / L²
```

端点导数为零，要求可信 GroundPortal 与入口 landing 也符合平接，不可拿它硬接一条斜路并声称 C1。车辆轴距/离地量可用第二导数上界作保守坡折检查；这一有限检查不能代替生产全车身复杂转弯验证。超出 profile 的候选拒绝，不自动升级为立交、蛇形山路或挖山。

地形穿入检测遍历整个接入矩形与所有覆盖地形三角形的交集。在每块三角形上，`rampY - terrainY` 是纵向三次函数减平面，横向只有线性项；最小值可在裁切多边形各边上通过端点和二次导数方程的内部根求得。这样能捕捉中心线以外的隆起，也能捕捉固定间距采样错过的内部极值。非有限运算、区间越界与接触精度冲突应拒绝。

## 6. 单栋有效与邻楼安全的可证明不变量

对每栋已批准楼 `b`、每个必需入口 `e`，记已验证通道 `P(b,e)` 及其保护域 `R(b,e)`。建楼事务维持：

```text
ValidProfilePath(P(b,e), GroundPortal, Entrance)
LegalRights(P(b,e))
Clearance(P(b,e))
NewPermanentOrTemporaryOccupation ∩ R(b,e) = empty
```

地形冻结、旧表面/接点不被建楼事务修改、旧权利仍成立，加上新增占用不相交，意味着旧路径本身仍成立；不需要期待一次新的寻路找到替代路径。这是后建邻楼保证的具体内容。

施工围网、卸货等待和基础侧面都属于新增占用检查，不只检查最终房子 mesh。后建楼改变邻路标高、挤占旧门前 landing、罩住旧坡道或以屋檐吃掉净高都必须被拒绝。路径短暂接触边缘可以在统一精度规则下允许实体边界邻接，但必须有正宽度连接与实际净空，不能依赖零面积擦边。

首轮 oracle 为所有主 footprint、接入、当前地面配送路线和 apron 预约保守二维投影。它不允许上下叠建，也没有跨项目共享许可表，故跨项目保护域相交一律拒绝；同项目的 footprint 包含于施工范围、apron 包含于配送范围是有意的嵌套。未来更精细 3D 预约应保持同一安全不变量，而不是把当前保守拒绝改成无检查放行。

同一个项目的不同永久接入构件也须互检。两个朝北/朝东的宽矩形坡道可以在主体外侧相交，即使它们各自的纵坡、入口与地形检查都通过；交叉区域通常有不同标高，不能直接当共享路口。首轮只允许同一项目的人/车使用完全相同矩形、同一 portal、同一方向且顶面一致的接入，分别验证每种 profile；其他自身接入相交拒绝，不能只检查对已有邻楼的碰撞。

未来街区可以采用先批准公共交通带、再划分有 frontage 的地块。自家专用通道有限预约，共享公共交通带由独立基础设施管理，不复制一条无限禁建带给每栋楼。相邻基础标高可以不同，默认不连平台；需要共享时重新批准接合表面、双方权利和新的证书。

改路事务先构造有效 `Pnew`、取得其权利/占用并检查所有依赖，再原子交换 certificate 和 reservations。旧路释放发生在成功提交后；预览过期、分配失败或几何不成立均保留旧路。改动影响多个建筑时完整校验后批量提交，不能逐栋半改。

这个保证不承诺任意邻楼都能建。有限地块、陡坡、道路和旧房权利可能使某个新方案无解；拒绝/移动/旋转是正确结果，不侵占已建楼来满足新楼。

## 7. 审批、配送、施工与完工

1. 预览读取冻结地形、定义、道路 portal、产权、现有保护域，求完整方案及成本。未齐数据或搜索预算耗尽返回 Pending，不把“尚未找到”写成永久无解。
2. 批准事务重验读集与几何，预约主楼/基础/接入/配送/施工范围与权利，登记证书，创建项目。全部一次提交，失败无部分禁建区。
3. 人物沿**当前存在的地面路线**把真实材料送到网罩外前侧 apron；未来地基顶或永久坡道不得提前承载物流。到货与退出按真实任务和批次结算。
4. 材料齐备、人员退出、相关证书/清场仍有效时，一次投入房屋、地基和专用接入配方并启动自动工期，沿用 ADR 0002。外部道路/桥工程独立，不改填海劳动。
5. 原创罩组件围住同一项目内的主楼、基础和接入，固定完整 bounds 用于剔除；必要的多个有界矩形罩避免包住大片不属于项目的邻地。坡地下沿闭合/遮挡方案须通过前后侧观察验证。
6. 到期完成事务重验并原子启用永久 surface、entrance 与 capacity，撤除临时占用，发布版本/事件。网罩在开放通道位置首帧让行，其余逐层反向拆去；动画不延迟业务功能。
7. 取消沿用已消费材料不全退、真实在途/现场物料恢复规则；释放该项目权利与占用时不得误删邻楼或共享公共设施。存档保存批准几何、证书/profile/哈希/权利、阶段、实际材料和完成事实。

前侧接货必须显式绑定 `deliveryFront`、当前地面上的 delivery socket、apron 和施工罩子的对应前边。apron 在罩外，沿正确前边有正宽度接触，并能以携货人物的 profile 到达 socket、放料及退出。仅证明一个远端 apron 连到道路不成立：材料会留在远处，建造却从那里凭空认作前侧到货。网罩边和前向随主体同一四向变换旋转，不能把任意碰到罩侧的矩形当正面配送。

本轮 API 为 `SiteRequest.deliveryFront`（默认 `Facing::North`）与可选 `deliverySocketAcrossMm`。横向坐标在 North/South 取世界 X、East/West 取世界 Z；省略时取 footprint 对应前边中点。实际 socket 位于 `constructionEnvelope` 对应前边，apron 必须在罩外贴该边，socket 两侧须容纳人物 profile 的半宽且处于 footprint 前投影内。配送路线到 portal 的边接触也须有当前人物 profile 的完整 1000mm 宽度；远端/错侧/窄边触达返回 `DELIVERY_FRONT_NOT_CONTINUOUS` 或对应路线拒绝。

以上流程是生产接口要求。当前 `SiteRegistry` 只做 preview/reserve，没有 phase transitions 或 release API；deliveryRoute/apron、constructionEnvelope 与永久接入都作为保守保护域持续保留。它尚不在完成时自动释放临时配送/罩子范围，也不运行材料/自动工期/入口开启。后续接入阶段状态与原子释放时必须保留旧永久路径，并重新验证释放范围没有第三方依赖。释放以有角色/阶段的 claim 为单位：同一矩形可能同时承担临时配送和永久坡道，删除配送 claim 不得清空该处仍存在的永久保护。

## 8. 必须击穿的反例与验收

|场景|期望|
|footprint 四角是 0m，中间网格顶点是 6m|平台高于中心，不能靠四角建入山中|
|坡道中心线清空，侧缘地形凸起|完整条带检测拒绝|
|12×8m 地块从 0m 升到 6m、低侧只有短院子|平台可算，但接车库方案拒绝/转向高侧|
|6m 高差、最大坡度 10%|线性下界需 60m；本轮 smoothstep 最大坡约束需至少 90m，另验坡折和接点|
|平均坡度合法，局部最陡段或轴距坡折不合法|车辆 profile 拒绝|
|门口与路径仅相碰一点|无正宽度 portal，拒绝|
|车可入另一侧车库，但人门无路|缺人行入口证书，拒绝|
|送货路线依赖未建平台|当前地面配送验证拒绝|
|apron 虽连合法道路却在远离房子的另一处|缺与前侧 socket/网罩边的正宽度触达，拒绝|
|apron 碰到网罩后侧或侧面但项目 front 朝另一面|前向不匹配，拒绝|
|新楼本体不碰邻楼，但地基/罩子盖住旧坡道|批准前拒绝，旧证书/预约不变|
|邻楼平台标高不同且未批准接合|不自动连接、人物不得跳缝|
|同栋两个不同方向缓坡各自合法但在主体外相交|未批准共享表面则拒绝，不能生成互穿接入|
|旋转 0/90/180/270|主体、入口、接入、配送与罩子使用同一变换|
|预览后邻楼抢先批准、权限变化或 portal 修改|RequiresRepreview，不部分投入/预约|
|替代路线失败或内存分配失败|旧路仍可用，所有旧事实保留|
|更新高程包、换外观、LOD/浮动原点变化|已建 H/入口/保护域/世界高程 hash 不变|
|WATER 之上用高地基假冒陆地|拒绝；Port 仍先合法逐格造陆|
|原始 ETOPO 窗口重导入、哈希损坏、NoData|可重建一致；损坏/缺失明确拒绝|

生产验收追加曲线路口整车扫掠、stairs profile、shared-rights、桥下净空、密集坡地城市的导航与预约预算，以及真实区域误差报告。随机候选测试应与细分几何参考互检内部极值，不能只构造永远通过的平坡。

## 9. 本轮实现边界

当前高程导入只证明真实源窗口可离线解码，未生成全球游戏地形。当前水体归档/CPU FFT 参考未完成 native GPU 材质、海岸遮罩或实机帧率验证。有限建址 oracle 为 [site.hpp](../../engine/include/sonnheide/site.hpp)、[site.cpp](../../engine/src/site.cpp) 与 [site_tests.cpp](../../tests/site_tests.cpp)，CMake 测试名 `site_geometry`。它使用不可变 `TerrainGrid`（整数毫米顶点、每格 NW–SE 两三角形）与网格对齐矩形，仅接受可信平地 `RoadPortal`/已有合法网络 ID，四向直线 smoothstep 坡道、有限 profile 与保守二维预约。`SiteRegistry` 批准时检查 expectedRevision 和 fixture actor 的地权，再一次登记所有永久/临时保护域；这套 fixture 身份边界不是完整法域/产权系统。

oracle 的 `terrainFingerprint` 是 FNV64 回归指纹，用于确认测试中的只读地表没有变化，不是数据来源鉴真或抗碰撞承诺。原始高程、水体源文件与生产内容包仍使用独立 SHA-256 manifest。

oracle **不证明**外部 network ID 的全城/全球交通、人物与车辆实际控制器、私有产权法域、完整材料经济、真实 GPU 工地或存档兼容已经实现。它当前以天然 LAND fixtures 校验普通建址，尚未将 ReclaimedSurface/Port 与已有填海内核合成同一个生产 world，也未实现曲线转弯、台阶、跨项目共享通道、复杂道路施工和替代路径事务；上述生产合同是下一步接口与验收标准。性能测量需加入真实坡地、密集邻楼与争用，不用平地空图或单纯位置更新代替。
