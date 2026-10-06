# 当前实现和验证边界

记录日期2026-10-06；初步框架0.1。本文件的“已验证”只指下面实际执行的代码，不覆盖完整游戏规格。

## 已执行的本机验证

- Apple clang21、arm64 macOS，`-std=c++20 -Wall -Wextra -Wpedantic -Werror`编译。
- 18组内核场景：材料预约/消费；完工前水面；四邻接/禁孤岛；拒绝无半写；幂等/冲突/修订；可信Scheduler劳动入口；取消/永久完成；dirty导航；单格海峡分裂；reclaimed-only港口/泊位/重叠；4旋转；湖海/非法角度；中途checkpoint/回执续跑；损坏/版本/截断；2000命令守恒；伪造孤岛/epoch/坐标边界；伪造权限回执拒绝。
- 对事务全部allocation位置逐一注入bad_alloc：世界不出现半提交，原commandId可重试；当前11个失败位置得到验证。
- headless贯穿演示：2个完成造陆格、1个港口、材料100→80、reserved=0、天然mask hash不变、checkpoint相同。
- 本机`python3 tools/build.py --sanitizers`已通过：headless、全部18场景、11个allocation故障位置、目录与原创几何校验。GitHub三平台CMake构建结果以Checks为准；CI配置本身不是平台成绩。
- 内容校验包括原稿SHA-256、科技424项计数/唯一ID/前置DAG、35法律/13业务、27章追踪、3原创glTF来源/权威hash/尺寸/入口/三角绕序及本地文档链接。

## 独立审查发现并修复

不合法孤岛存档、无支撑未完成工程、极端公开坐标查询整数溢出、epoch/ID/revision耗尽处理、Accepted回执和世界分配失败分离、Observer Accepted伪回执。这些边界均有具体复现，修复没有扩大到无关领域。

## 当前实现与生产差异

|当前|生产仍需|
|---|---|
|合成6×6等小图；安全容量上限100万格|全球真实数据、投影、coast coverage、物理clearance、内存预算|
|一种通用材料；Scheduler直接给5单位劳动|实名CitySquare库存、真实运输/Person劳动/施工合同、部分投入结算|
|PlaceRoad无工程成本；KernelPlacePort几何探针|科技、预算、产权、法域、建设流程和广场完整路网|
|整状态复制、同步全图BFS、map/set|分页写集、SoA、Chunk/门户动态连通、异步三态query|
|kernel文本checkpoint/FNV回归fingerprint|SHA-256块/pack、生产fsync/多代保存、版本迁移、全域L1/L2重放|
|3个原创几何灰盒|所有建筑家族、写实模块/材质、容量/三时代/文化外观、完整LOD|
|C++/Python标准库可执行|SDL3/bgfx/PBR/water/ozz/RmlUi/IME实际集成与GPU实测|
|目录提取数据与27章设计映射|完整中英定义/flags/doctrines/naming与全领域规则实现|

没有声称10000/100000完整人口达到帧率，没有经济平衡、真实战斗、社会传播的实测结论。原稿提到199条旧追踪、独立国旗工具和TS参考未提供，本轮不声称已经运行它们。
