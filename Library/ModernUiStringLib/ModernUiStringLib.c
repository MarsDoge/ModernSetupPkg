/** @file
  Localized string table for ModernSetupPkg.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/PcdLib.h>

#include <ModernUi/ModernUiString.h>

STATIC CONST CHAR16  *mEnglishStrings[ModernUiStringMax] = {
  L"MODERN UEFI BIOS UTILITY",
  L"ADVANCED MODE",
  L"Dashboard",
  L"Platform overview",
  L"Boot",
  L"Boot order and entries",
  L"Devices",
  L"Firmware-visible handles",
  L"Security",
  L"Secure Boot state",
  L"HII",
  L"DriverSample VFR bridge",
  L"Exit",
  L"Leave setup or reset",
  L"Left/Right: tab    Down/Enter: page    Esc: continue boot",
  L"Up/Down: select    Left/Esc: tabs    Enter: action    Tab: switch focus",
  L"Firmware Vendor",
  L"Firmware Revision",
  L"Display",
  L"Boot Options",
  L"%u entries",
  L"Prototype Status",
  L"GOP renderer online. Keyboard navigation is active.",
  L"Enter launches the selected boot option. Boot order editing is not implemented yet.",
  L"No visible boot options found.",
  L"(no description)",
  L"Active",
  L"Inactive",
  L"Unable to enumerate handles.",
  L"%u handles visible to DXE",
  L"Secure Boot",
  L"Enabled",
  L"Disabled",
  L"Key management is intentionally read-only in v1.",
  L"Use Up/Down to select an action, Enter to run it.",
  L"Continue boot",
  L"Launch classic UiApp fallback",
  L"Reset system",
  L"No DriverSample HII formsets found.",
  L"DriverSample formsets",
  L"Forms",
  L"Questions",
  L"Unsupported in HII bridge v1",
  L"Read-only",
  L"Enter opens the selected form or advances a supported value.",
  L"RouteConfig returned: %r",
  L"Boot option returned: %r",
  L"Classic UiApp returned: %r",
  L"ModernSetupApp: graphics initialization failed: %r\n"
};

STATIC CONST CHAR16  *mSimplifiedChineseStrings[ModernUiStringMax] = {
  L"现代UEFI设置工具",
  L"高级模式",
  L"仪表盘",
  L"平台概览",
  L"启动",
  L"启动顺序与启动项",
  L"设备",
  L"固件可见句柄",
  L"安全",
  L"安全启动状态",
  L"高级设置",
  L"DriverSample VFR桥接",
  L"退出",
  L"离开设置或重启",
  L"左/右：切换页面    下/回车：进入内容    Esc：继续启动",
  L"上/下：选择    左/Esc：返回页面    回车：执行    Tab：切换焦点",
  L"固件厂商",
  L"固件版本",
  L"显示",
  L"启动项",
  L"%u 项",
  L"原型状态",
  L"GOP渲染已就绪，键盘导航已启用。",
  L"回车启动选中的启动项。启动顺序编辑尚未实现。",
  L"未找到可见启动项。",
  L"(无描述)",
  L"启用",
  L"未启用",
  L"无法枚举句柄。",
  L"DXE可见%u个句柄",
  L"安全启动",
  L"已启用",
  L"已禁用",
  L"v1中密钥管理暂为只读。",
  L"使用上/下选择操作，回车执行。",
  L"继续启动",
  L"启动传统UiApp备用界面",
  L"重启系统",
  L"未找到DriverSample HII页面。",
  L"DriverSample页面集",
  L"表单",
  L"问题项",
  L"HII桥接v1暂不支持",
  L"只读",
  L"回车打开选中表单，或切换支持的设置值。",
  L"RouteConfig返回：%r",
  L"启动项返回：%r",
  L"传统UiApp返回：%r",
  L"ModernSetupApp：图形初始化失败：%r\n"
};

/**
  Return TRUE when the active language should use Simplified Chinese strings.

  @retval TRUE   PcdModernSetupDefaultLanguage starts with "zh".
  @retval FALSE  English strings should be used.
**/
STATIC
BOOLEAN
UseSimplifiedChinese (
  VOID
  )
{
  CONST CHAR8  *Language;

  Language = ModernUiGetLanguage ();
  return (BOOLEAN)((Language[0] == 'z') && (Language[1] == 'h'));
}

/**
  Return the active language tag.

  The returned pointer is owned by the platform PCD database and must not be
  freed or modified by the caller.

  @return Non-NULL ASCII language tag. The default is "zh-Hans".
**/
CONST CHAR8 *
EFIAPI
ModernUiGetLanguage (
  VOID
  )
{
  CONST CHAR8  *Language;

  Language = (CONST CHAR8 *)FixedPcdGetPtr (PcdModernSetupDefaultLanguage);
  if ((Language == NULL) || (Language[0] == '\0')) {
    return "zh-Hans";
  }

  return Language;
}

/**
  Return one localized string for the active language.

  If the active language is unknown or the localized string is absent, English
  text is returned as the fallback.

  @param[in] Id  String identifier to resolve.

  @return Non-NULL UCS-2 string owned by this library.
**/
CONST CHAR16 *
EFIAPI
ModernUiGetString (
  IN MODERN_UI_STRING_ID  Id
  )
{
  if (Id >= ModernUiStringMax) {
    return L"";
  }

  if (UseSimplifiedChinese () && (mSimplifiedChineseStrings[Id] != NULL)) {
    return mSimplifiedChineseStrings[Id];
  }

  if (mEnglishStrings[Id] != NULL) {
    return mEnglishStrings[Id];
  }

  return L"";
}
