# DisplayEngine 动态数据流约束

本文档定义 ModernSetupPkg 后续动态平台数据、Setup 配置和刷新 UX 的产品方向。

## 目标

ModernSetupApp 和 Modern DisplayEngine 需要能显示当前平台信息、反映配置变更，并更新时间或类似 sensor 的动态数据；但 renderer 不能变成 policy owner 或 hardware owner。

## 目标链路

```text
PEI 平台发现 / 默认策略
  -> HOB / PCD / Variable / protocol handoff
    -> DXE platform services
      -> ModernSetupApp / DisplayEngine view model
        -> Modern UI display + dynamic refresh
        -> app 通过平台拥有的 PCD / Variable / protocol 路径写入配置
          -> 安全时立即生效，或标记 reboot-required
            -> 下一次 PEI 消费非默认配置并重新发布最新状态
```

## 职责边界

### PEI / platform discovery

- 收集早期平台信息。
- 读取默认和非默认配置输入。
- 通过平台拥有的机制发布 handoff 数据。
- 下一次启动时消费 reboot-persistent 配置。

### DXE platform services

- 为 app 归一化平台数据。
- 拥有 policy、validation、persistence 和 reset requirement。
- 向 app/display 层暴露当前数据和更新通知。

### ModernSetupApp

- 展示产品级 Setup 页面。
- 通过批准的平台服务或现有 setup 机制发起配置变更。
- 显示变更是 live、unsaved，还是 require reboot。

### Modern DisplayEngine

- 渲染 FormBrowser-owned form state 和 ModernSetup page state。
- 显示 live/refresh/unsaved/reboot-required/status affordances。
- 支持 redraw-friendly 的动态字段，例如时间。
- 不拥有硬件探测、policy decision 或 storage writes。

## DisplayEngine 约束

DisplayEngine/UI code 允许：

- 消费已经 materialized 的 FormBrowser display data。
- 消费未来 app/platform view-model state。
- 渲染 row kind/state、status chips、refresh indicators 和 dynamic values。
- 在 FormBrowser refresh event 或 app-driven redraw 时重绘。

DisplayEngine/UI renderer code 禁止：

- 直接硬件探测。
- 独立 IFR parsing。
- ConfigAccess 语义。
- 直接拥有 `SetVariable`、`RouteConfig`、`ExtractConfig` 或 `HiiSetBrowserData`。
- 没有 platform/FormBrowser source 时，把普通 unsaved changes 当成 reboot-required。

## UX 状态

当前私有 DisplayEngine status slots：

```text
LIVE VIEW        默认 live page surface
LIVE REFRESH     FormBrowser/page 有 refresh event 或等价 update source
UNSAVED CHANGES  存在 changed state，但尚未提交
REBOOT REQUIRED  未来 platform/FormBrowser source 明确要求重启
MODAL VIEW       modal FormBrowser state
```

`REBOOT REQUIRED` 现在只是预留 UX state，直到真实来源接入前不能伪造。UI 不能从 generic changed state 推断它。

## 验证预期

日常 UX iteration 保持 Modern DisplayEngine focused：

```bash
python3 Tests/Smoke/smoke_validate.py
git diff --check
TARGET=RELEASE MODERN_SETUP_DISPLAY_ENGINE=modern MODERN_SETUP_REPLACE_UIAPP=1 Scripts/build-ovmf-x64.sh
```

native-vs-modern capture 只在 milestone 或 PR review baseline 使用，不要每轮 iteration 都跑。
