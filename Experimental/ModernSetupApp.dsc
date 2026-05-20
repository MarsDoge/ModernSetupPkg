## @file
# Experimental build description for the ModernSetupApp front-page shell.
#
# Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
# Author: MarsDoge (Dongyan Qian)
# Open source: https://github.com/MarsDoge/ModernSetupPkg
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
##

[Defines]
  PLATFORM_NAME                  = ModernSetupAppExperimental
  PLATFORM_GUID                  = 85B592CB-F44F-4759-8A8D-6D03EBCA9F31
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010005
  OUTPUT_DIRECTORY               = Build/ModernSetupPkgExperimental
  SUPPORTED_ARCHITECTURES        = AARCH64|X64|LOONGARCH64
  BUILD_TARGETS                  = DEBUG|RELEASE
  SKUID_IDENTIFIER               = DEFAULT

[LibraryClasses]
  BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
  DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  RegisterFilterLib|MdePkg/Library/RegisterFilterLibNull/RegisterFilterLibNull.inf
  UefiApplicationEntryPoint|MdePkg/Library/UefiApplicationEntryPoint/UefiApplicationEntryPoint.inf
  UefiBootManagerLib|MdeModulePkg/Library/UefiBootManagerLib/UefiBootManagerLib.inf
  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  UefiHiiServicesLib|MdeModulePkg/Library/UefiHiiServicesLib/UefiHiiServicesLib.inf
  HiiLib|MdeModulePkg/Library/UefiHiiLib/UefiHiiLib.inf
  HobLib|MdePkg/Library/DxeHobLib/DxeHobLib.inf
  PerformanceLib|MdePkg/Library/BasePerformanceLibNull/BasePerformanceLibNull.inf
  PeCoffGetEntryPointLib|MdePkg/Library/BasePeCoffGetEntryPointLib/BasePeCoffGetEntryPointLib.inf
  DxeServicesTableLib|MdePkg/Library/DxeServicesTableLib/DxeServicesTableLib.inf
  DxeServicesLib|MdePkg/Library/DxeServicesLib/DxeServicesLib.inf
  ReportStatusCodeLib|MdePkg/Library/BaseReportStatusCodeLibNull/BaseReportStatusCodeLibNull.inf
  SortLib|MdeModulePkg/Library/BaseSortLib/BaseSortLib.inf
  StackCheckLib|MdePkg/Library/StackCheckLibNull/StackCheckLibNull.inf
  VariablePolicyHelperLib|MdeModulePkg/Library/VariablePolicyHelperLib/VariablePolicyHelperLib.inf
  UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  ModernUiEngineLib|ModernSetupPkg/Library/ModernUiEngineLib/ModernUiEngineLib.inf
  ModernUiPlatformDataLib|ModernSetupPkg/Library/ModernUiPlatformDataLib/ModernUiPlatformDataLib.inf
  ModernUiBootDataLib|ModernSetupPkg/Library/ModernUiBootDataLib/ModernUiBootDataLib.inf
  ModernUiDeviceDataLib|ModernSetupPkg/Library/ModernUiDeviceDataLib/ModernUiDeviceDataLib.inf
  ModernUiSecurityDataLib|ModernSetupPkg/Library/ModernUiSecurityDataLib/ModernUiSecurityDataLib.inf
  ModernUiFirmwareDataLib|ModernSetupPkg/Library/ModernUiFirmwareDataLib/ModernUiFirmwareDataLib.inf
  ModernUiDiagnosticsDataLib|ModernSetupPkg/Library/ModernUiDiagnosticsDataLib/ModernUiDiagnosticsDataLib.inf
  ModernUiManagementDataLib|ModernSetupPkg/Library/ModernUiManagementDataLib/ModernUiManagementDataLib.inf
  ModernUiPowerDataLib|ModernSetupPkg/Library/ModernUiPowerDataLib/ModernUiPowerDataLib.inf
  ModernUiPerformanceDataLib|ModernSetupPkg/Library/ModernUiPerformanceDataLib/ModernUiPerformanceDataLib.inf
  ModernUiPcieDataLib|ModernSetupPkg/Library/ModernUiPcieDataLib/ModernUiPcieDataLib.inf
  ModernUiRendererLib|ModernSetupPkg/Library/ModernUiRendererLib/ModernUiRendererLib.inf
  ModernUiInputLib|ModernSetupPkg/Library/ModernUiInputLib/ModernUiInputLib.inf
  ModernUiPreferencesLib|ModernSetupPkg/Library/ModernUiPreferencesLib/ModernUiPreferencesLib.inf
  ModernUiThemeLib|ModernSetupPkg/Library/ModernUiThemeLib/ModernUiThemeLib.inf
  ModernUiStringLib|ModernSetupPkg/Library/ModernUiStringLib/ModernUiStringLib.inf

[PcdsFixedAtBuild]
  gModernSetupPkgTokenSpaceGuid.PcdModernSetupDefaultLanguage|"zh-Hans"

[Components]
  ModernSetupPkg/Library/ModernUiEngineLib/ModernUiEngineLib.inf
  ModernSetupPkg/Library/ModernUiPlatformDataLib/ModernUiPlatformDataLib.inf
  ModernSetupPkg/Library/ModernUiBootDataLib/ModernUiBootDataLib.inf
  ModernSetupPkg/Library/ModernUiDeviceDataLib/ModernUiDeviceDataLib.inf
  ModernSetupPkg/Library/ModernUiSecurityDataLib/ModernUiSecurityDataLib.inf
  ModernSetupPkg/Library/ModernUiFirmwareDataLib/ModernUiFirmwareDataLib.inf
  ModernSetupPkg/Library/ModernUiDiagnosticsDataLib/ModernUiDiagnosticsDataLib.inf
  ModernSetupPkg/Library/ModernUiManagementDataLib/ModernUiManagementDataLib.inf
  ModernSetupPkg/Library/ModernUiPowerDataLib/ModernUiPowerDataLib.inf
  ModernSetupPkg/Library/ModernUiPerformanceDataLib/ModernUiPerformanceDataLib.inf
  ModernSetupPkg/Library/ModernUiPcieDataLib/ModernUiPcieDataLib.inf
  ModernSetupPkg/Library/ModernUiThemeLib/ModernUiThemeLib.inf
  ModernSetupPkg/Library/ModernUiRendererLib/ModernUiRendererLib.inf
  ModernSetupPkg/Library/ModernUiInputLib/ModernUiInputLib.inf
  ModernSetupPkg/Library/ModernUiPreferencesLib/ModernUiPreferencesLib.inf
  ModernSetupPkg/Library/ModernUiStringLib/ModernUiStringLib.inf
  ModernSetupPkg/Application/ModernSetupApp/ModernSetupApp.inf
