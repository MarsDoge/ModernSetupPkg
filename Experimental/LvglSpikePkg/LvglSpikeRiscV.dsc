## @file
#  Minimal self-contained DSC for the experimental/lvgl-spike RISC-V build
#  validation. Builds just the LvglSpikeProbe UEFI_APPLICATION (LVGL core +
#  software renderer + upstream UEFI port, with the LoongArch64/RISC-V64
#  arch-gate patch) for RISCV64. Per repo convention RISC-V is build-only
#  (no run/capture helper), so this validates compile+link, not render.
#
#  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
#  SPDX-License-Identifier: BSD-2-Clause-Patent
##

[Defines]
  PLATFORM_NAME                  = LvglSpikeRiscV
  PLATFORM_GUID                  = 9B2E4D71-0A3C-4F65-8E2D-1C7A6F0B1103
  PLATFORM_VERSION               = 0.01
  DSC_SPECIFICATION              = 0x00010005
  OUTPUT_DIRECTORY               = Build/LvglSpikeRiscV
  SUPPORTED_ARCHITECTURES        = RISCV64
  BUILD_TARGETS                  = DEBUG|RELEASE
  SKUID_IDENTIFIER               = DEFAULT

!include MdePkg/MdeLibs.dsc.inc

[LibraryClasses]
  UefiApplicationEntryPoint|MdePkg/Library/UefiApplicationEntryPoint/UefiApplicationEntryPoint.inf
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

[Components]
  LvglSpikePkg/Library/LvglLib/LvglSpikeProbe.inf {
    <LibraryClasses>
      # Force-link the compiler intrinsics (memcpy/memset/memmove/memcmp) GCC
      # emits for aggregate copy/zero in LVGL, even under -ffreestanding.
      NULL|CryptoPkg/Library/IntrinsicLib/IntrinsicLib.inf
  }
