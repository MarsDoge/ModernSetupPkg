/** @file
  Modern graphical setup application prototype.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "ModernSetupAppInternal.h"

/**
  Draw one provider-summary label/value row.

  @param[in] Ui     Initialized render context. Must not be NULL.
  @param[in] Theme  Theme token table. Must not be NULL.
  @param[in] X      Left coordinate in pixels.
  @param[in] Y      Top coordinate in pixels.
  @param[in] Width  Available row width in pixels.
  @param[in] Label  Label text. Must not be NULL.
  @param[in] Value  Value text. Must not be NULL.
**/
STATIC
VOID
DrawProviderSummaryInfoRow (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN UINTN                     X,
  IN UINTN                     Y,
  IN UINTN                     Width,
  IN CONST CHAR16              *Label,
  IN CONST CHAR16              *Value
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Background;
  UINTN  LabelWidth;
  UINTN  ValueX;

  if (Width < 32) {
    return;
  }

  Background = ModernUiBlendColor (Theme->Surface, Theme->BackgroundBlack, 30);
  LabelWidth = (Width > 240) ? 180 : (Width / 2);
  ValueX     = X + LabelWidth;
  ModernUiDrawTextFit (Ui, X, Y, LabelWidth - 8, Label, Theme->MutedText, Background);
  ModernUiDrawTextFit (Ui, ValueX, Y, (Width > LabelWidth) ? (Width - LabelWidth) : Width, Value, Theme->Text, Background);
}

/**
  Draw a subtle provider-summary section surface.

  @param[in] Ui      Initialized render context. Must not be NULL.
  @param[in] Theme   Theme token table. Must not be NULL.
  @param[in] Rect    Section rectangle in pixels.
  @param[in] Title   Section title text. Must not be NULL.
  @param[in] Accent  TRUE to draw a stronger top accent line.
**/
STATIC
VOID
DrawProviderSummarySection (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN MODERN_UI_RECT            Rect,
  IN CONST CHAR16              *Title,
  IN BOOLEAN                   Accent
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  PanelColor;

  PanelColor = ModernUiBlendColor (Theme->Surface, Theme->BackgroundBlack, 30);
  ModernUiFillRect (Ui, Rect, PanelColor);
  ModernUiStrokeRect (Ui, Rect, Theme->Border);
  ModernUiFillRect (
    Ui,
    (MODERN_UI_RECT){ Rect.X, Rect.Y, Rect.Width, Accent ? 2 : 1 },
    Accent ? Theme->AccentOrange : ModernUiBlendColor (Theme->Border, Theme->BackgroundBlack, 40)
    );
  ModernUiDrawTextFit (Ui, Rect.X + 18, Rect.Y + 16, Rect.Width - 36, Title, Accent ? Theme->AccentYellow : Theme->MutedText, PanelColor);
}

/**
  Draw a lightweight provider-page subsection label.

  @param[in] Ui     Initialized render context. Must not be NULL.
  @param[in] Theme  Theme token table. Must not be NULL.
  @param[in] X      Left coordinate in pixels.
  @param[in] Y      Top coordinate in pixels.
  @param[in] Width  Available label width in pixels.
  @param[in] Label  Subsection label text. Must not be NULL.
**/
STATIC
VOID
DrawProviderSubsectionHeader (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN UINTN                     X,
  IN UINTN                     Y,
  IN UINTN                     Width,
  IN CONST CHAR16              *Label
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Background;

  if (Width < 32) {
    return;
  }

  Background = ModernUiBlendColor (Theme->Surface, Theme->BackgroundBlack, 30);
  ModernUiFillRect (Ui, (MODERN_UI_RECT){ X, Y + 18, Width, 1 }, Theme->Border);
  ModernUiDrawTextFit (Ui, X, Y, Width, Label, Theme->WarningText, Background);
}

/**
  Return localized capability text for a boolean provider state.

  @param[in] Present  TRUE when the capability is available.

  @return Non-NULL localized capability text.
**/
STATIC
CONST CHAR16 *
CapabilityText (
  IN BOOLEAN  Present
  )
{
  return Present ? ModernUiGetString (ModernUiStringAvailable) : ModernUiGetString (ModernUiStringNotAvailable);
}

/**
  Return localized text for one security provider state.

  @param[in] State  Security state to render.

  @return Non-NULL localized status text.
**/
STATIC
CONST CHAR16 *
SecurityStateText (
  IN MODERN_UI_SECURITY_STATE  State
  )
{
  switch (State) {
    case ModernUiSecurityStatePresent:
      return ModernUiGetString (ModernUiStringPresent);
    case ModernUiSecurityStateAbsent:
      return ModernUiGetString (ModernUiStringAbsent);
    case ModernUiSecurityStateEnabled:
      return ModernUiGetString (ModernUiStringEnabled);
    case ModernUiSecurityStateDisabled:
      return ModernUiGetString (ModernUiStringDisabled);
    default:
      return ModernUiGetString (ModernUiStringUnknown);
  }
}

/**
  Draw a provider summary page with one section and a row list.

  @param[in] Ui        Initialized render context. Must not be NULL.
  @param[in] Theme     Theme token table. Must not be NULL.
  @param[in] Focus     Current focus area.
  @param[in] Section   Section title text. Must not be NULL.
  @param[in] Labels    Row label array. Must not be NULL when RowCount is nonzero.
  @param[in] Values    Row value array. Must not be NULL when RowCount is nonzero.
  @param[in] Groups    Optional group label array. Non-NULL entries add a
                       subsection header before the corresponding row.
  @param[in] RowCount  Number of rows in Labels and Values.
**/
STATIC
VOID
DrawProviderSummaryPage (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_FOCUS               Focus,
  IN CONST CHAR16              *Section,
  IN CONST CHAR16              **Labels,
  IN CONST CHAR16              **Values,
  IN CONST CHAR16              **Groups,
  IN UINTN                     RowCount
  )
{
  MODERN_UI_RECT  Content;
  MODERN_UI_RECT  Panel;
  UINTN           Index;
  UINTN           RowY;
  UINTN           RowStep;
  UINTN           HeaderStep;

  Content = ModernSetupContentRect (Ui);
  Panel      = (MODERN_UI_RECT){ Content.X, Content.Y, Content.Width, MIN (Content.Height, 430) };
  RowY       = Panel.Y + 58;
  RowStep    = (Panel.Height < 380) ? 28 : 34;
  HeaderStep = (Panel.Height < 380) ? 20 : 28;

  DrawProviderSummarySection (Ui, Theme, Panel, Section, TRUE);
  ModernUiDrawFocusFrame (Ui, Panel, (BOOLEAN)(Focus == SetupFocusContent), Theme);

  for (Index = 0; Index < RowCount; Index++) {
    if ((Groups != NULL) && (Groups[Index] != NULL)) {
      if ((RowY + 24) > (Panel.Y + Panel.Height)) {
        break;
      }

      DrawProviderSubsectionHeader (Ui, Theme, Panel.X + 22, RowY, Panel.Width - 44, Groups[Index]);
      RowY += HeaderStep;
    }

    if ((RowY + 24) > (Panel.Y + Panel.Height)) {
      break;
    }

    DrawProviderSummaryInfoRow (
      Ui,
      Theme,
      Panel.X + 22,
      RowY,
      Panel.Width - 44,
      Labels[Index],
      Values[Index]
      );
    RowY += RowStep;
  }
}

/**
  Draw the Boot page with launchable BootOrder entries.

  @param[in] Ui        Initialized render context. Must not be NULL.
  @param[in] Theme     Theme token table. Must not be NULL.
  @param[in] Focus     Current focus area.
  @param[in] Selected  Selected BootOrder row.
**/
STATIC
VOID
DrawBoot (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_FOCUS               Focus,
  IN UINTN                     Selected
  )
{
  EFI_STATUS                    Status;
  MODERN_UI_BOOT_OPTION         *BootOptions;
  UINTN                         BootOptionCount;
  UINTN                         Index;
  UINTN                         Y;
  CHAR16                        Line[160];
  CHAR16                        Value[96];
  CONST CHAR16                  *State;
  BOOLEAN                       IsSelected;
  MODERN_UI_RECT                Panel;
  UINTN                         RowX;
  UINTN                         RowWidth;
  UINTN                         MaxRows;
  MODERN_UI_ROW_MODEL           RowModel;

  Panel = ModernSetupContentRect (Ui);
  RowX = Panel.X + 20;
  RowWidth = Panel.Width - 40;
  ModernUiDrawPanel (Ui, Panel, Theme);
  ModernUiDrawFocusFrame (Ui, Panel, (BOOLEAN)(Focus == SetupFocusContent), Theme);
  ModernUiDrawText (Ui, Panel.X + 20, Panel.Y + 20, ModernUiGetString (ModernUiStringBootInstruction), Theme->MutedText, Theme->Surface);

  BootOptions = NULL;
  Status = ModernUiBootDataGetOptions (mModernSetupImageHandle, &BootOptions, &BootOptionCount);
  if (EFI_ERROR (Status) || (BootOptions == NULL)) {
    ModernUiDrawText (Ui, Panel.X + 20, Panel.Y + 66, ModernUiGetString (ModernUiStringNoBootOptions), Theme->Warning, Theme->Surface);
    return;
  }

  MaxRows = (Panel.Height > 96) ? ((Panel.Height - 92) / 58) : 0;
  MaxRows = MIN (MaxRows, MAX_BOOT_ROWS);
  for (Index = 0; (Index < BootOptionCount) && (Index < MaxRows); Index++) {
    Y           = Panel.Y + 62 + Index * 58;
    State       = BootOptions[Index].Active ? ModernUiGetString (ModernUiStringActive) : ModernUiGetString (ModernUiStringInactive);
    IsSelected  = (BOOLEAN)((Focus == SetupFocusContent) && (Index == Selected));
    UnicodeSPrint (
      Line,
      sizeof (Line),
      L"%02u  Boot%04x  %s",
      Index + 1,
      BootOptions[Index].OptionNumber,
      BootOptions[Index].Description
      );
    UnicodeSPrint (
      Value,
      sizeof (Value),
      L"%s%s%s",
      State,
      BootOptions[Index].Hidden ? L" / Hidden / " : L" / ",
      BootOptions[Index].Category
      );
    RowModel.Rect      = (MODERN_UI_RECT){ RowX, Y - 8, RowWidth, 42 };
    RowModel.Prompt    = Line;
    RowModel.Value     = Value;
    RowModel.Role      = IsSelected ? ModernUiRowSelected : ModernUiRowNormal;
    RowModel.ValueType = ModernUiValueText;
    ModernUiEngineDrawRows (Ui, &RowModel, 1, Theme);
    ModernUiDrawTextFit (
      Ui,
      RowX + 20,
      Y + 18,
      RowWidth - 40,
      BootOptions[Index].FilePathSummary,
      Theme->MutedText,
      IsSelected ? Theme->SelectedBand : Theme->Surface
      );
  }

  if (BootOptionCount == 0) {
    ModernUiDrawText (Ui, Panel.X + 20, Panel.Y + 66, ModernUiGetString (ModernUiStringNoBootOptions), Theme->Warning, Theme->Surface);
  }

  ModernUiBootDataFreeOptions (BootOptions, BootOptionCount);
}

/**
  Draw the Devices page with a small handle/device-path inventory.

  @param[in] Ui        Initialized render context. Must not be NULL.
  @param[in] Theme     Theme token table. Must not be NULL.
  @param[in] Focus     Current focus area.
  @param[in] Selected  Selected device-path row.
**/
STATIC
VOID
DrawDevices (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_FOCUS               Focus,
  IN UINTN                     Selected
  )
{
  EFI_STATUS                Status;
  MODERN_UI_DEVICE_ENTRY    *Entries;
  UINTN                     EntryCount;
  UINTN                     Index;
  UINTN                     HiiCount;
  CHAR16                    Line[168];
  CHAR16                    Summary[96];
  BOOLEAN                   IsSelected;
  MODERN_UI_RECT            Panel;
  UINTN                     RowX;
  UINTN                     RowWidth;
  MODERN_UI_ROW_MODEL     RowModel;

  Panel = ModernSetupContentRect (Ui);
  RowX = Panel.X + 20;
  RowWidth = Panel.Width - 40;
  ModernUiDrawPanel (Ui, Panel, Theme);
  ModernUiDrawFocusFrame (Ui, Panel, (BOOLEAN)(Focus == SetupFocusContent), Theme);

  Entries = NULL;
  Status = ModernUiDeviceDataGetEntries (&Entries, &EntryCount);
  if (EFI_ERROR (Status)) {
    ModernUiDrawText (Ui, 280, 150, ModernUiGetString (ModernUiStringUnableEnumerateHandles), Theme->Warning, Theme->Surface);
    return;
  }

  HiiCount = 0;
  for (Index = 0; Index < EntryCount; Index++) {
    if (Entries[Index].HasForm) {
      HiiCount++;
    }
  }

  UnicodeSPrint (Summary, sizeof (Summary), L"%u entries (%u HII, %u device)", EntryCount, HiiCount, EntryCount - HiiCount);
  ModernUiDrawText (Ui, Panel.X + 20, Panel.Y + 20, Summary, Theme->MutedText, Theme->Surface);

  for (Index = 0; (Index < EntryCount) && (Index < MAX_DEVICE_ROWS); Index++) {
    UnicodeSPrint (Line, sizeof (Line), L"%02u  %s", Index + 1, Entries[Index].Title);
    IsSelected = (BOOLEAN)((Focus == SetupFocusContent) && (Index == Selected));
    RowModel.Rect      = (MODERN_UI_RECT){ RowX, Panel.Y + 54 + Index * 36, RowWidth, 30 };
    RowModel.Prompt    = Line;
    RowModel.Value     = Entries[Index].HasForm ? L"HII >" : L"Device";
    RowModel.Role      = IsSelected ? ModernUiRowSelected : ModernUiRowNormal;
    RowModel.ValueType = Entries[Index].HasForm ? ModernUiValueAction : ModernUiValueText;
    ModernUiEngineDrawRows (Ui, &RowModel, 1, Theme);
  }

  if (EntryCount == 0) {
    ModernUiDrawText (Ui, Panel.X + 20, Panel.Y + 66, L"No HII formsets found.", Theme->Warning, Theme->Surface);
  }

  ModernUiDeviceDataFreeEntries (Entries, EntryCount);
}


/**
  Draw the Security page with read-only Secure Boot state.

  @param[in] Ui     Initialized render context. Must not be NULL.
  @param[in] Theme  Theme token table. Must not be NULL.
  @param[in] Focus  Current focus area.
**/
STATIC
VOID
DrawSecurity (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_FOCUS               Focus
  )
{
  MODERN_SETUP_PROVIDER_SNAPSHOT  Providers;
  MODERN_UI_SECURITY_SUMMARY       *Summary;
  MODERN_UI_RECT                   Panel;
  CONST CHAR16                     *SecureBootText;
  CONST CHAR16    *SetupModeText;
  CONST CHAR16    *PkText;
  CONST CHAR16    *KekText;
  CONST CHAR16    *DbText;
  CONST CHAR16    *DbxText;
  CONST CHAR16    *Tcg2Text;
  CONST CHAR16    *TreeText;

  ModernSetupGetProviderSnapshot (&Providers);
  Summary = &Providers.Security;

  SecureBootText = (Summary->SecureBoot == ModernUiSecurityStateEnabled) ? ModernUiGetString (ModernUiStringEnabled) :
                   ((Summary->SecureBoot == ModernUiSecurityStateDisabled) ? ModernUiGetString (ModernUiStringDisabled) : ModernUiGetString (ModernUiStringUnknown));
  SetupModeText = (Summary->SetupMode == ModernUiSecurityStateEnabled) ? ModernUiGetString (ModernUiStringEnabled) :
                  ((Summary->SetupMode == ModernUiSecurityStateDisabled) ? ModernUiGetString (ModernUiStringDisabled) : ModernUiGetString (ModernUiStringUnknown));
  PkText   = SecurityStateText (Summary->PlatformKey);
  KekText  = SecurityStateText (Summary->KeyExchangeKey);
  DbText   = SecurityStateText (Summary->SignatureDb);
  DbxText  = SecurityStateText (Summary->ForbiddenSignatureDb);
  Tcg2Text = SecurityStateText (Summary->Tcg2Protocol);
  TreeText = SecurityStateText (Summary->TreeProtocol);
  Panel = ModernSetupContentRect (Ui);
  ModernUiDrawPanel (Ui, Panel, Theme);
  ModernUiDrawFocusFrame (Ui, Panel, (BOOLEAN)(Focus == SetupFocusContent), Theme);
  ModernUiDrawText (Ui, Panel.X + 20, Panel.Y + 24, ModernUiGetString (ModernUiStringSecureBoot), Theme->MutedText, Theme->Surface);
  ModernUiDrawText (Ui, Panel.X + 20, Panel.Y + 64, SecureBootText, (Summary->SecureBoot == ModernUiSecurityStateEnabled) ? Theme->Success : Theme->Warning, Theme->Surface);
  ModernUiDrawTextFormatted (Ui, Panel.X + 20, Panel.Y + 104, Theme->MutedText, Theme->Surface, L"Setup Mode: %s", SetupModeText);
  ModernUiDrawTextFormatted (Ui, Panel.X + 20, Panel.Y + 136, Theme->MutedText, Theme->Surface, L"PK: %s    KEK: %s", PkText, KekText);
  ModernUiDrawTextFormatted (Ui, Panel.X + 20, Panel.Y + 168, Theme->MutedText, Theme->Surface, L"db: %s    dbx: %s", DbText, DbxText);
  ModernUiDrawTextFormatted (Ui, Panel.X + 20, Panel.Y + 200, Theme->MutedText, Theme->Surface, L"TCG2: %s    TrEE: %s", Tcg2Text, TreeText);
  ModernUiDrawText (Ui, Panel.X + 20, Panel.Y + 252, ModernUiGetString (ModernUiStringSecurityReadOnly), Theme->MutedText, Theme->Surface);
}

/**
  Draw the Firmware page with read-only update and capsule state.

  @param[in] Ui     Initialized render context. Must not be NULL.
  @param[in] Theme  Theme token table. Must not be NULL.
  @param[in] Focus  Current focus area.
**/
STATIC
VOID
DrawFirmware (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_FOCUS               Focus
  )
{
  MODERN_SETUP_PROVIDER_SNAPSHOT  Providers;
  MODERN_UI_FIRMWARE_SUMMARY      *Summary;
  CONST CHAR16                    *Labels[5];
  CONST CHAR16                    *Values[5];
  CONST CHAR16                    *Groups[5] = { NULL };

  ModernSetupGetProviderSnapshot (&Providers);
  Summary = &Providers.Firmware;

  Labels[0] = ModernUiGetString (ModernUiStringFirmwareVendor);
  Groups[0] = ModernUiGetString (ModernUiStringGroupFirmware);
  Values[0] = Summary->Vendor;
  Labels[1] = ModernUiGetString (ModernUiStringFirmwareRevision);
  Values[1] = Summary->Revision;
  Labels[2] = ModernUiGetString (ModernUiStringCapsuleRuntime);
  Values[2] = CapabilityText (Summary->CapsuleRuntimeServices);
  Labels[3] = ModernUiGetString (ModernUiStringCapsuleProtocol);
  Values[3] = CapabilityText (Summary->CapsuleArchProtocol);
  Labels[4] = ModernUiGetString (ModernUiStringCapsuleReport);
  Values[4] = Summary->CapsuleReportPresent ? ModernUiGetString (ModernUiStringPresent) : ModernUiGetString (ModernUiStringNotAvailable);

  DrawProviderSummaryPage (
    Ui,
    Theme,
    Focus,
    ModernUiGetString (ModernUiStringFirmwareUpdate),
    Labels,
    Values,
    Groups,
    ARRAY_SIZE (Labels)
    );
}

/**
  Draw the Diagnostics page with read-only bring-up and table state.

  @param[in] Ui     Initialized render context. Must not be NULL.
  @param[in] Theme  Theme token table. Must not be NULL.
  @param[in] Focus  Current focus area.
**/
STATIC
VOID
DrawDiagnostics (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_FOCUS               Focus
  )
{
  MODERN_SETUP_PROVIDER_SNAPSHOT        Providers;
  MODERN_SETUP_PROVIDER_HEALTH_SUMMARY  ProviderHealth;
  MODERN_UI_DIAGNOSTICS_SUMMARY         *Summary;
  CHAR16                                MemoryMap[48];
  CHAR16                                Handles[48];
  CHAR16                                Tables[48];
  CHAR16                                ProviderCoverage[48];
  CHAR16                                ProviderIssue[96];
  CONST CHAR16                          *Labels[8];
  CONST CHAR16                          *Values[8];
  CONST CHAR16                          *Groups[8] = { NULL };

  ModernSetupGetProviderSnapshot (&Providers);
  ModernSetupGetProviderHealthSummary (&Providers, &ProviderHealth);
  Summary = &Providers.Diagnostics;

  UnicodeSPrint (MemoryMap, sizeof (MemoryMap), L"%u", Summary->MemoryDescriptorCount);
  UnicodeSPrint (Handles, sizeof (Handles), L"%u", Summary->HandleCount);
  UnicodeSPrint (Tables, sizeof (Tables), L"%u", Summary->ConfigurationTableCount);
  UnicodeSPrint (ProviderCoverage, sizeof (ProviderCoverage), L"%u/%u ready", ProviderHealth.ReadyProviders, ProviderHealth.TotalProviders);
  if (ProviderHealth.State == ModernSetupProviderHealthReady) {
    UnicodeSPrint (ProviderIssue, sizeof (ProviderIssue), L"None");
  } else {
    UnicodeSPrint (ProviderIssue, sizeof (ProviderIssue), L"%s (%r)", ProviderHealth.FirstIssueName, ProviderHealth.FirstIssueStatus);
  }

  Labels[0] = L"Provider Health";
  Groups[0] = ModernUiGetString (ModernUiStringGroupDiagnostics);
  Values[0] = ModernSetupGetProviderHealthStateText (ProviderHealth.State);
  Labels[1] = L"Provider Coverage";
  Values[1] = ProviderCoverage;
  Labels[2] = L"Provider Issue";
  Values[2] = ProviderIssue;
  Labels[3] = ModernUiGetString (ModernUiStringAcpiTables);
  Groups[3] = ModernUiGetString (ModernUiStringGroupPlatformHealth);
  Values[3] = CapabilityText (Summary->AcpiPresent);
  Labels[4] = ModernUiGetString (ModernUiStringSmbiosTables);
  Values[4] = CapabilityText (Summary->SmbiosPresent);
  Labels[5] = ModernUiGetString (ModernUiStringMemoryMap);
  Values[5] = MemoryMap;
  Labels[6] = ModernUiGetString (ModernUiStringDxeHandles);
  Values[6] = Handles;
  Labels[7] = ModernUiGetString (ModernUiStringConfigurationTables);
  Values[7] = Tables;

  DrawProviderSummaryPage (
    Ui,
    Theme,
    Focus,
    ModernUiGetString (ModernUiStringDiagnosticsLogs),
    Labels,
    Values,
    Groups,
    ARRAY_SIZE (Labels)
    );
}

/**
  Draw the Management page with read-only BMC/IPMI/Redfish state.

  @param[in] Ui     Initialized render context. Must not be NULL.
  @param[in] Theme  Theme token table. Must not be NULL.
  @param[in] Focus  Current focus area.
**/
STATIC
VOID
DrawManagement (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_FOCUS               Focus
  )
{
  MODERN_SETUP_PROVIDER_SNAPSHOT  Providers;
  MODERN_UI_MANAGEMENT_SUMMARY    *Summary;
  CONST CHAR16                    *Labels[3];
  CONST CHAR16                    *Values[3];
  CONST CHAR16                    *Groups[3] = { NULL };

  ModernSetupGetProviderSnapshot (&Providers);
  Summary = &Providers.Management;

  Labels[0] = ModernUiGetString (ModernUiStringIpmi);
  Groups[0] = ModernUiGetString (ModernUiStringGroupManagement);
  Values[0] = CapabilityText (Summary->IpmiProtocolPresent);
  Labels[1] = ModernUiGetString (ModernUiStringRedfish);
  Values[1] = CapabilityText (Summary->RedfishDiscoverPresent);
  Labels[2] = ModernUiGetString (ModernUiStringManagementInterface);
  Values[2] = CapabilityText (Summary->SmbiosManagementInterfacePresent);

  DrawProviderSummaryPage (
    Ui,
    Theme,
    Focus,
    ModernUiGetString (ModernUiStringManagement),
    Labels,
    Values,
    Groups,
    ARRAY_SIZE (Labels)
    );
}

/**
  Draw the Power page with read-only ACPI and thermal provider state.

  @param[in] Ui     Initialized render context. Must not be NULL.
  @param[in] Theme  Theme token table. Must not be NULL.
  @param[in] Focus  Current focus area.
**/
STATIC
VOID
DrawPower (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_FOCUS               Focus
  )
{
  MODERN_SETUP_PROVIDER_SNAPSHOT  Providers;
  MODERN_UI_POWER_SUMMARY         *Summary;
  CONST CHAR16                    *Labels[5];
  CONST CHAR16                    *Values[5];
  CONST CHAR16                    *Groups[5] = { NULL };

  ModernSetupGetProviderSnapshot (&Providers);
  Summary = &Providers.Power;

  Labels[0] = ModernUiGetString (ModernUiStringAcpiTablesProvider);
  Groups[0] = ModernUiGetString (ModernUiStringGroupPower);
  Values[0] = CapabilityText (Summary->AcpiTablePresent);
  Labels[1] = ModernUiGetString (ModernUiStringAcpiSdtProtocol);
  Values[1] = CapabilityText (Summary->AcpiSdtProtocolPresent);
  Labels[2] = ModernUiGetString (ModernUiStringChassisThermalState);
  Values[2] = Summary->ChassisThermalState;
  Labels[3] = ModernUiGetString (ModernUiStringPowerSupply);
  Groups[3] = ModernUiGetString (ModernUiStringGroupDiagnostics);
  Values[3] = CapabilityText (Summary->SmbiosPowerSupplyPresent);
  Labels[4] = ModernUiGetString (ModernUiStringSmbiosTables);
  Values[4] = CapabilityText (Summary->SmbiosChassisPresent);

  DrawProviderSummaryPage (
    Ui,
    Theme,
    Focus,
    ModernUiGetString (ModernUiStringPowerThermal),
    Labels,
    Values,
    Groups,
    ARRAY_SIZE (Labels)
    );
}

/**
  Draw the Performance page with read-only tuning provider availability.

  @param[in] Ui     Initialized render context. Must not be NULL.
  @param[in] Theme  Theme token table. Must not be NULL.
  @param[in] Focus  Current focus area.
**/
STATIC
VOID
DrawPerformance (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_FOCUS               Focus
  )
{
  MODERN_SETUP_PROVIDER_SNAPSHOT    Providers;
  MODERN_UI_PERFORMANCE_SUMMARY     *Summary;
  MODERN_UI_PCIE_SUMMARY            *Pcie;
  CHAR16                            PcieInventory[64];
  CHAR16                            PciePolicy[96];
  CONST CHAR16                      *Labels[8];
  CONST CHAR16                      *Values[8];
  CONST CHAR16                      *Groups[8] = { NULL };

  ModernSetupGetProviderSnapshot (&Providers);
  Summary = &Providers.Performance;
  Pcie    = &Providers.Pcie;

  Labels[0] = ModernUiGetString (ModernUiStringProcessorInventory);
  Groups[0] = ModernUiGetString (ModernUiStringGroupPerformance);
  Values[0] = CapabilityText (Summary->ProcessorInventoryPresent);
  Labels[1] = ModernUiGetString (ModernUiStringMemoryInventory);
  Values[1] = CapabilityText (Summary->MemoryInventoryPresent);
  Labels[2] = ModernUiGetString (ModernUiStringCpuIo2);
  Values[2] = CapabilityText (Summary->CpuIo2ProtocolPresent);
  Labels[3] = ModernUiGetString (ModernUiStringVirtualizationPolicy);
  Values[3] = CapabilityText (Summary->VirtualizationPolicyEntryPresent);
  Labels[4] = ModernUiGetString (ModernUiStringRasPolicy);
  Values[4] = CapabilityText (Summary->RasPolicyEntryPresent);
  UnicodeSPrint (
    PcieInventory,
    sizeof (PcieInventory),
    L"%u endpoints / %u bridges",
    Pcie->EndpointCount,
    Pcie->BridgeCount
    );
  UnicodeSPrint (
    PciePolicy,
    sizeof (PciePolicy),
    L"ReBAR %s / Above 4G %s / SR-IOV %s",
    CapabilityText (Pcie->ResizeBarPolicyEntryPresent),
    CapabilityText (Pcie->Above4GPolicyEntryPresent),
    CapabilityText (Pcie->SriovPolicyEntryPresent)
    );
  Labels[5] = L"PCIe Policy";
  Groups[5] = ModernUiGetString (ModernUiStringGroupPowerPerformance);
  Values[5] = EFI_ERROR (Providers.PcieStatus) ? ModernUiGetString (ModernUiStringNotAvailable) : CapabilityText (Pcie->PciePolicyEntryPresent);
  Labels[6] = L"ReBAR / Above 4G / SR-IOV";
  Values[6] = EFI_ERROR (Providers.PcieStatus) ? ModernUiGetString (ModernUiStringNotAvailable) : PciePolicy;
  Labels[7] = L"PCIe Inventory";
  Values[7] = EFI_ERROR (Providers.PcieStatus) ? ModernUiGetString (ModernUiStringNotAvailable) : PcieInventory;

  DrawProviderSummaryPage (
    Ui,
    Theme,
    Focus,
    ModernUiGetString (ModernUiStringPerformanceTuning),
    Labels,
    Values,
    Groups,
    ARRAY_SIZE (Labels)
    );
}

/**
  Draw the Exit page and selected action.

  @param[in] Ui        Initialized render context. Must not be NULL.
  @param[in] Theme     Theme token table. Must not be NULL.
  @param[in] Focus     Current focus area.
  @param[in] Selected  Selected action index. Values 0..3 are expected.
**/
STATIC
VOID
DrawExit (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_FOCUS               Focus,
  IN UINTN                     Selected
  )
{
  CONST CHAR16  *Items[4];
  CONST CHAR16  *LanguageName;
  UINTN         Index;
  UINTN         Y;
  UINTN         ValueWidth;
  BOOLEAN       IsSelected;
  MODERN_UI_RECT  Panel;
  UINTN         RowX;
  UINTN         RowWidth;
  MODERN_UI_ROW_MODEL  RowModel;
  MODERN_UI_POPUP_MODEL  PopupModel;

  LanguageName = ModernSetupGetLanguageOptionName (ModernSetupGetActiveLanguageSelection ());

  Items[0] = ModernUiGetString (ModernUiStringExitContinue);
  Items[1] = ModernUiGetString (ModernUiStringExitClassicUi);
  Items[2] = ModernUiGetString (ModernUiStringExitReset);
  Items[3] = ModernUiGetString (ModernUiStringLanguageLabel);

  Panel = ModernSetupContentRect (Ui);
  RowX = Panel.X + 26;
  RowWidth = Panel.Width - 52;
  ValueWidth = 220;
  ModernUiDrawPanel (Ui, Panel, Theme);
  ModernUiDrawFocusFrame (Ui, Panel, (BOOLEAN)(Focus == SetupFocusContent), Theme);
  ModernUiDrawText (Ui, Panel.X + 20, Panel.Y + 20, ModernUiGetString (ModernUiStringExitInstruction), Theme->MutedText, Theme->Surface);

  for (Index = 0; Index < ARRAY_SIZE (Items); Index++) {
    Y = Panel.Y + 72 + Index * 54;
    IsSelected = (BOOLEAN)((Focus == SetupFocusContent) && (Index == Selected));
    RowModel.Rect      = (MODERN_UI_RECT){ RowX, Y - 10, RowWidth, 40 };
    RowModel.Prompt    = Items[Index];
    RowModel.Value     = (Index == 3) ? LanguageName : NULL;
    RowModel.Role      = IsSelected ? ModernUiRowSelected : ModernUiRowAction;
    RowModel.ValueType = (Index == 3) ? ModernUiValueOneOf : ModernUiValueNone;
    ModernUiEngineDrawRows (Ui, &RowModel, 1, Theme);
  }

  if (mModernSetupLanguageDropdownOpen) {
    UINTN  DropdownY;
    UINTN  DropdownX;
    UINTN  Option;

    DropdownX = RowX + RowWidth - ValueWidth - 12;
    DropdownY = Panel.Y + 72 + 4 * 54 - 8;
    PopupModel.Rect  = (MODERN_UI_RECT){ DropdownX, DropdownY, ValueWidth, 80 };
    PopupModel.Title = NULL;
    ModernUiEngineDrawPopup (Ui, &PopupModel, Theme);

    for (Option = 0; Option < 2; Option++) {
      IsSelected = (BOOLEAN)(Option == mModernSetupLanguageDropdownSelection);
      RowModel.Rect      = (MODERN_UI_RECT){ DropdownX + 6, DropdownY + 7 + Option * 34, ValueWidth - 12, 30 };
      RowModel.Prompt    = ModernSetupGetLanguageOptionName (Option);
      RowModel.Value     = NULL;
      RowModel.Role      = IsSelected ? ModernUiRowSelected : ModernUiRowNormal;
      RowModel.ValueType = ModernUiValueNone;
      ModernUiEngineDrawRows (Ui, &RowModel, 1, Theme);
    }
  }
}

/**
  Draw the full application frame for one page.

  @param[in] Ui             Initialized render context. Must not be NULL.
  @param[in] Theme          Theme token table. Must not be NULL.
  @param[in] Page           Page to draw.
  @param[in] Focus          Current focus area.
  @param[in] DashboardSelection Selected Dashboard Quick Access entry.
  @param[in] BootSelection  Selected Boot page row.
  @param[in] DeviceSelection Selected Devices page row.
  @param[in] ExitSelection  Selected Exit page action.
  @param[in] StatusMessage  Optional status text. May be NULL.
**/
VOID
ModernSetupDrawCurrentPage (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_PAGE                Page,
  IN SETUP_FOCUS               Focus,
  IN UINTN                     DashboardSelection,
  IN UINTN                     BootSelection,
  IN UINTN                     DeviceSelection,
  IN UINTN                     ExitSelection,
  IN CONST CHAR16              *StatusMessage
  )
{
  ModernUiClear (Ui, Theme->Background);
  ModernSetupDrawHeader (Ui, Theme);
  ModernSetupDrawTabs (Ui, Theme, Page, Focus);
  ModernSetupDrawPageTitle (Ui, Theme, Page);

  switch (Page) {
    case PageDashboard:
      ModernSetupDrawDashboard (Ui, Theme, Focus, DashboardSelection);
      break;
    case PageBoot:
      DrawBoot (Ui, Theme, Focus, BootSelection);
      break;
    case PageDevices:
      DrawDevices (Ui, Theme, Focus, DeviceSelection);
      break;
    case PageSecurity:
      DrawSecurity (Ui, Theme, Focus);
      break;
    case PageFirmware:
      DrawFirmware (Ui, Theme, Focus);
      break;
    case PageDiagnostics:
      DrawDiagnostics (Ui, Theme, Focus);
      break;
    case PageManagement:
      DrawManagement (Ui, Theme, Focus);
      break;
    case PagePower:
      DrawPower (Ui, Theme, Focus);
      break;
    case PagePerformance:
      DrawPerformance (Ui, Theme, Focus);
      break;
    case PageExit:
      DrawExit (Ui, Theme, Focus, ExitSelection);
      break;
    default:
      break;
  }

  ModernSetupDrawFooter (Ui, Theme, Focus, StatusMessage);
}
