/** @file
  Modern graphical setup application prototype.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "ModernSetupAppInternal.h"

#include <ModernUi/ModernUiHiiBridge.h>

#if defined (__GNUC__) || defined (__clang__)
#define MODERN_SETUP_NOINLINE  __attribute__ ((noinline))
#else
#define MODERN_SETUP_NOINLINE
#endif

//
// Number of per-device PCIe identity rows the System Information page lists
// before summarizing the remainder.
//
#define MODERN_SETUP_SYSINFO_PCIE_ROWS  5

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

STATIC
VOID
DrawProviderSummaryPageHint (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN MODERN_UI_RECT            Rect
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Background;

  if ((Ui == NULL) || (Theme == NULL) || (Rect.Width < 160)) {
    return;
  }

  Background = ModernUiBlendColor (Theme->Surface, Theme->BackgroundBlack, 30);
  ModernUiDrawTextFit (
    Ui,
    Rect.X + 18,
    Rect.Y + 38,
    Rect.Width - 36,
    L"Read-only provider summary. N/A means this platform did not report that capability.",
    Theme->MutedText,
    Background
    );
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
MODERN_SETUP_NOINLINE
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
  MODERN_SETUP_PAGE_LIST_LAYOUT  Layout;
  UINTN                          Index;
  UINTN                          RowY;
  UINTN                          VisibleRows;
  UINTN                          HeaderStep;

  if (!ModernSetupGetPageListLayout (Ui, mModernSetupPreferences.DashboardDensity, RowCount, FALSE, &Layout)) {
    Layout.Panel = ModernSetupContentRect (Ui);
    DrawProviderSummarySection (Ui, Theme, Layout.Panel, Section, TRUE);
    ModernUiDrawFocusFrame (Ui, Layout.Panel, (BOOLEAN)(Focus == SetupFocusContent), Theme);
    return;
  }

  RowY        = Layout.FirstRowY;
  VisibleRows = 0;
  HeaderStep  = 20;

  DrawProviderSummarySection (Ui, Theme, Layout.Panel, Section, TRUE);
  DrawProviderSummaryPageHint (Ui, Theme, Layout.Panel);
  ModernUiDrawFocusFrame (Ui, Layout.Panel, (BOOLEAN)(Focus == SetupFocusContent), Theme);

  for (Index = 0; (Index < RowCount) && (VisibleRows < Layout.MaxVisibleRows); Index++) {
    if ((Groups != NULL) && (Groups[Index] != NULL)) {
      if ((RowY + HeaderStep) > (Layout.Panel.Y + Layout.Panel.Height)) {
        break;
      }

      DrawProviderSubsectionHeader (Ui, Theme, Layout.RowX, RowY, Layout.RowWidth, Groups[Index]);
      RowY += HeaderStep;
    }

    if ((RowY + Layout.RowHeight) > (Layout.Panel.Y + Layout.Panel.Height)) {
      break;
    }

    DrawProviderSummaryInfoRow (
      Ui,
      Theme,
      Layout.RowX,
      RowY,
      Layout.RowWidth,
      Labels[Index],
      Values[Index]
      );
    RowY += Layout.RowStride;
    VisibleRows++;
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
MODERN_SETUP_NOINLINE
DrawBoot (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_FOCUS               Focus,
  IN UINTN                     Selected
  )
{
  EFI_STATUS                    Status;
  CONST MODERN_UI_BOOT_OPTION   *BootOptions;
  UINTN                         BootOptionCount;
  UINTN                         Index;
  UINTN                         Y;
  CHAR16                        Line[160];
  CHAR16                        Value[96];
  UINT16                        BootNext;
  BOOLEAN                       BootNextPresent;
  CONST CHAR16                  *State;
  BOOLEAN                       IsSelected;
  BOOLEAN                       IsDefaultBootCandidate;
  MODERN_SETUP_PAGE_LIST_LAYOUT  Layout;
  MODERN_UI_ROW_MODEL           RowModel;

  if (!ModernSetupGetPageListLayout (Ui, mModernSetupPreferences.DashboardDensity, MAX_BOOT_ROWS + MODERN_SETUP_NATIVE_BOOT_TOOLS_ROW_COUNT, FALSE, &Layout)) {
    Layout.Panel = ModernSetupContentRect (Ui);
  }

  ModernUiDrawPanel (Ui, Layout.Panel, Theme);
  ModernUiDrawFocusFrame (Ui, Layout.Panel, (BOOLEAN)(Focus == SetupFocusContent), Theme);
  ModernUiDrawText (
    Ui,
    Layout.RowX,
    Layout.Panel.Y + 20,
    L"Enter=Boot  N=Next boot  C=Clear next  +/-=Move default rows",
    Theme->MutedText,
    Theme->Surface
    );
  ModernUiDrawTextFit (
    Ui,
    Layout.RowX,
    Layout.Panel.Y + 38,
    Layout.RowWidth,
    L"Only default Boot#### rows can move; app/shell/manual entries use Enter or Next boot.",
    Theme->MutedText,
    Theme->Surface
    );

  BootNext        = 0;
  BootNextPresent = FALSE;
  Status          = ModernUiBootDataGetBootNext (&BootNext, &BootNextPresent);
  if (EFI_ERROR (Status)) {
    BootNextPresent = FALSE;
  }

  BootOptions = NULL;
  BootOptionCount = 0;
  Status = ModernSetupGetCachedBootOptions (&BootOptions, &BootOptionCount);
  if (EFI_ERROR (Status) || (BootOptions == NULL)) {
    BootOptions = NULL;
    BootOptionCount = 0;
  }

  for (Index = 0; (Index < BootOptionCount) && (Index < MAX_BOOT_ROWS) && ((Index + MODERN_SETUP_NATIVE_BOOT_TOOLS_ROW_COUNT) < Layout.MaxVisibleRows); Index++) {
    Y           = Layout.FirstRowY + (Index * Layout.RowStride);
    State                  = BootOptions[Index].Active ? L"On" : L"Off";
    IsSelected             = (BOOLEAN)((Focus == SetupFocusContent) && (Index == Selected));
    IsDefaultBootCandidate = ModernSetupBootOptionIsDefaultBootCandidate (&BootOptions[Index]);
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
      L"%s%s/%s%s%s",
      State,
      BootOptions[Index].Hidden ? L"/Hid" : L"",
      BootOptions[Index].Category,
      IsDefaultBootCandidate ? L"" : L"/Next",
      (BootNextPresent && (BootNext == BootOptions[Index].OptionNumber)) ? L"/BootNext" : L""
      );
    RowModel.Rect      = (MODERN_UI_RECT){ Layout.RowX, Y - 8, Layout.RowWidth, Layout.RowHeight - 8 };
    RowModel.Prompt    = Line;
    RowModel.Value     = Value;
    RowModel.Role      = IsSelected ? ModernUiRowSelected : ModernUiRowNormal;
    RowModel.ValueType = ModernUiValueText;
    ModernUiEngineDrawRows (Ui, &RowModel, 1, Theme);
    if (IsSelected) {
      ModernUiFillRect (Ui, (MODERN_UI_RECT){ RowModel.Rect.X + 4, RowModel.Rect.Y + 4, 4, RowModel.Rect.Height - 8 }, Theme->AccentYellow);
    }
    ModernUiDrawTextFit (
      Ui,
      Layout.RowX + Layout.HorizontalPad,
      Y + 18,
      (Layout.RowWidth > (Layout.HorizontalPad * 2)) ? (Layout.RowWidth - (Layout.HorizontalPad * 2)) : Layout.RowWidth,
      BootOptions[Index].FilePathSummary,
      Theme->MutedText,
      IsSelected ? Theme->SelectedBand : Theme->Surface
      );
  }

  Y = Layout.FirstRowY + (Index * Layout.RowStride);
  IsSelected = (BOOLEAN)((Focus == SetupFocusContent) && ModernSetupBootSelectionIsNativeFallback (Selected, MIN (ModernSetupGetBootSelectableCount (), Layout.MaxVisibleRows)));
  RowModel.Rect      = (MODERN_UI_RECT){ Layout.RowX, Y - 8, Layout.RowWidth, Layout.RowHeight - 8 };
  RowModel.Prompt    = L"Native Boot Tools";
  RowModel.Value     = L"Open >";
  RowModel.Role      = IsSelected ? ModernUiRowSelected : ModernUiRowNormal;
  RowModel.ValueType = ModernUiValueText;
  ModernUiEngineDrawRows (Ui, &RowModel, 1, Theme);
  if (IsSelected) {
    ModernUiFillRect (Ui, (MODERN_UI_RECT){ RowModel.Rect.X + 4, RowModel.Rect.Y + 4, 4, RowModel.Rect.Height - 8 }, Theme->AccentYellow);
  }
  ModernUiDrawTextFit (
    Ui,
    Layout.RowX + Layout.HorizontalPad,
    Y + 18,
    (Layout.RowWidth > (Layout.HorizontalPad * 2)) ? (Layout.RowWidth - (Layout.HorizontalPad * 2)) : Layout.RowWidth,
    L"Open edk2 Boot Manager / Boot Maintenance; platform owns boot policy",
    Theme->MutedText,
    IsSelected ? Theme->SelectedBand : Theme->Surface
    );

  if (BootOptionCount == 0) {
    ModernUiDrawText (
      Ui,
      Layout.RowX,
      Y + Layout.RowStride,
      ModernUiGetString (ModernUiStringNoBootOptions),
      Theme->Warning,
      Theme->Surface
      );
  }
}

STATIC
CONST CHAR16 *
HiiPreviewDisplayKindText (
  IN MODERN_UI_DISPLAY_KIND  Kind
  )
{
  switch (Kind) {
    case ModernUiDisplayText:
      return L"Text";
    case ModernUiDisplaySubtitle:
      return L"Subtitle";
    case ModernUiDisplayLink:
      return L"Link";
    case ModernUiDisplayToggle:
      return L"Toggle";
    case ModernUiDisplayChoice:
      return L"Choice";
    case ModernUiDisplayNumeric:
      return L"Numeric";
    case ModernUiDisplayString:
      return L"String";
    case ModernUiDisplayPassword:
      return L"Password";
    case ModernUiDisplayAction:
      return L"Action";
    case ModernUiDisplayDate:
      return L"Date";
    case ModernUiDisplayTime:
      return L"Time";
    case ModernUiDisplayNativeOnly:
      return L"Native only";
    case ModernUiDisplayUnsupported:
      return L"Unsupported";
    default:
      return L"Info";
  }
}

STATIC
CONST CHAR16 *
HiiPreviewEditPolicyText (
  IN MODERN_UI_EDIT_POLICY  Policy
  )
{
  switch (Policy) {
    case ModernUiEditReadOnly:
      return L"read-only";
    case ModernUiEditNavigate:
      return L"opens native";
    case ModernUiEditToggle:
    case ModernUiEditChoose:
    case ModernUiEditInput:
    case ModernUiEditActivate:
      return L"native edit";
    case ModernUiEditNativeOnly:
      return L"native only";
    default:
      return L"view only";
  }
}

STATIC
CONST CHAR16 *
HiiPreviewPolicyReasonText (
  IN CONST MODERN_UI_HII_ITEM  *Item
  )
{
  if (Item == NULL) {
    return L"Preview-only; edits happen in native FormBrowser.";
  }

  if (Item->Policy.Unsupported) {
    return L"Unsupported IFR construct; open native FormBrowser.";
  }

  if (Item->Policy.NativeOnly && Item->Policy.RequiresNativeFallback) {
    return L"Firmware-owned behavior; native FormBrowser required.";
  }

  if (Item->Policy.NativeOnly) {
    return L"Native-only setup control; open native FormBrowser.";
  }

  if (Item->Policy.RequiresNativeFallback) {
    return L"Native fallback required by setup policy.";
  }

  if (Item->Policy.ReadOnly) {
    return L"Preview-only; no edits here.";
  }

  return L"Preview-only; edits happen in native FormBrowser.";
}

STATIC
VOID
HiiPreviewCountPagePolicy (
  IN  CONST MODERN_UI_HII_PAGE  *Page,
  IN  UINTN                     MaxShown,
  OUT UINTN                     *ShownCount,
  OUT UINTN                     *VisibleCount,
  OUT UINTN                     *NativeOnlyCount,
  OUT UINTN                     *FallbackCount,
  OUT UINTN                     *UnsupportedCount
  )
{
  UINTN                     Index;
  UINTN                     Shown;
  CONST MODERN_UI_HII_ITEM  *Item;

  Shown = 0;
  if (ShownCount != NULL) {
    *ShownCount = 0;
  }

  if (VisibleCount != NULL) {
    *VisibleCount = 0;
  }

  if (NativeOnlyCount != NULL) {
    *NativeOnlyCount = 0;
  }

  if (FallbackCount != NULL) {
    *FallbackCount = 0;
  }

  if (UnsupportedCount != NULL) {
    *UnsupportedCount = 0;
  }

  if (Page == NULL) {
    return;
  }

  for (Index = 0; Index < Page->ItemCount; Index++) {
    Item = &Page->Items[Index];
    if (!Item->Policy.VisibleByDefault) {
      continue;
    }

    if (VisibleCount != NULL) {
      (*VisibleCount)++;
    }

    if (Shown < MaxShown) {
      Shown++;
      if (Item->Policy.NativeOnly && (NativeOnlyCount != NULL)) {
        (*NativeOnlyCount)++;
      }

      if (Item->Policy.RequiresNativeFallback && (FallbackCount != NULL)) {
        (*FallbackCount)++;
      }

      if (Item->Policy.Unsupported && (UnsupportedCount != NULL)) {
        (*UnsupportedCount)++;
      }
    }
  }

  if (ShownCount != NULL) {
    *ShownCount = Shown;
  }
}

STATIC
CONST CHAR16 *
HiiPreviewResolveText (
  IN  CONST MODERN_UI_TEXT_REF  *TextRef,
  OUT CHAR16                    *Buffer,
  IN  UINTN                     BufferChars,
  IN  CONST CHAR16              *Fallback
  )
{
  if ((Buffer == NULL) || (BufferChars == 0)) {
    return Fallback;
  }

  Buffer[0] = L'\0';
  if ((TextRef != NULL) && !EFI_ERROR (ModernUiHiiBridgeResolveText (TextRef, Buffer, BufferChars)) && (Buffer[0] != L'\0')) {
    return Buffer;
  }

  return Fallback;
}

STATIC
MODERN_UI_HII_FORMSET *
HiiPreviewFindFormSet (
  IN MODERN_UI_HII_VIEW              *View,
  IN CONST MODERN_UI_DEVICE_ENTRY    *Entry
  )
{
  UINTN  Index;

  if ((View == NULL) || (Entry == NULL)) {
    return NULL;
  }

  for (Index = 0; Index < View->FormSetCount; Index++) {
    if ((View->FormSets[Index].Source.HiiHandle == Entry->HiiHandle) &&
        CompareGuid (&View->FormSets[Index].Source.FormSetGuid, &Entry->FormSetGuid))
    {
      return &View->FormSets[Index];
    }
  }

  return NULL;
}

STATIC
VOID
MODERN_SETUP_NOINLINE
DrawHiiReadOnlyPreview (
  IN MODERN_UI_RENDER_CONTEXT       *Ui,
  IN CONST MODERN_UI_THEME          *Theme,
  IN MODERN_UI_RECT                 Rect,
  IN CONST MODERN_UI_DEVICE_ENTRY   *Entry
  )
{
  EFI_STATUS                     Status;
  MODERN_UI_HII_VIEW             *View;
  MODERN_UI_HII_FORMSET          *FormSet;
  MODERN_UI_HII_PAGE             *Page;
  MODERN_UI_HII_ITEM             *Item;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Background;
  UINTN                          Index;
  UINTN                          RowY;
  UINTN                          MaxItems;
  UINTN                          ShownItems;
  UINTN                          VisibleItems;
  UINTN                          NativeOnlyItems;
  UINTN                          FallbackItems;
  UINTN                          UnsupportedItems;
  CHAR16                         Title[128];
  CHAR16                         Help[128];
  CHAR16                         PageTitle[128];
  CHAR16                         Prompt[128];
  CHAR16                         Value[128];
  CHAR16                         Summary[128];

  if ((Entry == NULL) || !Entry->HasForm || (Rect.Width < 180) || (Rect.Height < 120)) {
    return;
  }

  Background = ModernUiBlendColor (Theme->Surface, Theme->BackgroundBlack, 30);
  DrawProviderSummarySection (Ui, Theme, Rect, L"Native setup preview", TRUE);
  ModernUiDrawTextFit (
    Ui,
    Rect.X + 18,
    Rect.Y + 40,
    Rect.Width - 36,
    L"Read-only mirror. Enter opens native FormBrowser for edits.",
    Theme->MutedText,
    Background
    );

  View = AllocateZeroPool (sizeof (*View));
  if (View == NULL) {
    ModernUiDrawText (Ui, Rect.X + 18, Rect.Y + 76, L"Preview unavailable: out of resources.", Theme->Warning, Background);
    ModernUiDrawText (Ui, Rect.X + 18, Rect.Y + 100, L"Press Enter to open native FormBrowser.", Theme->MutedText, Background);
    return;
  }

  Status = ModernUiHiiBridgeBuildView (View, &Entry->FormSetGuid, 1);
  if (EFI_ERROR (Status)) {
    ModernUiDrawTextFormatted (Ui, Rect.X + 18, Rect.Y + 76, Theme->Warning, Background, L"Preview unavailable: %r", Status);
    ModernUiDrawText (Ui, Rect.X + 18, Rect.Y + 100, L"Press Enter to open native FormBrowser.", Theme->MutedText, Background);
    goto Exit;
  }

  FormSet = HiiPreviewFindFormSet (View, Entry);
  if (FormSet == NULL) {
    ModernUiDrawText (Ui, Rect.X + 18, Rect.Y + 76, L"No normalized preview for this formset; press Enter for native FormBrowser.", Theme->Warning, Background);
    goto Exit;
  }

  RowY = Rect.Y + 76;
  ModernUiDrawTextFit (
    Ui,
    Rect.X + 18,
    RowY,
    Rect.Width - 36,
    HiiPreviewResolveText (&FormSet->Title, Title, ARRAY_SIZE (Title), Entry->Title),
    Theme->Text,
    Background
    );
  RowY += 24;

  if (HiiPreviewResolveText (&FormSet->Help, Help, ARRAY_SIZE (Help), L"")[0] != L'\0') {
    ModernUiDrawTextFit (Ui, Rect.X + 18, RowY, Rect.Width - 36, Help, Theme->MutedText, Background);
    RowY += 22;
  }

  if (FormSet->PageCount == 0) {
    ModernUiDrawText (Ui, Rect.X + 18, RowY, L"No pages in normalized preview; press Enter for native FormBrowser.", Theme->MutedText, Background);
    goto Exit;
  }

  Page = &FormSet->Pages[0];
  MaxItems = 5;
  HiiPreviewCountPagePolicy (
    Page,
    MaxItems,
    &ShownItems,
    &VisibleItems,
    &NativeOnlyItems,
    &FallbackItems,
    &UnsupportedItems
    );
  ModernUiDrawTextFit (
    Ui,
    Rect.X + 18,
    RowY,
    Rect.Width - 36,
    HiiPreviewResolveText (&Page->Title, PageTitle, ARRAY_SIZE (PageTitle), L"First page"),
    Theme->AccentYellow,
    Background
    );
  RowY += 28;

  UnicodeSPrint (
    Summary,
    sizeof (Summary),
    L"Shown %u. Native-only %u, fallback %u, unsupported %u.",
    ShownItems,
    NativeOnlyItems,
    FallbackItems,
    UnsupportedItems
    );
  ModernUiDrawTextFit (Ui, Rect.X + 18, RowY, Rect.Width - 36, Summary, Theme->MutedText, Background);
  RowY += 24;

  ShownItems = 0;
  for (Index = 0; (Index < Page->ItemCount) && (ShownItems < MaxItems); Index++) {
    if ((RowY + 24) > (Rect.Y + Rect.Height - 10)) {
      break;
    }

    Item = &Page->Items[Index];
    if (!Item->Policy.VisibleByDefault) {
      continue;
    }

    HiiPreviewResolveText (&Item->Prompt, Prompt, ARRAY_SIZE (Prompt), HiiPreviewDisplayKindText (Item->Policy.DisplayKind));
    HiiPreviewResolveText (&Item->ValueText, Value, ARRAY_SIZE (Value), L"");
    if (Item->Policy.Unsupported || Item->Policy.NativeOnly || Item->Policy.RequiresNativeFallback || Item->Policy.ReadOnly) {
      UnicodeSPrint (
        Value,
        sizeof (Value),
        L"%s",
        HiiPreviewPolicyReasonText (Item)
        );
    } else if (Value[0] == L'\0') {
      UnicodeSPrint (
        Value,
        sizeof (Value),
        L"%s / %s",
        HiiPreviewDisplayKindText (Item->Policy.DisplayKind),
        HiiPreviewEditPolicyText (Item->Policy.EditPolicy)
        );
    }

    DrawProviderSummaryInfoRow (Ui, Theme, Rect.X + 18, RowY, Rect.Width - 36, Prompt, Value);
    RowY += 26;
    ShownItems++;
  }

  if ((VisibleItems > ShownItems) && ((RowY + 20) <= (Rect.Y + Rect.Height))) {
    ModernUiDrawTextFormatted (Ui, Rect.X + 18, RowY, Theme->MutedText, Background, L"+ %u more items in native browser", VisibleItems - ShownItems);
  }

Exit:
  ModernUiHiiBridgeClearView (View);
  FreePool (View);
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
MODERN_SETUP_NOINLINE
DrawDevices (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_FOCUS               Focus,
  IN UINTN                     Selected
  )
{
  EFI_STATUS                     Status;
  CONST MODERN_UI_DEVICE_ENTRY   *Entries;
  UINTN                          EntryCount;
  UINTN                          Index;
  UINTN                          HiiCount;
  UINTN                          VisibleHiiCount;
  UINTN                          VisibleDeviceCount;
  UINTN                          VisibleRows;
  UINTN                          RowY;
  CHAR16                         Line[168];
  CHAR16                         Summary[96];
  BOOLEAN                        IsSelected;
  MODERN_SETUP_PAGE_LIST_LAYOUT  Layout;
  MODERN_UI_ROW_MODEL            RowModel;
  CONST MODERN_UI_DEVICE_ENTRY   *SelectedEntry;
  BOOLEAN                        ShowPreview;

  if (!ModernSetupGetPageListLayout (Ui, mModernSetupPreferences.DashboardDensity, MAX_DEVICE_ROWS, TRUE, &Layout)) {
    Layout.Panel = ModernSetupContentRect (Ui);
  }

  ModernUiDrawPanel (Ui, Layout.Panel, Theme);
  ModernUiDrawFocusFrame (Ui, Layout.Panel, (BOOLEAN)(Focus == SetupFocusContent), Theme);

  Entries = NULL;
  Status = ModernSetupGetCachedDeviceEntries (&Entries, &EntryCount);
  if (EFI_ERROR (Status)) {
    ModernUiDrawText (Ui, 280, 150, ModernUiGetString (ModernUiStringUnableEnumerateHandles), Theme->Warning, Theme->Surface);
    return;
  }

  HiiCount = 0;
  VisibleHiiCount = 0;
  VisibleDeviceCount = 0;
  SelectedEntry = (Selected < EntryCount) ? &Entries[Selected] : NULL;
  ShowPreview = (BOOLEAN)((SelectedEntry != NULL) && SelectedEntry->HasForm && Layout.HasPreviewPane);

  for (Index = 0; Index < EntryCount; Index++) {
    if (Entries[Index].HasForm) {
      HiiCount++;
    }

    if (Index < Layout.MaxVisibleRows) {
      if (Entries[Index].HasForm) {
        VisibleHiiCount++;
      } else {
        VisibleDeviceCount++;
      }
    }
  }

  UnicodeSPrint (Summary, sizeof (Summary), L"%u entries (%u HII, %u device)", EntryCount, HiiCount, EntryCount - HiiCount);
  ModernUiDrawText (Ui, Layout.RowX, Layout.Panel.Y + 20, Summary, Theme->MutedText, Theme->Surface);
  ModernUiDrawTextFit (
    Ui,
    Layout.RowX,
    Layout.Panel.Y + 38,
    Layout.RowWidth,
    L"HII rows open native setup. Inventory rows are read-only device-path context.",
    Theme->MutedText,
    Theme->Surface
    );

  RowY = Layout.FirstRowY;
  VisibleRows = 0;

  if (VisibleHiiCount > 0) {
    DrawProviderSubsectionHeader (Ui, Theme, Layout.RowX, RowY, Layout.RowWidth, L"HII formsets");
    RowY += 26;
  }

  for (Index = 0; (Index < EntryCount) && (Index < Layout.MaxVisibleRows) && (VisibleRows < Layout.MaxVisibleRows); Index++) {
    if (!Entries[Index].HasForm) {
      continue;
    }

    UnicodeSPrint (Line, sizeof (Line), L"%02u  %s", Index + 1, Entries[Index].Title);
    IsSelected = (BOOLEAN)((Focus == SetupFocusContent) && (Index == Selected));
    RowModel.Rect      = (MODERN_UI_RECT){ Layout.RowX, RowY, Layout.RowWidth, Layout.RowHeight };
    RowModel.Prompt    = Line;
    RowModel.Value     = L"HII >";
    RowModel.Role      = IsSelected ? ModernUiRowSelected : ModernUiRowNormal;
    RowModel.ValueType = ModernUiValueAction;
    ModernUiEngineDrawRows (Ui, &RowModel, 1, Theme);
    RowY += Layout.RowStride;
    VisibleRows++;
  }

  if ((VisibleDeviceCount > 0) && (VisibleRows < Layout.MaxVisibleRows)) {
    RowY += (VisibleHiiCount > 0) ? 6 : 0;
    DrawProviderSubsectionHeader (Ui, Theme, Layout.RowX, RowY, Layout.RowWidth, L"Device inventory");
    RowY += 26;
  }

  for (Index = 0; (Index < EntryCount) && (Index < Layout.MaxVisibleRows) && (VisibleRows < Layout.MaxVisibleRows); Index++) {
    if (Entries[Index].HasForm) {
      continue;
    }

    UnicodeSPrint (Line, sizeof (Line), L"%02u  %s", Index + 1, Entries[Index].Title);
    IsSelected = (BOOLEAN)((Focus == SetupFocusContent) && (Index == Selected));
    RowModel.Rect      = (MODERN_UI_RECT){ Layout.RowX, RowY, Layout.RowWidth, Layout.RowHeight };
    RowModel.Prompt    = Line;
    RowModel.Value     = L"Device";
    RowModel.Role      = IsSelected ? ModernUiRowSelected : ModernUiRowNormal;
    RowModel.ValueType = ModernUiValueText;
    ModernUiEngineDrawRows (Ui, &RowModel, 1, Theme);
    RowY += Layout.RowStride;
    VisibleRows++;
  }

  if (EntryCount == 0) {
    ModernUiDrawText (Ui, Layout.RowX, Layout.FirstRowY, L"No HII formsets found.", Theme->Warning, Theme->Surface);
  }

  if (ShowPreview) {
    DrawHiiReadOnlyPreview (Ui, Theme, Layout.PreviewPanel, SelectedEntry);
  }
}


/**
  Draw the Security page with read-only Secure Boot state.

  @param[in] Ui     Initialized render context. Must not be NULL.
  @param[in] Theme  Theme token table. Must not be NULL.
  @param[in] Focus  Current focus area.
**/
STATIC
VOID
MODERN_SETUP_NOINLINE
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

  ModernSetupGetCachedProviderSnapshot (&Providers);
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
  ModernUiDrawText (Ui, Panel.X + 20, Panel.Y + 284, L"Unavailable means this OVMF/demo platform did not report that capability.", Theme->MutedText, Theme->Surface);
}

/**
  Draw the Firmware page with read-only update and capsule state.

  @param[in] Ui     Initialized render context. Must not be NULL.
  @param[in] Theme  Theme token table. Must not be NULL.
  @param[in] Focus  Current focus area.
**/
STATIC
VOID
MODERN_SETUP_NOINLINE
DrawFirmware (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_FOCUS               Focus
  )
{
  MODERN_SETUP_PROVIDER_SNAPSHOT  Providers;
  MODERN_UI_FIRMWARE_SUMMARY      *Summary;
  CONST CHAR16                    *Labels[6];
  CONST CHAR16                    *Values[6];
  CONST CHAR16                    *Groups[6];

  ModernSetupGetCachedProviderSnapshot (&Providers);
  Summary = &Providers.Firmware;

  Groups[0] = NULL;
  Groups[1] = NULL;
  Groups[2] = NULL;
  Groups[3] = NULL;
  Groups[4] = NULL;
  Groups[5] = NULL;

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
  Labels[5] = L"Ownership";
  Values[5] = L"Read-only summary; native capsule flow owns updates";

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
  Draw the System Information page: a read-only detail view of the real platform,
  processor, memory, and firmware identity collected by the platform provider.

  All values come from the cached provider snapshot (no re-probe). The page parses
  no IFR and writes nothing; it is a deeper read-only companion to the dashboard
  System Information panel.

  @param[in] Ui     Initialized render context. Must not be NULL.
  @param[in] Theme  Theme token table. Must not be NULL.
  @param[in] Focus  Current focus area.
**/
STATIC
VOID
MODERN_SETUP_NOINLINE
DrawSystemInfo (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_FOCUS               Focus
  )
{
  MODERN_SETUP_PROVIDER_SNAPSHOT  Providers;
  MODERN_UI_PLATFORM_SUMMARY      *Summary;
  CONST CHAR8                     *Language;
  BOOLEAN                         Zh;
  CHAR16                          MemoryText[96];
  CONST CHAR16                    *Labels[13];
  CONST CHAR16                    *Values[13];
  CONST CHAR16                    *Groups[13];
  UINTN                           Count;

  ModernSetupGetCachedProviderSnapshot (&Providers);
  Summary  = &Providers.Platform;
  Language = ModernUiGetLanguage ();
  Zh       = (BOOLEAN)((Language[0] == 'z') && (Language[1] == 'h'));

  if (Summary->MemoryDetail[0] != L'\0') {
    UnicodeSPrint (MemoryText, sizeof (MemoryText), L"%lu MB (%s)", Summary->MemorySizeMb, Summary->MemoryDetail);
  } else {
    UnicodeSPrint (MemoryText, sizeof (MemoryText), L"%lu MB", Summary->MemorySizeMb);
  }

  Count = 0;

  Groups[Count] = Zh ? L"系统" : L"System";
  Labels[Count] = Zh ? L"平台" : L"Platform";
  Values[Count] = Summary->Platform;
  Count++;
  if (Summary->Baseboard[0] != L'\0') {
    //
    // "Baseboard" stays English in the zh UI: the zh glyphs are outside the
    // embedded subset (graceful-fallback policy).
    //
    Groups[Count] = NULL;
    Labels[Count] = L"Baseboard";
    Values[Count] = Summary->Baseboard;
    Count++;
  }

  Groups[Count] = NULL;
  Labels[Count] = L"CPU";
  Values[Count] = Summary->Processor;
  Count++;
  Groups[Count] = NULL;
  Labels[Count] = Zh ? L"内存" : L"Memory";
  Values[Count] = MemoryText;
  Count++;
  Groups[Count] = NULL;
  Labels[Count] = Zh ? L"Arch" : L"Architecture";
  Values[Count] = Summary->Architecture;
  Count++;
  Groups[Count] = NULL;
  Labels[Count] = ModernUiGetString (ModernUiStringFormFactor);
  Values[Count] = Summary->FormFactor;
  Count++;
  Groups[Count] = NULL;
  Labels[Count] = ModernUiGetString (ModernUiStringBootMode);
  Values[Count] = Summary->BootMode;
  Count++;

  //
  // Identity rows are appended only when SMBIOS actually reports them, so the
  // page collapses cleanly on platforms with thin SMBIOS instead of stacking
  // empty rows (same philosophy as the dashboard N/A reflow).
  //
  if (Summary->Serial[0] != L'\0') {
    //
    // "Serial number" stays English in the zh UI (glyphs outside the subset).
    //
    Groups[Count] = NULL;
    Labels[Count] = L"Serial number";
    Values[Count] = Summary->Serial;
    Count++;
  }

  if (Summary->Uuid[0] != L'\0') {
    Groups[Count] = NULL;
    Labels[Count] = L"UUID";
    Values[Count] = Summary->Uuid;
    Count++;
  }

  Groups[Count] = ModernUiGetString (ModernUiStringGroupFirmware);
  Labels[Count] = ModernUiGetString (ModernUiStringFirmwareVendor);
  Values[Count] = Summary->FirmwareVendor;
  Count++;
  Groups[Count] = NULL;
  Labels[Count] = ModernUiGetString (ModernUiStringFirmwareRevision);
  Values[Count] = Summary->FirmwareRevision;
  Count++;
  if (Summary->BiosVersion[0] != L'\0') {
    Groups[Count] = NULL;
    Labels[Count] = Zh ? L"BIOS 版本" : L"BIOS version";
    Values[Count] = Summary->BiosVersion;
    Count++;
  }

  if (Summary->BiosDate[0] != L'\0') {
    //
    // "BIOS date" stays English in the zh UI (日/期 outside the subset).
    //
    Groups[Count] = NULL;
    Labels[Count] = L"BIOS date";
    Values[Count] = Summary->BiosDate;
    Count++;
  }

  DrawProviderSummaryPage (
    Ui,
    Theme,
    Focus,
    ModernUiGetString (ModernUiStringPageSystemInfo),
    Labels,
    Values,
    Groups,
    Count
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
MODERN_SETUP_NOINLINE
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
  CONST CHAR16                          *Groups[8];

  ModernSetupGetCachedProviderSnapshot (&Providers);
  ModernSetupGetProviderHealthSummary (&Providers, &ProviderHealth);
  Summary = &Providers.Diagnostics;

  Groups[0] = NULL;
  Groups[1] = NULL;
  Groups[2] = NULL;
  Groups[3] = NULL;
  Groups[4] = NULL;
  Groups[5] = NULL;
  Groups[6] = NULL;
  Groups[7] = NULL;

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
MODERN_SETUP_NOINLINE
DrawManagement (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_FOCUS               Focus
  )
{
  MODERN_SETUP_PROVIDER_SNAPSHOT  Providers;
  MODERN_UI_MANAGEMENT_SUMMARY    *Summary;
  CONST CHAR16                    *Labels[6];
  CONST CHAR16                    *Values[6];
  CONST CHAR16                    *Groups[6];

  ModernSetupGetCachedProviderSnapshot (&Providers);
  Summary = &Providers.Management;

  Groups[0] = NULL;
  Groups[1] = NULL;
  Groups[2] = NULL;
  Groups[3] = NULL;
  Groups[4] = NULL;
  Groups[5] = NULL;

  Labels[0] = ModernUiGetString (ModernUiStringIpmi);
  Groups[0] = ModernUiGetString (ModernUiStringGroupManagement);
  Values[0] = CapabilityText (Summary->IpmiProtocolPresent);
  Labels[1] = ModernUiGetString (ModernUiStringRedfish);
  Values[1] = CapabilityText (Summary->RedfishDiscoverPresent);
  Labels[2] = ModernUiGetString (ModernUiStringManagementInterface);
  Values[2] = CapabilityText (Summary->SmbiosManagementInterfacePresent);
  Labels[3] = L"Platform note";
  Groups[3] = L"Empty-state";
  Values[3] = L"OVMF normally reports no BMC interface";
  Labels[4] = L"Expected sources";
  Values[4] = L"IPMI / Redfish / SMBIOS records";
  Labels[5] = L"Ownership";
  Values[5] = L"Native platform firmware owns BMC policy";

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

STATIC
VOID
DrawTemperatureTrendSparkline (
  IN MODERN_UI_RENDER_CONTEXT                  *Ui,
  IN CONST MODERN_UI_THEME                     *Theme,
  IN UINTN                                     X,
  IN UINTN                                     Y,
  IN UINTN                                     Width,
  IN UINTN                                     Height,
  IN CONST MODERN_UI_HARDWARE_HEALTH_SENSOR   *Sensor,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL            Background
  )
{
  INTN   MinValue;
  INTN   MaxValue;
  INTN   Range;
  UINTN  Index;
  UINTN  SampleCount;
  UINTN  BarWidth;
  UINTN  BarX;
  UINTN  BarHeight;
  UINTN  BarTop;

  if ((Sensor == NULL) || (Width < 32) || (Height < 12) || (Sensor->SampleCount == 0)) {
    return;
  }

  SampleCount = MIN (Sensor->SampleCount, MODERN_UI_HARDWARE_HEALTH_MAX_SAMPLES);
  MinValue    = Sensor->Samples[0];
  MaxValue    = Sensor->Samples[0];
  for (Index = 1; Index < SampleCount; Index++) {
    if (Sensor->Samples[Index] < MinValue) {
      MinValue = Sensor->Samples[Index];
    }

    if (Sensor->Samples[Index] > MaxValue) {
      MaxValue = Sensor->Samples[Index];
    }
  }

  Range = MaxValue - MinValue;
  if (Range < 1) {
    Range = 1;
  }

  ModernUiFillRect (Ui, (MODERN_UI_RECT){ X, Y, Width, Height }, ModernUiBlendColor (Theme->Surface, Theme->BackgroundBlack, 44));
  ModernUiStrokeRect (Ui, (MODERN_UI_RECT){ X, Y, Width, Height }, Theme->Border);
  ModernUiFillRect (Ui, (MODERN_UI_RECT){ X + 3, Y + (Height / 2), Width - 6, 1 }, ModernUiBlendColor (Theme->Border, Theme->Surface, 128));

  BarWidth = MAX (3, Width / ((SampleCount * 2) + 1));
  for (Index = 0; Index < SampleCount; Index++) {
    BarX      = X + 3 + ((Width - 6) * Index) / SampleCount;
    BarHeight = 3 + (((UINTN)(Sensor->Samples[Index] - MinValue) * (Height - 7)) / (UINTN)Range);
    BarTop    = Y + Height - 4 - BarHeight;
    ModernUiFillRect (
      Ui,
      (MODERN_UI_RECT){ BarX, BarTop, BarWidth, BarHeight },
      (Sensor->Samples[Index] >= Sensor->WarningValue) ? Theme->Warning : Theme->AccentOrange
      );
  }

  ModernUiFillRect (Ui, (MODERN_UI_RECT){ X, Y + Height - 2, Width, 1 }, Background);
}

STATIC
VOID
DrawHardwareHealthSensorRow (
  IN MODERN_UI_RENDER_CONTEXT                 *Ui,
  IN CONST MODERN_UI_THEME                    *Theme,
  IN UINTN                                    X,
  IN UINTN                                    Y,
  IN UINTN                                    Width,
  IN CONST MODERN_UI_HARDWARE_HEALTH_SENSOR  *Sensor,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL           Background
  )
{
  CHAR16  ValueText[48];
  UINTN   TrendX;
  UINTN   TrendWidth;

  if ((Sensor == NULL) || (Width < 80)) {
    return;
  }

  UnicodeSPrint (ValueText, sizeof (ValueText), L"%d %s", Sensor->CurrentValue, Sensor->Unit);
  ModernUiDrawTextFit (Ui, X, Y, (Width >= 420) ? 150 : (Width / 2), Sensor->Name, Theme->Text, Background);
  ModernUiDrawTextFit (Ui, X + ((Width >= 420) ? 156 : (Width / 2)), Y, 72, ValueText, Theme->AccentYellow, Background);

  if (Width >= 420) {
    TrendX     = X + 238;
    TrendWidth = Width - 238;
    DrawTemperatureTrendSparkline (Ui, Theme, TrendX, Y - 4, TrendWidth, 22, Sensor, Background);
  }
}

/**
  Draw the Power page with read-only ACPI and thermal provider state.

  @param[in] Ui     Initialized render context. Must not be NULL.
  @param[in] Theme  Theme token table. Must not be NULL.
  @param[in] Focus  Current focus area.
**/
STATIC
VOID
MODERN_SETUP_NOINLINE
DrawPower (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_FOCUS               Focus
  )
{
  MODERN_SETUP_PROVIDER_SNAPSHOT  Providers;
  MODERN_UI_POWER_SUMMARY         *Summary;
  MODERN_UI_HARDWARE_HEALTH_SUMMARY *Health;
  CONST CHAR16                    *Labels[5];
  CONST CHAR16                    *Values[5];
  CONST CHAR16                    *Groups[5];
  MODERN_UI_RECT                  Content;
  MODERN_UI_RECT                  Panel;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL   PanelBackground;
  UINTN                           Index;
  UINTN                           RowY;
  UINTN                           RowStep;
  CHAR16                          ProviderText[96];

  ModernSetupGetCachedProviderSnapshot (&Providers);
  Summary = &Providers.Power;
  Health  = &Providers.HardwareHealth;

  Groups[0] = NULL;
  Groups[1] = NULL;
  Groups[2] = NULL;
  Groups[3] = NULL;
  Groups[4] = NULL;

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

  Content         = ModernSetupContentRect (Ui);
  Panel           = (MODERN_UI_RECT){ Content.X, Content.Y, Content.Width, MIN (Content.Height, 520) };
  PanelBackground = ModernUiBlendColor (Theme->Surface, Theme->BackgroundBlack, 30);
  RowY            = Panel.Y + 58;
  RowStep         = 26;

  DrawProviderSummarySection (Ui, Theme, Panel, ModernUiGetString (ModernUiStringPowerThermal), TRUE);
  ModernUiDrawFocusFrame (Ui, Panel, (BOOLEAN)(Focus == SetupFocusContent), Theme);

  for (Index = 0; Index < ARRAY_SIZE (Labels); Index++) {
    if ((Groups[Index] != NULL) && ((RowY + 24) <= (Panel.Y + Panel.Height))) {
      DrawProviderSubsectionHeader (Ui, Theme, Panel.X + 22, RowY, Panel.Width - 44, Groups[Index]);
      RowY += 20;
    }

    if ((RowY + 24) > (Panel.Y + Panel.Height)) {
      return;
    }

    DrawProviderSummaryInfoRow (Ui, Theme, Panel.X + 22, RowY, Panel.Width - 44, Labels[Index], Values[Index]);
    RowY += RowStep;
  }

  if ((RowY + 54) > (Panel.Y + Panel.Height)) {
    return;
  }

  RowY += 8;
  DrawProviderSubsectionHeader (Ui, Theme, Panel.X + 22, RowY, Panel.Width - 44, L"Hardware Health");
  RowY += 22;
  UnicodeSPrint (
    ProviderText,
    sizeof (ProviderText),
    L"%s%s / %u sensor%s",
    Health->ProviderName,
    Health->DemoData ? L" (demo)" : L"",
    Health->SensorCount,
    (Health->SensorCount == 1) ? L"" : L"s"
    );
  DrawProviderSummaryInfoRow (Ui, Theme, Panel.X + 22, RowY, Panel.Width - 44, L"Source", ProviderText);
  RowY += RowStep;

  for (Index = 0; Index < MIN (Health->SensorCount, MODERN_UI_HARDWARE_HEALTH_MAX_SENSORS); Index++) {
    if ((RowY + 28) > (Panel.Y + Panel.Height)) {
      break;
    }

    DrawHardwareHealthSensorRow (Ui, Theme, Panel.X + 22, RowY, Panel.Width - 44, &Health->Sensors[Index], PanelBackground);
    RowY += 30;
  }
}

/**
  Draw the Performance page with read-only tuning provider availability.

  @param[in] Ui     Initialized render context. Must not be NULL.
  @param[in] Theme  Theme token table. Must not be NULL.
  @param[in] Focus  Current focus area.
**/
STATIC
VOID
MODERN_SETUP_NOINLINE
DrawPerformance (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_FOCUS               Focus
  )
{
  MODERN_SETUP_PROVIDER_SNAPSHOT    Providers;
  MODERN_UI_PERFORMANCE_SUMMARY     *Summary;
  MODERN_UI_PCIE_SUMMARY            *Pcie;
  CONST CHAR16                      *PcieUnavailable;
  CHAR16                            PcieFabric[64];
  CHAR16                            PcieInventory[64];
  CHAR16                            PcieNativePolicy[112];
  CHAR16                            PcieFabricPolicy[112];
  CHAR16                            PcieIsolation[112];
  CHAR16                            PcieDeviceCapabilities[96];
  CONST CHAR16                      *Labels[11];
  CONST CHAR16                      *Values[11];
  CONST CHAR16                      *Groups[11];

  ModernSetupGetCachedProviderSnapshot (&Providers);
  Summary         = &Providers.Performance;
  Pcie            = &Providers.Pcie;
  PcieUnavailable = ModernUiGetString (ModernUiStringNotAvailable);

  Groups[0]  = NULL;
  Groups[1]  = NULL;
  Groups[2]  = NULL;
  Groups[3]  = NULL;
  Groups[4]  = NULL;
  Groups[5]  = NULL;
  Groups[6]  = NULL;
  Groups[7]  = NULL;
  Groups[8]  = NULL;
  Groups[9]  = NULL;
  Groups[10] = NULL;

  Labels[0] = ModernUiGetString (ModernUiStringProcessorInventory);
  Groups[0] = ModernUiGetString (ModernUiStringGroupPerformance);
  Values[0] = CapabilityText (Summary->ProcessorInventoryPresent);
  Labels[1] = ModernUiGetString (ModernUiStringMemoryInventory);
  Values[1] = CapabilityText (Summary->MemoryInventoryPresent);
  Labels[2] = ModernUiGetString (ModernUiStringCpuIo2);
  Values[2] = CapabilityText (Summary->CpuIo2ProtocolPresent);
  Labels[3] = L"Virtualization Policy";
  Values[3] = CapabilityText (Summary->VirtualizationPolicyEntryPresent);
  Labels[4] = ModernUiGetString (ModernUiStringRasPolicy);
  Values[4] = CapabilityText (Summary->RasPolicyEntryPresent);
  UnicodeSPrint (
    PcieFabric,
    sizeof (PcieFabric),
    L"%u controllers / %u roots",
    Pcie->ControllerCount,
    Pcie->RootBridgeCount
    );
  UnicodeSPrint (
    PcieInventory,
    sizeof (PcieInventory),
    L"%u endpoints / %u bridges",
    Pcie->EndpointCount,
    Pcie->BridgeCount
    );
  UnicodeSPrint (
    PcieNativePolicy,
    sizeof (PcieNativePolicy),
    L"ReBAR %s / Above 4G %s / SR-IOV %s",
    CapabilityText (Pcie->ResizeBarPolicyEntryPresent),
    CapabilityText (Pcie->Above4GPolicyEntryPresent),
    CapabilityText (Pcie->SriovPolicyEntryPresent)
    );
  UnicodeSPrint (
    PcieFabricPolicy,
    sizeof (PcieFabricPolicy),
    L"ASPM %s / Bifurcation %s / Hot Plug %s",
    CapabilityText (Pcie->AspmPolicyEntryPresent),
    CapabilityText (Pcie->BifurcationPolicyEntryPresent),
    CapabilityText (Pcie->HotPlugPolicyEntryPresent)
    );
  UnicodeSPrint (
    PcieIsolation,
    sizeof (PcieIsolation),
    L"ACS %s / ARI %s / IOMMU %s",
    CapabilityText (Pcie->AcsPolicyEntryPresent),
    CapabilityText (Pcie->AriPolicyEntryPresent),
    CapabilityText (Pcie->IommuPolicyEntryPresent)
    );
  UnicodeSPrint (
    PcieDeviceCapabilities,
    sizeof (PcieDeviceCapabilities),
    L"ReBAR %u / SR-IOV %u / ASPM %u",
    Pcie->ResizableBarDeviceCount,
    Pcie->SriovDeviceCount,
    Pcie->AspmCapableLinkCount
    );
  Labels[5] = L"PCIe Fabric";
  Groups[5] = ModernUiGetString (ModernUiStringGroupPowerPerformance);
  Values[5] = EFI_ERROR (Providers.PcieStatus) ? PcieUnavailable : PcieFabric;
  Labels[6] = L"PCIe Inventory";
  Values[6] = EFI_ERROR (Providers.PcieStatus) ? PcieUnavailable : PcieInventory;
  Labels[7] = L"Native Policy";
  Values[7] = EFI_ERROR (Providers.PcieStatus) ? PcieUnavailable : PcieNativePolicy;
  Labels[8] = L"Fabric Policy";
  Values[8] = EFI_ERROR (Providers.PcieStatus) ? PcieUnavailable : PcieFabricPolicy;
  Labels[9] = L"Isolation";
  Values[9] = EFI_ERROR (Providers.PcieStatus) ? PcieUnavailable : PcieIsolation;
  Labels[10] = L"Device Caps";
  Values[10] = EFI_ERROR (Providers.PcieStatus) ? PcieUnavailable : PcieDeviceCapabilities;

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
  Return a capability value only when its provider snapshot status is usable.

  @param[in] Status   Provider status from MODERN_SETUP_PROVIDER_SNAPSHOT.
  @param[in] Present  Capability bit from the provider snapshot.

  @return Non-NULL localized capability text, or Unknown for failed providers.
**/
STATIC
CONST CHAR16 *
SnapshotCapabilityText (
  IN EFI_STATUS  Status,
  IN BOOLEAN     Present
  )
{
  if (EFI_ERROR (Status)) {
    return ModernUiGetString (ModernUiStringUnknown);
  }

  return CapabilityText (Present);
}

/**
  Draw the Quick Settings page: high-churn platform knobs grouped by domain.

  Tier-B prototype per Docs/ConfigurableItemsAndQuickSettings.md. This curates
  the read-only policy-entry presence hints the providers already discover
  (SR-IOV, Above-4G, ASPM, VT-d/IOMMU, RAS, Secure Boot, ...) into one screen so
  a user can see, at a glance, which high-churn settings this platform exposes.
  It is intentionally read-only: changing any of these stays in native
  FormBrowser. (Per-row SendForm deep-link is a follow-up slice.)

  @param[in] Ui     Initialized render context. Must not be NULL.
  @param[in] Theme  Theme token table. Must not be NULL.
  @param[in] Focus  Current focus area.
**/
STATIC
VOID
MODERN_SETUP_NOINLINE
DrawQuickSettings (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_FOCUS               Focus
  )
{
  MODERN_SETUP_PROVIDER_SNAPSHOT  Providers;
  MODERN_UI_RECT                  Content;
  MODERN_UI_RECT                  Panel;
  MODERN_UI_RECT                  Column;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL   Background;
  UINTN                           LeftWidth;
  UINTN                           RightWidth;
  UINTN                           RowY;
  CONST CHAR16                    *SecureBootText;

  ModernSetupGetCachedProviderSnapshot (&Providers);

  SecureBootText = (Providers.Security.SecureBoot == ModernUiSecurityStateEnabled) ? ModernUiGetString (ModernUiStringEnabled) :
                   ((Providers.Security.SecureBoot == ModernUiSecurityStateDisabled) ? ModernUiGetString (ModernUiStringDisabled) : ModernUiGetString (ModernUiStringUnknown));

  Content    = ModernSetupContentRect (Ui);
  Panel      = (MODERN_UI_RECT){ Content.X, Content.Y, Content.Width, MIN (Content.Height, 540) };
  Background = ModernUiBlendColor (Theme->Surface, Theme->BackgroundBlack, 30);
  DrawProviderSummarySection (Ui, Theme, Panel, L"Quick Settings", TRUE);
  ModernUiDrawFocusFrame (Ui, Panel, (BOOLEAN)(Focus == SetupFocusContent), Theme);
  ModernUiDrawTextFit (
    Ui,
    Panel.X + 22,
    Panel.Y + 38,
    Panel.Width - 44,
    L"Read-only entry points to high-churn settings. Change them in native setup.",
    Theme->MutedText,
    Background
    );

  if (Panel.Width >= 720) {
    LeftWidth  = (Panel.Width - 60) / 2;
    RightWidth = Panel.Width - 60 - LeftWidth;
  } else {
    LeftWidth  = Panel.Width - 44;
    RightWidth = 0;
  }

  Column = (MODERN_UI_RECT){ Panel.X + 22, Panel.Y + 70, LeftWidth, Panel.Height - 86 };
  RowY   = Column.Y;
  DrawProviderSubsectionHeader (Ui, Theme, Column.X, RowY, Column.Width, L"Virtualization & Isolation");
  RowY += 22;
  DrawProviderSummaryInfoRow (Ui, Theme, Column.X, RowY, Column.Width, L"VT-d / IOMMU", SnapshotCapabilityText (Providers.PcieStatus, (BOOLEAN)(Providers.Pcie.IommuPolicyEntryPresent || Providers.Pcie.IoMmuProtocolPresent)));
  RowY += 26;
  DrawProviderSummaryInfoRow (Ui, Theme, Column.X, RowY, Column.Width, L"SR-IOV", SnapshotCapabilityText (Providers.PcieStatus, Providers.Pcie.SriovPolicyEntryPresent));
  RowY += 26;
  DrawProviderSummaryInfoRow (Ui, Theme, Column.X, RowY, Column.Width, L"ACS / ARI", SnapshotCapabilityText (Providers.PcieStatus, (BOOLEAN)(Providers.Pcie.AcsPolicyEntryPresent || Providers.Pcie.AriPolicyEntryPresent)));
  RowY += 26;
  DrawProviderSummaryInfoRow (Ui, Theme, Column.X, RowY, Column.Width, L"CPU virtualization", SnapshotCapabilityText (Providers.PerformanceStatus, Providers.Performance.VirtualizationPolicyEntryPresent));
  RowY += 36;

  DrawProviderSubsectionHeader (Ui, Theme, Column.X, RowY, Column.Width, L"Security");
  RowY += 22;
  DrawProviderSummaryInfoRow (Ui, Theme, Column.X, RowY, Column.Width, ModernUiGetString (ModernUiStringSecureBoot), SecureBootText);
  RowY += 26;
  DrawProviderSummaryInfoRow (Ui, Theme, Column.X, RowY, Column.Width, L"TPM (TCG2)", SnapshotCapabilityText (Providers.SecurityStatus, (BOOLEAN)(Providers.Security.Tcg2Protocol == ModernUiSecurityStateEnabled)));

  if (RightWidth == 0) {
    return;
  }

  Column = (MODERN_UI_RECT){ Panel.X + 38 + LeftWidth, Panel.Y + 70, RightWidth, Panel.Height - 86 };
  RowY   = Column.Y;
  DrawProviderSubsectionHeader (Ui, Theme, Column.X, RowY, Column.Width, L"PCIe Resource");
  RowY += 22;
  DrawProviderSummaryInfoRow (Ui, Theme, Column.X, RowY, Column.Width, L"Above 4G Decoding", SnapshotCapabilityText (Providers.PcieStatus, Providers.Pcie.Above4GPolicyEntryPresent));
  RowY += 26;
  DrawProviderSummaryInfoRow (Ui, Theme, Column.X, RowY, Column.Width, L"Resizable BAR", SnapshotCapabilityText (Providers.PcieStatus, Providers.Pcie.ResizeBarPolicyEntryPresent));
  RowY += 26;
  DrawProviderSummaryInfoRow (Ui, Theme, Column.X, RowY, Column.Width, L"ASPM", SnapshotCapabilityText (Providers.PcieStatus, Providers.Pcie.AspmPolicyEntryPresent));
  RowY += 26;
  DrawProviderSummaryInfoRow (Ui, Theme, Column.X, RowY, Column.Width, L"Hot-Plug", SnapshotCapabilityText (Providers.PcieStatus, Providers.Pcie.HotPlugPolicyEntryPresent));
  RowY += 36;

  DrawProviderSubsectionHeader (Ui, Theme, Column.X, RowY, Column.Width, L"Tuning / Serviceability");
  RowY += 22;
  DrawProviderSummaryInfoRow (Ui, Theme, Column.X, RowY, Column.Width, L"RAS policy", SnapshotCapabilityText (Providers.PerformanceStatus, Providers.Performance.RasPolicyEntryPresent));
  RowY += 26;
  DrawProviderSummaryInfoRow (Ui, Theme, Column.X, RowY, Column.Width, L"PCIe config page", SnapshotCapabilityText (Providers.PcieStatus, Providers.Pcie.PciePolicyEntryPresent));
}

/**
  Draw a compact read-only Server Inventory summary from app provider snapshots.

  This page is intentionally view-only. Native HII/FormBrowser owns policy
  changes; ModernSetupApp only presents normalized provider snapshot data.

  @param[in] Ui     Initialized render context. Must not be NULL.
  @param[in] Theme  Theme token table. Must not be NULL.
  @param[in] Focus  Current focus area.
**/
STATIC
VOID
MODERN_SETUP_NOINLINE
DrawServerInventorySummary (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_FOCUS               Focus
  )
{
  MODERN_SETUP_PROVIDER_SNAPSHOT        Providers;
  MODERN_SETUP_PROVIDER_HEALTH_SUMMARY  ProviderHealth;
  MODERN_UI_RECT                        Content;
  MODERN_UI_RECT                        Panel;
  MODERN_UI_RECT                        Column;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL         Background;
  UINTN                                 LeftWidth;
  UINTN                                 RightWidth;
  UINTN                                 RowY;
  UINTN                                 PcieIndex;
  CHAR16                                PcieMoreText[48];
  CHAR16                                StorageText[72];
  CHAR16                                NetworkText[72];
  CHAR16                                MemoryText[48];
  CHAR16                                ProviderCoverage[64];
  CHAR16                                ProviderIssue[96];
  CHAR16                                SmbiosAcpiText[64];
  CHAR16                                FabricText[80];
  CHAR16                                DeviceText[80];
  CHAR16                                IsolationText[112];
  CHAR16                                NativePolicyText[112];
  CONST CHAR16                          *UnknownText;

  ModernSetupGetCachedProviderSnapshot (&Providers);
  ModernSetupGetProviderHealthSummary (&Providers, &ProviderHealth);

  UnknownText = ModernUiGetString (ModernUiStringUnknown);
  if (EFI_ERROR (Providers.PlatformStatus)) {
    UnicodeSPrint (MemoryText, sizeof (MemoryText), L"%s", UnknownText);
  } else {
    UnicodeSPrint (MemoryText, sizeof (MemoryText), L"%lu MB", Providers.Platform.MemorySizeMb);
  }

  UnicodeSPrint (ProviderCoverage, sizeof (ProviderCoverage), L"%u/%u ready", ProviderHealth.ReadyProviders, ProviderHealth.TotalProviders);
  if (ProviderHealth.State == ModernSetupProviderHealthReady) {
    UnicodeSPrint (ProviderIssue, sizeof (ProviderIssue), L"None");
  } else {
    UnicodeSPrint (ProviderIssue, sizeof (ProviderIssue), L"%s (%r)", ProviderHealth.FirstIssueName, ProviderHealth.FirstIssueStatus);
  }

  if (EFI_ERROR (Providers.DiagnosticsStatus)) {
    UnicodeSPrint (SmbiosAcpiText, sizeof (SmbiosAcpiText), L"%s", UnknownText);
  } else {
    UnicodeSPrint (
      SmbiosAcpiText,
      sizeof (SmbiosAcpiText),
      L"SMBIOS %s / ACPI %s",
      CapabilityText (Providers.Diagnostics.SmbiosPresent),
      CapabilityText (Providers.Diagnostics.AcpiPresent)
      );
  }

  if (EFI_ERROR (Providers.PcieStatus)) {
    UnicodeSPrint (FabricText, sizeof (FabricText), L"%s", UnknownText);
    UnicodeSPrint (DeviceText, sizeof (DeviceText), L"%s", UnknownText);
    UnicodeSPrint (IsolationText, sizeof (IsolationText), L"%s", UnknownText);
    UnicodeSPrint (NativePolicyText, sizeof (NativePolicyText), L"%s", UnknownText);
  } else {
    UnicodeSPrint (
      FabricText,
      sizeof (FabricText),
      L"%u controllers / %u roots",
      Providers.Pcie.ControllerCount,
      Providers.Pcie.RootBridgeCount
      );
    UnicodeSPrint (
      DeviceText,
      sizeof (DeviceText),
      L"%u endpoints / %u bridges",
      Providers.Pcie.EndpointCount,
      Providers.Pcie.BridgeCount
      );
    UnicodeSPrint (
      IsolationText,
      sizeof (IsolationText),
      L"IOMMU %s / ACS %u / ARI %u",
      CapabilityText (Providers.Pcie.IommuPolicyEntryPresent || Providers.Pcie.IoMmuProtocolPresent),
      Providers.Pcie.AcsDeviceCount,
      Providers.Pcie.AriDeviceCount
      );
    UnicodeSPrint (
      NativePolicyText,
      sizeof (NativePolicyText),
      L"SR-IOV %u / ReBAR %u / 4G %s",
      Providers.Pcie.SriovDeviceCount,
      Providers.Pcie.ResizableBarDeviceCount,
      CapabilityText (Providers.Pcie.Above4GPolicyEntryPresent)
      );
  }

  //
  // Compact storage / network digest: count plus the first device identity,
  // read from the inventory provider (BlockIo/DiskInfo and SimpleNetwork).
  //
  if (EFI_ERROR (Providers.InventoryStatus) || (Providers.Inventory.StorageCount == 0)) {
    UnicodeSPrint (StorageText, sizeof (StorageText), L"%s", (Providers.Inventory.StorageCount == 0) ? L"None" : UnknownText);
  } else if (Providers.Inventory.StorageCount == 1) {
    UnicodeSPrint (StorageText, sizeof (StorageText), L"%s", Providers.Inventory.Storage[0].Label);
  } else {
    UnicodeSPrint (StorageText, sizeof (StorageText), L"%u disks, %s +%u", (UINT32)Providers.Inventory.StorageCount, Providers.Inventory.Storage[0].Label, (UINT32)(Providers.Inventory.StorageCount - 1));
  }

  if (EFI_ERROR (Providers.InventoryStatus) || (Providers.Inventory.NicCount == 0)) {
    UnicodeSPrint (NetworkText, sizeof (NetworkText), L"%s", (Providers.Inventory.NicCount == 0) ? L"None" : UnknownText);
  } else if (Providers.Inventory.NicCount == 1) {
    UnicodeSPrint (NetworkText, sizeof (NetworkText), L"%s", Providers.Inventory.Nic[0].Label);
  } else {
    UnicodeSPrint (NetworkText, sizeof (NetworkText), L"%u NICs, %s +%u", (UINT32)Providers.Inventory.NicCount, Providers.Inventory.Nic[0].Label, (UINT32)(Providers.Inventory.NicCount - 1));
  }

  Content    = ModernSetupContentRect (Ui);
  Panel      = (MODERN_UI_RECT){ Content.X, Content.Y, Content.Width, MIN (Content.Height, 540) };
  Background = ModernUiBlendColor (Theme->Surface, Theme->BackgroundBlack, 30);
  DrawProviderSummarySection (Ui, Theme, Panel, L"Asset Summary", TRUE);
  ModernUiDrawFocusFrame (Ui, Panel, (BOOLEAN)(Focus == SetupFocusContent), Theme);
  ModernUiDrawTextFit (
    Ui,
    Panel.X + 22,
    Panel.Y + 38,
    Panel.Width - 44,
    L"Read-only. Native HII/FormBrowser owns policy changes.",
    Theme->MutedText,
    Background
    );

  if (Panel.Width >= 720) {
    LeftWidth  = (Panel.Width - 60) / 2;
    RightWidth = Panel.Width - 60 - LeftWidth;
  } else {
    LeftWidth  = Panel.Width - 44;
    RightWidth = 0;
  }

  Column = (MODERN_UI_RECT){ Panel.X + 22, Panel.Y + 70, LeftWidth, Panel.Height - 86 };
  RowY   = Column.Y;
  DrawProviderSubsectionHeader (Ui, Theme, Column.X, RowY, Column.Width, L"Platform");
  RowY += 22;
  DrawProviderSummaryInfoRow (Ui, Theme, Column.X, RowY, Column.Width, ModernUiGetString (ModernUiStringFirmwareVendor), EFI_ERROR (Providers.PlatformStatus) ? UnknownText : Providers.Platform.FirmwareVendor);
  RowY += 26;
  DrawProviderSummaryInfoRow (Ui, Theme, Column.X, RowY, Column.Width, ModernUiGetString (ModernUiStringFirmwareRevision), EFI_ERROR (Providers.FirmwareStatus) ? (EFI_ERROR (Providers.PlatformStatus) ? UnknownText : Providers.Platform.FirmwareRevision) : Providers.Firmware.Revision);
  RowY += 26;
  DrawProviderSummaryInfoRow (Ui, Theme, Column.X, RowY, Column.Width, L"Architecture", EFI_ERROR (Providers.PlatformStatus) ? UnknownText : Providers.Platform.Architecture);
  RowY += 26;
  DrawProviderSummaryInfoRow (Ui, Theme, Column.X, RowY, Column.Width, L"Memory", MemoryText);
  RowY += 36;

  DrawProviderSubsectionHeader (Ui, Theme, Column.X, RowY, Column.Width, L"Management");
  RowY += 22;
  DrawProviderSummaryInfoRow (Ui, Theme, Column.X, RowY, Column.Width, ModernUiGetString (ModernUiStringIpmi), SnapshotCapabilityText (Providers.ManagementStatus, Providers.Management.IpmiProtocolPresent));
  RowY += 26;
  DrawProviderSummaryInfoRow (Ui, Theme, Column.X, RowY, Column.Width, ModernUiGetString (ModernUiStringRedfish), SnapshotCapabilityText (Providers.ManagementStatus, Providers.Management.RedfishDiscoverPresent));
  RowY += 26;
  DrawProviderSummaryInfoRow (Ui, Theme, Column.X, RowY, Column.Width, L"SMBIOS host interface", SnapshotCapabilityText (Providers.ManagementStatus, Providers.Management.SmbiosManagementInterfacePresent));
  RowY += 36;

  DrawProviderSubsectionHeader (Ui, Theme, Column.X, RowY, Column.Width, L"Serviceability");
  RowY += 22;
  DrawProviderSummaryInfoRow (Ui, Theme, Column.X, RowY, Column.Width, L"SMBIOS / ACPI", SmbiosAcpiText);
  RowY += 26;
  DrawProviderSummaryInfoRow (Ui, Theme, Column.X, RowY, Column.Width, L"Provider state", ModernSetupGetProviderHealthStateText (ProviderHealth.State));
  RowY += 26;
  DrawProviderSummaryInfoRow (Ui, Theme, Column.X, RowY, Column.Width, L"Coverage", ProviderCoverage);
  RowY += 26;
  DrawProviderSummaryInfoRow (Ui, Theme, Column.X, RowY, Column.Width, L"First issue", ProviderIssue);
  RowY += 26;
  DrawProviderSummaryInfoRow (Ui, Theme, Column.X, RowY, Column.Width, ModernUiGetString (ModernUiStringRasPolicy), SnapshotCapabilityText (Providers.PerformanceStatus, Providers.Performance.RasPolicyEntryPresent));
  RowY += 26;
  DrawProviderSummaryInfoRow (Ui, Theme, Column.X, RowY, Column.Width, L"Storage", StorageText);
  RowY += 26;
  DrawProviderSummaryInfoRow (Ui, Theme, Column.X, RowY, Column.Width, L"Network", NetworkText);

  if (RightWidth == 0) {
    return;
  }

  Column = (MODERN_UI_RECT){ Panel.X + 38 + LeftWidth, Panel.Y + 70, RightWidth, Panel.Height - 86 };
  RowY   = Column.Y;
  DrawProviderSubsectionHeader (Ui, Theme, Column.X, RowY, Column.Width, L"Processor");
  RowY += 22;
  DrawProviderSummaryInfoRow (Ui, Theme, Column.X, RowY, Column.Width, L"Model", (EFI_ERROR (Providers.PlatformStatus) || (Providers.Platform.Processor[0] == L'\0')) ? UnknownText : Providers.Platform.Processor);
  RowY += 26;
  DrawProviderSummaryInfoRow (Ui, Theme, Column.X, RowY, Column.Width, L"Speed", (Providers.Platform.ProcessorSpeed[0] != L'\0') ? Providers.Platform.ProcessorSpeed : UnknownText);
  RowY += 26;
  DrawProviderSummaryInfoRow (Ui, Theme, Column.X, RowY, Column.Width, L"Cache", (Providers.Platform.Cache[0] != L'\0') ? Providers.Platform.Cache : UnknownText);
  RowY += 26;
  DrawProviderSummaryInfoRow (Ui, Theme, Column.X, RowY, Column.Width, L"Logical CPUs", (Providers.Platform.LogicalProcessors[0] != L'\0') ? Providers.Platform.LogicalProcessors : UnknownText);
  RowY += 36;
  DrawProviderSubsectionHeader (Ui, Theme, Column.X, RowY, Column.Width, L"PCIe Fabric");
  RowY += 22;
  DrawProviderSummaryInfoRow (Ui, Theme, Column.X, RowY, Column.Width, L"Controllers / Roots", FabricText);
  RowY += 26;
  DrawProviderSummaryInfoRow (Ui, Theme, Column.X, RowY, Column.Width, L"Endpoints / Bridges", DeviceText);
  RowY += 26;
  DrawProviderSummaryInfoRow (Ui, Theme, Column.X, RowY, Column.Width, L"Isolation caps", IsolationText);
  RowY += 26;
  DrawProviderSummaryInfoRow (Ui, Theme, Column.X, RowY, Column.Width, L"Policy caps", NativePolicyText);
  RowY += 36;
  DrawProviderSubsectionHeader (Ui, Theme, Column.X, RowY, Column.Width, L"PCIe Devices");
  RowY += 24;
  if (EFI_ERROR (Providers.PcieStatus) || (Providers.Pcie.DeviceCount == 0)) {
    ModernUiDrawTextFit (Ui, Column.X, RowY, Column.Width, UnknownText, Theme->MutedText, Background);
  } else {
    //
    // List up to MODERN_SETUP_SYSINFO_PCIE_ROWS device identities; summarize the
    // remainder so the read-only inventory stays within the panel.
    //
    for (PcieIndex = 0; (PcieIndex < Providers.Pcie.DeviceCount) && (PcieIndex < MODERN_SETUP_SYSINFO_PCIE_ROWS); PcieIndex++) {
      ModernUiDrawTextFit (Ui, Column.X, RowY, Column.Width, Providers.Pcie.Devices[PcieIndex].Label, Theme->Text, Background);
      RowY += 22;
    }

    if (Providers.Pcie.DeviceCount > MODERN_SETUP_SYSINFO_PCIE_ROWS) {
      UnicodeSPrint (
        PcieMoreText,
        sizeof (PcieMoreText),
        L"+%u more (%u total)",
        (UINT32)(Providers.Pcie.DeviceCount - MODERN_SETUP_SYSINFO_PCIE_ROWS),
        (UINT32)Providers.Pcie.DeviceCount
        );
      ModernUiDrawTextFit (Ui, Column.X, RowY, Column.Width, PcieMoreText, Theme->MutedText, Background);
    }
  }
}

/**
  Draw the app-owned Preferences page.

  @param[in] Ui        Initialized render context. Must not be NULL.
  @param[in] Theme     Theme token table. Must not be NULL.
  @param[in] Focus     Current focus area.
  @param[in] Selected  Selected preference row. Values 0..2 are expected.
**/
STATIC
VOID
MODERN_SETUP_NOINLINE
DrawPreferences (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_FOCUS               Focus,
  IN UINTN                     Selected
  )
{
  CONST CHAR16           *Prompts[MODERN_SETUP_PREFERENCE_ROW_COUNT];
  UINTN                  Index;
  UINTN                  Y;
  BOOLEAN                IsSelected;
  MODERN_UI_RECT         Panel;
  CONST CHAR16           *Hint;
  UINTN                  RowX;
  UINTN                  RowWidth;
  UINTN                  ValueWidth;
  UINTN                  ChoiceCount;
  UINTN                  Choice;
  UINTN                  PopupX;
  UINTN                  PopupY;
  MODERN_UI_ROW_MODEL    RowModel;
  MODERN_UI_POPUP_MODEL  PopupModel;

  Prompts[MODERN_SETUP_PREFERENCE_ROW_THEME]                  = L"Theme";
  Prompts[MODERN_SETUP_PREFERENCE_ROW_DASHBOARD_DENSITY]      = ModernUiGetString (ModernUiStringPreferenceDensity);
  Prompts[MODERN_SETUP_PREFERENCE_ROW_BOOT_TIMEOUT]           = L"Boot countdown";
  Prompts[MODERN_SETUP_PREFERENCE_ROW_PROFILE_NAME]           = L"Profile name";
  Prompts[MODERN_SETUP_PREFERENCE_ROW_REMEMBER_LAST_PAGE]     = ModernUiGetString (ModernUiStringPreferenceRememberLastPage);
  Prompts[MODERN_SETUP_PREFERENCE_ROW_SHOW_ADVANCED_HINTS]    = ModernUiGetString (ModernUiStringPreferenceShowAdvancedHints);
  Prompts[MODERN_SETUP_PREFERENCE_ROW_CONFIRM_RESET]          = ModernUiGetString (ModernUiStringPreferenceConfirmReset);

  Panel = ModernSetupContentRect (Ui);
  RowX = Panel.X + 26;
  RowWidth = Panel.Width - 52;
  ValueWidth = 240;
  ModernUiDrawPanel (Ui, Panel, Theme);
  ModernUiDrawFocusFrame (Ui, Panel, (BOOLEAN)(Focus == SetupFocusContent), Theme);
  ModernUiDrawText (Ui, Panel.X + 20, Panel.Y + 20, ModernUiGetString (ModernUiStringPreferencesInstruction), Theme->MutedText, Theme->Surface);

  for (Index = 0; Index < MODERN_SETUP_PREFERENCE_ROW_COUNT; Index++) {
    Y = Panel.Y + 72 + Index * 42;
    IsSelected = (BOOLEAN)((Focus == SetupFocusContent) && (Index == Selected));
    RowModel.Rect      = (MODERN_UI_RECT){ RowX, Y - 8, RowWidth, 34 };
    RowModel.Prompt    = Prompts[Index];
    RowModel.Value     = ModernSetupGetPreferenceValueName (Index);
    RowModel.Role      = IsSelected ? ModernUiRowSelected : ModernUiRowNormal;
    if (Index <= MODERN_SETUP_PREFERENCE_ROW_DASHBOARD_DENSITY) {
      RowModel.ValueType = ModernUiValueOneOf;
    } else if (Index == MODERN_SETUP_PREFERENCE_ROW_BOOT_TIMEOUT) {
      RowModel.ValueType = ModernUiValueNumeric;
    } else if (Index == MODERN_SETUP_PREFERENCE_ROW_PROFILE_NAME) {
      RowModel.ValueType = ModernUiValueString;
    } else {
      RowModel.ValueType = ModernUiValueCheckbox;
    }
    ModernUiEngineDrawRows (Ui, &RowModel, 1, Theme);
  }

  if (mModernSetupPreferencePopupOpen) {
    PopupX      = RowX + RowWidth - ValueWidth - 12;
    PopupY      = Panel.Y + 72 + (mModernSetupPreferencePopupRow + 1) * 42 - 8;
    if (mModernSetupPreferencePopupKind == ModernSetupPreferencePopupChoice) {
      ChoiceCount = ModernSetupGetPreferenceChoiceCount (mModernSetupPreferencePopupRow);
      PopupModel.Rect  = (MODERN_UI_RECT){ PopupX, PopupY, ValueWidth, 40 + ChoiceCount * 34 };
      PopupModel.Title = ModernSetupGetPreferenceValueName (mModernSetupPreferencePopupRow);
      ModernUiEngineDrawPopup (Ui, &PopupModel, Theme);

      for (Choice = 0; Choice < ChoiceCount; Choice++) {
        IsSelected = (BOOLEAN)(Choice == mModernSetupPreferencePopupSelection);
        RowModel.Rect      = (MODERN_UI_RECT){ PopupX + 6, PopupY + 28 + Choice * 34, ValueWidth - 12, 30 };
        RowModel.Prompt    = ModernSetupGetPreferenceChoiceName (mModernSetupPreferencePopupRow, Choice);
        RowModel.Value     = NULL;
        RowModel.Role      = IsSelected ? ModernUiRowSelected : ModernUiRowNormal;
        RowModel.ValueType = ModernUiValueNone;
        ModernUiEngineDrawRows (Ui, &RowModel, 1, Theme);
      }
    } else {
      PopupModel.Rect  = (MODERN_UI_RECT){ PopupX, PopupY, ValueWidth, 118 };
      PopupModel.Title = Prompts[mModernSetupPreferencePopupRow];
      ModernUiEngineDrawPopup (Ui, &PopupModel, Theme);

      Hint = (mModernSetupPreferencePopupKind == ModernSetupPreferencePopupNumericInput) ?
             L"Digits only, range 0..30. Enter saves, Esc cancels." :
             L"Printable ASCII, max 31 chars. Enter saves, Esc cancels.";
      RowModel.Rect      = (MODERN_UI_RECT){ PopupX + 8, PopupY + 36, ValueWidth - 16, 34 };
      RowModel.Prompt    = mModernSetupPreferenceInputBuffer;
      RowModel.Value     = NULL;
      RowModel.Role      = ModernUiRowSelected;
      RowModel.ValueType = (mModernSetupPreferencePopupKind == ModernSetupPreferencePopupNumericInput) ? ModernUiValueNumeric : ModernUiValueString;
      ModernUiEngineDrawRows (Ui, &RowModel, 1, Theme);
      ModernUiDrawTextFit (Ui, PopupX + 12, PopupY + 80, ValueWidth - 24, Hint, Theme->MutedText, Theme->Surface);
    }
  }
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
MODERN_SETUP_NOINLINE
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
  ValueWidth = MODERN_SETUP_EXIT_VALUE_WIDTH;
  ModernUiDrawPanel (Ui, Panel, Theme);
  ModernUiDrawFocusFrame (Ui, Panel, (BOOLEAN)(Focus == SetupFocusContent), Theme);
  ModernUiDrawText (Ui, Panel.X + 20, Panel.Y + 20, ModernUiGetString (ModernUiStringExitInstruction), Theme->MutedText, Theme->Surface);

  for (Index = 0; Index < ARRAY_SIZE (Items); Index++) {
    Y = Panel.Y + MODERN_SETUP_EXIT_ROW_TOP + Index * MODERN_SETUP_EXIT_ROW_STRIDE;
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
    DropdownY = Panel.Y + MODERN_SETUP_EXIT_ROW_TOP + MODERN_SETUP_EXIT_ROW_COUNT * MODERN_SETUP_EXIT_ROW_STRIDE - 8;
    PopupModel.Rect  = (MODERN_UI_RECT){ DropdownX, DropdownY, ValueWidth, 14 + MODERN_SETUP_LANGUAGE_OPTION_COUNT * 34 };
    PopupModel.Title = NULL;
    ModernUiEngineDrawPopup (Ui, &PopupModel, Theme);

    for (Option = 0; Option < MODERN_SETUP_LANGUAGE_OPTION_COUNT; Option++) {
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
MODERN_SETUP_NOINLINE
ModernSetupDrawCurrentPage (
  IN MODERN_UI_RENDER_CONTEXT  *Ui,
  IN CONST MODERN_UI_THEME     *Theme,
  IN SETUP_PAGE                Page,
  IN SETUP_FOCUS               Focus,
  IN UINTN                     DashboardSelection,
  IN UINTN                     BootSelection,
  IN UINTN                     DeviceSelection,
  IN UINTN                     PreferencesSelection,
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
    case PageSystemInfo:
      DrawSystemInfo (Ui, Theme, Focus);
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
    case PageQuickSettings:
      DrawQuickSettings (Ui, Theme, Focus);
      break;
    case PageServerInventory:
      DrawServerInventorySummary (Ui, Theme, Focus);
      break;
    case PagePreferences:
      DrawPreferences (Ui, Theme, Focus, PreferencesSelection);
      break;
    case PageExit:
      DrawExit (Ui, Theme, Focus, ExitSelection);
      break;
    default:
      break;
  }

  ModernSetupDrawFooter (Ui, Theme, Focus, StatusMessage);
}
