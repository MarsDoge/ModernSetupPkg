/** @file
  Read-only universal setting reference browser. No live bindings or edits.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include "ModernSetupAppInternal.h"
#include <Library/BaseLib.h>
#include "ModernSetupSettings.generated.h"

STATIC UINTN mCategories[MODERN_SETUP_SETTING_COUNT];
STATIC UINTN mCategoryCount;
STATIC UINTN mCategory;
STATIC UINTN mSelection;
STATIC UINTN mCategorySelection;
STATIC UINTN mLevel;
STATIC UINTN mHelpOffset;
STATIC CHAR16 mHelp[4096];
STATIC CHAR16 mNativeStatus[128];
STATIC BOOLEAN mHelpChinese;

// Explicit navigation allowlist, not a question/value binding or category match.
STATIC UINTN CatalogNativeRoute (CONST CHAR8 *Id)
{
  STATIC CONST CHAR8 *BootIds[] = {
    "boot.order", "boot.next", "boot.add", "boot.remove", "boot.file", "boot.timeout"
  };
  STATIC CONST CHAR8 *SecurityIds[] = {
    "security.secure_boot", "security.setup_mode", "security.pk", "security.kek",
    "security.db", "security.dbx", "security.key_management"
  };
  UINTN Index;
  for (Index = 0; Index < ARRAY_SIZE (BootIds); Index++) {
    if (AsciiStrCmp (Id, BootIds[Index]) == 0) {
      return 1;
    }
  }
  for (Index = 0; Index < ARRAY_SIZE (SecurityIds); Index++) {
    if (AsciiStrCmp (Id, SecurityIds[Index]) == 0) {
      return 2;
    }
  }
  return 0;
}

// Use the active App language, not the platform language or selector preview.
STATIC BOOLEAN CatalogChinese (VOID)
{
  CONST CHAR8 *Language = ModernUiGetLanguage ();
  return Language != NULL && Language[0] == 'z' && Language[1] == 'h';
}

CONST CHAR16 *ModernSetupCatalogUi (CONST CHAR16 *English, CONST CHAR16 *Chinese)
{
  return CatalogChinese () ? Chinese : English;
}

STATIC CONST CHAR8 *CatalogLocalized (CONST CHAR8 *English, CONST CHAR8 *Chinese)
{
  return (CatalogChinese () && Chinese != NULL && Chinese[0] != '\0') ? Chinese : English;
}

// Strict UTF-8 decoding. Malformed bytes become '?'; never read past NUL,
// overrun the destination, or emit half a UTF-16 surrogate pair.
STATIC VOID CatalogText (CONST CHAR8 *Source, CHAR16 *Dest, UINTN Chars)
{
  UINTN Used = 0;
  UINTN Count;
  UINTN Index;
  UINT32 Code;
  UINT32 Minimum;
  UINT8 Byte;
  if (Chars == 0 || Dest == NULL) {
    return;
  }
  Dest[0] = L'\0';
  if (Source == NULL) {
    return;
  }
  while (*Source != '\0' && Used + 1 < Chars) {
    Byte = (UINT8)*Source;
    Count = 1;
    Code = Byte;
    Minimum = 0;
    if (Byte >= 0xC2 && Byte <= 0xDF) {
      Count = 2; Code = Byte & 0x1F; Minimum = 0x80;
    } else if (Byte >= 0xE0 && Byte <= 0xEF) {
      Count = 3; Code = Byte & 0x0F; Minimum = 0x800;
    } else if (Byte >= 0xF0 && Byte <= 0xF4) {
      Count = 4; Code = Byte & 7; Minimum = 0x10000;
    } else if (Byte >= 0x80) {
      Code = '?';
    }
    for (Index = 1; Index < Count; Index++) {
      Byte = (UINT8)Source[Index];
      if (Byte < 0x80 || Byte > 0xBF) {
        break;
      }
      Code = (Code << 6) | (Byte & 0x3F);
    }
    if (Index != Count || Code < Minimum || Code > 0x10FFFF ||
        (Code >= 0xD800 && Code <= 0xDFFF)) {
      Code = '?'; Count = 1;
    }
    if (Code > 0xFFFF) {
      if (Used + 2 >= Chars) {
        break;
      }
      Code -= 0x10000;
      Dest[Used++] = (CHAR16)(0xD800 + (Code >> 10));
      Dest[Used++] = (CHAR16)(0xDC00 + (Code & 0x3FF));
    } else {
      Dest[Used++] = (CHAR16)Code;
    }
    Source += Count;
  }
  Dest[Used] = L'\0';
}

STATIC CONST CHAR8 *CatalogCategoryText (CONST CHAR8 *Id)
{
  UINTN Index;
  for (Index = 0; Index < ARRAY_SIZE (mModernSetupCategories); Index++) {
    if (AsciiStrCmp (Id, mModernSetupCategories[Index].Id) == 0) {
      return CatalogLocalized (mModernSetupCategories[Index].LabelEn,
                               mModernSetupCategories[Index].LabelZh);
    }
  }
  return Id;
}

STATIC VOID CatalogInit (VOID)
{
  UINTN Index;
  UINTN Category;
  if (mCategoryCount != 0) {
    return;
  }
  for (Index = 0; Index < MODERN_SETUP_SETTING_COUNT; Index++) {
    for (Category = 0; Category < mCategoryCount; Category++) {
      if (AsciiStrCmp (mModernSetupSettings[Index].Category,
                      mModernSetupSettings[mCategories[Category]].Category) == 0) {
        break;
      }
    }
    if (Category == mCategoryCount) {
      mCategories[mCategoryCount++] = Index;
    }
  }
}

STATIC UINTN CatalogItem (UINTN Ordinal)
{
  UINTN Index;
  for (Index = 0; Index < MODERN_SETUP_SETTING_COUNT; Index++) {
    if (AsciiStrCmp (mModernSetupSettings[Index].Category,
                    mModernSetupSettings[mCategories[mCategory]].Category) == 0) {
      if (Ordinal == 0) {
        return Index;
      }
      Ordinal--;
    }
  }
  return MODERN_SETUP_SETTING_COUNT;
}

STATIC UINTN CatalogCount (VOID)
{
  UINTN Count;
  if (mLevel == 0) {
    return mCategoryCount;
  }
  for (Count = 0; CatalogItem (Count) < MODERN_SETUP_SETTING_COUNT; Count++) {
  }
  return Count;
}

// One geometry contract for painted rows, selection window and pointer hits.
UINTN ModernSetupCatalogSelectableCount (VOID)
{
  CatalogInit ();
  return CatalogCount ();
}

STATIC UINTN CatalogRows (MODERN_UI_RENDER_CONTEXT *Ui, MODERN_UI_RECT *Panel)
{
  *Panel = ModernSetupContentRect (Ui);
  return (Panel->Height > 112 && Panel->Width > 80) ? (Panel->Height - 112) / 32 : 0;
}

STATIC VOID CatalogBuildHelp (UINTN Index)
{
  CHAR16 Help[2048];
  CHAR16 Owner[256];
  CHAR16 Label[256];
  CatalogText (CatalogLocalized (mModernSetupSettings[Index].LabelEn, mModernSetupSettings[Index].LabelZh), Label, ARRAY_SIZE (Label));
  CatalogText (CatalogLocalized (mModernSetupSettings[Index].HelpEn, mModernSetupSettings[Index].HelpZh), Help, ARRAY_SIZE (Help));
  CatalogText (mModernSetupSettings[Index].IntendedOwner, Owner, ARRAY_SIZE (Owner));
  UnicodeSPrint (mHelp, sizeof (mHelp),
    ModernSetupCatalogUi (L"%s | N/A. Unbound: no live backend is resolved. %s Intended owner: %s. No value can be edited, applied or submitted here. Open native setup is separate navigation, not an individual setting binding; the native owner controls available questions and saving.", L"%s | N/A。暂未接入配置数据。%s 配置提供方：%s。目录只提供说明，不能在这里修改数值。如需修改，请打开原生配置页面，在该页面中设置和保存。"),
    Label, Help, Owner);
}

STATIC VOID CatalogActivate (VOID)
{
  UINTN Index;
  UINTN Route;
  EFI_STATUS Status;
  if (mLevel == 0) {
    mCategorySelection = mSelection;
    mCategory = mSelection;
    mSelection = 0;
    mLevel = 1;
  } else if (mLevel == 1) {
    Index = CatalogItem (mSelection);
    if (Index >= MODERN_SETUP_SETTING_COUNT) {
      return;
    }
    CatalogBuildHelp (Index);
    mHelpChinese = CatalogChinese ();
    mNativeStatus[0] = L'\0';
    mHelpOffset = 0;
    mLevel = 2;
  } else if (mLevel == 2) {
    Index = CatalogItem (mSelection);
    if (Index >= MODERN_SETUP_SETTING_COUNT) {
      return;
    }
    Route = CatalogNativeRoute (mModernSetupSettings[Index].Id);
    if (Route == 0) {
      return;
    }
    Status = (Route == 1) ?
      ModernSetupOpenBootConfigurationWithFallback (mModernSetupImageHandle, FALSE) :
      ModernSetupOpenSecureBootConfiguration ();
    // Discovery and handoff helpers refresh caches; never infer a live value.
    UnicodeSPrint (mNativeStatus, sizeof (mNativeStatus),
      (Status == EFI_NOT_FOUND) ? ModernSetupCatalogUi (L"Native owner unavailable; value remains N/A", L"原生配置不可用；数值仍为 N/A") : ModernSetupCatalogUi (L"Native setup returned: %r; value remains N/A", L"原生配置返回：%r；数值仍为 N/A"), Status);
  }
}

BOOLEAN ModernSetupCatalogInput (MODERN_UI_RENDER_CONTEXT *Ui, MODERN_UI_INPUT_TYPE Type, UINTN X, UINTN Y)
{
  MODERN_UI_RECT Panel;
  UINTN Rows;
  UINTN Count;
  UINTN First;
  UINTN Hit;
  UINTN Width;
  CatalogInit ();
  Rows = CatalogRows (Ui, &Panel);
  Count = CatalogCount ();
  if (Count == 0 || Rows == 0) {
    return FALSE;
  }
  Width = MIN ((Panel.Width - 40) / (CatalogChinese () ? 18 : 8), 80);
  if (Type == ModernUiInputEscape || Type == ModernUiInputLeft) {
    if (mLevel == 2) {
      mLevel = 1;
    } else if (mLevel == 1) {
      mLevel = 0;
      mSelection = mCategorySelection;
    } else {
      return FALSE;
    }
    return TRUE;
  }
  if (Type == ModernUiInputUp || Type == ModernUiInputDown) {
    if (mLevel == 2) {
      if (Type == ModernUiInputUp) {
        mHelpOffset = (mHelpOffset >= Width) ? mHelpOffset - Width : 0;
      } else if (mHelpOffset + Rows * Width < StrLen (mHelp)) {
        mHelpOffset += Width;
      }
    } else if (Type == ModernUiInputUp) {
      mSelection = (mSelection == 0) ? Count - 1 : mSelection - 1;
    } else {
      mSelection = (mSelection + 1) % Count;
    }
    return TRUE;
  }
  if (Type == ModernUiInputEnter) {
    CatalogActivate ();
    return TRUE;
  }
  if (Type == ModernUiInputPointer) {
    if (X < Panel.X || X >= Panel.X + Panel.Width || Y < Panel.Y || Y >= Panel.Y + Panel.Height) {
      return FALSE;
    }
    if (Y < Panel.Y + 32) {
      return ModernSetupCatalogInput (Ui, ModernUiInputEscape, 0, 0);
    }
    if (mLevel == 2 && Y >= Panel.Y + 32 && Y < Panel.Y + 64) {
      CatalogActivate ();
      return TRUE;
    }
    if (Y >= Panel.Y + Panel.Height - 40) {
      if (mLevel == 2) {
        return ModernSetupCatalogInput (Ui, (X < Panel.X + Panel.Width / 2) ? ModernUiInputUp : ModernUiInputDown, 0, 0);
      }
      if (X < Panel.X + Panel.Width / 2) {
        mSelection = (mSelection >= Rows) ? mSelection - Rows : 0;
      } else {
        mSelection = MIN (mSelection + Rows, Count - 1);
      }
      return TRUE;
    }
    if (mLevel != 2 && Y >= Panel.Y + 64 && Y < Panel.Y + 64 + Rows * 32) {
      First = (mSelection / Rows) * Rows;
      Hit = First + (Y - Panel.Y - 64) / 32;
      if (Hit < Count) {
        mSelection = Hit;
        CatalogActivate ();
      }
    }
    return TRUE;
  }
  return FALSE;
}

VOID ModernSetupDrawCatalog (MODERN_UI_RENDER_CONTEXT *Ui, CONST MODERN_UI_THEME *Theme, SETUP_FOCUS Focus)
{
  MODERN_UI_RECT Panel;
  MODERN_UI_ROW_MODEL Row;
  UINTN Rows;
  UINTN Count;
  UINTN First;
  UINTN Index;
  UINTN Item;
  UINTN Width;
  UINTN Offset;
  UINTN Length;
  CHAR16 Text[256];
  CHAR16 Heading[256];
  CatalogInit ();
  Rows = CatalogRows (Ui, &Panel);
  Count = CatalogCount ();
  if (Rows == 0 || Count == 0) {
    return;
  }
  // A language change elsewhere in the App must not leave cached help stale.
  if (mLevel == 2 && mHelpChinese != CatalogChinese ()) {
    CatalogBuildHelp (CatalogItem (mSelection));
    mHelpChinese = CatalogChinese ();
    mHelpOffset = 0;
    mNativeStatus[0] = L'\0';
  }
  First = (mSelection / Rows) * Rows;
  if (mLevel == 0) {
    UnicodeSPrint (Heading, sizeof (Heading), ModernSetupCatalogUi (L"Categories: %u | Settings: %u | All N/A", L"分类：%u | 设置：%u | 全部 N/A"), mCategoryCount, (UINTN)MODERN_SETUP_SETTING_COUNT);
  } else {
    CatalogText (CatalogCategoryText (mModernSetupSettings[mCategories[mCategory]].Category), Heading, ARRAY_SIZE (Heading));
  }
  ModernUiDrawText (Ui, Panel.X + 16, Panel.Y + 8, ModernSetupCatalogUi (L"< Back | Up/Down | Enter: inspect", L"< 返回 | 上下键 | Enter：查看"), Theme->AccentYellow, Theme->Background);
  if (mLevel == 2) {
    Item = CatalogItem (mSelection);
    if (Item < MODERN_SETUP_SETTING_COUNT && CatalogNativeRoute (mModernSetupSettings[Item].Id) != 0) {
      StrCpyS (Heading, ARRAY_SIZE (Heading), ModernSetupCatalogUi (L"[Enter / Click] Open native setup >", L"[Enter / 点击] 打开原生配置 >"));
    } else {
      StrCpyS (Heading, ARRAY_SIZE (Heading), ModernSetupCatalogUi (L"N/A | No native navigation mapped", L"N/A | 未关联原生配置入口"));
    }
  }
  ModernUiDrawText (Ui, Panel.X + 16, Panel.Y + 38, Heading, Theme->AccentYellow, Theme->Background);
  Width = MIN ((Panel.Width - 40) / (CatalogChinese () ? 18 : 8), 80);
  for (Index = 0; Index < Rows; Index++) {
    if (mLevel == 2) {
      if (mNativeStatus[0] != L'\0' && Index == Rows - 1) {
        ModernUiDrawText (Ui, Panel.X + 16, Panel.Y + 64 + Index * 32, mNativeStatus, Theme->Warning, Theme->Background);
        continue;
      }
      Offset = mHelpOffset + Index * Width;
      if (Offset >= StrLen (mHelp)) {
        break;
      }
      Length = MIN (Width, StrLen (mHelp) - Offset);
      CopyMem (Text, mHelp + Offset, Length * sizeof (CHAR16));
      Text[Length] = L'\0';
      ModernUiDrawText (Ui, Panel.X + 16, Panel.Y + 64 + Index * 32, Text, Theme->Text, Theme->Background);
      continue;
    }
    if (First + Index >= Count) {
      break;
    }
    Item = (mLevel == 0) ? mCategories[First + Index] : CatalogItem (First + Index);
    CatalogText ((mLevel == 0) ? CatalogCategoryText (mModernSetupSettings[Item].Category) : CatalogLocalized (mModernSetupSettings[Item].LabelEn, mModernSetupSettings[Item].LabelZh), Text, ARRAY_SIZE (Text));
    ZeroMem (&Row, sizeof (Row));
    Row.Rect = (MODERN_UI_RECT){ Panel.X + 12, Panel.Y + 64 + Index * 32, Panel.Width - 24, 30 };
    Row.Prompt = Text;
    Row.Value = (mLevel == 0) ? ModernSetupCatalogUi (L"Browse >", L"浏览 >") : L"N/A";
    Row.Role = (Focus == SetupFocusContent && First + Index == mSelection) ? ModernUiRowSelected : ModernUiRowReadOnly;
    Row.ValueType = ModernUiValueText;
    ModernUiEngineDrawRows (Ui, &Row, 1, Theme);
  }
  if (mLevel == 2) {
    if (mNativeStatus[0] != L'\0') {
      ModernUiDrawText (Ui, Panel.X + 16, Panel.Y + 64 + (Rows - 1) * 32, mNativeStatus, Theme->Warning, Theme->Background);
    }
    StrCpyS (Text, ARRAY_SIZE (Text), ModernSetupCatalogUi (L"< Scroll up", L"< 向上滚动"));
  } else {
    UnicodeSPrint (Text, sizeof (Text), ModernSetupCatalogUi (L"< Previous | %u-%u / %u", L"< 上一页 | %u-%u / %u"), First + 1, MIN (First + Rows, Count), Count);
  }
  ModernUiDrawText (Ui, Panel.X + 16, Panel.Y + Panel.Height - 30, Text, Theme->AccentYellow, Theme->Background);
  ModernUiDrawText (Ui, Panel.X + Panel.Width / 2 + 16, Panel.Y + Panel.Height - 30,
    (mLevel == 2) ? ModernSetupCatalogUi (L"Scroll down >", L"向下滚动 >") : ModernSetupCatalogUi (L"Next >", L"下一页 >"), Theme->AccentYellow, Theme->Background);
}
