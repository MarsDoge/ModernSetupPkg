/** @file Native staged-change review presentation; never writes settings.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include "FormDisplay.h"
#include <Protocol/ModernSetupChangeReview.h>
#include <ModernUi/ModernUiString.h>
#include <ModernUi/ModernUiRenderer.h>
#include <ModernUi/ModernUiInput.h>

#define REVIEW_TEXT_LIMIT  4096
#define REVIEW_LINE_CHARS  160
#define REVIEW_LINE_HEIGHT 28

// A path spans both columns; value rows pair the baseline with the pending value.
// All text is wrapped, never ellipsized. Long entries can continue on later pages.
typedef struct {
  BOOLEAN Path;
  CHAR16  Left[REVIEW_LINE_CHARS];
  CHAR16  Right[REVIEW_LINE_CHARS];
} REVIEW_LINE;

STATIC EFI_GUID mReviewGuid = MODERN_SETUP_CHANGE_REVIEW_PROTOCOL_GUID;

STATIC BOOLEAN
ReviewValidText (CONST CHAR16 *Text)
{
  UINTN Index;
  if (Text == NULL) {
    return FALSE;
  }
  for (Index = 0; Index <= REVIEW_TEXT_LIMIT; Index++) {
    if (Text[Index] == 0) {
      return TRUE;
    }
  }
  return FALSE;
}

// Redaction is checked BEFORE any inspection, including length measurement.
STATIC CONST CHAR16 *
ReviewValue (CONST MODERN_SETUP_REVIEW_ITEM *Item, BOOLEAN Old, BOOLEAN Chinese)
{
  CONST CHAR16 *Value;
  if (((Item->Flags & (MODERN_SETUP_REVIEW_REDACTED | MODERN_SETUP_REVIEW_OPAQUE)) != 0) ||
      ((Item->Flags & MODERN_SETUP_REVIEW_BASELINE_KNOWN) == 0)) {
    return Chinese ? L"[受保护]" : L"[protected]";
  }
  Value = Old ? Item->OldValue : Item->NewValue;
  return Value != NULL ? Value : (Chinese ? L"[受保护]" : L"[protected]");
}

STATIC VOID
ReviewWrap (CONST CHAR16 **Text, CHAR16 *Line, UINTN Width)
{
  CHAR16 Glyph[2];
  UINTN  Count, Used, Size;
  Count = 0;
  Used = 0;
  Glyph[1] = 0;
  while (**Text != 0 && Count + 1 < REVIEW_LINE_CHARS) {
    Glyph[0] = **Text;
    // HII width controls and bidi controls must not change the review layout.
    if (Glyph[0] < L' ' || Glyph[0] == 0x7f || Glyph[0] >= 0xfff0 ||
        (Glyph[0] >= 0x202a && Glyph[0] <= 0x202e) ||
        (Glyph[0] >= 0x2066 && Glyph[0] <= 0x2069)) {
      Glyph[0] = L' ';
    }
    Size = ModernUiMeasureText (Glyph);
    if (Count != 0 && Used + Size > Width) {
      break;
    }
    Line[Count++] = Glyph[0];
    Used += Size;
    (*Text)++;
  }
  Line[Count] = 0;
}

// Run once to count and once to materialize, keeping allocations bounded by data.
STATIC UINTN
ReviewMakeLines (
  CONST MODERN_SETUP_REVIEW_SNAPSHOT *Snapshot,
  BOOLEAN Chinese,
  UINTN Width,
  REVIEW_LINE *Lines OPTIONAL
  )
{
  UINTN I, Count;
  CONST CHAR16 *Path, *Old, *New;
  REVIEW_LINE Line;
  Count = 0;
  for (I = 0; I < Snapshot->ItemCount; I++) {
    Path = Snapshot->Items[I].Path;
    do {
      ZeroMem (&Line, sizeof (Line));
      Line.Path = TRUE;
      ReviewWrap (&Path, Line.Left, Width);
      if (Lines != NULL) { Lines[Count] = Line; }
      Count++;
    } while (*Path != 0);
    Old = ReviewValue (&Snapshot->Items[I], TRUE, Chinese);
    New = ReviewValue (&Snapshot->Items[I], FALSE, Chinese);
    do {
      ZeroMem (&Line, sizeof (Line));
      ReviewWrap (&Old, Line.Left, (Width - 40) / 2);
      ReviewWrap (&New, Line.Right, (Width - 40) / 2);
      if (Lines != NULL) { Lines[Count] = Line; }
      Count++;
    } while (*Old != 0 || *New != 0);
  }
  return Count;
}

STATIC BOOLEAN
ReviewHit (MODERN_UI_RECT Rect, UINTN X, UINTN Y)
{
  return (BOOLEAN)(X >= Rect.X && X - Rect.X < Rect.Width &&
                   Y >= Rect.Y && Y - Rect.Y < Rect.Height);
}

STATIC UINTN
ReviewCoordinate (UINTN Value, UINT64 Min, UINT64 Max, UINTN Extent)
{
  if (Max <= Min || Extent == 0) { return 0; }
  if (Value <= Min) { return 0; }
  if (Value >= Max) { return Extent - 1; }
  // Absolute pointer ranges in UEFI are UINT64; avoid an overflowing product.
  while (Max - Min > MAX_UINT64 / Extent) {
    Value >>= 1;
    Min >>= 1;
    Max >>= 1;
  }
  return (UINTN)DivU64x64Remainder (MultU64x64 (Value - Min, Extent - 1), Max - Min, NULL);
}

#define REVIEW_TRY(Expression) do { Status = (Expression); if (EFI_ERROR (Status)) { return Status; } } while (FALSE)

STATIC EFI_STATUS
ReviewPaint (
  MODERN_UI_RENDER_CONTEXT *Ui,
  MODERN_UI_RECT Panel,
  MODERN_UI_RECT *Buttons,
  CONST REVIEW_LINE *Lines,
  UINTN Count, UINTN Page, UINTN Pages, UINTN PerPage, UINTN Focus,
  BOOLEAN Chinese, BOOLEAN Complete, BOOLEAN CanConfirm, UINTN ItemCount
  )
{
  CONST MODERN_UI_THEME *Theme;
  CONST CHAR16 *Labels[4];
  CONST CHAR16 *Notice;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL Color;
  EFI_STATUS Status;
  UINTN I, Y, Width, Column;
  BOOLEAN Enabled;
  CHAR16 Label[80];
  MODERN_UI_RECT Row;
  Theme = ModernUiGetTheme ();
  Width = Panel.Width - 40;
  Column = (Width - 40) / 2;
  REVIEW_TRY (ModernUiDrawPanel (Ui, Panel, Theme));
  REVIEW_TRY (ModernUiDrawTextFit (Ui, Panel.X + 20, Panel.Y + 16, Width,
    Chinese ? L"保存修改" : L"Review changes before saving", Theme->Text, Theme->Surface));
  Notice = !Complete ?
    (Chinese ? L"修改未完整显示，不能确认保存。" : L"Review incomplete. Confirmation blocked.") :
    (Chinese ? L"确认后由原生配置页面验证并保存" : L"Confirm to attempt native validation and save.");
  REVIEW_TRY (ModernUiDrawTextFit (Ui, Panel.X + 20, Panel.Y + 48, Width,
    Notice, Complete ? Theme->MutedText : Theme->WarningText, Theme->Surface));
  UnicodeSPrint (Label, sizeof (Label), Chinese ? L"%u 项修改    %u / %u 页" : L"%u changes    Page %u / %u",
    (UINT32)ItemCount, (UINT32)(Page + 1), (UINT32)Pages);
  REVIEW_TRY (ModernUiDrawTextFit (Ui, Panel.X + 20, Panel.Y + 80, Width,
    Label, Theme->MutedText, Theme->Surface));
  REVIEW_TRY (ModernUiDrawTextFit (Ui, Panel.X + 20, Panel.Y + 112, Column,
    Chinese ? L"原值" : L"Old value", Theme->MutedText, Theme->Surface));
  REVIEW_TRY (ModernUiDrawTextFit (Ui, Panel.X + 20 + Column + 40, Panel.Y + 112, Column,
    Chinese ? L"新值" : L"New value", Theme->Accent, Theme->Surface));
  for (I = Page * PerPage; I < Count && I < (Page + 1) * PerPage; I++) {
    Y = Panel.Y + 144 + (I - Page * PerPage) * REVIEW_LINE_HEIGHT;
    Row.X = Panel.X + 16;
    Row.Y = Y - 2;
    Row.Width = Panel.Width - 32;
    Row.Height = REVIEW_LINE_HEIGHT;
    if (Lines[I].Path) {
      REVIEW_TRY (ModernUiFillRect (Ui, Row, Theme->SurfaceRaised));
    }
    REVIEW_TRY (ModernUiDrawText (Ui, Panel.X + 20, Y, Lines[I].Left,
      Lines[I].Path ? Theme->Text : Theme->MutedText,
      Lines[I].Path ? Theme->SurfaceRaised : Theme->Surface));
    if (!Lines[I].Path) {
      REVIEW_TRY (ModernUiDrawText (Ui, Panel.X + 20 + Column + 8, Y, L"->", Theme->Accent, Theme->Surface));
      REVIEW_TRY (ModernUiDrawText (Ui, Panel.X + 20 + Column + 40, Y, Lines[I].Right,
        Theme->Text, Theme->Surface));
    }
  }
  if (Count == 0) {
    REVIEW_TRY (ModernUiDrawTextFit (Ui, Panel.X + 20, Panel.Y + 144, Width,
      Complete ? (Chinese ? L"没有待保存的修改" : L"No pending changes.") :
      (Chinese ? L"修改仍保留；部分修改可能已保存，请检查后重试。" : L"Edits retained; some may already be saved. Check and retry."),
      Theme->Text, Theme->Surface));
  }
  Labels[0] = Chinese ? L"上一页" : L"Previous";
  Labels[1] = Chinese ? L"下一页" : L"Next";
  Labels[2] = Chinese ? L"继续编辑" : L"Continue editing";
  Labels[3] = Chinese ? L"确认保存" : L"Confirm save";
  for (I = 0; I < 4; I++) {
    Enabled = (BOOLEAN)((I == 0) ? Page > 0 : (I == 1) ? Page + 1 < Pages : (I == 2) || CanConfirm);
    Color = Enabled ? Theme->Text : Theme->MutedText;
    REVIEW_TRY (ModernUiFillRect (Ui, Buttons[I], Theme->SurfaceRaised));
    REVIEW_TRY (ModernUiStrokeRect (Ui, Buttons[I], Focus == I ? Theme->Accent : Theme->Border));
    REVIEW_TRY (ModernUiDrawTextFit (Ui, Buttons[I].X + 10, Buttons[I].Y + 10,
      Buttons[I].Width - 20, Labels[I], Color, Theme->SurfaceRaised));
  }
  REVIEW_TRY (ModernUiDrawTextFit (Ui, Panel.X + 20, Panel.Y + Panel.Height - 36, Width,
    Complete && !CanConfirm ?
      (Chinese ? L"请先查看全部页面；Esc: 继续编辑" : L"Read all pages first; Esc: continue editing") :
      (Chinese ? L"Tab: 选择   Enter: 执行   PgUp/PgDn: 翻页   Esc: 返回   F10: 保存" :
                 L"Tab: focus  Enter: select  PgUp/PgDn: page  Esc: back  F10: save"),
    Theme->MutedText, Theme->Surface));
  return EFI_SUCCESS;
}
#undef REVIEW_TRY

STATIC EFI_STATUS EFIAPI
ReviewChanges (
  MODERN_SETUP_CHANGE_REVIEW_PROTOCOL *This,
  CONST MODERN_SETUP_REVIEW_SNAPSHOT *Snapshot,
  MODERN_SETUP_REVIEW_DECISION *Decision
  )
{
  MODERN_UI_RENDER_CONTEXT Ui;
  MODERN_UI_INPUT_CONTEXT Input;
  MODERN_UI_INPUT_EVENT Event;
  MODERN_UI_RECT Panel, Buttons[4], Cursor;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Saved, CursorPixels[12 * 16];
  REVIEW_LINE *Lines;
  BOOLEAN *Seen;
  BOOLEAN Chinese, CanConfirm, Redraw, CursorDrawn, PointerVisible, WasPressed, Click, Confirm;
  UINTN Count, Page, Pages, PerPage, Focus, I, Action, X, Y, SeenCount;
  EFI_STATUS Status, RestoreStatus;

  if (Decision == NULL) { return EFI_INVALID_PARAMETER; }
  *Decision = ModernSetupContinueEditing;
  if (Snapshot == NULL || Snapshot->Revision != MODERN_SETUP_CHANGE_REVIEW_REVISION ||
      Snapshot->Size != sizeof (*Snapshot) || Snapshot->ItemCount > 256 ||
      (Snapshot->ItemCount != 0 && Snapshot->Items == NULL)) { return EFI_INVALID_PARAMETER; }
  Chinese = (BOOLEAN)(AsciiStrnCmp (ModernUiGetLanguage (), "zh", 2) == 0);
  for (I = 0; I < Snapshot->ItemCount; I++) {
    if (!ReviewValidText (Snapshot->Items[I].Path) ||
        !ReviewValidText (ReviewValue (&Snapshot->Items[I], TRUE, Chinese)) ||
        !ReviewValidText (ReviewValue (&Snapshot->Items[I], FALSE, Chinese))) {
      return EFI_INVALID_PARAMETER;
    }
  }
  // Reuse the already active native renderer. Do NOT initialize it again: that
  // can select a different GOP mode and destroy the native form's backing store.
  ZeroMem (&Ui, sizeof (Ui));
  Status = gBS->HandleProtocol (gST->ConsoleOutHandle, &gEfiGraphicsOutputProtocolGuid, (VOID **)&Ui.Gop);
  if (EFI_ERROR (Status)) {
    Status = gBS->LocateProtocol (&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **)&Ui.Gop);
  }
  if (EFI_ERROR (Status) || Ui.Gop == NULL || Ui.Gop->Mode == NULL || Ui.Gop->Mode->Info == NULL) {
    return EFI_NOT_READY;
  }
  Ui.Width = Ui.Gop->Mode->Info->HorizontalResolution;
  Ui.Height = Ui.Gop->Mode->Info->VerticalResolution;
  if (Ui.Width < 640 || Ui.Height < 480) { return EFI_UNSUPPORTED; }
  gBS->LocateProtocol (&gEfiHiiFontProtocolGuid, NULL, (VOID **)&Ui.Font);
  Status = ModernUiInputInit (&Input);
  if (EFI_ERROR (Status)) { return Status; }
  // The shared input adapter requires a keyboard for spurious pointer wakes.
  if (Input.TextIn == NULL && Input.TextInEx == NULL) { return EFI_NOT_READY; }
  Panel.Width = MIN (Ui.Width - 48, 1120);
  Panel.Height = MIN (Ui.Height - 48, 800);
  Panel.X = (Ui.Width - Panel.Width) / 2;
  Panel.Y = (Ui.Height - Panel.Height) / 2;
  PerPage = (Panel.Height - 244) / REVIEW_LINE_HEIGHT;
  Count = ReviewMakeLines (Snapshot, Chinese, Panel.Width - 40, NULL);
  Pages = MAX (1, (Count + PerPage - 1) / PerPage);
  Lines = AllocateZeroPool (MAX (1, Count) * sizeof (*Lines));
  Seen = AllocateZeroPool (Pages * sizeof (*Seen));
  Saved = AllocatePool (Panel.Width * Panel.Height * sizeof (*Saved));
  if (Lines == NULL || Seen == NULL || Saved == NULL) {
    if (Lines != NULL) { FreePool (Lines); }
    if (Seen != NULL) { FreePool (Seen); }
    if (Saved != NULL) { FreePool (Saved); }
    return EFI_OUT_OF_RESOURCES;
  }
  ReviewMakeLines (Snapshot, Chinese, Panel.Width - 40, Lines);
  Status = ModernUiCaptureRect (&Ui, Panel, Saved);
  if (EFI_ERROR (Status)) { goto Free; }
  for (I = 0; I < 4; I++) {
    Buttons[I].Width = (Panel.Width - 64) / 4;
    Buttons[I].Height = 44;
    Buttons[I].X = Panel.X + 20 + I * (Buttons[I].Width + 8);
    Buttons[I].Y = Panel.Y + Panel.Height - 92;
  }
  Page = 0;
  Focus = 2; // Enter defaults to continue editing, never implicit consent.
  SeenCount = 0;
  Redraw = TRUE;
  CursorDrawn = FALSE;
  PointerVisible = FALSE;
  WasPressed = FALSE;
  Confirm = FALSE;
  for (;;) {
    if (Redraw) {
      if (CursorDrawn) {
        Status = ModernUiRestoreRect (&Ui, Cursor, CursorPixels);
        CursorDrawn = FALSE;
        if (EFI_ERROR (Status)) { break; }
      }
      CanConfirm = (BOOLEAN)(Snapshot->Complete && (SeenCount + (Seen[Page] ? 0 : 1) == Pages));
      Status = ReviewPaint (&Ui, Panel, Buttons, Lines, Count, Page, Pages, PerPage,
        Focus, Chinese, Snapshot->Complete, CanConfirm, Snapshot->ItemCount);
      if (EFI_ERROR (Status)) { break; }
      if (!Seen[Page]) { Seen[Page] = TRUE; SeenCount++; }
      if (PointerVisible) {
        Status = ModernUiCaptureRect (&Ui, Cursor, CursorPixels);
        if (EFI_ERROR (Status)) { break; }
        CursorDrawn = TRUE;
        Status = ModernUiFillTriangle (&Ui, Cursor, ModernUiTriRight, ModernUiGetTheme ()->Text);
        if (EFI_ERROR (Status)) { break; }
      }
      Redraw = FALSE;
    }
    Status = ModernUiReadInput (&Input, &Event);
    if (Status == EFI_NOT_READY) { continue; }
    if (EFI_ERROR (Status)) { break; }
    Action = MAX_UINTN;
    if (Event.Type == ModernUiInputEscape) { break; }
    if (Event.ScanCode == SCAN_F10 && CanConfirm) { Confirm = TRUE; break; }
    if (Event.Type == ModernUiInputTab || Event.Type == ModernUiInputRight) {
      Focus = (Focus + 1) % 4;
      Redraw = TRUE;
    } else if (Event.Type == ModernUiInputLeft) {
      Focus = (Focus + 3) % 4;
      Redraw = TRUE;
    } else if (Event.Type == ModernUiInputEnter) {
      Action = Focus;
    } else if (Event.Type == ModernUiInputUp || Event.ScanCode == SCAN_PAGE_UP) {
      Action = 0;
    } else if (Event.Type == ModernUiInputDown || Event.ScanCode == SCAN_PAGE_DOWN) {
      Action = 1;
    } else if (Event.Type == ModernUiInputPointer && Event.PointerValid && Input.Pointer != NULL && Input.Pointer->Mode != NULL) {
      X = ReviewCoordinate (Event.PointerX, Input.Pointer->Mode->AbsoluteMinX, Input.Pointer->Mode->AbsoluteMaxX, Ui.Width);
      Y = ReviewCoordinate (Event.PointerY, Input.Pointer->Mode->AbsoluteMinY, Input.Pointer->Mode->AbsoluteMaxY, Ui.Height);
      Click = (BOOLEAN)(Event.PointerPressed && !WasPressed);
      WasPressed = Event.PointerPressed;
      if (CursorDrawn) {
        Status = ModernUiRestoreRect (&Ui, Cursor, CursorPixels);
        CursorDrawn = FALSE;
        if (EFI_ERROR (Status)) { break; }
      }
      Cursor.X = MIN (X, Ui.Width - 12);
      Cursor.Y = MIN (Y, Ui.Height - 16);
      Cursor.Width = 12;
      Cursor.Height = 16;
      PointerVisible = TRUE;
      Status = ModernUiCaptureRect (&Ui, Cursor, CursorPixels);
      if (EFI_ERROR (Status)) { break; }
      CursorDrawn = TRUE;
      Status = ModernUiFillTriangle (&Ui, Cursor, ModernUiTriRight, ModernUiGetTheme ()->Text);
      if (EFI_ERROR (Status)) { break; }
      for (I = 0; I < 4; I++) {
        if (ReviewHit (Buttons[I], X, Y)) {
          if (Focus != I) { Focus = I; Redraw = TRUE; }
          if (Click) { Action = I; }
          break;
        }
      }
    }
    if (Action == 0 && Page > 0) { Page--; Redraw = TRUE; }
    if (Action == 1 && Page + 1 < Pages) { Page++; Redraw = TRUE; }
    if (Action == 2) { break; }
    if (Action == 3 && CanConfirm) { Confirm = TRUE; break; }
  }
  // Restore both the shared shadow canvas and GOP, without clearing native
  // selection/edit state or invoking a nested browser. Restoration errors veto save.
  if (CursorDrawn) {
    RestoreStatus = ModernUiRestoreRect (&Ui, Cursor, CursorPixels);
    if (EFI_ERROR (RestoreStatus)) { Status = RestoreStatus; }
  }
  RestoreStatus = ModernUiRestoreRect (&Ui, Panel, Saved);
  if (EFI_ERROR (RestoreStatus)) { Status = RestoreStatus; }
  if (!EFI_ERROR (Status) && Confirm) { *Decision = ModernSetupConfirmSave; }
Free:
  FreePool (Saved);
  FreePool (Seen);
  FreePool (Lines);
  return Status;
}

STATIC MODERN_SETUP_CHANGE_REVIEW_PROTOCOL mReviewProtocol = {
  MODERN_SETUP_CHANGE_REVIEW_REVISION, ReviewChanges
};

EFI_STATUS
ModernInstallSaveReview (
  EFI_HANDLE *Handle,
  EDKII_FORM_DISPLAY_ENGINE_PROTOCOL *Display,
  EFI_HII_POPUP_PROTOCOL *Popup
  )
{
  return gBS->InstallMultipleProtocolInterfaces (Handle,
    &gEdkiiFormDisplayEngineProtocolGuid, Display,
    &gEfiHiiPopupProtocolGuid, Popup,
    &mReviewGuid, &mReviewProtocol, NULL);
}

EFI_STATUS
ModernUninstallSaveReview (
  EFI_HANDLE Handle,
  EDKII_FORM_DISPLAY_ENGINE_PROTOCOL *Display,
  EFI_HII_POPUP_PROTOCOL *Popup
  )
{
  return gBS->UninstallMultipleProtocolInterfaces (Handle,
    &gEdkiiFormDisplayEngineProtocolGuid, Display,
    &gEfiHiiPopupProtocolGuid, Popup,
    &mReviewGuid, &mReviewProtocol, NULL);
}
