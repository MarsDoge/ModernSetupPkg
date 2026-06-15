/** @file
  Read-only PCIe policy and capability summary provider for ModernSetupApp.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <IndustryStandard/Pci.h>
#include <IndustryStandard/PciExpress30.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/HiiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiHiiServicesLib.h>
#include <Protocol/IoMmu.h>
#include <Protocol/PciEnumerationComplete.h>
#include <Protocol/PciHotPlugInit.h>
#include <Protocol/PciHotPlugRequest.h>
#include <Protocol/PciIo.h>
#include <Protocol/PciRootBridgeIo.h>
#include <Uefi/UefiInternalFormRepresentation.h>

#include <ModernUi/ModernUiPcieData.h>

#define MODERN_UI_PCIE_EXTENDED_CONFIG_LIMIT  0x1000
#define MODERN_UI_PCIE_CAPABILITY_GUARD       48
#define MODERN_UI_PCIE_EXT_CAPABILITY_GUARD   256
#define MODERN_UI_PCIE_LINK_CAP_ASPM_MASK     (BIT10 | BIT11)
#define MODERN_UI_PCIE_SLOT_CAP_HOTPLUG       BIT6

/**
  Return whether a protocol is currently installed.

  @param[in] ProtocolGuid  Protocol GUID to locate. Must not be NULL.

  @retval TRUE   The protocol is installed.
  @retval FALSE  The protocol is absent or ProtocolGuid is NULL.
**/
STATIC
BOOLEAN
IsProtocolPresent (
  IN CONST EFI_GUID  *ProtocolGuid
  )
{
  VOID  *Protocol;

  if (ProtocolGuid == NULL) {
    return FALSE;
  }

  Protocol = NULL;
  return (BOOLEAN)!EFI_ERROR (gBS->LocateProtocol ((EFI_GUID *)ProtocolGuid, NULL, &Protocol));
}

/**
  Count handles that support a protocol.

  @param[in] ProtocolGuid  Protocol GUID to locate. Must not be NULL.

  @return Number of handles found, or zero if none/error.
**/
STATIC
UINTN
CountProtocolHandles (
  IN CONST EFI_GUID  *ProtocolGuid
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  *Handles;
  UINTN       HandleCount;

  if (ProtocolGuid == NULL) {
    return 0;
  }

  Handles     = NULL;
  HandleCount = 0;
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  (EFI_GUID *)ProtocolGuid,
                  NULL,
                  &HandleCount,
                  &Handles
                  );
  if (EFI_ERROR (Status)) {
    return 0;
  }

  if (Handles != NULL) {
    FreePool (Handles);
  }

  return HandleCount;
}

STATIC
EFI_STATUS
PciRead8 (
  IN  EFI_PCI_IO_PROTOCOL  *PciIo,
  IN  UINT32               Offset,
  OUT UINT8                *Value
  )
{
  if ((PciIo == NULL) || (Value == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  return PciIo->Pci.Read (PciIo, EfiPciIoWidthUint8, Offset, 1, Value);
}

STATIC
EFI_STATUS
PciRead16 (
  IN  EFI_PCI_IO_PROTOCOL  *PciIo,
  IN  UINT32               Offset,
  OUT UINT16               *Value
  )
{
  if ((PciIo == NULL) || (Value == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  return PciIo->Pci.Read (PciIo, EfiPciIoWidthUint16, Offset, 1, Value);
}

STATIC
EFI_STATUS
PciRead32 (
  IN  EFI_PCI_IO_PROTOCOL  *PciIo,
  IN  UINT32               Offset,
  OUT UINT32               *Value
  )
{
  if ((PciIo == NULL) || (Value == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  return PciIo->Pci.Read (PciIo, EfiPciIoWidthUint32, Offset, 1, Value);
}

/**
  Return whether a string contains one of the requested keywords.

  @param[in] Text          Source text. May be NULL.
  @param[in] Keywords      Keyword array. Must not be NULL when KeywordCount is
                           nonzero.
  @param[in] KeywordCount  Number of entries in Keywords.

  @retval TRUE   Text contains at least one keyword.
  @retval FALSE  No keyword matched.
**/
STATIC
BOOLEAN
ContainsAnyKeyword (
  IN CONST CHAR16       *Text OPTIONAL,
  IN CONST CHAR16 *CONST *Keywords,
  IN UINTN              KeywordCount
  )
{
  UINTN  Index;

  if ((Text == NULL) || ((KeywordCount > 0) && (Keywords == NULL))) {
    return FALSE;
  }

  for (Index = 0; Index < KeywordCount; Index++) {
    if ((Keywords[Index] != NULL) && (StrStr (Text, Keywords[Index]) != NULL)) {
      return TRUE;
    }
  }

  return FALSE;
}

/**
  Return whether any HII formset title/help mentions one of the keywords.

  @param[in] Keywords      Keyword array. Must not be NULL when KeywordCount is
                           nonzero.
  @param[in] KeywordCount  Number of entries in Keywords.

  @retval TRUE   A matching HII formset exists.
  @retval FALSE  No matching HII formset was found.
**/
STATIC
BOOLEAN
HasHiiFormsetKeyword (
  IN CONST CHAR16 *CONST *Keywords,
  IN UINTN              KeywordCount
  )
{
  EFI_HII_HANDLE    *Handles;
  UINTN             HandleIndex;
  EFI_IFR_FORM_SET  *FormSets;
  EFI_IFR_OP_HEADER *OpHeader;
  UINTN             BufferSize;
  UINTN             Offset;
  CHAR16            *Title;
  CHAR16            *Help;
  BOOLEAN           Found;

  if ((KeywordCount > 0) && (Keywords == NULL)) {
    return FALSE;
  }

  Handles = HiiGetHiiHandles (NULL);
  if (Handles == NULL) {
    return FALSE;
  }

  Found = FALSE;
  for (HandleIndex = 0; !Found && (Handles[HandleIndex] != NULL); HandleIndex++) {
    FormSets   = NULL;
    BufferSize = 0;
    if (EFI_ERROR (HiiGetFormSetFromHiiHandle (Handles[HandleIndex], &FormSets, &BufferSize))) {
      continue;
    }

    Offset = 0;
    while (!Found && (Offset + sizeof (EFI_IFR_OP_HEADER) <= BufferSize)) {
      OpHeader = (EFI_IFR_OP_HEADER *)((UINT8 *)FormSets + Offset);
      if ((OpHeader->Length == 0) || (Offset + OpHeader->Length > BufferSize)) {
        break;
      }

      if (OpHeader->OpCode == EFI_IFR_FORM_SET_OP) {
        Title = HiiGetString (Handles[HandleIndex], ((EFI_IFR_FORM_SET *)OpHeader)->FormSetTitle, NULL);
        Help  = HiiGetString (Handles[HandleIndex], ((EFI_IFR_FORM_SET *)OpHeader)->Help, NULL);
        Found = (BOOLEAN)(ContainsAnyKeyword (Title, Keywords, KeywordCount) || ContainsAnyKeyword (Help, Keywords, KeywordCount));
        if (Title != NULL) {
          FreePool (Title);
        }

        if (Help != NULL) {
          FreePool (Help);
        }
      }

      Offset += OpHeader->Length;
    }

    FreePool (FormSets);
  }

  FreePool (Handles);
  return Found;
}

/**
  Locate the PCI Express capability structure in conventional config space.

  @param[in]   PciIo      PCI I/O protocol. Must not be NULL.
  @param[out]  CapOffset  Receives the capability offset on success. Must not be
                          NULL; set to 0 when the capability is absent.

  @retval TRUE   The PCI Express capability was found; *CapOffset is its offset.
  @retval FALSE  Absent, unreadable, or a parameter was NULL.
**/
STATIC
BOOLEAN
FindPciExpressCapability (
  IN  EFI_PCI_IO_PROTOCOL  *PciIo,
  OUT UINT8                *CapOffset
  )
{
  EFI_STATUS  Status;
  UINT16      PciStatus;
  UINT8       HeaderType;
  UINT8       Pointer;
  UINT8       CapabilityId;
  UINT8       NextPointer;
  UINTN       Guard;

  if ((PciIo == NULL) || (CapOffset == NULL)) {
    return FALSE;
  }

  *CapOffset = 0;
  Status     = PciRead16 (PciIo, PCI_PRIMARY_STATUS_OFFSET, &PciStatus);
  if (EFI_ERROR (Status) || ((PciStatus & EFI_PCI_STATUS_CAPABILITY) == 0)) {
    return FALSE;
  }

  Status = PciRead8 (PciIo, PCI_HEADER_TYPE_OFFSET, &HeaderType);
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  if ((HeaderType & HEADER_LAYOUT_CODE) == HEADER_TYPE_CARDBUS_BRIDGE) {
    Pointer = 0x14;
  } else {
    Pointer = PCI_CAPABILITY_POINTER_OFFSET;
  }

  Status = PciRead8 (PciIo, Pointer, &Pointer);
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  Pointer &= (UINT8)~0x3;
  for (Guard = 0; (Pointer >= 0x40) && (Guard < MODERN_UI_PCIE_CAPABILITY_GUARD); Guard++) {
    Status = PciRead8 (PciIo, Pointer, &CapabilityId);
    if (EFI_ERROR (Status) || (CapabilityId == 0xFF)) {
      break;
    }

    if (CapabilityId == PCI_EXPRESS_CAPABILITY_ID) {
      *CapOffset = Pointer;
      return TRUE;
    }

    Status = PciRead8 (PciIo, Pointer + 1, &NextPointer);
    if (EFI_ERROR (Status)) {
      break;
    }

    NextPointer &= (UINT8)~0x3;
    if (NextPointer == Pointer) {
      break;
    }

    Pointer = NextPointer;
  }

  return FALSE;
}

/**
  Scan conventional PCI capabilities for PCIe link/slot capabilities.

  @param[in]      PciIo    PCI I/O protocol. Must not be NULL.
  @param[in,out]  Summary  Summary counters to update. Must not be NULL.
**/
STATIC
VOID
ScanPciExpressCapability (
  IN     EFI_PCI_IO_PROTOCOL      *PciIo,
  IN OUT MODERN_UI_PCIE_SUMMARY   *Summary
  )
{
  UINT8   CapOffset;
  UINT32  LinkCapability;
  UINT32  SlotCapability;

  if ((PciIo == NULL) || (Summary == NULL)) {
    return;
  }

  if (!FindPciExpressCapability (PciIo, &CapOffset)) {
    return;
  }

  if (!EFI_ERROR (PciRead32 (PciIo, (UINT32)CapOffset + 0x0C, &LinkCapability)) &&
      ((LinkCapability & MODERN_UI_PCIE_LINK_CAP_ASPM_MASK) != 0))
  {
    Summary->AspmCapableLinkCount++;
  }

  if (!EFI_ERROR (PciRead32 (PciIo, (UINT32)CapOffset + 0x14, &SlotCapability)) &&
      ((SlotCapability & MODERN_UI_PCIE_SLOT_CAP_HOTPLUG) != 0))
  {
    Summary->HotPlugPortCount++;
  }
}

/**
  Map a PCI base class code to a short display label.

  @param[in] BaseClass  PCI base class code (config space offset 0x0B).

  @return Non-NULL static short label, e.g. L"Storage".
**/
STATIC
CONST CHAR16 *
PcieClassName (
  IN UINT8  BaseClass
  )
{
  switch (BaseClass) {
    case 0x00:
      return L"Legacy";
    case 0x01:
      return L"Storage";
    case 0x02:
      return L"Network";
    case 0x03:
      return L"Display";
    case 0x04:
      return L"Multimedia";
    case 0x05:
      return L"Memory";
    case 0x06:
      return L"Bridge";
    case 0x07:
      return L"Comm";
    case 0x08:
      return L"System";
    case 0x09:
      return L"Input";
    case 0x0B:
      return L"Processor";
    case 0x0C:
      return L"Serial bus";
    case 0x0D:
      return L"Wireless";
    case 0x10:
      return L"Crypto";
    case 0x12:
      return L"Accelerator";
    default:
      return L"Device";
  }
}

/**
  Capture read-only identity for one PCI(e) device into the summary's device
  list. Reads location, vendor/device ID, class code, and the negotiated PCIe
  link (speed/width) from the Link Status register. Skips empty slots
  (vendor 0x0000/0xFFFF) and stops once MODERN_UI_PCIE_MAX_DEVICES are recorded.

  @param[in]      PciIo    PCI I/O protocol. Must not be NULL.
  @param[in,out]  Summary  Summary to append the device to. Must not be NULL.
**/
STATIC
VOID
RecordPciDevice (
  IN     EFI_PCI_IO_PROTOCOL     *PciIo,
  IN OUT MODERN_UI_PCIE_SUMMARY  *Summary
  )
{
  EFI_STATUS             Status;
  MODERN_UI_PCIE_DEVICE  *Entry;
  UINTN                  Seg;
  UINTN                  Bus;
  UINTN                  Dev;
  UINTN                  Func;
  UINT16                 VendorId;
  UINT16                 DeviceId;
  UINT8                  SubClass;
  UINT8                  BaseClass;
  UINT8                  CapOffset;
  UINT16                 LinkStatus;
  CHAR16                 LinkText[16];

  if ((PciIo == NULL) || (Summary == NULL) || (Summary->DeviceCount >= MODERN_UI_PCIE_MAX_DEVICES)) {
    return;
  }

  Seg    = 0;
  Bus    = 0;
  Dev    = 0;
  Func   = 0;
  Status = PciIo->GetLocation (PciIo, &Seg, &Bus, &Dev, &Func);
  if (EFI_ERROR (Status)) {
    return;
  }

  VendorId = 0xFFFF;
  DeviceId = 0xFFFF;
  PciRead16 (PciIo, PCI_VENDOR_ID_OFFSET, &VendorId);
  PciRead16 (PciIo, PCI_DEVICE_ID_OFFSET, &DeviceId);
  if ((VendorId == 0xFFFF) || (VendorId == 0x0000)) {
    return;
  }

  //
  // Class code occupies offsets 0x09 (ProgIf), 0x0A (SubClass), 0x0B (BaseClass).
  //
  SubClass  = 0;
  BaseClass = 0;
  PciRead8 (PciIo, 0x0A, &SubClass);
  PciRead8 (PciIo, 0x0B, &BaseClass);

  Entry = &Summary->Devices[Summary->DeviceCount];
  ZeroMem (Entry, sizeof (*Entry));
  Entry->Segment   = (UINT16)Seg;
  Entry->Bus       = (UINT8)Bus;
  Entry->Device    = (UINT8)Dev;
  Entry->Function  = (UINT8)Func;
  Entry->VendorId  = VendorId;
  Entry->DeviceId  = DeviceId;
  Entry->SubClass  = SubClass;
  Entry->BaseClass = BaseClass;

  //
  // Negotiated link from the PCIe Link Status register (capability + 0x12):
  // bits[3:0] current link speed (1..n => Gen1..GenN), bits[9:4] negotiated width.
  //
  LinkText[0] = L'\0';
  if (FindPciExpressCapability (PciIo, &CapOffset)) {
    LinkStatus = 0;
    if (!EFI_ERROR (PciRead16 (PciIo, (UINT32)CapOffset + 0x12, &LinkStatus))) {
      Entry->LinkSpeed = (UINT8)(LinkStatus & 0x0F);
      Entry->LinkWidth = (UINT8)((LinkStatus >> 4) & 0x3F);
      if ((Entry->LinkSpeed > 0) && (Entry->LinkWidth > 0)) {
        UnicodeSPrint (LinkText, sizeof (LinkText), L" Gen%u x%u", Entry->LinkSpeed, Entry->LinkWidth);
      }
    }
  }

  UnicodeSPrint (
    Entry->Label,
    sizeof (Entry->Label),
    L"%02x:%02x.%x %04x:%04x %s%s",
    Entry->Bus,
    Entry->Device,
    Entry->Function,
    Entry->VendorId,
    Entry->DeviceId,
    PcieClassName (Entry->BaseClass),
    LinkText
    );

  Summary->DeviceCount++;
}

/**
  Scan PCIe extended capabilities with a bounded read-only walker.

  @param[in]      PciIo    PCI I/O protocol. Must not be NULL.
  @param[in,out]  Summary  Summary counters to update. Must not be NULL.
**/
STATIC
VOID
ScanPciExpressExtendedCapabilities (
  IN     EFI_PCI_IO_PROTOCOL      *PciIo,
  IN OUT MODERN_UI_PCIE_SUMMARY   *Summary
  )
{
  EFI_STATUS  Status;
  UINT32      Offset;
  UINT32      Header;
  UINT16      CapabilityId;
  UINT32      NextOffset;
  UINTN       Guard;
  BOOLEAN     ResizableBarFound;
  BOOLEAN     SriovFound;
  BOOLEAN     AcsFound;
  BOOLEAN     AriFound;

  if ((PciIo == NULL) || (Summary == NULL)) {
    return;
  }

  ResizableBarFound = FALSE;
  SriovFound        = FALSE;
  AcsFound          = FALSE;
  AriFound          = FALSE;
  Offset            = EFI_PCIE_CAPABILITY_BASE_OFFSET;
  for (Guard = 0; (Offset >= EFI_PCIE_CAPABILITY_BASE_OFFSET) &&
                  (Offset + sizeof (UINT32) <= MODERN_UI_PCIE_EXTENDED_CONFIG_LIMIT) &&
                  (Guard < MODERN_UI_PCIE_EXT_CAPABILITY_GUARD); Guard++)
  {
    Header = 0;
    Status = PciRead32 (PciIo, Offset, &Header);
    if (EFI_ERROR (Status) || (Header == 0) || (Header == MAX_UINT32)) {
      break;
    }

    CapabilityId = (UINT16)(Header & 0xFFFF);
    switch (CapabilityId) {
      case PCI_EXPRESS_EXTENDED_CAPABILITY_RESIZABLE_BAR_ID:
      case PCI_EXPRESS_EXTENDED_CAPABILITY_VF_RESIZABLE_BAR_ID:
        ResizableBarFound = TRUE;
        break;
      case PCI_EXPRESS_EXTENDED_CAPABILITY_SRIOV_ID:
        SriovFound = TRUE;
        break;
      case PCI_EXPRESS_EXTENDED_CAPABILITY_ACS_EXTENDED_ID:
        AcsFound = TRUE;
        break;
      case PCI_EXPRESS_EXTENDED_CAPABILITY_ARI_CAPABILITY_ID:
        AriFound = TRUE;
        break;
      default:
        break;
    }

    NextOffset = (Header >> 20) & 0xFFF;
    NextOffset &= (UINT32)~0x3;
    if ((NextOffset == 0) || (NextOffset <= Offset)) {
      break;
    }

    Offset = NextOffset;
  }

  if (ResizableBarFound) {
    Summary->ResizableBarDeviceCount++;
  }

  if (SriovFound) {
    Summary->SriovDeviceCount++;
  }

  if (AcsFound) {
    Summary->AcsDeviceCount++;
  }

  if (AriFound) {
    Summary->AriDeviceCount++;
  }
}

/**
  Collect read-only PCIe controller inventory and capability counters.

  @param[in,out] Summary  Summary counters to update. Must not be NULL.
**/
STATIC
VOID
CollectPciInventory (
  IN OUT MODERN_UI_PCIE_SUMMARY  *Summary
  )
{
  EFI_STATUS           Status;
  EFI_HANDLE           *Handles;
  UINTN                HandleCount;
  UINTN                HandleIndex;
  EFI_PCI_IO_PROTOCOL  *PciIo;
  UINT8                HeaderType;

  if (Summary == NULL) {
    return;
  }

  Handles     = NULL;
  HandleCount = 0;
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiPciIoProtocolGuid,
                  NULL,
                  &HandleCount,
                  &Handles
                  );
  if (EFI_ERROR (Status)) {
    return;
  }

  Summary->ControllerCount = HandleCount;
  for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex++) {
    PciIo = NULL;
    Status = gBS->HandleProtocol (Handles[HandleIndex], &gEfiPciIoProtocolGuid, (VOID **)&PciIo);
    if (EFI_ERROR (Status) || (PciIo == NULL)) {
      continue;
    }

    HeaderType = 0;
    Status = PciRead8 (PciIo, PCI_HEADER_TYPE_OFFSET, &HeaderType);
    if (!EFI_ERROR (Status)) {
      if (((HeaderType & HEADER_LAYOUT_CODE) == HEADER_TYPE_PCI_TO_PCI_BRIDGE) ||
          ((HeaderType & HEADER_LAYOUT_CODE) == HEADER_TYPE_CARDBUS_BRIDGE))
      {
        Summary->BridgeCount++;
      } else {
        Summary->EndpointCount++;
      }
    }

    ScanPciExpressCapability (PciIo, Summary);
    ScanPciExpressExtendedCapabilities (PciIo, Summary);
    RecordPciDevice (PciIo, Summary);
  }

  if (Handles != NULL) {
    FreePool (Handles);
  }
}

/**
  Collect read-only PCIe inventory, protocol presence, and policy-entry hints.

  @param[out] Summary  PCIe summary to fill. Must not be NULL.

  @retval EFI_SUCCESS            Summary was filled.
  @retval EFI_INVALID_PARAMETER  Summary is NULL.
**/
EFI_STATUS
EFIAPI
ModernUiPcieDataGetSummary (
  OUT MODERN_UI_PCIE_SUMMARY  *Summary
  )
{
  STATIC CONST CHAR16  *mPcieKeywords[] = {
    L"PCIe",
    L"PCI Express",
    L"PCI"
  };
  STATIC CONST CHAR16  *mResizeBarKeywords[] = {
    L"Resizable BAR",
    L"Re-Size BAR",
    L"Resize BAR",
    L"Above 4G Decoding"
  };
  STATIC CONST CHAR16  *mAbove4GKeywords[] = {
    L"Above 4G",
    L"4G Decoding",
    L"64-bit BAR"
  };
  STATIC CONST CHAR16  *mSriovKeywords[] = {
    L"SR-IOV",
    L"Sriov",
    L"Single Root I/O Virtualization"
  };
  STATIC CONST CHAR16  *mAspmKeywords[] = {
    L"ASPM",
    L"Active State Power Management",
    L"L1 Substates"
  };
  STATIC CONST CHAR16  *mBifurcationKeywords[] = {
    L"Bifurcation",
    L"PCIe x4x4",
    L"PCIe x8x8"
  };
  STATIC CONST CHAR16  *mHotPlugKeywords[] = {
    L"Hot Plug",
    L"HotPlug",
    L"Thunderbolt"
  };
  STATIC CONST CHAR16  *mAcsKeywords[] = {
    L"ACS",
    L"Access Control Services"
  };
  STATIC CONST CHAR16  *mAriKeywords[] = {
    L"ARI",
    L"Alternative Routing-ID"
  };
  STATIC CONST CHAR16  *mIommuKeywords[] = {
    L"IOMMU",
    L"VT-d",
    L"AMD-Vi",
    L"DMA Remapping"
  };

  if (Summary == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (Summary, sizeof (*Summary));
  CollectPciInventory (Summary);
  Summary->RootBridgeCount                    = CountProtocolHandles (&gEfiPciRootBridgeIoProtocolGuid);
  Summary->RootBridgeIoPresent                = (BOOLEAN)(Summary->RootBridgeCount > 0);
  Summary->EnumerationCompleteProtocolPresent = IsProtocolPresent (&gEfiPciEnumerationCompleteProtocolGuid);
  Summary->HotPlugRequestProtocolPresent      = IsProtocolPresent (&gEfiPciHotPlugRequestProtocolGuid);
  Summary->HotPlugInitProtocolPresent         = IsProtocolPresent (&gEfiPciHotPlugInitProtocolGuid);
  Summary->IoMmuProtocolPresent               = IsProtocolPresent (&gEdkiiIoMmuProtocolGuid);
  Summary->PciePolicyEntryPresent             = HasHiiFormsetKeyword (mPcieKeywords, ARRAY_SIZE (mPcieKeywords));
  Summary->ResizeBarPolicyEntryPresent        = HasHiiFormsetKeyword (mResizeBarKeywords, ARRAY_SIZE (mResizeBarKeywords));
  Summary->Above4GPolicyEntryPresent          = HasHiiFormsetKeyword (mAbove4GKeywords, ARRAY_SIZE (mAbove4GKeywords));
  Summary->SriovPolicyEntryPresent            = HasHiiFormsetKeyword (mSriovKeywords, ARRAY_SIZE (mSriovKeywords));
  Summary->AspmPolicyEntryPresent             = HasHiiFormsetKeyword (mAspmKeywords, ARRAY_SIZE (mAspmKeywords));
  Summary->BifurcationPolicyEntryPresent      = HasHiiFormsetKeyword (mBifurcationKeywords, ARRAY_SIZE (mBifurcationKeywords));
  Summary->HotPlugPolicyEntryPresent          = HasHiiFormsetKeyword (mHotPlugKeywords, ARRAY_SIZE (mHotPlugKeywords));
  Summary->AcsPolicyEntryPresent              = HasHiiFormsetKeyword (mAcsKeywords, ARRAY_SIZE (mAcsKeywords));
  Summary->AriPolicyEntryPresent              = HasHiiFormsetKeyword (mAriKeywords, ARRAY_SIZE (mAriKeywords));
  Summary->IommuPolicyEntryPresent            = HasHiiFormsetKeyword (mIommuKeywords, ARRAY_SIZE (mIommuKeywords));
  return EFI_SUCCESS;
}
