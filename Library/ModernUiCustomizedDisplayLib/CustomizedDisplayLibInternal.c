/** @file

  This library class defines a set of interfaces to customize Display module

Copyright (c) 2013-2018, Intel Corporation. All rights reserved.<BR>
Copyright (c) 2025, Loongson Technology Corporation Limited. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include "CustomizedDisplayLibInternal.h"

EFI_SCREEN_DESCRIPTOR  gScreenDimensions;
CHAR16                 *mLibUnknownString;
extern EFI_HII_HANDLE  mCDLStringPackHandle;
CHAR16                 *mSpaceBuffer;
#define SPACE_BUFFER_SIZE  1000

STATIC MODERN_UI_RENDER_CONTEXT  mModernRenderContext;
STATIC BOOLEAN                   mModernRenderReady;
STATIC UINTN                     mModernCursorColumn;
STATIC UINTN                     mModernCursorRow;

STATIC
UINTN
ModernDisplayColumns (
  VOID
  );

STATIC
UINTN
ModernDisplayRows (
  VOID
  );

/**
  Return GOP cell metrics that match the active text-mode grid.

  @param[out] CellWidth   Width in pixels for one text column. Must not be NULL.
  @param[out] CellHeight  Height in pixels for one text row. Must not be NULL.
**/
STATIC
VOID
ModernDisplayGetCellMetrics (
  OUT UINTN  *CellWidth,
  OUT UINTN  *CellHeight
  )
{
  UINTN  Columns;
  UINTN  Rows;

  ASSERT ((CellWidth != NULL) && (CellHeight != NULL));

  Columns     = ModernDisplayColumns ();
  Rows        = ModernDisplayRows ();
  *CellWidth  = MAX (1, mModernRenderContext.Width / Columns);
  *CellHeight = MAX (18, mModernRenderContext.Height / Rows);
}

/**
  Initialize the GOP-backed drawing context on first use.

  @retval EFI_SUCCESS  GOP and renderer state are available.
  @retval others       Status from ModernUiRendererInit().
**/
STATIC
EFI_STATUS
ModernDisplayEnsureRenderer (
  VOID
  )
{
  EFI_STATUS  Status;

  if (mModernRenderReady) {
    return EFI_SUCCESS;
  }

  Status = ModernUiRendererInit (&mModernRenderContext);
  if (!EFI_ERROR (Status)) {
    mModernRenderReady = TRUE;
  }

  return Status;
}

/**
  Clear the GOP-backed ModernSetup drawing surface and reset the emulated text
  cursor used by the display engine backend.
**/
VOID
ModernDisplayClearGop (
  VOID
  )
{
  if (!EFI_ERROR (ModernDisplayEnsureRenderer ())) {
    ModernUiClear (&mModernRenderContext, ModernUiGetTheme ()->Background);
  }

  mModernCursorColumn = 0;
  mModernCursorRow    = 0;
}

/**
  Return the current text-mode column count used as a layout grid.

  @return Number of columns. A conservative 80-column default is used if the
          text output mode cannot report dimensions.
**/
STATIC
UINTN
ModernDisplayColumns (
  VOID
  )
{
  UINTN  Columns;
  UINTN  Rows;

  Columns = 0;
  Rows    = 0;
  if ((gST != NULL) && (gST->ConOut != NULL) && (gST->ConOut->Mode != NULL)) {
    gST->ConOut->QueryMode (gST->ConOut, gST->ConOut->Mode->Mode, &Columns, &Rows);
  }

  return (Columns == 0) ? 80 : Columns;
}

/**
  Return the current text-mode row count used as a layout grid.

  @return Number of rows. A conservative 25-row default is used if the text
          output mode cannot report dimensions.
**/
STATIC
UINTN
ModernDisplayRows (
  VOID
  )
{
  UINTN  Columns;
  UINTN  Rows;

  Columns = 0;
  Rows    = 0;
  if ((gST != NULL) && (gST->ConOut != NULL) && (gST->ConOut->Mode != NULL)) {
    gST->ConOut->QueryMode (gST->ConOut, gST->ConOut->Mode->Mode, &Columns, &Rows);
  }

  return (Rows == 0) ? 25 : Rows;
}

/**
  Convert an EFI text attribute into a foreground pixel color.

  @param[in] Attribute  EFI text attribute from ConOut.

  @return Pixel color selected from the active ModernSetup theme.
**/
STATIC
EFI_GRAPHICS_OUTPUT_BLT_PIXEL
ModernDisplayForeground (
  IN UINTN  Attribute
  )
{
  CONST MODERN_UI_THEME  *Theme;

  Theme = ModernUiGetTheme ();
  switch (Attribute & 0x0F) {
    case EFI_RED:
    case EFI_LIGHTRED:
      return Theme->Warning;
    case EFI_GREEN:
    case EFI_LIGHTGREEN:
      return Theme->Success;
    case EFI_CYAN:
    case EFI_LIGHTCYAN:
    case EFI_BLUE:
    case EFI_LIGHTBLUE:
      return Theme->Accent;
    case EFI_DARKGRAY:
    case EFI_LIGHTGRAY:
      return Theme->MutedText;
    case EFI_WHITE:
    case EFI_YELLOW:
    case EFI_BROWN:
    default:
      return Theme->Text;
  }
}

/**
  Convert an EFI text attribute into a background pixel color.

  @param[in] Attribute  EFI text attribute from ConOut.

  @return Pixel color selected from the active ModernSetup theme.
**/
STATIC
EFI_GRAPHICS_OUTPUT_BLT_PIXEL
ModernDisplayBackground (
  IN UINTN  Attribute
  )
{
  CONST MODERN_UI_THEME  *Theme;

  Theme = ModernUiGetTheme ();
  switch ((Attribute >> 4) & 0x07) {
    case EFI_BLUE:
      return Theme->SurfaceRaised;
    case EFI_CYAN:
    case EFI_LIGHTBLUE:
      return Theme->AccentSoft;
    case EFI_LIGHTGRAY:
      return Theme->Surface;
    default:
      return Theme->Background;
  }
}

/**
  Copy a formatted string into a printable buffer and remove HII width markers.

  @param[out] Output     Destination buffer. Must not be NULL.
  @param[in]  OutputLen  Number of CHAR16 entries in Output.
  @param[in]  Input      Source string. Must not be NULL.

  @return Number of visible characters copied.
**/
STATIC
UINTN
ModernDisplayCopyPrintable (
  OUT CHAR16        *Output,
  IN  UINTN         OutputLen,
  IN  CONST CHAR16  *Input
  )
{
  UINTN  Index;
  UINTN  OutIndex;

  if ((Output == NULL) || (OutputLen == 0) || (Input == NULL)) {
    return 0;
  }

  OutIndex = 0;
  for (Index = 0; (Input[Index] != CHAR_NULL) && ((OutIndex + 1) < OutputLen); Index++) {
    if ((Input[Index] == NARROW_CHAR) || (Input[Index] == WIDE_CHAR)) {
      continue;
    }

    Output[OutIndex++] = Input[Index];
  }

  Output[OutIndex] = CHAR_NULL;
  return OutIndex;
}

/**
  Draw the modern DisplayEngine shell behind the native FormBrowser content.

  This function does not parse HII or own any FormBrowser semantics. It only
  paints the GOP chrome used by the customized display backend.

  @param[in] FormData  Form currently being displayed. Must not be NULL.
**/
STATIC
VOID
ModernDisplayDrawPageChrome (
  IN FORM_DISPLAY_ENGINE_FORM  *FormData
  )
{
  CONST MODERN_UI_THEME  *Theme;
  CHAR16                 *Title;
  CHAR16                 *PrintableTitle;
  UINTN                  CellWidth;
  UINTN                  CellHeight;
  UINTN                  HeaderHeight;
  UINTN                  FooterTop;
  UINTN                  ContentX;
  UINTN                  ContentY;
  UINTN                  ContentWidth;
  UINTN                  ContentHeight;
  UINTN                  HorizontalMargin;
  UINTN                  ScreenColumns;
  MODERN_UI_RECT         ContentRect;

  ASSERT (FormData != NULL);
  if ((FormData == NULL) || EFI_ERROR (ModernDisplayEnsureRenderer ())) {
    return;
  }

  Theme = ModernUiGetTheme ();
  ModernDisplayGetCellMetrics (&CellWidth, &CellHeight);

  HeaderHeight = (gClassOfVfr == FORMSET_CLASS_FRONT_PAGE) ? (FRONT_PAGE_HEADER_HEIGHT * CellHeight) :
                 (NONE_FRONT_PAGE_HEADER_HEIGHT * CellHeight);
  FooterTop = (gScreenDimensions.BottomRow - STATUS_BAR_HEIGHT - gFooterHeight) * CellHeight;
  ScreenColumns = gScreenDimensions.RightColumn - gScreenDimensions.LeftColumn;
  HorizontalMargin = (ScreenColumns > (2 * MODERN_SETUP_HORIZONTAL_MARGIN + 4)) ? MODERN_SETUP_HORIZONTAL_MARGIN : 0;

  ModernUiClear (&mModernRenderContext, Theme->Background);
  ModernUiFillRect (&mModernRenderContext, (MODERN_UI_RECT){ 0, 0, mModernRenderContext.Width, HeaderHeight }, Theme->SurfaceRaised);
  ModernUiFillRect (&mModernRenderContext, (MODERN_UI_RECT){ 0, HeaderHeight - 2, mModernRenderContext.Width, 2 }, Theme->Accent);
  ModernUiFillRect (&mModernRenderContext, (MODERN_UI_RECT){ 0, FooterTop, mModernRenderContext.Width, mModernRenderContext.Height - FooterTop }, Theme->SurfaceRaised);
  ModernUiFillRect (&mModernRenderContext, (MODERN_UI_RECT){ 0, FooterTop, mModernRenderContext.Width, 1 }, Theme->Border);

  ContentX      = (gScreenDimensions.LeftColumn + HorizontalMargin) * CellWidth;
  ContentY      = (gScreenDimensions.TopRow + NONE_FRONT_PAGE_HEADER_HEIGHT) * CellHeight;
  ContentWidth  = MAX (1, ScreenColumns - (2 * HorizontalMargin)) * CellWidth;
  ContentHeight = (FooterTop > ContentY + CellHeight) ? (FooterTop - ContentY - CellHeight) : CellHeight;
  ContentRect   = (MODERN_UI_RECT){ ContentX, ContentY, ContentWidth, ContentHeight };

  ModernUiFillRect (&mModernRenderContext, ContentRect, Theme->Surface);
  ModernUiStrokeRect (&mModernRenderContext, ContentRect, Theme->Border);

  Title = LibGetToken (FormData->FormTitle, FormData->HiiHandle);
  if (Title != NULL) {
    PrintableTitle = AllocateZeroPool ((StrLen (Title) + 1) * sizeof (CHAR16));
    if (PrintableTitle != NULL) {
      ModernDisplayCopyPrintable (PrintableTitle, StrLen (Title) + 1, Title);
      ModernUiDrawText (
        &mModernRenderContext,
        (gScreenDimensions.LeftColumn + HorizontalMargin + 1) * CellWidth,
        gScreenDimensions.TopRow * CellHeight + ((CellHeight > 18) ? ((CellHeight - 18) / 2) : 0),
        PrintableTitle,
        Theme->Text,
        Theme->SurfaceRaised
        );
      FreePool (PrintableTitle);
    }

    FreePool (Title);
  }
}

//
// Browser Global Strings
//
CHAR16  *gEnterString;
CHAR16  *gEnterCommitString;
CHAR16  *gEnterEscapeString;
CHAR16  *gEscapeString;
CHAR16  *gMoveHighlight;
CHAR16  *gDecNumericInput;
CHAR16  *gHexNumericInput;
CHAR16  *gToggleCheckBox;
CHAR16  *gLibEmptyString;
CHAR16  *gAreYouSure;
CHAR16  *gYesResponse;
CHAR16  *gNoResponse;
CHAR16  *gPlusString;
CHAR16  *gMinusString;
CHAR16  *gAdjustNumber;
CHAR16  *gSaveChanges;
CHAR16  *gNvUpdateMessage;
CHAR16  *gInputErrorMessage;

/**

  Print banner info for front page.

  @param[in]  FormData             Form Data to be shown in Page

**/
VOID
PrintBannerInfo (
  IN FORM_DISPLAY_ENGINE_FORM  *FormData
  )
{
  UINT8   Line;
  UINT8   Alignment;
  CHAR16  *StrFrontPageBanner;
  UINT8   RowIdx;
  UINT8   ColumnIdx;

  //
  //    ClearLines(0, LocalScreen.RightColumn, 0, BANNER_HEIGHT-1, BANNER_TEXT | BANNER_BACKGROUND);
  //
  ClearLines (
    gScreenDimensions.LeftColumn,
    gScreenDimensions.RightColumn,
    gScreenDimensions.TopRow,
    FRONT_PAGE_HEADER_HEIGHT - 1 + gScreenDimensions.TopRow,
    BANNER_TEXT | BANNER_BACKGROUND
    );

  //
  //    for (Line = 0; Line < BANNER_HEIGHT; Line++) {
  //
  for (Line = (UINT8)gScreenDimensions.TopRow; Line < BANNER_HEIGHT + (UINT8)gScreenDimensions.TopRow; Line++) {
    //
    //      for (Alignment = 0; Alignment < BANNER_COLUMNS; Alignment++) {
    //
    for (Alignment = (UINT8)gScreenDimensions.LeftColumn;
         Alignment < BANNER_COLUMNS + (UINT8)gScreenDimensions.LeftColumn;
         Alignment++
         )
    {
      RowIdx    = (UINT8)(Line - (UINT8)gScreenDimensions.TopRow);
      ColumnIdx = (UINT8)(Alignment - (UINT8)gScreenDimensions.LeftColumn);

      ASSERT (RowIdx < BANNER_HEIGHT && ColumnIdx < BANNER_COLUMNS);

      if ((gBannerData != NULL) && (gBannerData->Banner[RowIdx][ColumnIdx] != 0x0000)) {
        StrFrontPageBanner = LibGetToken (gBannerData->Banner[RowIdx][ColumnIdx], FormData->HiiHandle);
      } else {
        continue;
      }

      switch (Alignment - gScreenDimensions.LeftColumn) {
        case 0:
          //
          // Handle left column
          //
          PrintStringAt (gScreenDimensions.LeftColumn + BANNER_LEFT_COLUMN_INDENT, Line, StrFrontPageBanner);
          break;

        case 1:
          //
          // Handle center column
          //
          PrintStringAt (
            gScreenDimensions.LeftColumn + (gScreenDimensions.RightColumn - gScreenDimensions.LeftColumn) / 3,
            Line,
            StrFrontPageBanner
            );
          break;

        case 2:
          //
          // Handle right column
          //
          PrintStringAt (
            gScreenDimensions.LeftColumn + (gScreenDimensions.RightColumn - gScreenDimensions.LeftColumn) * 2 / 3,
            Line,
            StrFrontPageBanner
            );
          break;
      }

      FreePool (StrFrontPageBanner);
    }
  }
}

/**
  Print framework and form title for a page.

  @param[in]  FormData             Form Data to be shown in Page
**/
VOID
PrintFramework (
  IN FORM_DISPLAY_ENGINE_FORM  *FormData
  )
{
  CHAR16  *TitleStr;
  UINTN   TitleColumn;
  UINTN   TitleWidth;
  UINTN   HeaderLeft;
  UINTN   HeaderWidth;
  UINTN   HorizontalMargin;
  UINTN   ScreenColumns;

  if (gClassOfVfr != FORMSET_CLASS_PLATFORM_SETUP) {
    //
    // Only Setup page needs Framework
    //
    ClearLines (
      gScreenDimensions.LeftColumn,
      gScreenDimensions.RightColumn,
      gScreenDimensions.BottomRow - STATUS_BAR_HEIGHT - gFooterHeight,
      gScreenDimensions.BottomRow - STATUS_BAR_HEIGHT - 1,
      KEYHELP_TEXT | KEYHELP_BACKGROUND
      );
    return;
  }

  ScreenColumns = gScreenDimensions.RightColumn - gScreenDimensions.LeftColumn;
  HorizontalMargin = (ScreenColumns > (2 * MODERN_SETUP_HORIZONTAL_MARGIN + 4)) ? MODERN_SETUP_HORIZONTAL_MARGIN : 0;
  HeaderLeft       = gScreenDimensions.LeftColumn + HorizontalMargin + 1;
  HeaderWidth      = MAX (1, ScreenColumns - (2 * HorizontalMargin) - 1);

  ClearLines (
    gScreenDimensions.LeftColumn,
    gScreenDimensions.RightColumn,
    gScreenDimensions.TopRow,
    gScreenDimensions.TopRow + NONE_FRONT_PAGE_HEADER_HEIGHT - 1,
    TITLE_TEXT | TITLE_BACKGROUND
    );

  ClearLines (
    gScreenDimensions.LeftColumn,
    gScreenDimensions.RightColumn,
    gScreenDimensions.TopRow + NONE_FRONT_PAGE_HEADER_HEIGHT,
    gScreenDimensions.BottomRow - STATUS_BAR_HEIGHT - gFooterHeight - 1,
    PcdGet8 (PcdBrowserFieldTextColor) | FIELD_BACKGROUND
    );

  ClearLines (
    gScreenDimensions.LeftColumn,
    gScreenDimensions.RightColumn,
    gScreenDimensions.BottomRow - STATUS_BAR_HEIGHT - gFooterHeight,
    gScreenDimensions.BottomRow - STATUS_BAR_HEIGHT - 1,
    KEYHELP_TEXT | KEYHELP_BACKGROUND
    );

  ModernDisplayDrawPageChrome (FormData);

  //
  // Print the form title through the text-grid path so the browser cursor and
  // width accounting stay consistent with the rest of DisplayEngine.
  //
  TitleStr = LibGetToken (FormData->FormTitle, FormData->HiiHandle);
  if (TitleStr != NULL) {
    TitleWidth  = LibGetStringWidth (TitleStr);
    TitleWidth  = (TitleWidth > 2) ? ((TitleWidth - 2) / 2) : 0;
    TitleColumn = HeaderLeft;
    gST->ConOut->SetAttribute (gST->ConOut, TITLE_TEXT | TITLE_BACKGROUND);
    PrintStringAtWithWidth (
      TitleColumn,
      gScreenDimensions.TopRow + 1,
      TitleStr,
      (TitleWidth > HeaderWidth) ? HeaderWidth : TitleWidth
      );
    FreePool (TitleStr);
  }
}

/**
  Process some op code which is not recognized by browser core.

  @param OpCodeData                  The pointer to the op code buffer.

  @return EFI_SUCCESS            Pass the statement success.

**/
VOID
ProcessUserOpcode (
  IN  EFI_IFR_OP_HEADER  *OpCodeData
  )
{
  EFI_GUID  *ClassGuid;
  UINT8     ClassGuidNum;

  ClassGuid    = NULL;
  ClassGuidNum = 0;

  switch (OpCodeData->OpCode) {
    case EFI_IFR_FORM_SET_OP:
      //
      // process the statement outside of form,if it is formset op, get its formsetguid or classguid and compared with gFrontPageFormSetGuid
      //
      if (CompareMem (PcdGetPtr (PcdFrontPageFormSetGuid), &((EFI_IFR_FORM_SET *)OpCodeData)->Guid, sizeof (EFI_GUID)) == 0) {
        gClassOfVfr = FORMSET_CLASS_FRONT_PAGE;
      } else {
        ClassGuidNum = (UINT8)(((EFI_IFR_FORM_SET *)OpCodeData)->Flags & 0x3);
        ClassGuid    = (EFI_GUID *)(VOID *)((UINT8 *)OpCodeData + sizeof (EFI_IFR_FORM_SET));
        while (ClassGuidNum-- > 0) {
          if (CompareGuid ((EFI_GUID *)PcdGetPtr (PcdFrontPageFormSetGuid), ClassGuid)) {
            gClassOfVfr = FORMSET_CLASS_FRONT_PAGE;
            break;
          }

          ClassGuid++;
        }
      }

      break;

    case EFI_IFR_GUID_OP:
      if (CompareGuid (&gEfiIfrTianoGuid, (EFI_GUID *)((CHAR8 *)OpCodeData + sizeof (EFI_IFR_OP_HEADER)))) {
        //
        // Tiano specific GUIDed opcodes
        //
        switch (((EFI_IFR_GUID_LABEL *)OpCodeData)->ExtendOpCode) {
          case EFI_IFR_EXTEND_OP_LABEL:
            //
            // just ignore label
            //
            break;

          case EFI_IFR_EXTEND_OP_BANNER:
            //
            // Only in front page form set, we care about the banner data.
            //
            if (gClassOfVfr == FORMSET_CLASS_FRONT_PAGE) {
              //
              // Initialize Driver private data
              //
              if (gBannerData == NULL) {
                gBannerData = AllocateZeroPool (sizeof (BANNER_DATA));
                ASSERT (gBannerData != NULL);
              }

              CopyMem (
                &gBannerData->Banner[((EFI_IFR_GUID_BANNER *)OpCodeData)->LineNumber][
                                                                                      ((EFI_IFR_GUID_BANNER *)OpCodeData)->Alignment],
                &((EFI_IFR_GUID_BANNER *)OpCodeData)->Title,
                sizeof (EFI_STRING_ID)
                );
            }

            break;

          case EFI_IFR_EXTEND_OP_SUBCLASS:
            if (((EFI_IFR_GUID_SUBCLASS *)OpCodeData)->SubClass == EFI_FRONT_PAGE_SUBCLASS) {
              gClassOfVfr = FORMSET_CLASS_FRONT_PAGE;
            }

            break;

          default:
            break;
        }
      }

      break;

    default:
      break;
  }
}

/**
  Process some op codes which is out side of current form.

  @param FormData                Pointer to the form data.

  @return EFI_SUCCESS            Pass the statement success.

**/
VOID
ProcessExternedOpcode (
  IN FORM_DISPLAY_ENGINE_FORM  *FormData
  )
{
  LIST_ENTRY                     *Link;
  LIST_ENTRY                     *NestLink;
  FORM_DISPLAY_ENGINE_STATEMENT  *Statement;
  FORM_DISPLAY_ENGINE_STATEMENT  *NestStatement;

  Link = GetFirstNode (&FormData->StatementListOSF);
  while (!IsNull (&FormData->StatementListOSF, Link)) {
    Statement = FORM_DISPLAY_ENGINE_STATEMENT_FROM_LINK (Link);
    Link      = GetNextNode (&FormData->StatementListOSF, Link);

    ProcessUserOpcode (Statement->OpCode);
  }

  Link = GetFirstNode (&FormData->StatementListHead);
  while (!IsNull (&FormData->StatementListHead, Link)) {
    Statement = FORM_DISPLAY_ENGINE_STATEMENT_FROM_LINK (Link);
    Link      = GetNextNode (&FormData->StatementListHead, Link);

    ProcessUserOpcode (Statement->OpCode);

    NestLink = GetFirstNode (&Statement->NestStatementList);
    while (!IsNull (&Statement->NestStatementList, NestLink)) {
      NestStatement = FORM_DISPLAY_ENGINE_STATEMENT_FROM_LINK (NestLink);
      NestLink      = GetNextNode (&Statement->NestStatementList, NestLink);

      ProcessUserOpcode (NestStatement->OpCode);
    }
  }
}

/**
  Validate the input screen dimension info.

  @param  FormData               The input form data info.

  @return EFI_SUCCESS            The input screen info is acceptable.
  @return EFI_INVALID_PARAMETER  The input screen info is not acceptable.

**/
EFI_STATUS
ScreenDimensionInfoValidate (
  IN FORM_DISPLAY_ENGINE_FORM  *FormData
  )
{
  LIST_ENTRY  *Link;
  UINTN       Index;

  //
  // Calculate total number of Register HotKeys.
  //
  Index = 0;
  if (!IsListEmpty (&FormData->HotKeyListHead)) {
    Link = GetFirstNode (&FormData->HotKeyListHead);
    while (!IsNull (&FormData->HotKeyListHead, Link)) {
      Link = GetNextNode (&FormData->HotKeyListHead, Link);
      Index++;
    }
  }

  //
  // Show three HotKeys help information on one row.
  //
  gFooterHeight = FOOTER_HEIGHT + (Index / 3);

  ZeroMem (&gScreenDimensions, sizeof (EFI_SCREEN_DESCRIPTOR));
  gST->ConOut->QueryMode (
                 gST->ConOut,
                 gST->ConOut->Mode->Mode,
                 &gScreenDimensions.RightColumn,
                 &gScreenDimensions.BottomRow
                 );

  //
  // Check local dimension vs. global dimension.
  //
  if (FormData->ScreenDimensions != NULL) {
    if ((gScreenDimensions.RightColumn < FormData->ScreenDimensions->RightColumn) ||
        (gScreenDimensions.BottomRow < FormData->ScreenDimensions->BottomRow)
        )
    {
      return EFI_INVALID_PARAMETER;
    } else {
      //
      // Local dimension validation.
      //
      if ((FormData->ScreenDimensions->RightColumn > FormData->ScreenDimensions->LeftColumn) &&
          (FormData->ScreenDimensions->BottomRow > FormData->ScreenDimensions->TopRow) &&
          ((FormData->ScreenDimensions->RightColumn - FormData->ScreenDimensions->LeftColumn) > 2) &&
          ((FormData->ScreenDimensions->BottomRow - FormData->ScreenDimensions->TopRow) > STATUS_BAR_HEIGHT +
           FRONT_PAGE_HEADER_HEIGHT + gFooterHeight + 3))
      {
        CopyMem (&gScreenDimensions, (VOID *)FormData->ScreenDimensions, sizeof (EFI_SCREEN_DESCRIPTOR));
      } else {
        return EFI_INVALID_PARAMETER;
      }
    }
  }

  return EFI_SUCCESS;
}

/**
  Get the string based on the StringId and HII Package List Handle.

  @param  Token                  The String's ID.
  @param  HiiHandle              The package list in the HII database to search for
                                 the specified string.

  @return The output string.

**/
CHAR16 *
LibGetToken (
  IN  EFI_STRING_ID   Token,
  IN  EFI_HII_HANDLE  HiiHandle
  )
{
  EFI_STRING  String;

  String = HiiGetString (HiiHandle, Token, NULL);
  if (String == NULL) {
    String = AllocateCopyPool (StrSize (mLibUnknownString), mLibUnknownString);
    ASSERT (String != NULL);
  }

  return (CHAR16 *)String;
}

/**
  Count the storage space of a Unicode string.

  This function handles the Unicode string with NARROW_CHAR
  and WIDE_CHAR control characters. NARROW_HCAR and WIDE_CHAR
  does not count in the resultant output. If a WIDE_CHAR is
  hit, then 2 Unicode character will consume an output storage
  space with size of CHAR16 till a NARROW_CHAR is hit.

  If String is NULL, then ASSERT ().

  @param String          The input string to be counted.

  @return Storage space for the input string.

**/
UINTN
LibGetStringWidth (
  IN CHAR16  *String
  )
{
  UINTN  Index;
  UINTN  Count;
  UINTN  IncrementValue;

  ASSERT (String != NULL);
  if (String == NULL) {
    return 0;
  }

  Index          = 0;
  Count          = 0;
  IncrementValue = 1;

  do {
    //
    // Advance to the null-terminator or to the first width directive
    //
    for ( ;
          (String[Index] != NARROW_CHAR) && (String[Index] != WIDE_CHAR) && (String[Index] != 0);
          Index++, Count = Count + IncrementValue
          )
    {
    }

    //
    // We hit the null-terminator, we now have a count
    //
    if (String[Index] == 0) {
      break;
    }

    //
    // We encountered a narrow directive - strip it from the size calculation since it doesn't get printed
    // and also set the flag that determines what we increment by.(if narrow, increment by 1, if wide increment by 2)
    //
    if (String[Index] == NARROW_CHAR) {
      //
      // Skip to the next character
      //
      Index++;
      IncrementValue = 1;
    } else {
      //
      // Skip to the next character
      //
      Index++;
      IncrementValue = 2;
    }
  } while (String[Index] != 0);

  //
  // Increment by one to include the null-terminator in the size
  //
  Count++;

  return Count * sizeof (CHAR16);
}

/**
  Show all registered HotKey help strings on bottom Rows.

  @param FormData          The curent input form data info.
  @param SetState          Set HotKey or Clear HotKey

**/
VOID
PrintHotKeyHelpString (
  IN FORM_DISPLAY_ENGINE_FORM  *FormData,
  IN BOOLEAN                   SetState
  )
{
  UINTN                  CurrentCol;
  UINTN                  CurrentRow;
  UINTN                  BottomRowOfHotKeyHelp;
  UINTN                  ColumnIndexWidth;
  UINTN                  ColumnWidth;
  UINTN                  ColumnIndex;
  UINTN                  Index;
  EFI_SCREEN_DESCRIPTOR  LocalScreen;
  LIST_ENTRY             *Link;
  BROWSER_HOT_KEY        *HotKey;
  CHAR16                 BakChar;
  CHAR16                 *ColumnStr;

  CopyMem (&LocalScreen, &gScreenDimensions, sizeof (EFI_SCREEN_DESCRIPTOR));
  ColumnWidth           = (LocalScreen.RightColumn - LocalScreen.LeftColumn) / 3;
  BottomRowOfHotKeyHelp = LocalScreen.BottomRow - STATUS_BAR_HEIGHT - 3;
  ColumnStr             = gLibEmptyString;

  //
  // Calculate total number of Register HotKeys.
  //
  Index = 0;
  Link  = GetFirstNode (&FormData->HotKeyListHead);
  while (!IsNull (&FormData->HotKeyListHead, Link)) {
    HotKey = BROWSER_HOT_KEY_FROM_LINK (Link);
    //
    // Calculate help information Column and Row.
    //
    ColumnIndex = Index % 3;
    if (ColumnIndex == 0) {
      CurrentCol       = LocalScreen.LeftColumn + 2 * ColumnWidth;
      ColumnIndexWidth = LocalScreen.RightColumn - CurrentCol - 1;
    } else if (ColumnIndex == 1) {
      CurrentCol       = LocalScreen.LeftColumn + ColumnWidth;
      ColumnIndexWidth = ColumnWidth;
    } else {
      CurrentCol       = LocalScreen.LeftColumn + 2;
      ColumnIndexWidth = ColumnWidth - 2;
    }

    CurrentRow = BottomRowOfHotKeyHelp - Index / 3;

    //
    // Help string can't exceed ColumnWidth. One Row will show three Help information.
    //
    BakChar = L'\0';
    if (StrLen (HotKey->HelpString) > ColumnIndexWidth) {
      BakChar                              = HotKey->HelpString[ColumnIndexWidth];
      HotKey->HelpString[ColumnIndexWidth] = L'\0';
    }

    //
    // Print HotKey help string on bottom Row.
    //
    if (SetState) {
      ColumnStr = HotKey->HelpString;
    }

    PrintStringAtWithWidth (CurrentCol, CurrentRow, ColumnStr, ColumnIndexWidth);

    if (BakChar != L'\0') {
      HotKey->HelpString[ColumnIndexWidth] = BakChar;
    }

    //
    // Get Next Hot Key.
    //
    Link = GetNextNode (&FormData->HotKeyListHead, Link);
    Index++;
  }

  if (SetState) {
    //
    // Clear KeyHelp
    //
    CurrentRow  = BottomRowOfHotKeyHelp - Index / 3;
    ColumnIndex = Index % 3;
    if (ColumnIndex == 0) {
      CurrentCol       = LocalScreen.LeftColumn + 2 * ColumnWidth;
      ColumnIndexWidth = LocalScreen.RightColumn - CurrentCol - 1;
      ColumnIndex++;
      PrintStringAtWithWidth (CurrentCol, CurrentRow, gLibEmptyString, ColumnIndexWidth);
    }

    if (ColumnIndex == 1) {
      CurrentCol       = LocalScreen.LeftColumn + ColumnWidth;
      ColumnIndexWidth = ColumnWidth;
      PrintStringAtWithWidth (CurrentCol, CurrentRow, gLibEmptyString, ColumnIndexWidth);
    }
  }

  return;
}

/**
  Get step info from numeric opcode.

  @param[in] OpCode     The input numeric op code.

  @return step info for this opcode.
**/
UINT64
LibGetFieldFromNum (
  IN  EFI_IFR_OP_HEADER  *OpCode
  )
{
  EFI_IFR_NUMERIC  *NumericOp;
  UINT64           Step;

  NumericOp = (EFI_IFR_NUMERIC *)OpCode;

  switch (NumericOp->Flags & EFI_IFR_NUMERIC_SIZE) {
    case EFI_IFR_NUMERIC_SIZE_1:
      Step = NumericOp->data.u8.Step;
      break;

    case EFI_IFR_NUMERIC_SIZE_2:
      Step = NumericOp->data.u16.Step;
      break;

    case EFI_IFR_NUMERIC_SIZE_4:
      Step = NumericOp->data.u32.Step;
      break;

    case EFI_IFR_NUMERIC_SIZE_8:
      Step = NumericOp->data.u64.Step;
      break;

    default:
      Step = 0;
      break;
  }

  return Step;
}

/**
  Initialize the HII String Token to the correct values.

**/
VOID
InitializeLibStrings (
  VOID
  )
{
  mLibUnknownString = L"!";

  gEnterString       = LibGetToken (STRING_TOKEN (ENTER_STRING), mCDLStringPackHandle);
  gEnterCommitString = LibGetToken (STRING_TOKEN (ENTER_COMMIT_STRING), mCDLStringPackHandle);
  gEnterEscapeString = LibGetToken (STRING_TOKEN (ENTER_ESCAPE_STRING), mCDLStringPackHandle);
  gEscapeString      = LibGetToken (STRING_TOKEN (ESCAPE_STRING), mCDLStringPackHandle);
  gMoveHighlight     = LibGetToken (STRING_TOKEN (MOVE_HIGHLIGHT), mCDLStringPackHandle);
  gDecNumericInput   = LibGetToken (STRING_TOKEN (DEC_NUMERIC_INPUT), mCDLStringPackHandle);
  gHexNumericInput   = LibGetToken (STRING_TOKEN (HEX_NUMERIC_INPUT), mCDLStringPackHandle);
  gToggleCheckBox    = LibGetToken (STRING_TOKEN (TOGGLE_CHECK_BOX), mCDLStringPackHandle);

  gAreYouSure   = LibGetToken (STRING_TOKEN (ARE_YOU_SURE), mCDLStringPackHandle);
  gYesResponse  = LibGetToken (STRING_TOKEN (ARE_YOU_SURE_YES), mCDLStringPackHandle);
  gNoResponse   = LibGetToken (STRING_TOKEN (ARE_YOU_SURE_NO), mCDLStringPackHandle);
  gPlusString   = LibGetToken (STRING_TOKEN (PLUS_STRING), mCDLStringPackHandle);
  gMinusString  = LibGetToken (STRING_TOKEN (MINUS_STRING), mCDLStringPackHandle);
  gAdjustNumber = LibGetToken (STRING_TOKEN (ADJUST_NUMBER), mCDLStringPackHandle);
  gSaveChanges  = LibGetToken (STRING_TOKEN (SAVE_CHANGES), mCDLStringPackHandle);

  gLibEmptyString = LibGetToken (STRING_TOKEN (EMPTY_STRING), mCDLStringPackHandle);

  gNvUpdateMessage   = LibGetToken (STRING_TOKEN (NV_UPDATE_MESSAGE), mCDLStringPackHandle);
  gInputErrorMessage = LibGetToken (STRING_TOKEN (INPUT_ERROR_MESSAGE), mCDLStringPackHandle);

  //
  // SpaceBuffer;
  //
  mSpaceBuffer = AllocatePool ((SPACE_BUFFER_SIZE + 1) * sizeof (CHAR16));
  ASSERT (mSpaceBuffer != NULL);
  LibSetUnicodeMem (mSpaceBuffer, SPACE_BUFFER_SIZE, L' ');
  mSpaceBuffer[SPACE_BUFFER_SIZE] = L'\0';
}

/**
  Free the HII String.

**/
VOID
FreeLibStrings (
  VOID
  )
{
  FreePool (gEnterString);
  FreePool (gEnterCommitString);
  FreePool (gEnterEscapeString);
  FreePool (gEscapeString);
  FreePool (gMoveHighlight);
  FreePool (gDecNumericInput);
  FreePool (gHexNumericInput);
  FreePool (gToggleCheckBox);

  FreePool (gAreYouSure);
  FreePool (gYesResponse);
  FreePool (gNoResponse);
  FreePool (gPlusString);
  FreePool (gMinusString);
  FreePool (gAdjustNumber);
  FreePool (gSaveChanges);

  FreePool (gLibEmptyString);

  FreePool (gNvUpdateMessage);
  FreePool (gInputErrorMessage);

  FreePool (mSpaceBuffer);
}

/**
  Wait for a key to be pressed by user.

  @param Key         The key which is pressed by user.

  @retval EFI_SUCCESS The function always completed successfully.

**/
EFI_STATUS
WaitForKeyStroke (
  OUT  EFI_INPUT_KEY  *Key
  )
{
  EFI_STATUS  Status;
  UINTN       Index;

  while (TRUE) {
    Status = gST->ConIn->ReadKeyStroke (gST->ConIn, Key);
    if (!EFI_ERROR (Status)) {
      break;
    }

    if (Status != EFI_NOT_READY) {
      continue;
    }

    gBS->WaitForEvent (1, &gST->ConIn->WaitForKey, &Index);
  }

  return Status;
}

/**
  Set Buffer to Value for Size bytes.

  @param  Buffer                 Memory to set.
  @param  Size                   Number of bytes to set
  @param  Value                  Value of the set operation.

**/
VOID
LibSetUnicodeMem (
  IN VOID    *Buffer,
  IN UINTN   Size,
  IN CHAR16  Value
  )
{
  CHAR16  *Ptr;

  Ptr = Buffer;
  while ((Size--)  != 0) {
    *(Ptr++) = Value;
  }
}

/**
  The internal function prints to the EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL
  protocol instance.

  @param Width           Width of string to be print.
  @param Column          The position of the output string.
  @param Row             The position of the output string.
  @param Out             The EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL instance.
  @param Fmt             The format string.
  @param Args            The additional argument for the variables in the format string.

  @return Number of Unicode character printed.

**/
UINTN
PrintInternal (
  IN UINTN                            Width,
  IN UINTN                            Column,
  IN UINTN                            Row,
  IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *Out,
  IN CHAR16                           *Fmt,
  IN VA_LIST                          Args
  )
{
  CHAR16  *Buffer;
  UINTN   Index;
  UINTN   TotalCount;
  UINTN   PrintWidth;
  UINTN   CharWidth;
  UINTN   Attribute;
  UINTN   CellWidth;
  UINTN   CellHeight;
  UINTN   DrawColumn;
  UINTN   DrawRow;
  UINTN   DrawWidth;
  CHAR16  *Printable;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Foreground;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Background;

  //
  // For now, allocate an arbitrarily long buffer
  //
  Buffer = AllocateZeroPool (0x10000);
  ASSERT (Buffer);

  if (Column != (UINTN)-1) {
    Out->SetCursorPosition (Out, Column, Row);
    mModernCursorColumn = Column;
    mModernCursorRow    = Row;
  }

  UnicodeVSPrint (Buffer, 0x10000, Fmt, Args);

  Out->Mode->Attribute = Out->Mode->Attribute & 0x7f;

  Out->SetAttribute (Out, Out->Mode->Attribute);

  Index      = 0;
  TotalCount = 0;
  PrintWidth = 0;
  CharWidth  = 1;

  do {
    if (Buffer[Index] == 0) {
      break;
    }

    switch (Buffer[Index]) {
      case NARROW_CHAR:
        CharWidth = 1;
        break;
      case WIDE_CHAR:
        CharWidth = 2;
        break;
      default:
        PrintWidth += CharWidth;
        TotalCount += 1;
        break;
    }

    Index++;
  } while (Buffer[Index] != 0);

  //
  // We hit the end of the string - fill remaining space with SPACE.
  //
  if (PrintWidth < Width) {
    PrintWidth = Width;
  }

  if (!EFI_ERROR (ModernDisplayEnsureRenderer ())) {
    ModernDisplayGetCellMetrics (&CellWidth, &CellHeight);
    DrawColumn = (Column == (UINTN)-1) ? mModernCursorColumn : Column;
    DrawRow    = (Row == (UINTN)-1) ? mModernCursorRow : Row;
    DrawWidth  = (Width == 0) ? MAX (1, PrintWidth) : Width;
    Attribute  = Out->Mode->Attribute;
    Foreground = ModernDisplayForeground (Attribute);
    Background = ModernDisplayBackground (Attribute);

    ModernUiFillRect (
      &mModernRenderContext,
      (MODERN_UI_RECT){ DrawColumn * CellWidth, DrawRow * CellHeight, DrawWidth * CellWidth, CellHeight },
      Background
      );

    Printable = AllocateZeroPool ((StrLen (Buffer) + 1) * sizeof (CHAR16));
    if (Printable != NULL) {
      ModernDisplayCopyPrintable (Printable, StrLen (Buffer) + 1, Buffer);
      ModernUiDrawText (
        &mModernRenderContext,
        DrawColumn * CellWidth + 2,
        DrawRow * CellHeight + ((CellHeight > 18) ? ((CellHeight - 18) / 2) : 0),
        Printable,
        Foreground,
        Background
        );
      FreePool (Printable);
    }

    mModernCursorColumn = DrawColumn + TotalCount;
    mModernCursorRow    = DrawRow;
  }

  FreePool (Buffer);
  return TotalCount;
}

/**
  Prints a formatted unicode string to the default console, at
  the supplied cursor position.

  @param  Width      Width of String to be printed.
  @param  Column     The cursor position to print the string at.
  @param  Row        The cursor position to print the string at.
  @param  Fmt        Format string.
  @param  ...        Variable argument list for format string.

  @return Length of string printed to the console

**/
UINTN
EFIAPI
PrintAt (
  IN UINTN   Width,
  IN UINTN   Column,
  IN UINTN   Row,
  IN CHAR16  *Fmt,
  ...
  )
{
  VA_LIST  Args;
  UINTN    LengthOfPrinted;

  VA_START (Args, Fmt);
  LengthOfPrinted = PrintInternal (Width, Column, Row, gST->ConOut, Fmt, Args);
  VA_END (Args);
  return LengthOfPrinted;
}
