/** @file
  Read-only hardware health demo summary provider for ModernSetupApp.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MODERN_UI_HARDWARE_HEALTH_DATA_H_
#define MODERN_UI_HARDWARE_HEALTH_DATA_H_

#include <Uefi.h>

#define MODERN_UI_HARDWARE_HEALTH_MAX_SENSORS  4
#define MODERN_UI_HARDWARE_HEALTH_MAX_SAMPLES  16
#define MODERN_UI_HARDWARE_HEALTH_TEXT_MAX     64

typedef enum {
  ModernUiHardwareSensorTemperature = 0,
  ModernUiHardwareSensorTypeMax
} MODERN_UI_HARDWARE_SENSOR_TYPE;

typedef struct {
  MODERN_UI_HARDWARE_SENSOR_TYPE  Type;
  CHAR16                          Name[MODERN_UI_HARDWARE_HEALTH_TEXT_MAX];
  CHAR16                          Unit[MODERN_UI_HARDWARE_HEALTH_TEXT_MAX];
  INT16                           CurrentValue;
  INT16                           WarningValue;
  INT16                           CriticalValue;
  UINTN                           SampleCount;
  INT16                           Samples[MODERN_UI_HARDWARE_HEALTH_MAX_SAMPLES];
} MODERN_UI_HARDWARE_HEALTH_SENSOR;

typedef struct {
  BOOLEAN                           DemoData;
  CHAR16                            ProviderName[MODERN_UI_HARDWARE_HEALTH_TEXT_MAX];
  UINTN                             SensorCount;
  MODERN_UI_HARDWARE_HEALTH_SENSOR  Sensors[MODERN_UI_HARDWARE_HEALTH_MAX_SENSORS];
} MODERN_UI_HARDWARE_HEALTH_SUMMARY;

/**
  Return deterministic read-only hardware health demo data.

  @param[out] Summary  Hardware health summary to fill. Must not be NULL.

  @retval EFI_SUCCESS            Summary was filled.
  @retval EFI_INVALID_PARAMETER  Summary is NULL.
**/
EFI_STATUS
EFIAPI
ModernUiHardwareHealthDataGetSummary (
  OUT MODERN_UI_HARDWARE_HEALTH_SUMMARY  *Summary
  );

#endif
