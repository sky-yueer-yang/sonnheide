# 阶段1：真实地理、共用呈现与可靠空世界创建

2026-10-08。实现与测试边界以本记录为准；原始设计基线逐字保留。阶段2的完整游戏工具栏、权威明暗自动计时和完整保存选择器另行施工。

## 实际交付链

`GSHHG 原版 ZIP → 五级来源独立 SHA 校验 → 全球导航 → full 冻结候选 → 同路径 GPU 预览 → 候选首 checkpoint/回读 → durable continue 指针 → 单写者发布 → 返回/继续同 WorldId`

- 地理原件为 GSHHG 2.3.7，完整 ZIP 118,617,033 字节，SHA-256 `28600e8f7a08645aab43079326df6504212ec5ccb2b4bcf3b5f4f12ed60e82bc`。原件、LGPL/GPL通知、五成员哈希和复建脚本都有锁；归档在本项目 [GitHub Release geography-sources-v1](https://github.com/sky-yueer-yang/sonnheide/releases/tag/geography-sources-v1)，5个原件/通知全部上传后下载逐SHA复核通过。官方GMT镜像恢复同一原ZIP，暂时的网络失败有界重试，哈希不符立即拒绝。最高地表始终读取 full，导航简化不改变 World。源岸线较旧，full 不代表现实米级准确度。
- 默认首区域为意大利 Amalfi，中心经纬度 14.382/40.626，游戏水平尺寸 4096×4096 m，worldScale 0.04，对应约102.4×102.4 km真实范围。AEQD两轴统一米尺度，支持日期线和极区；每边真实跨度最大4000 km及几何/格数预算，超出拒绝。
- 普通陆地固定2 m，固定窄水边坡带24 m；没有真实高程。128 m选区内侧带生成冻结不规则闭合，核心保真实海陆；外围512 m纯海缓冲，最外64 m保护带。所有距离均为游戏米。域外海只有表现，不产生实体或航路。
- 水域身份来自full连续边界，而非粗格中心flood；被闭合边截开的湖与外海相通，完整内部湖保持独立。逻辑格只是摘要，不决定最高地表精度。
- 8套实际2K Poly Haven材质，保留sRGB base color、线性GL normal及AO/roughness/metallic通道；所有通道使用同一Hex变换/梯度/权重，旋转后的法线还原到统一切线基。PNG由固定bimg逐字节解码，避免系统色彩管理改变数据通道。
- 两套8K Pure Sky实际全像素解码，替换拍摄下半球、固定明暗曝光；SH9漫反射和128样本Hammersley/GGX粗糙度预滤波、PDF/solid-angle源mip及split-sum BRDF。天空和反射使用同一处理后环境。没有天文或每日昼夜计算。
- GPU预览和游戏共享同一EarthRenderer、full sampler、物理材质尺度/seed及岸坡。完整有限域与细相机网格构成连续网格，浮动原点只改绘制坐标。海平面ray pass延伸到地平线，当前是**静态mean-water**；固定Abyssal原生FFT/泡沫/折射不在本阶段冒充完成。
- 原生创建页英汉德、底部无边框工具、来源窗口、命名、平移/指针缩放/框选/精细预览/取消/创建。右键拖动精细场景旋转，滚轮缩放；输入框/按钮优先接收输入。地图为宏观导航，最高可见精度通过精细预览查看，使用真实材质而非卫星图。
- 新World初始人口/建筑/国家/任务均0，tick0、Light/manual。Darkness预览是呈现检查，创建初始固定Light；正式最高Light预览与游戏采用相同材质/seed/画质，不提供会在创建时静默重置的材质选择。八套材质由独立实际GPU探针逐项验收，生态分布与多材质区域仍按后续玩法设计。
- 源/recipe/所有实际GPU资源就绪才允许准备。后台首写/回读，所有者poll最后落盘continue并发布；WorldId稳定、恢复SessionGeneration更新。旧世界直到新候选完成才替换；取消、旧请求、坏档和源/预算拒绝不会静默发布。

## 可复建入口

```sh
python3 tools/prepare_geography.py --fetch
python3 tools/build.py
python3 tools/build.py --sanitizers
python3 tools/build_native.py --run
```

`build_native.py`先恢复固定源码/中文字体，再执行`prepare_phase1.py`恢复项目Release原件、烘焙天空和编译shader，最后构建。全部缓存与程序在忽略的`.build`，存档默认用户目录。已准备所有原件后可`--offline`；暖缓存逐输出校验，不反复烘焙8K。损坏派生文件明确拒绝，检查后用`prepare_phase1.py --offline --rebuild`重新生成。

实际原生场景验收入口（macOS开发设备）：

```sh
.build/native/Sonnheide.app/Contents/MacOS/Sonnheide --phase1-smoke --saves .build/phase1-smoke-saves
```

它走正式主菜单/创建器/renderer路径，验证精细预览、两套环境、PBR、可靠创建、返回/继续、取消另一候选后旧Continue、三语与稳定WorldId；不是NullRenderer。Windows对应程序为`.build/native/RelWithDebInfo/Sonnheide.exe`，需实际桌面/GPU运行后才记为Windows图形证据。

## 验收证据

- 已通过：16组真实源地理场景；18组真实创建/持久化场景；指针锚定导航。包含真实湖/小岛/极区/日期线、full字节位翻转拒绝、连续水域与粗格独立、13个落盘故障边界、最后指针重命名取消回滚、跨线程拒绝、迟到/取消及UTF-8命名。normal及ASan/UBSan均实际运行。
- 已通过：实际Metal独立渲染探针加载全部原材质/天空，近景材质与天空/海面截图；损坏天空文件在GPU上传前被SHA拒绝。
- 已通过：最终配方 `d01b731ff0f6358c53037fe0793719bba6717a4510b8e39dda02db459c03c941` 的实际Metal整机窗口贯穿，真实创建→返回→继续同WorldId→取消新候选→载入旧世界；八套材质2560×1600实际GPU输出各异，正射海面平行射线修复与shader字节篡改拒绝验证。
- 已通过：真实分配失败扫点644个冻结几何、22个所有者start、13个durable poll；普通与ASan/UBSan均通过。生产及原生地理/预览/载入工作显式持有线程和packaged_task，启动失败可回退，取消/析构join，发布前线程已收尾。测试覆盖当前线程实际分配，后台线程分配不注入；各标准库具体分配数量可以不同。
- 已通过：英汉德原生Rml布局、真实指针/键盘/Escape，1280×800与400×600、DPI 1与2，来源窗口不遮挡底栏；完整native CTest 16/16，项目准入校验及原生offline复建。
- 已通过：[跨平台核心CI](https://github.com/sky-yueer-yang/sonnheide/actions/runs/37742563215)，Ubuntu Debug、Linux ASan/UBSan、Windows Release、macOS Debug分别15/15。原生Direct3D shader统一使用bgfx跨后端向量构造，实际Metal再次复验；Windows原生编译结果单独记录在PR，不能替代显卡实测。
- 验收摘要和实际GPU帧hash见 [PHASE1_EVIDENCE.json](PHASE1_EVIDENCE.json)。本机未设置Node，旧网页验证在CI单独运行；本机不以此声称浏览器检查通过。

当前未做Windows GPU实机/Steam Overlay/发行包；CI编译和NullRenderer不能替代它们。纹理采用未压缩RGBA8/RGBA16F完整mip，上限约1.26 GB纹理显存（8套2K约537 MB，两套8K约716 MB，GGX约2.8 MB）；这是实际格式字节预算，不是此前BC压缩128 MB估计。后续压缩/流送和完整游戏性能另行测量。
