/** @file
  Modern graphical setup application Dashboard page.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "ModernSetupAppInternal.h"

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
  ModernUiDrawTextFit (Ui, Rect.X + 18, Rect.Y + 16, Rect.Width - 36, Title, Accent ? Theme->AccentYellow : Theme->MutedText, PanelColor);
}

/**
  Draw one compact Dashboard status card.

  @param[in] Ui       Initialized render context. Must not be NULL.
  @param[in] Theme    Theme token table. Must not be NULL.
  @param[in] Rect     Tile rectangle in pixels.
  @param[in] Title    Tile title text. Must not be NULL.
  @param[in] Value    Tile value text. Must not be NULL.
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
  IN BOOLEAN                   Emphasis,
  IN BOOLEAN                   Selected
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  TileColor;

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
  ModernUiDrawTextFit (Ui, Rect.X + 18, Rect.Y + 12, Rect.Width - 36, Title, Theme->MutedText, TileColor);
  ModernUiDrawTextFit (Ui, Rect.X + 18, Rect.Y + 40, Rect.Width - 36, Value, (Emphasis || Selected) ? Theme->AccentYellow : Theme->Text, TileColor);
}


/**
  Return localized capability text for a Dashboard boolean provider state.

  @param[in] Present  TRUE when the capability is available.

  @return Non-NULL localized capability text.
**/
STATIC
CONST CHAR16 *
DashboardCapabilityText (
  IN BOOLEAN  Present
  )
{
  return Present ? ModernUiGetString (ModernUiStringAvailable) : ModernUiGetString (ModernUiStringNotAvailable);
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
  MODERN_UI_RECT  Content;
  MODERN_UI_RECT  SystemPanel;
  MODERN_UI_RECT  MonitorPanel;
  MODERN_UI_RECT  QuickPanel;
  MODERN_UI_RECT  QuickCard;
  UINTN           MonitorWidth;
  UINTN           QuickY;
  UINTN           QuickHeight;
  UINTN           CardWidth;
  UINTN           CardGap;
  UINTN           TopHeight;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  PanelBackground;
  MODERN_UI_PLATFORM_SUMMARY  Platform;
  MODERN_UI_SECURITY_SUMMARY  Security;
  MODERN_UI_FIRMWARE_SUMMARY  Firmware;
  MODERN_UI_DIAGNOSTICS_SUMMARY  Diagnostics;
  MODERN_UI_MANAGEMENT_SUMMARY  Management;

  Content = ModernSetupContentRect (Ui);
  PanelBackground = ModernUiBlendColor (Theme->Surface, Theme->BackgroundBlack, 30);
  UnicodeSPrint (Resolution, sizeof (Resolution), L"%u x %u", Ui->Width, Ui->Height);
  UnicodeSPrint (BootCount, sizeof (BootCount), ModernUiGetString (ModernUiStringBootCountFormat), ModernSetupGetBootCount ());
  UnicodeSPrint (DeviceCount, sizeof (DeviceCount), L"%u entries", ModernSetupGetVisibleDeviceCount ());
  if (EFI_ERROR (ModernUiPlatformDataGetSummary (&Platform))) {
    ZeroMem (&Platform, sizeof (Platform));
    StrCpyS (Platform.FirmwareVendor, ARRAY_SIZE (Platform.FirmwareVendor), L"Unknown");
    StrCpyS (Platform.FirmwareRevision, ARRAY_SIZE (Platform.FirmwareRevision), L"Unknown");
    StrCpyS (Platform.Architecture, ARRAY_SIZE (Platform.Architecture), L"Unknown");
    StrCpyS (Platform.Platform, ARRAY_SIZE (Platform.Platform), L"Unknown");
    StrCpyS (Platform.FormFactor, ARRAY_SIZE (Platform.FormFactor), L"Unknown");
    StrCpyS (Platform.BootMode, ARRAY_SIZE (Platform.BootMode), L"Unknown");
  }

  ModernUiSecurityDataGetSummary (&Security);
  if (EFI_ERROR (ModernUiFirmwareDataGetSummary (&Firmware))) {
    ZeroMem (&Firmware, sizeof (Firmware));
  }

  if (EFI_ERROR (ModernUiDiagnosticsDataGetSummary (&Diagnostics))) {
    ZeroMem (&Diagnostics, sizeof (Diagnostics));
  }

  if (EFI_ERROR (ModernUiManagementDataGetSummary (&Management))) {
    ZeroMem (&Management, sizeof (Management));
  }

  UnicodeSPrint (MemoryText, sizeof (MemoryText), L"%lu MB", Platform.MemorySizeMb);
  UnicodeSPrint (ArchitectureText, sizeof (ArchitectureText), L"%s", Platform.Architecture);
  UnicodeSPrint (
    SecurityText,
    sizeof (SecurityText),
    L"%s",
    (Security.SecureBoot == ModernUiSecurityStateEnabled) ? ModernUiGetString (ModernUiStringEnabled) : ModernUiGetString (ModernUiStringDisabled)
    );

  TopHeight   = (Content.Height >= 460) ? 300 : 232;
  QuickY      = Content.Y + TopHeight + 16;
  QuickHeight = (Content.Height > (TopHeight + 16)) ? (Content.Height - TopHeight - 16) : 0;
  MonitorWidth = (Content.Width >= 760) ? ((Content.Width * 31) / 100) : 0;
  if ((MonitorWidth > 0) && (Content.Width > (MonitorWidth + CARD_GAP))) {
    SystemPanel  = (MODERN_UI_RECT){ Content.X, Content.Y, Content.Width - MonitorWidth - CARD_GAP, TopHeight };
    MonitorPanel = (MODERN_UI_RECT){ SystemPanel.X + SystemPanel.Width + CARD_GAP, Content.Y, MonitorWidth, TopHeight };
  } else {
    SystemPanel  = (MODERN_UI_RECT){ Content.X, Content.Y, Content.Width, TopHeight };
    MonitorPanel = (MODERN_UI_RECT){ 0, 0, 0, 0 };
  }

  QuickPanel = (MODERN_UI_RECT){ Content.X, QuickY, Content.Width, QuickHeight };
  DrawDashboardSection (Ui, Theme, SystemPanel, L"System Information", TRUE);
  ModernUiDrawFocusFrame (Ui, SystemPanel, (BOOLEAN)(Focus == SetupFocusContent), Theme);
  DrawDashboardInfoRow (Ui, Theme, SystemPanel.X + 22, SystemPanel.Y + 58, SystemPanel.Width - 44, ModernUiGetString (ModernUiStringFirmwareVendor), Platform.FirmwareVendor);
  DrawDashboardInfoRow (Ui, Theme, SystemPanel.X + 22, SystemPanel.Y + 90, SystemPanel.Width - 44, ModernUiGetString (ModernUiStringFirmwareRevision), Platform.FirmwareRevision);
  DrawDashboardInfoRow (Ui, Theme, SystemPanel.X + 22, SystemPanel.Y + 122, SystemPanel.Width - 44, L"Platform", Platform.Platform);
  DrawDashboardInfoRow (Ui, Theme, SystemPanel.X + 22, SystemPanel.Y + 154, SystemPanel.Width - 44, ModernUiGetString (ModernUiStringFormFactor), Platform.FormFactor);
  DrawDashboardInfoRow (Ui, Theme, SystemPanel.X + 22, SystemPanel.Y + 186, SystemPanel.Width - 44, ModernUiGetString (ModernUiStringBootMode), Platform.BootMode);
  DrawDashboardInfoRow (Ui, Theme, SystemPanel.X + 22, SystemPanel.Y + 218, SystemPanel.Width - 44, L"Memory", MemoryText);
  if (TopHeight >= 260) {
    DrawDashboardInfoRow (Ui, Theme, SystemPanel.X + 22, SystemPanel.Y + 250, SystemPanel.Width - 44, ModernUiGetString (ModernUiStringDisplay), Resolution);
  }

  if (MonitorPanel.Width > 0) {
    DrawDashboardSection (Ui, Theme, MonitorPanel, L"Hardware Monitor", FALSE);
    ModernUiDrawText (Ui, MonitorPanel.X + 22, MonitorPanel.Y + 58, L"CPU", Theme->WarningText, PanelBackground);
    DrawDashboardInfoRow (Ui, Theme, MonitorPanel.X + 22, MonitorPanel.Y + 88, MonitorPanel.Width - 44, L"Architecture", ArchitectureText);
    DrawDashboardInfoRow (Ui, Theme, MonitorPanel.X + 22, MonitorPanel.Y + 120, MonitorPanel.Width - 44, L"Provider", L"UEFI");
    ModernUiFillRect (Ui, (MODERN_UI_RECT){ MonitorPanel.X + 22, MonitorPanel.Y + 154, MonitorPanel.Width - 44, 1 }, Theme->Border);
    ModernUiDrawText (Ui, MonitorPanel.X + 22, MonitorPanel.Y + 178, L"Providers", Theme->WarningText, PanelBackground);
    DrawDashboardInfoRow (Ui, Theme, MonitorPanel.X + 22, MonitorPanel.Y + 208, MonitorPanel.Width - 44, ModernUiGetString (ModernUiStringFirmwareUpdate), DashboardCapabilityText (Firmware.CapsuleRuntimeServices || Firmware.CapsuleArchProtocol));
    DrawDashboardInfoRow (Ui, Theme, MonitorPanel.X + 22, MonitorPanel.Y + 240, MonitorPanel.Width - 44, ModernUiGetString (ModernUiStringDiagnosticsLogs), DashboardCapabilityText (Diagnostics.AcpiPresent || Diagnostics.SmbiosPresent));
    if (TopHeight >= 300) {
      DrawDashboardInfoRow (Ui, Theme, MonitorPanel.X + 22, MonitorPanel.Y + 272, MonitorPanel.Width - 44, ModernUiGetString (ModernUiStringManagement), DashboardCapabilityText (Management.IpmiProtocolPresent || Management.RedfishDiscoverPresent || Management.SmbiosManagementInterfacePresent));
    }
  }

  if (QuickPanel.Height > 110) {
    DrawDashboardSection (Ui, Theme, QuickPanel, L"Quick Access", FALSE);
    CardGap   = 14;
    CardWidth = (QuickPanel.Width > ((CardGap * 2) + 40)) ? ((QuickPanel.Width - (CardGap * 2) - 40) / 3) : QuickPanel.Width;
    QuickCard = (MODERN_UI_RECT){ QuickPanel.X + 20, QuickPanel.Y + 54, CardWidth, QuickPanel.Height - 74 };
    DrawDashboardTile (Ui, Theme, QuickCard, ModernUiGetString (ModernUiStringBootOptions), BootCount, TRUE, (BOOLEAN)((Focus == SetupFocusContent) && (Selection == 0)));
    QuickCard.X += CardWidth + CardGap;
    DrawDashboardTile (Ui, Theme, QuickCard, L"Devices / HII", DeviceCount, TRUE, (BOOLEAN)((Focus == SetupFocusContent) && (Selection == 1)));
    QuickCard.X += CardWidth + CardGap;
    DrawDashboardTile (Ui, Theme, QuickCard, ModernUiGetString (ModernUiStringSecureBoot), SecurityText, (BOOLEAN)(Security.SecureBoot == ModernUiSecurityStateEnabled), (BOOLEAN)((Focus == SetupFocusContent) && (Selection == 2)));
  }
}
