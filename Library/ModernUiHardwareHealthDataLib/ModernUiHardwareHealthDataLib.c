/** @file
  Read-only hardware health demo summary provider for ModernSetupApp.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>

#include <ModernUi/ModernUiHardwareHealthData.h>

typedef struct {
  CONST CHAR16  *Name;
  INT16         CurrentValue;
  INT16         WarningValue;
  INT16         CriticalValue;
  INT16         Samples[MODERN_UI_HARDWARE_HEALTH_MAX_SAMPLES];
} HARDWARE_HEALTH_DEMO_SENSOR;

STATIC CONST HARDWARE_HEALTH_DEMO_SENSOR  mHardwareHealthDemoSensors[] = {
  {
    L"CPU Package",
    54,
    85,
    100,
    { 48, 49, 51, 50, 52, 53, 55, 56, 54, 53, 55, 57, 56, 55, 54, 54 }
  },
  {
    L"Board Ambient",
    32,
    55,
    70,
    { 30, 30, 31, 31, 32, 32, 32, 33, 33, 32, 32, 32, 31, 32, 32, 32 }
  },
  {
    L"VRM Zone",
    61,
    95,
    110,
    { 56, 57, 58, 60, 61, 62, 63, 62, 61, 60, 61, 62, 63, 62, 61, 61 }
  }
};

STATIC
VOID
FillDemoSensor (
  OUT MODERN_UI_HARDWARE_HEALTH_SENSOR  *Sensor,
  IN  CONST HARDWARE_HEALTH_DEMO_SENSOR *Demo
  )
{
  if ((Sensor == NULL) || (Demo == NULL)) {
    return;
  }

  ZeroMem (Sensor, sizeof (*Sensor));
  Sensor->Type          = ModernUiHardwareSensorTemperature;
  Sensor->CurrentValue  = Demo->CurrentValue;
  Sensor->WarningValue  = Demo->WarningValue;
  Sensor->CriticalValue = Demo->CriticalValue;
  Sensor->SampleCount   = MODERN_UI_HARDWARE_HEALTH_MAX_SAMPLES;
  CopyMem (Sensor->Samples, Demo->Samples, sizeof (Sensor->Samples));
  UnicodeSPrint (Sensor->Name, sizeof (Sensor->Name), L"%s", Demo->Name);
  UnicodeSPrint (Sensor->Unit, sizeof (Sensor->Unit), L"C");
}

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
  )
{
  UINTN  Index;

  if (Summary == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (Summary, sizeof (*Summary));
  Summary->DemoData    = TRUE;
  Summary->SensorCount = MIN (ARRAY_SIZE (mHardwareHealthDemoSensors), MODERN_UI_HARDWARE_HEALTH_MAX_SENSORS);
  UnicodeSPrint (Summary->ProviderName, sizeof (Summary->ProviderName), L"Demo provider");

  for (Index = 0; Index < Summary->SensorCount; Index++) {
    FillDemoSensor (&Summary->Sensors[Index], &mHardwareHealthDemoSensors[Index]);
  }

  return EFI_SUCCESS;
}
