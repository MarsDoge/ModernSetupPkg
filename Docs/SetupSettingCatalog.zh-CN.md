# 通用 Setup 配置目录

语言：[English](SetupSettingCatalog.md) | 简体中文

## 这一批做什么

现在可从 Advanced → Setting Catalog 浏览 21 类、203 项配置。上下键选择，
Enter 打开分类或说明，Esc 返回。分类、名称、说明、导航和状态提示跟随 App
当前语言显示简体中文或英文；俄语暂时回退英文目录。
所有新目录项仍显示 N/A，没有绑定硬件数值。部分 Boot、Secure Boot 项目的说明页
提供独立的 Open native setup 操作，按 Enter 或点击可复用现有入口打开原生页面，
不代表直接定位到单个设置。缺少对应页面时不跳转其他工具，返回后保留目录位置。

`Config/SetupSettings.json` 是配置项的唯一维护来源。每项都有稳定 ID、中英文
文案、分类、控件/值类型、适用条件、读写意图、默认值来源、owner/绑定、生效方式
和风险信息。`Include/ModernUi/ModernSetupSetting.h` 定义 C 元数据契约；生成器
输出只读元数据，不再手工维护第二份清单。

功能范围承接 `IbvAndPlatformSetupSurvey.zh-CN.md`：AMI Aptio、InsydeH2O、
Phoenix 系列、百敖 ByoCore 作为 IBV 参考，主板、客户端和服务器 OEM 作为工作流
参考。目录代表产品覆盖范围，不表示每个平台都支持每一项。

## 没有后端的功能也保留

未绑定的配置项留在对应分类，显示 `N/A`。用户可以查看说明，但不能编辑、执行
或提交，也不能进入保存前变更清单。未接后端、平台不支持、未报告数据、读取失败
是不同原因；真实的 0 或 Disabled 不是 N/A。

这条规则不覆盖原生安全、权限以及 suppressif/grayoutif/disableif 条件。
保留一个不可用的目录项，不代表可以暴露受保护的问题或绕过驱动的限制。

“设计上可以修改”和“当前可以修改”必须分开。设计为开关的项目，在后端尚未
绑定时仍然不可编辑。只有真实后端明确可用并允许写入，才能启用编辑器；静态元数据
不能自行授予写权限。

## 底层怎么接

- 原生 HII：注册准确的 owner/上下文和 FormSet/Form/Question 标识，不靠显示
  文本猜入口。保留 browser 的校验、回调、默认值处理和真实提交范围。
- 平台服务：通过经过评审的类型化适配器/owner 接入，不让 ModernSetupApp
  直接调用 ConfigAccess 或任意写变量。
- 只读 provider：返回真实值及可用状态，不用演示数字冒充硬件读数。
- App 偏好：只承载 App 自己拥有且实际使用的配置，系统日期时间不属于这一类。
- 未绑定：明确记录缺少连接，不编造 GUID、偏移、枚举选项、硬件限制或默认值。

变量布局必须说明 GUID/名称、存储/schema 版本、字节或位布局、编码、属性、
校验 owner 和真正读取它的底层模块。写入一个没人使用的变量不算功能实现。
HII 不要求全部使用 DynamicHii PCD；varstore、ConfigAccess 和 callback 动作
也可能承载配置。密码、清 TPM、密钥注册和固件更新不能当成普通字节写入，仍保留
原生流程及敏感数据规则。

## 怎么检查

在项目根目录执行：

```sh
python3 Scripts/setup-setting-catalog.py --check
python3 Scripts/setup-setting-catalog.py --output /tmp/ModernSetupSettings.generated.h
python3 Tests/Smoke/setting_catalog_test.py
python3 Tests/Smoke/catalog_localization_test.py
python3 Tests/Smoke/catalog_routing_test.py
python3 Tests/Smoke/smoke_validate.py
```

schema 第 1 版有意只允许未绑定记录：`binding.hii`、`binding.variable` 和
`binding.service` 必须是 null。这还不是可以直接接入平台的绑定 ABI；后续需要
经过评审的 schema 扩展及校验，不能填一个猜测的 GUID 就启用后端。
完整元数据保留在 UTF-8 的 `DescriptorJson` 中，常用显示字段另外直接提供给 C。
固件读取检入的生成头，按 App 当前语言选择中英文显示字段。元数据仍是
UTF-8，通过有边界检查的 UTF-16 解码显示；非法字节替换为 `?`，不写入半个
代理对。一致性测试检查生成头及全部名称/说明译文，并检查内置字形覆盖。
这些 host 测试不能代替 QEMU 中的实际显示验收。

可以从同一份清单导出便于阅读的列表，不再手工维护第二份：

```sh
python3 Scripts/setup-setting-catalog.py --markdown-output Build/Reports/SetupSettingCatalog.zh-CN.md --language zh-CN
```

生成的头文件是后续构建/接入输入，不表示已经连接运行时。host 验证需覆盖重复 ID、
非法元数据、不安全可用状态、错误绑定以及确定性生成，并真正编译生成的 C 数据。
新增界面控件以后还需要 QEMU 截图和键盘/鼠标验收。

## 下一步接入真实后端

沿用 Main/Advanced/Boot/Security/Exit，在下面组织详情分类，不为每个领域增加
顶栏。不同页面引用同一配置项时共用 SettingId。先接真实 Boot/Secure Boot owner，
其他项留 N/A，再接 browser 提供的保存审阅。后续补平台支持不应逐项重写页面，
也不把平台配置逻辑复制进 App。
