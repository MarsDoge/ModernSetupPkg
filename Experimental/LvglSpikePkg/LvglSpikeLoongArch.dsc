## @file
#  Minimal self-contained DSC for the experimental/lvgl-spike LoongArch build.
#  Builds the standalone render probe AND the LvglDisplayEngineDxe backend (LVGL
#  behind EDKII_FORM_DISPLAY_ENGINE_PROTOCOL) for LOONGARCH64. Not a shipped
#  platform; no FD.
#
#  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
#  SPDX-License-Identifier: BSD-2-Clause-Patent
##

[Defines]
  PLATFORM_NAME                  = LvglSpike
  PLATFORM_GUID                  = 9B2E4D71-0A3C-4F65-8E2D-1C7A6F0B1102
  PLATFORM_VERSION               = 0.01
  DSC_SPECIFICATION              = 0x00010005
  OUTPUT_DIRECTORY               = Build/LvglSpike
  SUPPORTED_ARCHITECTURES        = LOONGARCH64
  BUILD_TARGETS                  = DEBUG|RELEASE
  SKUID_IDENTIFIER               = DEFAULT

!include MdePkg/MdeLibs.dsc.inc

[LibraryClasses]
  UefiApplicationEntryPoint|MdePkg/Library/UefiApplicationEntryPoint/UefiApplicationEntryPoint.inf
  UefiDriverEntryPoint|MdePkg/Library/UefiDriverEntryPoint/UefiDriverEntryPoint.inf
  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
  BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  TimerLib|MdePkg/Library/BaseTimerLibNullTemplate/BaseTimerLibNullTemplate.inf
  DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
  HiiLib|MdeModulePkg/Library/UefiHiiLib/UefiHiiLib.inf
  UefiHiiServicesLib|MdeModulePkg/Library/UefiHiiServicesLib/UefiHiiServicesLib.inf
  LvglCoreLib|LvglSpikePkg/Library/LvglLib/LvglCoreLib.inf

[Components]
  #
  # Compiler intrinsics (memcpy/memset/...) that GCC emits for aggregate
  # copy/zero in LVGL are force-linked per component via NULL|IntrinsicLib.
  #
  LvglSpikePkg/Library/LvglLib/LvglSpikeProbe.inf {
    <LibraryClasses>
      NULL|CryptoPkg/Library/IntrinsicLib/IntrinsicLib.inf
  }
  LvglSpikePkg/LvglDisplayEngineDxe/LvglDisplayEngineDxe.inf {
    <LibraryClasses>
      NULL|CryptoPkg/Library/IntrinsicLib/IntrinsicLib.inf
  }
