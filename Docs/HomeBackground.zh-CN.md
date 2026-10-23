# 可选 Setup 背景

Language: [English](HomeBackground.md) | 简体中文

1.2.0 提供构建时可选背景，贯穿 App 与 ModernDisplayEngine 绘制的原生 HII 页面。
整个视口铺设不透明图片，保持比例并居中裁切。首页分区/卡片适度半透明
（140/255、150/255，选中 180/255），设置页启用背景的面板采用更厚实的
205/255。文字底板、活动控件、输入/密码、错误与保存确认保持实色；不是每一行都透明。

## 构建

```sh
MODERN_SETUP_HOME_BACKGROUND_PNG=/absolute/path/background.png \
MODERN_SETUP_ASSET_PYTHON=/usr/bin/python3 \
MODERN_SETUP_REPLACE_UIAPP=1 \
MODERN_SETUP_DISPLAY_ENGINE=lvgl \
TARGET=RELEASE Scripts/build-loongarchvirt.sh
```

使用正常 WORKSPACE/交叉工具链配置，只在素材转换解释器安装 Pillow。
整个验收批次固定同一个 SOURCE_DATE_EPOCH；App 将冻结的构建标签与产品版本、
实时钟分别显示，原生表单不显示 App 的构建标签。
省略 MODERN_SETUP_HOME_BACKGROUND_PNG 即恢复原有纯色外观，也不需要 Pillow。
当前构建开关只接入 LoongArchVirt。

## 输入与绘制边界

- 接受 Pillow 可解码的 PNG，不强制原图为 1280×800 或 800×600。本次蓝图为
  1263×705 RGB，原文件不修改。
- 转 RGB（丢弃 alpha，不按 alpha 混合），等比例缩至不超过 1280×800，小图不放大，
  编码为小端 RGB565。每个链接镜像的资源最多 2,048,000 字节及字符串终止符。
- 资源生成在构建 overlay。用户原图、生成资源头和 FD 不进入仓库；只分发获授权素材。
  没有运行时 PNG 解码、文件加载或壁纸选择器。
- 运行时双线性采样、全视口居中 cover 裁切；RGB565 色深有限，放大不能恢复细节。
  超过 4096×2160 的绘制区域拒绝处理；边界/分配/可报告的恢复失败回退纯色。
  现有 LVGL flush 不能报告所有底层 GOP 错误。
- App 与原生 DisplayEngine 独立持有资源与缓存，不跨镜像卸载借用指针。原生返回时
  恢复纯净缓存；App 每次整页绘制建立并释放缓存。面板始终与原始背景混合，
  不在已绘制的文字、面板上重复叠加。
- 仅显式绘制表面启用透明度，不修改全局 FillRect，也不修改 VFR 隐藏/禁用、校验、
  回调、ConfigAccess 或变量写入语义。原生刷新仍由 FormBrowser 管理。
  本 PR 不包含新的控件局部刷新系统，也不包含未接入的 geometry 实验。

## 验证

```sh
python3 Tests/Smoke/home_background_test.py
python3 Tests/Smoke/native_backdrop_frame_test.py
python3 Tests/Smoke/hii_presentation_test.py
python3 Tests/Smoke/product_version_test.py
python3 Tests/Smoke/smoke_validate.py
```

背景测试在 ASan/UBSan 下执行真实 C（模拟 renderer），检查边界、裁切、双线性、
重复/重叠混合、所有权、失败回退、OFF 分支及原生框架控制台/光标状态。
`--legacy-header-clear` 为必须失败的负对照。这不等同于固件视觉或回调事务验收；
发布证据须注明确切 FD、源码哈希、QEMU 实际帧缓冲尺寸与已执行交互。
不宣称实机验收或经过测量的性能提升。
