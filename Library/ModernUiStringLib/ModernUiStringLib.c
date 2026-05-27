/** @file
  Localized string table for ModernSetupPkg.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#include <ModernUi/ModernUiString.h>

#define MODERN_SETUP_LANGUAGE_VARIABLE  L"ModernSetupLanguage"
#define MODERN_SETUP_LANGUAGE_MAX_SIZE  16

STATIC CHAR8    mActiveLanguage[MODERN_SETUP_LANGUAGE_MAX_SIZE];
STATIC BOOLEAN  mLanguageInitialized;

STATIC CONST CHAR16  *mEnglishStrings[ModernUiStringMax] = {
  L"MODERN UEFI BIOS UTILITY",
  L"ADVANCED MODE",
  L"Setup Categories",
  L"Choose a setup category",
  L"Boot",
  L"Read-only Boot#### summaries",
  L"Devices",
  L"Firmware-visible handles",
  L"Security",
  L"Secure Boot state",
  L"Firmware",
  L"Update and recovery state",
  L"Status",
  L"Provider, table, and bring-up state",
  L"Management",
  L"Remote and server management",
  L"Power / Thermal",
  L"Power and thermal providers",
  L"Performance",
  L"CPU and memory tuning entries",
  L"Assets",
  L"Read-only platform inventory",
  L"Preferences",
  L"ModernSetupApp-owned UX preferences",
  L"HII",
  L"DriverSample VFR bridge",
  L"Exit",
  L"Leave setup or reset",
  L"Left/Right: tab    Down/Enter: page    Esc: continue boot",
  L"Up/Down: select    Left/Esc: tabs    Enter: action    Tab: switch focus",
  L"Firmware Vendor",
  L"Firmware Revision",
  L"Form Factor",
  L"Boot Mode",
  L"Display",
  L"Boot Options",
  L"Category",
  L"Device Path",
  L"%u entries",
  L"Prototype Status",
  L"GOP renderer online. Keyboard navigation is active.",
  L"Select a Boot#### entry and press Enter to launch it. Native Boot Manager remains available on Exit.",
  L"No visible boot options found.",
  L"(no description)",
  L"Active",
  L"Inactive",
  L"Unable to enumerate handles.",
  L"%u handles visible to DXE",
  L"Secure Boot",
  L"Enabled",
  L"Disabled",
  L"Available",
  L"N/A",
  L"Present",
  L"Absent",
  L"Unknown",
  L"Firmware Update",
  L"Capsule Runtime",
  L"Capsule Protocol",
  L"Capsule Report",
  L"Diagnostics / Logs",
  L"ACPI Tables",
  L"SMBIOS Tables",
  L"Memory Map",
  L"DXE Handles",
  L"Configuration Tables",
  L"Management",
  L"IPMI",
  L"Redfish",
  L"Management Interface",
  L"Power / Thermal",
  L"ACPI Tables",
  L"ACPI SDT Protocol",
  L"Chassis Thermal State",
  L"Power Supply",
  L"Performance / Tuning",
  L"Processor Inventory",
  L"Memory Inventory",
  L"CPU I/O Protocol",
  L"Virtualization Policy Entry",
  L"RAS Policy Entry",
  L"TCG2",
  L"TrEE",
  L"Key management is intentionally read-only in v1.",
  L"Boot actions, system actions, and language selection.",
  L"Continue boot",
  L"Open native boot tools",
  L"Reset system...",
  L"Language: %s",
  L"Language",
  L"Chinese",
  L"English",
  L"Language changed: %s",
  L"No DriverSample HII formsets found.",
  L"DriverSample formsets",
  L"Forms",
  L"Questions",
  L"Unsupported in HII bridge v1",
  L"Read-only",
  L"Enter opens the selected form or advances a supported value.",
  L"RouteConfig returned: %r",
  L"Boot option returned: %r",
  L"Native Boot Manager returned: %r",
  L"ModernSetupApp: graphics initialization failed: %r\n",
  L"Boot & Devices",
  L"Platform Health",
  L"Power & Performance",
  L"Firmware",
  L"Diagnostics",
  L"Management",
  L"Power",
  L"Performance",
  L"Setup Categories",
  L"Open / Enter",
  L"Use Up/Down to select, Enter to toggle app-owned preferences.",
  L"Theme",
  L"System",
  L"Dark",
  L"Red",
  L"Dashboard density",
  L"Comfortable",
  L"Compact",
  L"Remember last page",
  L"Show advanced hints",
  L"Confirm reset",
  L"Save preferences",
  L"Load defaults",
  L"Preferences saved: %r",
  L"Preference defaults loaded; save to persist."
};

STATIC CONST CHAR16  *mSimplifiedChineseStrings[ModernUiStringMax] = {
  L"现代UEFI设置工具",
  L"高级模式",
  L"设置分类",
  L"选择设置分类",
  L"启动",
  L"启动项",
  L"设备",
  L"固件可见句柄",
  L"安全",
  L"安全启动状态",
  L"固件",
  L"固件状态",
  L"状态",
  L"Provider与平台状态",
  L"管理",
  L"管理状态",
  L"电源",
  L"电源与温度状态",
  L"性能",
  L"性能与PCIe状态",
  L"资产",
  L"只读平台资产摘要",
  L"偏好设置",
  L"ModernSetupApp自有界面偏好",
  L"高级设置",
  L"DriverSample VFR桥接",
  L"退出",
  L"离开设置或重启",
  L"左/右：切换页面    下/回车：进入内容    Esc：继续启动",
  L"上/下：选择    左/Esc：返回页面    回车：执行    Tab：切换焦点",
  L"固件厂商",
  L"固件版本",
  L"外形规格",
  L"启动模式",
  L"显示",
  L"启动项",
  L"类别",
  L"设备路径",
  L"%u 项",
  L"原型状态",
  L"GOP渲染已就绪，键盘导航已启用。",
  L"选择Boot####,回车启动。Exit页可打开Boot Manager。",
  L"未找到可见启动项。",
  L"(无描述)",
  L"启用",
  L"未启用",
  L"无法枚举句柄。",
  L"DXE可见%u个句柄",
  L"安全启动",
  L"已启用",
  L"已禁用",
  L"可用",
  L"N/A",
  L"可用",
  L"N/A",
  L"N/A",
  L"固件",
  L"Capsule Runtime",
  L"Capsule Protocol",
  L"Capsule Report",
  L"系统状态",
  L"ACPI表",
  L"SMBIOS表",
  L"Memory Map",
  L"DXE句柄",
  L"系统表",
  L"管理",
  L"IPMI",
  L"Redfish",
  L"Management Interface",
  L"Power / Thermal",
  L"ACPI表",
  L"ACPI SDT Protocol",
  L"Chassis Thermal State",
  L"Power Supply",
  L"Performance / Tuning",
  L"Processor Inventory",
  L"Memory Inventory",
  L"CPU I/O Protocol",
  L"Virtualization Policy Entry",
  L"RAS Policy Entry",
  L"TCG2",
  L"TrEE",
  L"密钥与平台策略由原生HII/FormBrowser管理；本页只读。",
  L"启动操作、系统操作与语言选择。",
  L"继续启动",
  L"打开原生启动工具",
  L"重启系统...",
  L"语言：%s",
  L"语言",
  L"中文",
  L"English",
  L"语言已切换：%s",
  L"未找到DriverSample HII页面。",
  L"DriverSample页面集",
  L"表单",
  L"问题项",
  L"HII桥接v1暂不支持",
  L"只读",
  L"回车打开选中表单，或切换支持的设置值。",
  L"RouteConfig返回：%r",
  L"启动项返回：%r",
  L"Boot Manager返回：%r",
  L"ModernSetupApp：图形初始化失败：%r\n",
  L"启动与设备",
  L"平台健康",
  L"电源与性能",
  L"固件",
  L"诊断",
  L"管理",
  L"电源",
  L"性能",
  L"设置分类",
  L"打开/回车",
  L"使用上/下选择，回车切换应用自有偏好。",
  L"主题",
  L"系统",
  L"深色",
  L"红色",
  L"仪表盘密度",
  L"舒适",
  L"紧凑",
  L"记住上次页面",
  L"显示高级提示",
  L"重启前确认",
  L"保存偏好",
  L"载入默认值",
  L"偏好保存结果：%r",
  L"已载入默认偏好；保存后持久化。"
};

/**
  Return TRUE when the active language should use Simplified Chinese strings.

  @retval TRUE   Active language starts with "zh".
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
  Normalize a caller-provided language tag into a supported ModernSetup tag.

  @param[in]  Language    Source language tag. Must not be NULL.
  @param[out] Normalized  Receives a supported language tag pointer.
                          Must not be NULL.

  @retval EFI_SUCCESS            Language is supported.
  @retval EFI_INVALID_PARAMETER  Language or Normalized is NULL, or the tag is
                                 not supported by the built-in string table.
**/
STATIC
EFI_STATUS
NormalizeLanguage (
  IN  CONST CHAR8  *Language,
  OUT CONST CHAR8  **Normalized
  )
{
  if ((Language == NULL) || (Normalized == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Language[0] == 'z') && (Language[1] == 'h')) {
    *Normalized = "zh-Hans";
    return EFI_SUCCESS;
  }

  if ((Language[0] == 'e') && (Language[1] == 'n')) {
    *Normalized = "en-US";
    return EFI_SUCCESS;
  }

  return EFI_INVALID_PARAMETER;
}

/**
  Set the active language buffer.

  @param[in] Language  Supported normalized language tag. Must not be NULL.
**/
STATIC
VOID
SetActiveLanguageBuffer (
  IN CONST CHAR8  *Language
  )
{
  ZeroMem (mActiveLanguage, sizeof (mActiveLanguage));
  AsciiStrnCpyS (mActiveLanguage, sizeof (mActiveLanguage), Language, sizeof (mActiveLanguage) - 1);
  mLanguageInitialized = TRUE;
}

/**
  Initialize the active language from NVRAM or the fixed PCD.

  Runtime variable ModernSetupLanguage wins when it contains a supported tag.
  Unsupported or missing variable data falls back to PCD.
**/
STATIC
VOID
EnsureLanguageInitialized (
  VOID
  )
{
  EFI_STATUS   Status;
  CHAR8        VariableLanguage[MODERN_SETUP_LANGUAGE_MAX_SIZE];
  UINTN        Size;
  CONST CHAR8  *Normalized;
  CONST CHAR8  *PcdLanguage;

  if (mLanguageInitialized) {
    return;
  }

  Size = sizeof (VariableLanguage);
  ZeroMem (VariableLanguage, sizeof (VariableLanguage));
  Status = gRT->GetVariable (
                  MODERN_SETUP_LANGUAGE_VARIABLE,
                  &gModernSetupPkgTokenSpaceGuid,
                  NULL,
                  &Size,
                  VariableLanguage
                  );
  if (!EFI_ERROR (Status) && !EFI_ERROR (NormalizeLanguage (VariableLanguage, &Normalized))) {
    SetActiveLanguageBuffer (Normalized);
    return;
  }

  PcdLanguage = (CONST CHAR8 *)FixedPcdGetPtr (PcdModernSetupDefaultLanguage);
  if ((PcdLanguage == NULL) || EFI_ERROR (NormalizeLanguage (PcdLanguage, &Normalized))) {
    Normalized = "zh-Hans";
  }

  SetActiveLanguageBuffer (Normalized);
}

/**
  Return the active language tag.

  Runtime variable ModernSetupLanguage is preferred when present. The fixed PCD
  language is used as the fallback.

  @return Non-NULL ASCII language tag. The default is "zh-Hans".
**/
CONST CHAR8 *
EFIAPI
ModernUiGetLanguage (
  VOID
  )
{
  EnsureLanguageInitialized ();
  return mActiveLanguage;
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

/**
  Set the active ModernSetup language.

  Supported language families are "zh" and "en". Other language tags are
  rejected so callers do not persist an unsupported UI state.

  @param[in] Language  ASCII language tag. Must not be NULL.
  @param[in] Persist   TRUE writes the language to non-volatile variables.

  @retval EFI_SUCCESS            Active language was changed.
  @retval EFI_INVALID_PARAMETER  Language is NULL or unsupported.
  @retval others                 Variable write failed after the in-memory
                                 language was changed.
**/
EFI_STATUS
EFIAPI
ModernUiSetLanguage (
  IN CONST CHAR8  *Language,
  IN BOOLEAN      Persist
  )
{
  EFI_STATUS   Status;
  CONST CHAR8  *Normalized;
  UINT32       Attributes;

  Status = NormalizeLanguage (Language, &Normalized);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  SetActiveLanguageBuffer (Normalized);
  if (!Persist) {
    return EFI_SUCCESS;
  }

  Attributes = EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS;
  return gRT->SetVariable (
                MODERN_SETUP_LANGUAGE_VARIABLE,
                &gModernSetupPkgTokenSpaceGuid,
                Attributes,
                AsciiStrSize (Normalized),
                (VOID *)Normalized
                );
}
