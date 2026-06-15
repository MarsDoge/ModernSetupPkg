/** @file
  Platform summary provider for ModernSetupApp.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <IndustryStandard/SmBios.h>
#include <Protocol/Smbios.h>
#include <Pi/PiMultiPhase.h>
#include <Protocol/MpService.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <ModernUi/ModernUiPlatformTables.h>
#include <ModernUi/ModernUiPlatformData.h>

/**
  Return the compile-time CPU architecture string.

  @return Non-NULL UCS-2 architecture name.
**/
STATIC
CONST CHAR16 *
GetArchitectureName (
  VOID
  )
{
#if defined (MDE_CPU_AARCH64)
  return L"AARCH64";
#elif defined (MDE_CPU_ARM)
  return L"ARM";
#elif defined (MDE_CPU_X64)
  return L"X64";
#elif defined (MDE_CPU_IA32)
  return L"IA32";
#elif defined (MDE_CPU_LOONGARCH64)
  return L"LOONGARCH64";
#elif defined (MDE_CPU_RISCV64)
  return L"RISCV64";
#else
  return L"UNKNOWN";
#endif
}

/**
  Sum a coarse memory size from the current UEFI memory map.

  @param[out] MemorySizeMb  Receives memory size in MiB. Must not be NULL.

  @retval EFI_SUCCESS            Memory size was computed.
  @retval EFI_INVALID_PARAMETER  MemorySizeMb is NULL.
  @retval EFI_OUT_OF_RESOURCES   Allocation failed.
  @retval others                 Status returned by GetMemoryMap().
**/
STATIC
EFI_STATUS
GetMemorySizeMb (
  OUT UINT64  *MemorySizeMb
  )
{
  EFI_STATUS                 Status;
  EFI_MEMORY_DESCRIPTOR      *MemoryMap;
  EFI_MEMORY_DESCRIPTOR      *Descriptor;
  UINTN                      MemoryMapSize;
  UINTN                      MapKey;
  UINTN                      DescriptorSize;
  UINT32                     DescriptorVersion;
  UINTN                      Index;
  UINTN                      DescriptorCount;
  UINT64                     Pages;

  if (MemorySizeMb == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *MemorySizeMb     = 0;
  MemoryMap         = NULL;
  MemoryMapSize     = 0;
  MapKey            = 0;
  DescriptorSize    = 0;
  DescriptorVersion = 0;

  Status = gBS->GetMemoryMap (&MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    return Status;
  }

  MemoryMapSize += DescriptorSize * 8;
  MemoryMap = AllocateZeroPool (MemoryMapSize);
  if (MemoryMap == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = gBS->GetMemoryMap (&MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
  if (EFI_ERROR (Status)) {
    FreePool (MemoryMap);
    return Status;
  }

  Pages           = 0;
  DescriptorCount = MemoryMapSize / DescriptorSize;
  for (Index = 0; Index < DescriptorCount; Index++) {
    Descriptor = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)MemoryMap + (Index * DescriptorSize));
    Pages += Descriptor->NumberOfPages;
  }

  *MemorySizeMb = EFI_PAGES_TO_SIZE (Pages) / (1024 * 1024);
  FreePool (MemoryMap);
  return EFI_SUCCESS;
}

/**
  Convert an SMBIOS chassis type to a product form factor string.

  @param[in] ChassisType  SMBIOS Type 3 chassis type field.

  @return Non-NULL UCS-2 form factor name.
**/
STATIC
CONST CHAR16 *
GetFormFactorName (
  IN UINT8  ChassisType
  )
{
  switch (ChassisType & 0x7F) {
    case MiscChassisTypeDeskTop:
    case MiscChassisTypeLowProfileDesktop:
    case MiscChassisTypeMiniTower:
    case MiscChassisTypeTower:
      return L"Desktop";
    case MiscChassisTypePortable:
    case MiscChassisTypeLapTop:
    case MiscChassisTypeNotebook:
    case MiscChassisTypeSubNotebook:
    case MiscChassisConvertible:
    case MiscChassisDetachable:
      return L"Laptop / 2-in-1";
    case MiscChassisTypeAllInOne:
      return L"All-in-one";
    case MiscChassisTypeSpaceSaving:
    case MiscChassisTypeSealedCasePc:
    case MiscChassisMiniPc:
    case MiscChassisStickPc:
      return L"Mini PC";
    case MiscChassisTypeMainServerChassis:
    case MiscChassisTypeRackMountChassis:
    case MiscChassisBlade:
    case MiscChassisBladeEnclosure:
      return L"Server";
    case MiscChassisIoTGateway:
    case MiscChassisEmbeddedPc:
    case MiscChassisTypeHandHeld:
      return L"Embedded / appliance";
    default:
      //
      // Unrecognized chassis type: report empty so the consumer can show its
      // own localized "unknown" text instead of duplicating the platform name.
      //
      return L"";
  }
}

/**
  Read the platform/product identity from SMBIOS Type 1 when available.

  Composes "Manufacturer ProductName" (or whichever single field is present) so
  the dashboard shows the real system name instead of a generic placeholder. The
  buffer is left empty when SMBIOS Type 1 is absent or reports no identity
  strings, so the caller can apply its own fallback.

  @param[out] Buffer  Destination buffer. Must not be NULL.
  @param[in]  Count   Number of CHAR16 entries in Buffer.
**/
STATIC
VOID
GetSmbiosSystemName (
  OUT CHAR16  *Buffer,
  IN  UINTN   Count
  )
{
  EFI_SMBIOS_TABLE_HEADER  *Record;
  SMBIOS_TABLE_TYPE1       *Type1;
  CHAR8                    *Manufacturer;
  CHAR8                    *ProductName;

  if ((Buffer == NULL) || (Count == 0)) {
    return;
  }

  Buffer[0] = L'\0';
  Record = ModernUiSmbiosFindStructure (SMBIOS_TYPE_SYSTEM_INFORMATION, 0);
  if (Record == NULL) {
    return;
  }

  Type1        = (SMBIOS_TABLE_TYPE1 *)Record;
  Manufacturer = ModernUiSmbiosGetString (Record, Type1->Manufacturer);
  ProductName  = ModernUiSmbiosGetString (Record, Type1->ProductName);
  if (ModernUiSmbiosIsPlaceholder (Manufacturer)) {
    Manufacturer = NULL;
  }

  if (ModernUiSmbiosIsPlaceholder (ProductName)) {
    ProductName = NULL;
  }

  //
  // SMBIOS strings are ASCII; widen with %a. Prefer "Manufacturer ProductName"
  // when both are present, otherwise whichever single field is reported.
  //
  if ((Manufacturer != NULL) && (ProductName != NULL)) {
    UnicodeSPrint (Buffer, Count * sizeof (CHAR16), L"%a %a", Manufacturer, ProductName);
  } else if (ProductName != NULL) {
    UnicodeSPrint (Buffer, Count * sizeof (CHAR16), L"%a", ProductName);
  } else if (Manufacturer != NULL) {
    UnicodeSPrint (Buffer, Count * sizeof (CHAR16), L"%a", Manufacturer);
  }
}

/**
  Read the processor identity from SMBIOS Type 4 when available.

  Composes "<Processor Version> (<cores>C/<threads>T)" -- e.g.
  "Intel(R) Xeon(R) ... (8C/16T)" -- so the dashboard shows a real CPU instead of
  a placeholder. Honors the SMBIOS 0xFF "see CoreCount2/ThreadCount2" escape for
  high core counts. Leaves the buffer empty when Type 4 is absent or reports
  neither a usable version string nor a core count, so the caller can fall back.

  @param[out] Buffer  Destination buffer. Must not be NULL.
  @param[in]  Count   Number of CHAR16 entries in Buffer.
**/
STATIC
VOID
GetSmbiosProcessor (
  OUT CHAR16  *Buffer,
  IN  UINTN   Count
  )
{
  EFI_SMBIOS_TABLE_HEADER  *Record;
  SMBIOS_TABLE_TYPE4       *Type4;
  CHAR8                    *Version;
  UINT32                   Cores;
  UINT32                   Threads;

  if ((Buffer == NULL) || (Count == 0)) {
    return;
  }

  Buffer[0] = L'\0';
  Record = ModernUiSmbiosFindStructure (SMBIOS_TYPE_PROCESSOR_INFORMATION, 0);
  if (Record == NULL) {
    return;
  }

  Type4   = (SMBIOS_TABLE_TYPE4 *)Record;
  Version = ModernUiSmbiosGetString (Record, Type4->ProcessorVersion);
  if (ModernUiSmbiosIsPlaceholder (Version)) {
    Version = NULL;
  }

  //
  // CoreCount/ThreadCount are UINT8; 0xFF means the real value is in the UINT16
  // CoreCount2/ThreadCount2 fields (present when the record is long enough).
  //
  Cores   = Type4->CoreCount;
  Threads = Type4->ThreadCount;
  if ((Cores == 0xFF) && (Record->Length >= OFFSET_OF (SMBIOS_TABLE_TYPE4, CoreCount2) + sizeof (Type4->CoreCount2))) {
    Cores = Type4->CoreCount2;
  }

  if ((Threads == 0xFF) && (Record->Length >= OFFSET_OF (SMBIOS_TABLE_TYPE4, ThreadCount2) + sizeof (Type4->ThreadCount2))) {
    Threads = Type4->ThreadCount2;
  }

  if ((Version != NULL) && (Cores > 0)) {
    UnicodeSPrint (Buffer, Count * sizeof (CHAR16), L"%a (%uC/%uT)", Version, Cores, Threads);
  } else if (Version != NULL) {
    UnicodeSPrint (Buffer, Count * sizeof (CHAR16), L"%a", Version);
  } else if (Cores > 0) {
    UnicodeSPrint (Buffer, Count * sizeof (CHAR16), L"%u cores / %u threads", Cores, Threads);
  }
}

/**
  Read the system serial number and UUID from SMBIOS Type 1 when available.

  The serial is filtered through the placeholder list; the UUID is rendered in
  canonical GUID text form and suppressed when SMBIOS reports the all-zero
  ("not present") or all-FF ("not settable") sentinel values. Either buffer is
  left empty when its field is absent, so the caller can hide the row.

  @param[out] Serial       Destination for the serial number. Must not be NULL.
  @param[in]  SerialCount  Number of CHAR16 entries in Serial.
  @param[out] Uuid         Destination for the UUID text. Must not be NULL.
  @param[in]  UuidCount    Number of CHAR16 entries in Uuid.
**/
STATIC
VOID
GetSmbiosSystemDetail (
  OUT CHAR16  *Serial,
  IN  UINTN   SerialCount,
  OUT CHAR16  *Uuid,
  IN  UINTN   UuidCount
  )
{
  EFI_SMBIOS_TABLE_HEADER  *Record;
  SMBIOS_TABLE_TYPE1       *Type1;
  CHAR8                    *SerialString;
  GUID                     UuidValue;
  CONST UINT8              *UuidBytes;
  BOOLEAN                  AllZero;
  BOOLEAN                  AllOnes;
  UINTN                    Index;

  if ((Serial == NULL) || (SerialCount == 0) || (Uuid == NULL) || (UuidCount == 0)) {
    return;
  }

  Serial[0] = L'\0';
  Uuid[0]   = L'\0';
  Record = ModernUiSmbiosFindStructure (SMBIOS_TYPE_SYSTEM_INFORMATION, 0);
  if (Record == NULL) {
    return;
  }

  Type1        = (SMBIOS_TABLE_TYPE1 *)Record;
  SerialString = ModernUiSmbiosGetString (Record, Type1->SerialNumber);
  if (!ModernUiSmbiosIsPlaceholder (SerialString)) {
    UnicodeSPrint (Serial, SerialCount * sizeof (CHAR16), L"%a", SerialString);
  }

  //
  // SMBIOS records are byte-packed, so Type1->Uuid can be unaligned. Copy it to
  // an aligned local before any structured (%g) access -- a multi-byte read of
  // the unaligned GUID faults on strict-alignment targets (e.g. AArch64).
  // SMBIOS defines all-zero as "UUID not present" and all-FF as "present but
  // not settable"; neither is a usable identity. The EFI_GUID field layout
  // already matches the SMBIOS 2.6+ byte order, so %g prints canonically.
  //
  CopyMem (&UuidValue, &Type1->Uuid, sizeof (UuidValue));
  UuidBytes = (CONST UINT8 *)&UuidValue;
  AllZero   = TRUE;
  AllOnes   = TRUE;
  for (Index = 0; Index < sizeof (UuidValue); Index++) {
    if (UuidBytes[Index] != 0x00) {
      AllZero = FALSE;
    }

    if (UuidBytes[Index] != 0xFF) {
      AllOnes = FALSE;
    }
  }

  if (!AllZero && !AllOnes) {
    UnicodeSPrint (Uuid, UuidCount * sizeof (CHAR16), L"%g", &UuidValue);
  }
}

/**
  Read the baseboard identity from SMBIOS Type 2 when available.

  Composes "<Manufacturer> <Product>" with placeholder filtering, mirroring the
  Type 1 system-name composition. Leaves the buffer empty when Type 2 is absent
  or reports no usable strings.

  @param[out] Buffer  Destination buffer. Must not be NULL.
  @param[in]  Count   Number of CHAR16 entries in Buffer.
**/
STATIC
VOID
GetSmbiosBaseboard (
  OUT CHAR16  *Buffer,
  IN  UINTN   Count
  )
{
  EFI_SMBIOS_TABLE_HEADER  *Record;
  SMBIOS_TABLE_TYPE2       *Type2;
  CHAR8                    *Manufacturer;
  CHAR8                    *Product;

  if ((Buffer == NULL) || (Count == 0)) {
    return;
  }

  Buffer[0] = L'\0';
  Record = ModernUiSmbiosFindStructure (SMBIOS_TYPE_BASEBOARD_INFORMATION, 0);
  if (Record == NULL) {
    return;
  }

  Type2        = (SMBIOS_TABLE_TYPE2 *)Record;
  Manufacturer = ModernUiSmbiosGetString (Record, Type2->Manufacturer);
  Product      = ModernUiSmbiosGetString (Record, Type2->ProductName);
  if (ModernUiSmbiosIsPlaceholder (Manufacturer)) {
    Manufacturer = NULL;
  }

  if (ModernUiSmbiosIsPlaceholder (Product)) {
    Product = NULL;
  }

  if ((Manufacturer != NULL) && (Product != NULL)) {
    UnicodeSPrint (Buffer, Count * sizeof (CHAR16), L"%a %a", Manufacturer, Product);
  } else if (Product != NULL) {
    UnicodeSPrint (Buffer, Count * sizeof (CHAR16), L"%a", Product);
  } else if (Manufacturer != NULL) {
    UnicodeSPrint (Buffer, Count * sizeof (CHAR16), L"%a", Manufacturer);
  }
}

/**
  Read the firmware version string and release date from SMBIOS Type 0 when
  available.

  These are the BIOS-vendor-owned strings (e.g. "edk2-stable202505" and
  "05/30/2025") and complement the numeric gST->FirmwareRevision. Each buffer is
  left empty when its field is absent or a placeholder, so the caller can hide
  the row.

  @param[out] Version       Destination for the version string. Must not be NULL.
  @param[in]  VersionCount  Number of CHAR16 entries in Version.
  @param[out] Date          Destination for the release date. Must not be NULL.
  @param[in]  DateCount     Number of CHAR16 entries in Date.
**/
STATIC
VOID
GetSmbiosBiosInfo (
  OUT CHAR16  *Version,
  IN  UINTN   VersionCount,
  OUT CHAR16  *Date,
  IN  UINTN   DateCount
  )
{
  EFI_SMBIOS_TABLE_HEADER  *Record;
  SMBIOS_TABLE_TYPE0       *Type0;
  CHAR8                    *VersionString;
  CHAR8                    *DateString;

  if ((Version == NULL) || (VersionCount == 0) || (Date == NULL) || (DateCount == 0)) {
    return;
  }

  Version[0] = L'\0';
  Date[0]    = L'\0';
  Record = ModernUiSmbiosFindStructure (SMBIOS_TYPE_BIOS_INFORMATION, 0);
  if (Record == NULL) {
    return;
  }

  Type0         = (SMBIOS_TABLE_TYPE0 *)Record;
  VersionString = ModernUiSmbiosGetString (Record, Type0->BiosVersion);
  DateString    = ModernUiSmbiosGetString (Record, Type0->BiosReleaseDate);
  if (!ModernUiSmbiosIsPlaceholder (VersionString)) {
    UnicodeSPrint (Version, VersionCount * sizeof (CHAR16), L"%a", VersionString);
  }

  if (!ModernUiSmbiosIsPlaceholder (DateString)) {
    UnicodeSPrint (Date, DateCount * sizeof (CHAR16), L"%a", DateString);
  }
}

/**
  Map an SMBIOS Type 17 MemoryType enumeration to a short display string.

  @param[in] MemoryType  SMBIOS MEMORY_DEVICE_TYPE value.

  @retval NULL    The type is unknown/unmapped (caller omits the type prefix).
  @retval others  Static short label, e.g. L"DDR4".
**/
STATIC
CONST CHAR16 *
MemoryTypeToString (
  IN UINT8  MemoryType
  )
{
  switch (MemoryType) {
    case MemoryTypeSdram:
      return L"SDRAM";
    case MemoryTypeDdr:
      return L"DDR";
    case MemoryTypeDdr2:
    case MemoryTypeDdr2FbDimm:
      return L"DDR2";
    case MemoryTypeDdr3:
      return L"DDR3";
    case MemoryTypeDdr4:
      return L"DDR4";
    case MemoryTypeDdr5:
      return L"DDR5";
    case MemoryTypeLpddr:
      return L"LPDDR";
    case MemoryTypeLpddr2:
      return L"LPDDR2";
    case MemoryTypeLpddr3:
      return L"LPDDR3";
    case MemoryTypeLpddr4:
      return L"LPDDR4";
    case MemoryTypeLpddr5:
      return L"LPDDR5";
    default:
      return NULL;
  }
}

/**
  Read the memory type/speed/DIMM-count detail from SMBIOS Type 17 when available.

  Walks every Type 17 (Memory Device) record, counts the populated modules, and
  takes the type and speed from the first populated module to compose a detail
  string such as "DDR4-3200, 2 DIMMs". The configured clock speed is preferred
  over the rated speed; the type prefix and speed are each omitted when not
  reported. Leaves the buffer empty when SMBIOS Type 17 is absent or no module is
  populated, so the caller can show the bare total size.

  @param[out] Buffer  Destination buffer. Must not be NULL.
  @param[in]  Count   Number of CHAR16 entries in Buffer.
**/
STATIC
VOID
GetSmbiosMemoryDetail (
  OUT CHAR16  *Buffer,
  IN  UINTN   Count
  )
{
  EFI_SMBIOS_TABLE_HEADER  *Record;
  SMBIOS_TABLE_TYPE17      *Type17;
  UINTN                    DimmIndex;
  CONST CHAR16             *TypeString;
  UINTN                    DimmCount;
  UINT16                   Speed;
  CONST CHAR16             *Unit;

  if ((Buffer == NULL) || (Count == 0)) {
    return;
  }

  Buffer[0] = L'\0';
  TypeString = NULL;
  DimmCount  = 0;
  Speed      = 0;
  for (DimmIndex = 0; (Record = ModernUiSmbiosFindStructure (SMBIOS_TYPE_MEMORY_DEVICE, DimmIndex)) != NULL; DimmIndex++) {
    Type17 = (SMBIOS_TABLE_TYPE17 *)Record;
    //
    // Size 0 = slot empty; 0xFFFF = unknown. Count only populated modules.
    //
    if ((Type17->Size == 0) || (Type17->Size == 0xFFFF)) {
      continue;
    }

    DimmCount++;
    if (TypeString == NULL) {
      TypeString = MemoryTypeToString (Type17->MemoryType);
      Speed      = (Type17->ConfiguredMemoryClockSpeed != 0) ? Type17->ConfiguredMemoryClockSpeed : Type17->Speed;
    }
  }

  if (DimmCount == 0) {
    return;
  }

  Unit = (DimmCount == 1) ? L"DIMM" : L"DIMMs";
  if ((TypeString != NULL) && (Speed > 0)) {
    UnicodeSPrint (Buffer, Count * sizeof (CHAR16), L"%s-%u, %u %s", TypeString, Speed, DimmCount, Unit);
  } else if (TypeString != NULL) {
    UnicodeSPrint (Buffer, Count * sizeof (CHAR16), L"%s, %u %s", TypeString, DimmCount, Unit);
  } else {
    UnicodeSPrint (Buffer, Count * sizeof (CHAR16), L"%u %s", DimmCount, Unit);
  }
}

/**
  Read the platform form factor from SMBIOS Type 3 when available.

  @param[out] Buffer  Destination buffer. Must not be NULL.
  @param[in]  Count   Number of CHAR16 entries in Buffer.
**/
STATIC
VOID
GetSmbiosFormFactor (
  OUT CHAR16  *Buffer,
  IN  UINTN   Count
  )
{
  EFI_SMBIOS_TABLE_HEADER  *Record;

  if ((Buffer == NULL) || (Count == 0)) {
    return;
  }

  //
  // Default to empty (form factor not reported). When SMBIOS Type 3 is absent
  // the consumer applies its own localized "unknown" text rather than echoing
  // the generic platform name.
  //
  Buffer[0] = L'\0';
  Record = ModernUiSmbiosFindStructure (SMBIOS_TYPE_SYSTEM_ENCLOSURE, 0);
  if (Record != NULL) {
    UnicodeSPrint (
      Buffer,
      Count * sizeof (CHAR16),
      L"%s",
      GetFormFactorName (((SMBIOS_TABLE_TYPE3 *)Record)->Type)
      );
  }
}

/**
  Return the boot mode label for the current application context.

  @param[out] Buffer  Destination buffer. Must not be NULL.
  @param[in]  Count   Number of CHAR16 entries in Buffer.
**/
STATIC
VOID
GetBootModeName (
  OUT CHAR16  *Buffer,
  IN  UINTN   Count
  )
{
  if ((Buffer == NULL) || (Count == 0)) {
    return;
  }

  UnicodeSPrint (Buffer, Count * sizeof (CHAR16), L"UEFI");
}

/**
  Read the processor clock speed from SMBIOS Type 4 when available.

  Composes "<current> GHz" and appends "(max <rated> GHz)" when the rated speed
  differs. CurrentSpeed/MaxSpeed are reported in MHz; 0 means "unknown". Leaves
  the buffer empty when Type 4 is absent or the current speed is unknown.

  @param[out] Buffer  Destination buffer. Must not be NULL.
  @param[in]  Count   Number of CHAR16 entries in Buffer.
**/
STATIC
VOID
GetSmbiosProcessorSpeed (
  OUT CHAR16  *Buffer,
  IN  UINTN   Count
  )
{
  EFI_SMBIOS_TABLE_HEADER  *Record;
  SMBIOS_TABLE_TYPE4       *Type4;
  UINT16                   Current;
  UINT16                   Max;

  if ((Buffer == NULL) || (Count == 0)) {
    return;
  }

  Buffer[0] = L'\0';
  Record    = ModernUiSmbiosFindStructure (SMBIOS_TYPE_PROCESSOR_INFORMATION, 0);
  if (Record == NULL) {
    return;
  }

  Type4   = (SMBIOS_TABLE_TYPE4 *)Record;
  Current = Type4->CurrentSpeed;
  Max     = Type4->MaxSpeed;
  if (Current == 0) {
    return;
  }

  //
  // MHz -> GHz with two decimals: integer GHz = MHz / 1000; hundredths of a GHz
  // = (MHz % 1000) / 10. Show the rated max only when it differs from current.
  //
  if ((Max != 0) && (Max != Current)) {
    UnicodeSPrint (
      Buffer,
      Count * sizeof (CHAR16),
      L"%u.%02u GHz (max %u.%02u GHz)",
      Current / 1000,
      (Current % 1000) / 10,
      Max / 1000,
      (Max % 1000) / 10
      );
  } else {
    UnicodeSPrint (
      Buffer,
      Count * sizeof (CHAR16),
      L"%u.%02u GHz",
      Current / 1000,
      (Current % 1000) / 10
      );
  }
}

/**
  Read the per-level cache sizes from SMBIOS Type 7 when available.

  Aggregates the installed size of every enabled-level cache record by cache
  level (L1/L2/L3) and composes "L1 64 KB / L2 512 KB / L3 8 MB". The 32-bit
  InstalledSize2 (SMBIOS 3.1+) is preferred over the 16-bit InstalledSize; in
  both encodings the top bit selects 64 KB granularity. Leaves the buffer empty
  when Type 7 is absent or reports no installed cache.

  @param[out] Buffer  Destination buffer. Must not be NULL.
  @param[in]  Count   Number of CHAR16 entries in Buffer.
**/
STATIC
VOID
GetSmbiosCache (
  OUT CHAR16  *Buffer,
  IN  UINTN   Count
  )
{
  EFI_SMBIOS_TABLE_HEADER  *Record;
  SMBIOS_TABLE_TYPE7       *Type7;
  UINTN                    Index;
  UINT32                   LevelKb[3];
  UINT8                    Level;
  UINT32                   SizeKb;
  CHAR16                   Segment[40];
  CONST CHAR16             *Sep;

  if ((Buffer == NULL) || (Count == 0)) {
    return;
  }

  Buffer[0]  = L'\0';
  LevelKb[0] = 0;
  LevelKb[1] = 0;
  LevelKb[2] = 0;

  for (Index = 0; (Record = ModernUiSmbiosFindStructure (SMBIOS_TYPE_CACHE_INFORMATION, Index)) != NULL; Index++) {
    Type7 = (SMBIOS_TABLE_TYPE7 *)Record;
    //
    // Cache Configuration bits[2:0] hold the 0-based cache level. Clamp to the
    // L1..L3 buckets the dashboard shows; higher levels (rare) are ignored.
    //
    Level = (UINT8)((Type7->CacheConfiguration & 0x07) + 1);
    if ((Level < 1) || (Level > 3)) {
      continue;
    }

    //
    // InstalledSize/InstalledSize2 are bitfields: Size with a Granularity64K
    // flag (0 => 1 KB units, 1 => 64 KB units). Prefer the 32-bit InstalledSize2
    // (SMBIOS 3.1+) when the record is long enough and reports a size.
    //
    SizeKb = 0;
    if ((Record->Length >= OFFSET_OF (SMBIOS_TABLE_TYPE7, InstalledSize2) + sizeof (Type7->InstalledSize2)) &&
        (Type7->InstalledSize2.Size != 0))
    {
      SizeKb = Type7->InstalledSize2.Granularity64K ? (Type7->InstalledSize2.Size * 64)
                                                    : Type7->InstalledSize2.Size;
    } else if (Type7->InstalledSize.Size != 0) {
      SizeKb = Type7->InstalledSize.Granularity64K ? ((UINT32)Type7->InstalledSize.Size * 64)
                                                   : (UINT32)Type7->InstalledSize.Size;
    }

    LevelKb[Level - 1] += SizeKb;
  }

  if ((LevelKb[0] == 0) && (LevelKb[1] == 0) && (LevelKb[2] == 0)) {
    return;
  }

  Sep = L"";
  for (Index = 0; Index < 3; Index++) {
    if (LevelKb[Index] == 0) {
      continue;
    }

    //
    // Show whole-MB caches in MB, otherwise KB.
    //
    if ((LevelKb[Index] >= 1024) && ((LevelKb[Index] % 1024) == 0)) {
      UnicodeSPrint (Segment, sizeof (Segment), L"%sL%u %u MB", Sep, (UINT32)Index + 1, LevelKb[Index] / 1024);
    } else {
      UnicodeSPrint (Segment, sizeof (Segment), L"%sL%u %u KB", Sep, (UINT32)Index + 1, LevelKb[Index]);
    }

    StrCatS (Buffer, Count, Segment);
    Sep = L" / ";
  }
}

/**
  Read the live logical-processor count from EFI_MP_SERVICES_PROTOCOL.

  Composes "<enabled> enabled / <total> total" from GetNumberOfProcessors. This
  is the authoritative runtime count, distinct from the SMBIOS Type 4
  core/thread fields. Leaves the buffer empty when MP Services is unavailable
  (e.g. before it is published, or on a platform that does not provide it).

  @param[out] Buffer  Destination buffer. Must not be NULL.
  @param[in]  Count   Number of CHAR16 entries in Buffer.
**/
STATIC
VOID
GetMpLogicalProcessors (
  OUT CHAR16  *Buffer,
  IN  UINTN   Count
  )
{
  EFI_STATUS                Status;
  EFI_MP_SERVICES_PROTOCOL  *Mp;
  UINTN                     Total;
  UINTN                     Enabled;

  if ((Buffer == NULL) || (Count == 0)) {
    return;
  }

  Buffer[0] = L'\0';
  Mp        = NULL;
  Status    = gBS->LocateProtocol (&gEfiMpServiceProtocolGuid, NULL, (VOID **)&Mp);
  if (EFI_ERROR (Status) || (Mp == NULL)) {
    return;
  }

  Total   = 0;
  Enabled = 0;
  Status  = Mp->GetNumberOfProcessors (Mp, &Total, &Enabled);
  if (EFI_ERROR (Status) || (Total == 0)) {
    return;
  }

  UnicodeSPrint (Buffer, Count * sizeof (CHAR16), L"%u enabled / %u total", (UINT32)Enabled, (UINT32)Total);
}

EFI_STATUS
EFIAPI
ModernUiPlatformDataGetSummary (
  OUT MODERN_UI_PLATFORM_SUMMARY  *Summary
  )
{
  EFI_STATUS  Status;

  if (Summary == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (Summary, sizeof (*Summary));
  UnicodeSPrint (
    Summary->FirmwareVendor,
    sizeof (Summary->FirmwareVendor),
    L"%s",
    (gST->FirmwareVendor == NULL) ? L"Unknown" : gST->FirmwareVendor
    );
  //
  // gST->FirmwareRevision is conventionally encoded as (major << 16) | minor.
  // Surface the human-readable major.minor form while keeping the raw hex so a
  // firmware engineer can still read the exact encoded value.
  //
  UnicodeSPrint (
    Summary->FirmwareRevision,
    sizeof (Summary->FirmwareRevision),
    L"%u.%02u (0x%08x)",
    (UINT32)(gST->FirmwareRevision >> 16),
    (UINT32)(gST->FirmwareRevision & 0xFFFF),
    gST->FirmwareRevision
    );
  UnicodeSPrint (Summary->Architecture, sizeof (Summary->Architecture), L"%s", GetArchitectureName ());
  //
  // Prefer the real SMBIOS Type 1 system identity; fall back to the generic
  // label only when SMBIOS reports no product/manufacturer strings.
  //
  GetSmbiosSystemName (Summary->Platform, ARRAY_SIZE (Summary->Platform));
  if (Summary->Platform[0] == L'\0') {
    UnicodeSPrint (Summary->Platform, sizeof (Summary->Platform), L"UEFI platform");
  }

  GetSmbiosProcessor (Summary->Processor, ARRAY_SIZE (Summary->Processor));
  GetSmbiosProcessorSpeed (Summary->ProcessorSpeed, ARRAY_SIZE (Summary->ProcessorSpeed));
  GetSmbiosCache (Summary->Cache, ARRAY_SIZE (Summary->Cache));
  GetMpLogicalProcessors (Summary->LogicalProcessors, ARRAY_SIZE (Summary->LogicalProcessors));
  GetSmbiosMemoryDetail (Summary->MemoryDetail, ARRAY_SIZE (Summary->MemoryDetail));
  GetSmbiosSystemDetail (
    Summary->Serial,
    ARRAY_SIZE (Summary->Serial),
    Summary->Uuid,
    ARRAY_SIZE (Summary->Uuid)
    );
  GetSmbiosBaseboard (Summary->Baseboard, ARRAY_SIZE (Summary->Baseboard));
  GetSmbiosBiosInfo (
    Summary->BiosVersion,
    ARRAY_SIZE (Summary->BiosVersion),
    Summary->BiosDate,
    ARRAY_SIZE (Summary->BiosDate)
    );
  GetSmbiosFormFactor (Summary->FormFactor, ARRAY_SIZE (Summary->FormFactor));
  GetBootModeName (Summary->BootMode, ARRAY_SIZE (Summary->BootMode));

  Status = GetMemorySizeMb (&Summary->MemorySizeMb);
  if (EFI_ERROR (Status)) {
    Summary->MemorySizeMb = 0;
    return Status;
  }

  return EFI_SUCCESS;
}
