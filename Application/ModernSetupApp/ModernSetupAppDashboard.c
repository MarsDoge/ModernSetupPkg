/** @file
  Modern graphical setup application Dashboard page.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "ModernSetupAppInternal.h"

STATIC
BOOLEAN
DashboardUseZh (
  VOID
  );

STATIC
CONST CHAR16 *
DashboardProviderHealthText (
  IN MODERN_SETUP_PROVIDER_HEALTH_STATE  State
  );

STATIC
CONST CHAR16 *
DashboardEnterActionText (
  VOID
  );

/**
  Draw one dashboard label/value row.

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
DrawDashboardInfoRow (
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
  Draw a subtle Dashboard section surface.

  @param[in] Ui      Initialized render context. Must not be NULL.
  @param[in] Theme   Theme token table. Must not be NULL.
  @param[in] Rect    Section rectangle in pixels.
  @param[in] Title   Section title text. Must not be NULL.
  @param[in] Accent  TRUE to draw a stronger top accent line.
**/
STATIC
VOID
DrawDashboardSection (
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
  ModernUiDrawTextFit (Ui, Rect.X + 18, Rect.Y + DASHBOARD_SECTION_TITLE_TOP, Rect.Width - 36, Title, Accent ? Theme->AccentYellow : Theme->MutedText, PanelColor);
}

/**
  Draw one compact Dashboard status card.

  @param[in] Ui       Initialized render context. Must not be NULL.
  @param[in] Theme    Theme token table. Must not be NULL.
  @param[in] Rect     Tile rectangle in pixels.
  @param[in] Title    Tile title text. Must not be NULL.
  @param[in] Value    Tile value text. Must not be NULL.
  @param[in] Detail   Optional secondary detail text.
  @param[in] Emphasis TRUE to draw the value as a highlighted state.
  @param[in] Selected TRUE to draw the tile as the active Quick Access entry.
**/
STATIC
VOID
DrawDashboardTile (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN MODERN_UI_RECT            Rect,
  IN CONST CHAR16              *Title,
  IN CONST CHAR16              *Value,
  IN CONST CHAR16              *Detail,
  IN BOOLEAN                   Emphasis,
  IN BOOLEAN                   Selected
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  TileColor;

  if ((Rect.Width < 40) || (Rect.Height < 24)) {
    return;
  }

  TileColor = Selected ?
              ModernUiBlendColor (Theme->SelectedBand, Theme->BackgroundBlack, 42) :
              ModernUiBlendColor (Theme->Surface, Theme->BackgroundBlack, 24);
  ModernUiFillRect (Ui, Rect, TileColor);
  ModernUiStrokeRect (Ui, Rect, Selected ? Theme->PopupBorder : Theme->Border);
  if (Selected) {
    ModernUiFillRect (Ui, (MODERN_UI_RECT){ Rect.X, Rect.Y, Rect.Width, 2 }, Theme->GlowOrange);
    ModernUiFillRect (Ui, (MODERN_UI_RECT){ Rect.X, Rect.Y + Rect.Height - 3, Rect.Width, 2 }, Theme->AccentOrange);
  }

  ModernUiFillRect (Ui, (MODERN_UI_RECT){ Rect.X, Rect.Y, Selected ? 7 : 4, Rect.Height }, (Emphasis || Selected) ? Theme->AccentYellow : Theme->AccentSoft);
  ModernUiDrawTextFit (Ui, Rect.X + 18, Rect.Y + 8, (Rect.Width >= 160) ? (Rect.Width - 92) : (Rect.Width - 36), Title, Theme->MutedText, TileColor);
  if (Rect.Width >= 160) {
    ModernUiDrawTextFit (Ui, Rect.X + Rect.Width - 66, Rect.Y + 8, 48, DashboardEnterActionText (), Selected ? Theme->AccentYellow : Theme->MutedText, TileColor);
  }
  if (Rect.Height >= DASHBOARD_QUICK_VALUE_MIN_HEIGHT) {
    ModernUiDrawTextFit (
      Ui,
      Rect.X + 18,
      Rect.Y + ((Rect.Height >= 58) ? 36 : 26),
      Rect.Width - 36,
      Value,
      (Emphasis || Selected) ? Theme->AccentYellow : Theme->Text,
      TileColor
      );
  }

  if ((Detail != NULL) && (Rect.Height >= 82)) {
    ModernUiDrawTextFit (Ui, Rect.X + 18, Rect.Y + 64, Rect.Width - 36, Detail, Theme->MutedText, TileColor);
  }
}

/**
  Draw a lightweight group label above one Dashboard Quick Access card.

  @param[in] Ui     Initialized render context. Must not be NULL.
  @param[in] Theme  Theme token table. Must not be NULL.
  @param[in] Rect   Card rectangle in pixels.
  @param[in] Label  Group label text. Must not be NULL.
**/
STATIC
VOID
DrawDashboardQuickGroupLabel (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN MODERN_UI_RECT            Rect,
  IN CONST CHAR16              *Label
  )
{
  if ((Rect.Width < 40) || (Rect.Y < 24)) {
    return;
  }

  ModernUiDrawTextFit (
    Ui,
    Rect.X + 4,
    Rect.Y - DASHBOARD_QUICK_GROUP_LABEL_OFFSET,
    Rect.Width - 8,
    Label,
    Theme->WarningText,
    ModernUiBlendColor (Theme->Surface, Theme->BackgroundBlack, 30)
    );
}

/**
  Return stable present/absent display text for Dashboard details.

  @param[in] Present  TRUE when a read-only capability was detected.

  @return Non-NULL literal display text.
**/
STATIC
CONST CHAR16 *
DashboardPresenceText (
  IN BOOLEAN  Present
  )
{
  if ((ModernUiGetLanguage ()[0] == 'z') && (ModernUiGetLanguage ()[1] == 'h')) {
    return Present ? L"可用" : L"N/A";
  }

  return Present ? L"Present" : L"Absent";
}

/**
  Return TRUE when the Dashboard should use compact Simplified Chinese literals.

  @retval TRUE   Active language starts with "zh".
  @retval FALSE  English literals should be used.
**/
STATIC
BOOLEAN
DashboardUseZh (
  VOID
  )
{
  CONST CHAR8  *Language;

  Language = ModernUiGetLanguage ();
  return (BOOLEAN)((Language[0] == 'z') && (Language[1] == 'h'));
}

/**
  Return compact Dashboard status text for provider health.

  @param[in] State  Provider health summary state.

  @return Non-NULL display text.
**/
STATIC
CONST CHAR16 *
DashboardProviderHealthText (
  IN MODERN_SETUP_PROVIDER_HEALTH_STATE  State
  )
{
  if (DashboardUseZh ()) {
    switch (State) {
      case ModernSetupProviderHealthReady:
        return L"就绪";
      case ModernSetupProviderHealthDegraded:
        return L"Degraded";
      case ModernSetupProviderHealthNotReady:
      default:
        return L"未就绪";
    }
  }

  return ModernSetupGetProviderHealthStateText (State);
}

/**
  Return compact Dashboard action hint text for card action lanes.

  @return Non-NULL display text.
**/
STATIC
CONST CHAR16 *
DashboardEnterActionText (
  VOID
  )
{
  return DashboardUseZh () ? L"回车" : L"Enter";
}

/**
  Return TRUE when at least one capability in a small card category is detected.

  @param[in] First   First capability state.
  @param[in] Second  Second capability state.

  @retval TRUE   At least one capability is present.
  @retval FALSE  Neither capability is present.
**/
STATIC
BOOLEAN
DashboardAnyCapability (
  IN BOOLEAN  First,
  IN BOOLEAN  Second
  )
{
  return (BOOLEAN)(First || Second);
}


/**
  Draw the Dashboard page.

  @param[in] Ui     Initialized render context. Must not be NULL.
  @param[in] Theme  Theme token table. Must not be NULL.
  @param[in] Focus      Current focus area.
  @param[in] Selection  Selected Quick Access entry.
**/
VOID
ModernSetupDrawDashboard (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_FOCUS               Focus,
  IN UINTN                     Selection
  )
{
  CHAR16  Resolution[48];
  CHAR16  BootCount[48];
  CHAR16  DeviceCount[48];
  CHAR16  MemoryText[48];
  CHAR16  SecurityText[48];
  CHAR16  ArchitectureText[96];
  CHAR16  ProviderCountText[48];
  CHAR16  ProviderIssueText[96];
  CHAR16  ProviderDetailText[96];
  CHAR16  FirmwareValueText[96];
  CHAR16  FirmwareDetailText[96];
  CHAR16  PowerValueText[96];
  CHAR16  PowerDetailText[96];
  CHAR16  PerformanceValueText[64];
  CHAR16  PerformanceDetailText[96];
  CHAR16  PciePolicyText[96];
  CHAR16  ServerValueText[96];
  CHAR16  ServerDetailText[96];
  CHAR16  BootDetailText[96];
  CHAR16  DeviceDetailText[96];
  MODERN_UI_RECT  Content;
  MODERN_UI_RECT  SystemPanel;
  MODERN_UI_RECT  MonitorPanel;
  MODERN_UI_RECT  QuickPanel;
  MODERN_UI_RECT  QuickCard;
  MODERN_SETUP_DASHBOARD_QUICK_GRID  Grid;
  UINTN           MonitorWidth;
  UINTN           CardIndex;
  UINTN           CardX;
  UINTN           CardY;
  UINTN           TopHeight;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  PanelBackground;
  MODERN_SETUP_PROVIDER_SNAPSHOT        Providers;
  MODERN_SETUP_PROVIDER_HEALTH_SUMMARY  ProviderHealth;
  BOOLEAN                              Zh;

  Content = ModernSetupContentRect (Ui);
  Zh = DashboardUseZh ();
  PanelBackground = ModernUiBlendColor (Theme->Surface, Theme->BackgroundBlack, 30);
  UnicodeSPrint (Resolution, sizeof (Resolution), L"%u x %u", Ui->Width, Ui->Height);
  UnicodeSPrint (BootCount, sizeof (BootCount), ModernUiGetString (ModernUiStringBootCountFormat), ModernSetupGetBootCount ());
  UnicodeSPrint (DeviceCount, sizeof (DeviceCount), L"%u entries", ModernSetupGetVisibleDeviceCount ());
  ModernSetupGetCachedProviderSnapshot (&Providers);
  ModernSetupGetProviderHealthSummary (&Providers, &ProviderHealth);

  UnicodeSPrint (MemoryText, sizeof (MemoryText), L"%lu MB", Providers.Platform.MemorySizeMb);
  UnicodeSPrint (ArchitectureText, sizeof (ArchitectureText), L"%s", Providers.Platform.Architecture);
  UnicodeSPrint (
    SecurityText,
    sizeof (SecurityText),
    L"%s",
    (Providers.Security.SecureBoot == ModernUiSecurityStateEnabled) ? ModernUiGetString (ModernUiStringEnabled) :
    ((Providers.Security.SecureBoot == ModernUiSecurityStateDisabled) ? ModernUiGetString (ModernUiStringDisabled) : ModernUiGetString (ModernUiStringUnknown))
    );
  UnicodeSPrint (ProviderCountText, sizeof (ProviderCountText), Zh ? L"%u/%u 就绪" : L"%u/%u ready", ProviderHealth.ReadyProviders, ProviderHealth.TotalProviders);
  if (ProviderHealth.State == ModernSetupProviderHealthReady) {
    UnicodeSPrint (ProviderIssueText, sizeof (ProviderIssueText), Zh ? L"OK" : L"All providers ready");
  } else {
    UnicodeSPrint (ProviderIssueText, sizeof (ProviderIssueText), Zh ? L"%s 不可用" : L"%s unavailable", ProviderHealth.FirstIssueName);
  }
  UnicodeSPrint (ProviderDetailText, sizeof (ProviderDetailText), Zh ? L"%u/%u 就绪, N/A %u" : L"Coverage %u/%u, unavailable %u", ProviderHealth.ReadyProviders, ProviderHealth.TotalProviders, ProviderHealth.UnavailableProviders);
  UnicodeSPrint (FirmwareValueText, sizeof (FirmwareValueText), L"%s %s", Providers.Firmware.Vendor, Providers.Firmware.Revision);
  UnicodeSPrint (
    FirmwareDetailText,
    sizeof (FirmwareDetailText),
      L"Capsule runtime: %s",
    DashboardPresenceText (Providers.Firmware.CapsuleRuntimeServices)
    );
  UnicodeSPrint (PowerValueText, sizeof (PowerValueText), L"%s", Providers.Power.ChassisThermalState);
  if (!EFI_ERROR (Providers.HardwareHealthStatus) && (Providers.HardwareHealth.SensorCount > 0)) {
    UnicodeSPrint (
      PowerDetailText,
      sizeof (PowerDetailText),
      L"%s %d %s / %s",
      Providers.HardwareHealth.Sensors[0].Name,
      Providers.HardwareHealth.Sensors[0].CurrentValue,
      Providers.HardwareHealth.Sensors[0].Unit,
      Providers.HardwareHealth.DemoData ? L"demo" : (Zh ? L"Provider" : L"provider")
      );
  } else {
    UnicodeSPrint (
      PowerDetailText,
      sizeof (PowerDetailText),
      Zh ? L"ACPI %s / SMBIOS %s" : L"ACPI: %s / SMBIOS: %s",
      DashboardPresenceText (DashboardAnyCapability (Providers.Power.AcpiTablePresent, Providers.Power.AcpiSdtProtocolPresent)),
      DashboardPresenceText (DashboardAnyCapability (Providers.Power.SmbiosChassisPresent, Providers.Power.SmbiosPowerSupplyPresent))
      );
  }
  UnicodeSPrint (
    PerformanceValueText,
    sizeof (PerformanceValueText),
    !EFI_ERROR (Providers.PcieStatus) ? (Zh ? L"CPU / 内存 / PCIe" : L"CPU / Memory / PCIe") :
    (DashboardAnyCapability (Providers.Performance.ProcessorInventoryPresent, Providers.Performance.MemoryInventoryPresent) ? (Zh ? L"资产就绪" : L"Inventory ready") : (Zh ? L"N/A" : L"Limited data"))
    );
  UnicodeSPrint (
    PciePolicyText,
    sizeof (PciePolicyText),
    Zh ? L"ReBAR %s / 4G %s / SR-IOV %s" : L"PCIe ReBAR %s / 4G %s / SR-IOV %s",
    DashboardPresenceText (Providers.Pcie.ResizeBarPolicyEntryPresent),
    DashboardPresenceText (Providers.Pcie.Above4GPolicyEntryPresent),
    DashboardPresenceText (Providers.Pcie.SriovPolicyEntryPresent)
    );
  if (!EFI_ERROR (Providers.PcieStatus)) {
    UnicodeSPrint (PerformanceDetailText, sizeof (PerformanceDetailText), L"%s", PciePolicyText);
  } else {
    UnicodeSPrint (
      PerformanceDetailText,
      sizeof (PerformanceDetailText),
      Zh ? L"CPU %s / 内存 %s" : L"CPU %s / Mem %s",
      DashboardPresenceText (Providers.Performance.ProcessorInventoryPresent),
      DashboardPresenceText (Providers.Performance.MemoryInventoryPresent)
      );
  }
  UnicodeSPrint (BootDetailText, sizeof (BootDetailText), Zh ? L"模式 %s / 安全 %s" : L"Mode %s / Secure %s", Providers.Platform.BootMode, SecurityText);
  UnicodeSPrint (DeviceDetailText, sizeof (DeviceDetailText), Zh ? L"%u 句柄 / %u 系统表" : L"%u handles / %u tables", Providers.Diagnostics.HandleCount, Providers.Diagnostics.ConfigurationTableCount);
  if (EFI_ERROR (Providers.PcieStatus)) {
    UnicodeSPrint (
      ServerValueText,
      sizeof (ServerValueText),
      Zh ? L"管理 %s / PCIe %s" : L"Mgmt %s / PCIe %s",
      EFI_ERROR (Providers.ManagementStatus) ? ModernUiGetString (ModernUiStringUnknown) : DashboardPresenceText (DashboardAnyCapability (Providers.Management.IpmiProtocolPresent, Providers.Management.RedfishDiscoverPresent)),
      ModernUiGetString (ModernUiStringUnknown)
      );
  } else {
    UnicodeSPrint (
      ServerValueText,
      sizeof (ServerValueText),
      Zh ? L"管理 %s / PCIe %u roots" : L"Mgmt %s / PCIe %u roots",
      EFI_ERROR (Providers.ManagementStatus) ? ModernUiGetString (ModernUiStringUnknown) : DashboardPresenceText (DashboardAnyCapability (Providers.Management.IpmiProtocolPresent, Providers.Management.RedfishDiscoverPresent)),
      Providers.Pcie.RootBridgeCount
      );
  }
  UnicodeSPrint (
    ServerDetailText,
    sizeof (ServerDetailText),
    Zh ? L"只读; 原生管理" : L"Read-only; native owns policy"
    );

  TopHeight   = (mModernSetupPreferences.DashboardDensity == ModernUiDashboardDensityCompact) ?
                ((Content.Height >= 460) ? 236 : 204) : ((Content.Height >= 460) ? 300 : 232);
  MonitorWidth = (Content.Width >= 760) ? ((Content.Width * 31) / 100) : 0;
  if ((MonitorWidth > 0) && (Content.Width > (MonitorWidth + CARD_GAP))) {
    SystemPanel  = (MODERN_UI_RECT){ Content.X, Content.Y, Content.Width - MonitorWidth - CARD_GAP, TopHeight };
    MonitorPanel = (MODERN_UI_RECT){ SystemPanel.X + SystemPanel.Width + CARD_GAP, Content.Y, MonitorWidth, TopHeight };
  } else {
    SystemPanel  = (MODERN_UI_RECT){ Content.X, Content.Y, Content.Width, TopHeight };
    MonitorPanel = (MODERN_UI_RECT){ 0, 0, 0, 0 };
  }

  if (!ModernSetupGetDashboardQuickGrid (Ui, mModernSetupPreferences.DashboardDensity, &Grid)) {
    QuickPanel = (MODERN_UI_RECT){ 0, 0, 0, 0 };
  } else {
    QuickPanel = Grid.Panel;
  }
  DrawDashboardSection (Ui, Theme, SystemPanel, Zh ? L"系统状态" : L"System Information", TRUE);
  ModernUiDrawFocusFrame (Ui, SystemPanel, (BOOLEAN)(Focus == SetupFocusContent), Theme);
  DrawDashboardInfoRow (Ui, Theme, SystemPanel.X + 22, SystemPanel.Y + 58, SystemPanel.Width - 44, ModernUiGetString (ModernUiStringFirmwareVendor), Providers.Platform.FirmwareVendor);
  DrawDashboardInfoRow (Ui, Theme, SystemPanel.X + 22, SystemPanel.Y + 90, SystemPanel.Width - 44, ModernUiGetString (ModernUiStringFirmwareRevision), Providers.Platform.FirmwareRevision);
  DrawDashboardInfoRow (Ui, Theme, SystemPanel.X + 22, SystemPanel.Y + 122, SystemPanel.Width - 44, Zh ? L"平台" : L"Platform", Providers.Platform.Platform);
  DrawDashboardInfoRow (Ui, Theme, SystemPanel.X + 22, SystemPanel.Y + 154, SystemPanel.Width - 44, ModernUiGetString (ModernUiStringFormFactor), Providers.Platform.FormFactor);
  DrawDashboardInfoRow (Ui, Theme, SystemPanel.X + 22, SystemPanel.Y + 186, SystemPanel.Width - 44, ModernUiGetString (ModernUiStringBootMode), Providers.Platform.BootMode);
  DrawDashboardInfoRow (Ui, Theme, SystemPanel.X + 22, SystemPanel.Y + 218, SystemPanel.Width - 44, Zh ? L"内存" : L"Memory", MemoryText);
  if (TopHeight >= 260) {
    DrawDashboardInfoRow (Ui, Theme, SystemPanel.X + 22, SystemPanel.Y + 250, SystemPanel.Width - 44, ModernUiGetString (ModernUiStringDisplay), Resolution);
  }

  if (MonitorPanel.Width > 0) {
    DrawDashboardSection (Ui, Theme, MonitorPanel, Zh ? L"平台健康" : L"Hardware Monitor", FALSE);
    ModernUiDrawText (Ui, MonitorPanel.X + 22, MonitorPanel.Y + 58, L"CPU", Theme->WarningText, PanelBackground);
    DrawDashboardInfoRow (Ui, Theme, MonitorPanel.X + 22, MonitorPanel.Y + 88, MonitorPanel.Width - 44, Zh ? L"Arch" : L"Architecture", ArchitectureText);
    DrawDashboardInfoRow (Ui, Theme, MonitorPanel.X + 22, MonitorPanel.Y + 120, MonitorPanel.Width - 44, Zh ? L"Provider" : L"Provider", L"UEFI");
    ModernUiFillRect (Ui, (MODERN_UI_RECT){ MonitorPanel.X + 22, MonitorPanel.Y + 154, MonitorPanel.Width - 44, 1 }, Theme->Border);
    ModernUiDrawText (Ui, MonitorPanel.X + 22, MonitorPanel.Y + 178, Zh ? L"Provider状态" : L"Providers", Theme->WarningText, PanelBackground);
    DrawDashboardInfoRow (Ui, Theme, MonitorPanel.X + 22, MonitorPanel.Y + 208, MonitorPanel.Width - 44, Zh ? L"健康" : L"Health", DashboardProviderHealthText (ProviderHealth.State));
    if (TopHeight >= 270) {
      DrawDashboardInfoRow (Ui, Theme, MonitorPanel.X + 22, MonitorPanel.Y + 240, MonitorPanel.Width - 44, Zh ? L"状态" : L"Coverage", ProviderCountText);
    }
    if (TopHeight >= 300) {
      DrawDashboardInfoRow (Ui, Theme, MonitorPanel.X + 22, MonitorPanel.Y + 272, MonitorPanel.Width - 44, Zh ? L"问题" : L"First issue", ProviderIssueText);
    }
  }

  if (Grid.Visible) {
    DrawDashboardSection (Ui, Theme, QuickPanel, ModernUiGetString (ModernUiStringSetupCategories), FALSE);

    for (CardIndex = 0; CardIndex < DASHBOARD_QUICK_CARD_COUNT; CardIndex++) {
      CardX     = QuickPanel.X + 20 + ((CardIndex % Grid.CardsPerRow) * (Grid.CardWidth + Grid.CardGap));
      CardY     = QuickPanel.Y + Grid.CardTop + ((CardIndex / Grid.CardsPerRow) * (Grid.CardHeight + Grid.CardGap));
      QuickCard = (MODERN_UI_RECT){ CardX, CardY, Grid.CardWidth, Grid.CardHeight };
      if ((CardIndex == MODERN_SETUP_DASHBOARD_CONTINUE_CARD) || (CardIndex == 1) || (CardIndex == 3) || (CardIndex == 5) || (CardIndex == 7)) {
        DrawDashboardQuickGroupLabel (
          Ui,
          Theme,
          QuickCard,
          ModernUiGetString (
            (CardIndex == MODERN_SETUP_DASHBOARD_CONTINUE_CARD) ? ModernUiStringPageExit :
            ((CardIndex == 1) ? ModernUiStringGroupBootDevices :
            ((CardIndex == 3) ? ModernUiStringGroupPlatformHealth :
            ((CardIndex == 5) ? ModernUiStringGroupPowerPerformance : ModernUiStringGroupManagement)))
            )
          );
      }

      switch (CardIndex) {
        case MODERN_SETUP_DASHBOARD_CONTINUE_CARD:
          DrawDashboardTile (Ui, Theme, QuickCard, ModernUiGetString (ModernUiStringExitContinue), Zh ? L"继续启动" : L"Continue boot flow", Zh ? L"原生Continue" : L"Same as native Continue", TRUE, (BOOLEAN)((Focus == SetupFocusContent) && (Selection == CardIndex)));
          break;
        case 1:
          DrawDashboardTile (Ui, Theme, QuickCard, ModernUiGetString (ModernUiStringBootOptions), BootCount, BootDetailText, TRUE, (BOOLEAN)((Focus == SetupFocusContent) && (Selection == CardIndex)));
          break;
        case 2:
          DrawDashboardTile (Ui, Theme, QuickCard, ModernUiGetString (ModernUiStringPageDevices), DeviceCount, DeviceDetailText, TRUE, (BOOLEAN)((Focus == SetupFocusContent) && (Selection == CardIndex)));
          break;
        case 3:
          DrawDashboardTile (Ui, Theme, QuickCard, Zh ? L"Provider状态" : L"Provider Status", DashboardProviderHealthText (ProviderHealth.State), ProviderDetailText, (BOOLEAN)(ProviderHealth.State == ModernSetupProviderHealthReady), (BOOLEAN)((Focus == SetupFocusContent) && (Selection == CardIndex)));
          break;
        case 4:
          DrawDashboardTile (Ui, Theme, QuickCard, ModernUiGetString (ModernUiStringPageFirmware), FirmwareValueText, FirmwareDetailText, (BOOLEAN)!EFI_ERROR (Providers.FirmwareStatus), (BOOLEAN)((Focus == SetupFocusContent) && (Selection == CardIndex)));
          break;
        case 5:
          DrawDashboardTile (Ui, Theme, QuickCard, ModernUiGetString (ModernUiStringGroupPower), PowerValueText, PowerDetailText, (BOOLEAN)!EFI_ERROR (Providers.PowerStatus), (BOOLEAN)((Focus == SetupFocusContent) && (Selection == CardIndex)));
          break;
        case 6:
          DrawDashboardTile (Ui, Theme, QuickCard, ModernUiGetString (ModernUiStringGroupPerformance), PerformanceValueText, PerformanceDetailText, (BOOLEAN)!EFI_ERROR (Providers.PerformanceStatus), (BOOLEAN)((Focus == SetupFocusContent) && (Selection == CardIndex)));
          break;
        case 7:
        default:
          DrawDashboardTile (Ui, Theme, QuickCard, Zh ? ModernUiGetString (ModernUiStringPageServerInventory) : L"Assets", ServerValueText, ServerDetailText, (BOOLEAN)(!EFI_ERROR (Providers.ManagementStatus) || !EFI_ERROR (Providers.PcieStatus)), (BOOLEAN)((Focus == SetupFocusContent) && (Selection == CardIndex)));
          break;
      }
    }
  }
}
