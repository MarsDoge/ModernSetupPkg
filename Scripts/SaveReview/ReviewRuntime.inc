/** @file Browser-owned synchronous review. SPDX-License-Identifier: BSD-2-Clause-Patent **/
#include <Protocol/ModernSetupChangeReview.h>

#define REVIEW_MAX_ITEMS 256
#define REVIEW_MAX_STORES 128

typedef struct {
  BROWSER_STORAGE *Storage;
  UINTN Size;
  UINT8 *Before;
  UINT8 *After;
  CHAR16 *Request;
  CHAR16 *Payload;
} REVIEW_STORAGE_COPY;

typedef struct {
  MODERN_SETUP_REVIEW_SNAPSHOT Public;
  MODERN_SETUP_REVIEW_ITEM Items[REVIEW_MAX_ITEMS];
  REVIEW_STORAGE_COPY Stores[REVIEW_MAX_STORES];
  UINTN StoreCount;
} REVIEW_CONTEXT;

STATIC REVIEW_CONTEXT *mApprovedReview;
STATIC UINT64 mReviewContextEpoch;
STATIC BOOLEAN mReviewBusy;
extern UINTN gBrowserContextCount;

STATIC VOID
ReviewFree (REVIEW_CONTEXT *C)
{
  UINTN I;
  for (I = 0; I < C->Public.ItemCount; I++) {
    if (C->Items[I].Path != NULL) { FreePool ((VOID *)C->Items[I].Path); }
    if (C->Items[I].OldValue != NULL) { ZeroMem ((VOID *)C->Items[I].OldValue, StrSize (C->Items[I].OldValue)); FreePool ((VOID *)C->Items[I].OldValue); }
    if (C->Items[I].NewValue != NULL) { ZeroMem ((VOID *)C->Items[I].NewValue, StrSize (C->Items[I].NewValue)); FreePool ((VOID *)C->Items[I].NewValue); }
  }
  for (I = 0; I < C->StoreCount; I++) {
    if (C->Stores[I].Request != NULL) { FreePool (C->Stores[I].Request); }
    if (C->Stores[I].Payload != NULL) { ZeroMem (C->Stores[I].Payload, StrSize (C->Stores[I].Payload)); FreePool (C->Stores[I].Payload); }
    if (C->Stores[I].Before != NULL) {
      ZeroMem (C->Stores[I].Before, C->Stores[I].Size);
      FreePool (C->Stores[I].Before);
    }
    if (C->Stores[I].After != NULL) {
      ZeroMem (C->Stores[I].After, C->Stores[I].Size);
      FreePool (C->Stores[I].After);
    }
  }
  FreePool (C);
}

STATIC VOID
ModernReviewFinish (VOID)
{
  if (mApprovedReview != NULL) { ReviewFree (mApprovedReview); mApprovedReview = NULL; }
}

/* Compare the actual serialized bytes, not just the edit buffer. This is called
   immediately before every native route, after callbacks/serialization. */
STATIC EFI_STATUS
ModernReviewRouteGuard (BROWSER_STORAGE *Storage, CHAR16 *Request, CHAR16 *Payload)
{
  UINTN I;
  REVIEW_STORAGE_COPY *Copy;
  if (mApprovedReview == NULL || mHiiPackageListUpdated ||
      mApprovedReview->Public.ContextId != mReviewContextEpoch) { return EFI_ABORTED; }
  for (I = 0; I < mApprovedReview->StoreCount; I++) {
    Copy = &mApprovedReview->Stores[I];
    if (Copy->Storage == Storage && Copy->Request != NULL && Copy->Payload != NULL &&
        StrCmp (Copy->Request, Request) == 0 && StrCmp (Copy->Payload, Payload) == 0) { return EFI_SUCCESS; }
  }
  return EFI_ABORTED;
}

/* Success from ConfigToBlock alone does not prove coverage. Require every byte,
   valid hex VALUE lengths and no unparsed response suffix. */
STATIC BOOLEAN
ModernReviewFullCoverage (BROWSER_STORAGE *Storage, CHAR16 *Response)
{
  UINT8 *Mask;
  CHAR16 *P, *End;
  UINTN Offset, Width, I;
  BOOLEAN Complete;
  if (Storage->Type == EFI_HII_VARSTORE_NAME_VALUE) { return FALSE; }
  if (Response == NULL || Storage->Size == 0) { return FALSE; }
  Mask = AllocateZeroPool (Storage->Size);
  if (Mask == NULL) { return FALSE; }
  Complete = FALSE;
  P = StrStr (Response, L"&OFFSET=");
  while (P != NULL) {
    P += 8;
    if (EFI_ERROR (StrHexToUintnS (P, &End, &Offset)) || End == P || StrnCmp (End, L"&WIDTH=", 7) != 0) { goto Done; }
    P = End + 7;
    if (EFI_ERROR (StrHexToUintnS (P, &End, &Width)) || End == P || Width == 0 ||
        Offset > Storage->Size || Width > Storage->Size - Offset || StrnCmp (End, L"&VALUE=", 7) != 0) { goto Done; }
    P = End + 7;
    for (I = 0; I < Width * 2; I++, P++) {
      if (!((*P >= L'0' && *P <= L'9') || (*P >= L'a' && *P <= L'f') || (*P >= L'A' && *P <= L'F'))) { goto Done; }
    }
    SetMem (Mask + Offset, Width, 1);
    if (*P == 0) { break; }
    if (StrnCmp (P, L"&OFFSET=", 8) != 0) { goto Done; }
  }
  for (I = 0; I < Storage->Size; I++) { if (Mask[I] == 0) { goto Done; } }
  Complete = TRUE;
Done:
  FreePool (Mask);
  return Complete;
}

/* Validate the exact native request/header and each complete name/value token.
   Request order is deliberately required; ambiguous/extra/duplicate fields fail
   closed. Check the node set too: synchronization covers the entire storage. */
STATIC BOOLEAN
ModernReviewNameCoverage (FORMSET_STORAGE *Fs, CHAR16 *Response, BOOLEAN Converted)
{
  CHAR16 *R, *P, *Name, *Value, *Start, *End;
  UINTN Header, Length, Count;
  LIST_ENTRY *Link;
  NAME_VALUE_NODE *Node;
  BOOLEAN Found;
  if (Fs->ConfigHdr == NULL || Fs->ConfigRequest == NULL || Response == NULL) { return FALSE; }
  Header = StrLen (Fs->ConfigHdr);
  if (Header == 0 || StrnCmp (Response, Fs->ConfigHdr, Header) != 0 ||
      StrnCmp (Fs->ConfigRequest, Fs->ConfigHdr, Header) != 0) { return FALSE; }
  R = Fs->ConfigRequest + Header;
  P = Response + Header;
  if (*R != L'&') { return FALSE; }
  while (*R != 0) {
    if (*R++ != L'&' || *P++ != L'&') { return FALSE; }
    Name = R;
    while (*R != 0 && *R != L'&') { if (*R == L'=') { return FALSE; } R++; }
    Length = R - Name;
    if (Length == 0 || StrnCmp (P, Name, Length) != 0 || P[Length] != L'=') { return FALSE; }
    P += Length + 1;
    Value = P;
    while (*P != 0 && *P != L'&') {
      if (!((*P >= L'0' && *P <= L'9') || (*P >= L'a' && *P <= L'f') || (*P >= L'A' && *P <= L'F'))) { return FALSE; }
      P++;
    }
    if (((P - Value) % 2) != 0) { return FALSE; }
    Found = FALSE;
    for (Link = GetFirstNode (&Fs->BrowserStorage->NameValueListHead);
         !IsNull (&Fs->BrowserStorage->NameValueListHead, Link);
         Link = GetNextNode (&Fs->BrowserStorage->NameValueListHead, Link)) {
      Node = NAME_VALUE_NODE_FROM_LINK (Link);
      if (Node->Name != NULL && StrLen (Node->Name) == Length && StrnCmp (Node->Name, Name, Length) == 0) {
        if (Found) { return FALSE; }
        Found = TRUE;
        if (Converted && (Node->EditValue == NULL || StrLen (Node->EditValue) != (UINTN)(P - Value) ||
                          StrnCmp (Node->EditValue, Value, P - Value) != 0)) { return FALSE; }
      }
    }
    if (!Found) { return FALSE; }
  }
  if (*P != 0) { return FALSE; }
  for (Link = GetFirstNode (&Fs->BrowserStorage->NameValueListHead);
       !IsNull (&Fs->BrowserStorage->NameValueListHead, Link);
       Link = GetNextNode (&Fs->BrowserStorage->NameValueListHead, Link)) {
    Node = NAME_VALUE_NODE_FROM_LINK (Link);
    Count = 0;
    for (Start = Fs->ConfigRequest + Header; *Start != 0; Start = End) {
      Start++;
      End = Start;
      while (*End != 0 && *End != L'&') { End++; }
      if (Node->Name != NULL && StrLen (Node->Name) == (UINTN)(End - Start) &&
          StrnCmp (Node->Name, Start, End - Start) == 0) { Count++; }
    }
    if (Count != 1) { return FALSE; }
  }
  return TRUE;
}

/* Native name/value conversion mutates its input and ignores setter failures.
   Preserve the validated response for an exact post-conversion node comparison;
   wipe the entire temporary allocation, including native-inserted NULs. */
STATIC EFI_STATUS
ModernReviewLoadResponse (FORMSET_STORAGE *Fs, CHAR16 *Response)
{
  CHAR16 *Copy;
  UINTN Size;
  EFI_STATUS Status;
  if (Fs->BrowserStorage->Type != EFI_HII_VARSTORE_NAME_VALUE) {
    if (!ModernReviewFullCoverage (Fs->BrowserStorage, Response)) { return EFI_COMPROMISED_DATA; }
    return ConfigRespToStorage (Fs->BrowserStorage, Response);
  }
  if (!ModernReviewNameCoverage (Fs, Response, FALSE)) { return EFI_COMPROMISED_DATA; }
  Size = StrSize (Response);
  Copy = AllocateCopyPool (Size, Response);
  if (Copy == NULL) { return EFI_OUT_OF_RESOURCES; }
  Status = ConfigRespToStorage (Fs->BrowserStorage, Copy);
  ZeroMem (Copy, Size);
  FreePool (Copy);
  if (!EFI_ERROR (Status) && !ModernReviewNameCoverage (Fs, Response, TRUE)) { return EFI_COMPROMISED_DATA; }
  return Status;
}

STATIC BOOLEAN
ModernReviewNamesSynchronized (BROWSER_STORAGE *Storage)
{
  LIST_ENTRY *Link;
  NAME_VALUE_NODE *Node;
  if (Storage->Type != EFI_HII_VARSTORE_NAME_VALUE) { return TRUE; }
  for (Link = GetFirstNode (&Storage->NameValueListHead); !IsNull (&Storage->NameValueListHead, Link);
       Link = GetNextNode (&Storage->NameValueListHead, Link)) {
    Node = NAME_VALUE_NODE_FROM_LINK (Link);
    if (Node->Value == NULL || Node->EditValue == NULL || StrCmp (Node->Value, Node->EditValue) != 0) { return FALSE; }
  }
  return TRUE;
}

STATIC EFI_STATUS
ReviewOpaque (REVIEW_CONTEXT *C, BOOLEAN Known)
{
  MODERN_SETUP_REVIEW_ITEM *I;
  if (!Known) { C->Public.Complete = FALSE; }
  if (C->Public.ItemCount == REVIEW_MAX_ITEMS) { return EFI_OUT_OF_RESOURCES; }
  I = &C->Items[C->Public.ItemCount++];
  I->Flags = MODERN_SETUP_REVIEW_OPAQUE | MODERN_SETUP_REVIEW_REDACTED;
  if (Known) {
    I->Flags |= MODERN_SETUP_REVIEW_BASELINE_KNOWN;
    I->Path = AllocateCopyPool (sizeof (L"Additional protected changes (values hidden)"), L"Additional protected changes (values hidden)");
  } else {
    I->Path = AllocateCopyPool (sizeof (L"Unknown baseline or scope - save blocked"), L"Unknown baseline or scope - save blocked");
  }
  return I->Path == NULL ? EFI_OUT_OF_RESOURCES : EFI_SUCCESS;
}

/* Parse only native OFFSET/WIDTH request elements; never serialize values. */
STATIC EFI_STATUS
ReviewMask (CHAR16 *Request, UINTN Size, UINT8 *Mask)
{
  CHAR16 *P;
  CHAR16 *End;
  UINTN Offset;
  UINTN Width;
  if (Request == NULL) { return EFI_INVALID_PARAMETER; }
  P = StrStr (Request, L"&OFFSET=");
  if (P == NULL) { return EFI_UNSUPPORTED; }
  while (P != NULL) {
    P += 8;
    if (EFI_ERROR (StrHexToUintnS (P, &End, &Offset)) || (End == P) ||
        (StrnCmp (End, L"&WIDTH=", 7) != 0)) { return EFI_UNSUPPORTED; }
    P = End + 7;
    if (EFI_ERROR (StrHexToUintnS (P, &End, &Width)) || (End == P) ||
        (Offset > Size) || (Width > Size - Offset)) { return EFI_UNSUPPORTED; }
    SetMem (Mask + Offset, Width, 1);
    if (*End == 0) { break; }
    if (StrnCmp (End, L"&OFFSET=", 8) != 0) { return EFI_UNSUPPORTED; }
    P = End;
  }
  return EFI_SUCCESS;
}

STATIC CHAR16 *
ReviewValue (FORM_BROWSER_FORMSET *Fs, FORM_BROWSER_STATEMENT *Q, UINT8 *Buffer)
{
  UINT64 V;
  LIST_ENTRY *L;
  QUESTION_OPTION *O;
  CHAR16 Text[48];
  UINTN Width, Index, Used, PartLength;
  CHAR16 *Result, *Part;
  UINT8 *Data;
  Data = Buffer + Q->VarStoreInfo.VarOffset;
  if (Q->Operand == EFI_IFR_STRING_OP) {
    // IFR strings may occupy the complete fixed-width field without a NUL.
    for (Width = 0; Width + sizeof (CHAR16) <= Q->StorageWidth; Width += sizeof (CHAR16)) {
      UINT16 Ch;
      CopyMem (&Ch, Data + Width, sizeof (Ch));
      if (Ch == 0) { break; }
    }
    Result = AllocateZeroPool (Width + sizeof (CHAR16));
    if (Result != NULL) { CopyMem (Result, Data, Width); }
    return Result;
  }
  if (Q->Operand == EFI_IFR_ORDERED_LIST_OP) {
    Width = Q->StorageWidth / Q->MaxContainers;
    // Bounded output, fail closed instead of truncating an option label/list.
    Result = AllocateZeroPool (8192);
    if (Result == NULL) { return NULL; }
    Used = 0;
    for (Index = 0; Index < Q->MaxContainers; Index++) {
      V = 0;
      CopyMem (&V, Data + Index * Width, Width);
      if (V == 0) { break; } // native ordered-list terminator
      Part = NULL;
      for (L = GetFirstNode (&Q->OptionListHead); !IsNull (&Q->OptionListHead, L); L = GetNextNode (&Q->OptionListHead, L)) {
        O = QUESTION_OPTION_FROM_LINK (L);
        if (CompareMem (&V, &O->Value.Value, Width) == 0) { Part = HiiGetString (Fs->HiiHandle, O->Text, NULL); break; }
      }
      if (Part == NULL) {
        UnicodeSPrint (Text, sizeof (Text), L"%Lu", V);
        Part = AllocateCopyPool (StrSize (Text), Text);
      }
      if (Part == NULL) { FreePool (Result); return NULL; }
      PartLength = StrLen (Part);
      if (Used + PartLength + 4 >= 4096) { FreePool (Part); FreePool (Result); return NULL; }
      if (Used != 0) { Result[Used++] = L' '; Result[Used++] = L'>'; Result[Used++] = L' '; }
      CopyMem (Result + Used, Part, PartLength * sizeof (CHAR16));
      Used += PartLength;
      FreePool (Part);
    }
    return Result;
  }
  V = 0;
  CopyMem (&V, Data, Q->StorageWidth);
  if (Q->Operand == EFI_IFR_ONE_OF_OP) {
    for (L = GetFirstNode (&Q->OptionListHead); !IsNull (&Q->OptionListHead, L); L = GetNextNode (&Q->OptionListHead, L)) {
      O = QUESTION_OPTION_FROM_LINK (L);
      if (CompareMem (&V, &O->Value.Value, Q->StorageWidth) == 0) {
        return HiiGetString (Fs->HiiHandle, O->Text, NULL);
      }
    }
  }
  UnicodeSPrint (Text, sizeof (Text), L"%Lu", V);
  return AllocateCopyPool (StrSize (Text), Text);
}

/* Conservative alias policy: reveal public typed values only when every
   overlapping question has the same nonsecret span. Passwords, bitfields,
   expression/suppression-controlled fields and unknown bytes are never exported. */
STATIC BOOLEAN
ReviewScalar (FORM_BROWSER_STATEMENT *Q)
{
  UINTN Width;
  if (Q->QuestionReferToBitField || Q->StorageWidth == 0 || Q->Expression != NULL ||
      Q->ReadExpression != NULL || Q->ValueExpression != NULL) { return FALSE; }
  if (Q->Operand == EFI_IFR_STRING_OP) { return (BOOLEAN)(Q->StorageWidth % sizeof (CHAR16) == 0); }
  if (Q->Operand == EFI_IFR_ORDERED_LIST_OP) {
    if (Q->MaxContainers == 0 || Q->StorageWidth % Q->MaxContainers != 0) { return FALSE; }
    Width = Q->StorageWidth / Q->MaxContainers;
    return (BOOLEAN)(Width == 1 || Width == 2 || Width == 4 || Width == 8);
  }
  return (BOOLEAN)(Q->StorageWidth <= sizeof (UINT64) &&
    (Q->Operand == EFI_IFR_NUMERIC_OP || Q->Operand == EFI_IFR_ONE_OF_OP || Q->Operand == EFI_IFR_CHECKBOX_OP));
}

STATIC BOOLEAN
ReviewAliasesSafe (BROWSER_STORAGE *S, FORM_BROWSER_STATEMENT *Q)
{
  LIST_ENTRY *A, *B, *D;
  FORM_BROWSER_FORMSET *Fs;
  FORM_BROWSER_FORM *F;
  FORM_BROWSER_STATEMENT *Other;
  UINTN Start, End, Os, Oe;
  // Suspended contexts can contain secret aliases sharing global storage.
  // Until all their suppression state is evaluated, reveal no nested values.
  if (gBrowserContextCount > 1) { return FALSE; }
  Start = Q->VarStoreInfo.VarOffset;
  End = Start + Q->StorageWidth;
  for (A = GetFirstNode (&gBrowserFormSetList); !IsNull (&gBrowserFormSetList, A); A = GetNextNode (&gBrowserFormSetList, A)) {
    Fs = FORM_BROWSER_FORMSET_FROM_LINK (A);
    for (B = GetFirstNode (&Fs->FormListHead); !IsNull (&Fs->FormListHead, B); B = GetNextNode (&Fs->FormListHead, B)) {
      F = FORM_BROWSER_FORM_FROM_LINK (B);
      for (D = GetFirstNode (&F->StatementListHead); !IsNull (&F->StatementListHead, D); D = GetNextNode (&F->StatementListHead, D)) {
        Other = FORM_BROWSER_STATEMENT_FROM_LINK (D);
        if (Other->Storage != S) { continue; }
        Os = Other->VarStoreInfo.VarOffset;
        Oe = Os + Other->StorageWidth;
        if (Os < End && Start < Oe &&
            (!ReviewScalar (Other) || Os != Start || Oe != End || F->SuppressExpression != NULL)) { return FALSE; }
      }
    }
  }
  return TRUE;
}

STATIC EFI_STATUS
ReviewRequest (REVIEW_CONTEXT *C, FORM_BROWSER_FORMSET *Fs, BROWSER_STORAGE *S, CHAR16 *Request)
{
  UINT8 *Mask;
  UINTN N, J, Offset, Width;
  LIST_ENTRY *A, *B;
  FORM_BROWSER_FORM *F;
  FORM_BROWSER_STATEMENT *Q;
  MODERN_SETUP_REVIEW_ITEM *Item;
  REVIEW_STORAGE_COPY *Copy;
  EFI_STATUS Status;
  CHAR16 *Title, *Prompt;
  UINTN Length;
  BOOLEAN Changed;
  BOOLEAN Reveal;
  NAME_VALUE_NODE *Node;
  if (C->StoreCount == REVIEW_MAX_STORES) { return EFI_OUT_OF_RESOURCES; }
  Copy = &C->Stores[C->StoreCount++];
  Copy->Storage = S;
  Copy->Request = AllocateCopyPool (StrSize (Request), Request);
  Status = StorageToConfigResp (S, &Copy->Payload, Request, TRUE);
  if (EFI_ERROR (Status) || Copy->Request == NULL || Copy->Payload == NULL) {
    return EFI_ERROR (Status) ? Status : EFI_OUT_OF_RESOURCES;
  }
  if (S->ModernReviewBaselineKnown && S->Type == EFI_HII_VARSTORE_NAME_VALUE) {
    // An unchanged auxiliary name/value store must not block buffer edits.
    // Changed name/value scopes remain unsupported and fail closed.
    for (A = GetFirstNode (&S->NameValueListHead); !IsNull (&S->NameValueListHead, A); A = GetNextNode (&S->NameValueListHead, A)) {
      Node = NAME_VALUE_NODE_FROM_LINK (A);
      if (Node->Value == NULL || Node->EditValue == NULL || StrCmp (Node->Value, Node->EditValue) != 0) { return ReviewOpaque (C, FALSE); }
    }
    // Size=0: recheck unchanged names after UI
    return EFI_SUCCESS;
  }
  if (!S->ModernReviewBaselineKnown ||
      (S->Type != EFI_HII_VARSTORE_BUFFER && S->Type != EFI_HII_VARSTORE_EFI_VARIABLE_BUFFER) ||
      S->Buffer == NULL || S->EditBuffer == NULL || S->Size == 0) { return ReviewOpaque (C, FALSE); }
  Copy->Size = S->Size;
  Copy->Before = AllocateCopyPool (S->Size, S->Buffer);
  Copy->After = AllocateCopyPool (S->Size, S->EditBuffer);
  Mask = AllocateZeroPool (S->Size);
  if (Copy->Before == NULL || Copy->After == NULL || Mask == NULL) {
    if (Mask != NULL) { FreePool (Mask); }
    return EFI_OUT_OF_RESOURCES;
  }
  Status = ReviewMask (Request, S->Size, Mask);
  if (EFI_ERROR (Status)) { FreePool (Mask); return ReviewOpaque (C, FALSE); }
  for (A = GetFirstNode (&Fs->FormListHead); !IsNull (&Fs->FormListHead, A); A = GetNextNode (&Fs->FormListHead, A)) {
    F = FORM_BROWSER_FORM_FROM_LINK (A);
    for (B = GetFirstNode (&F->StatementListHead); !IsNull (&F->StatementListHead, B); B = GetNextNode (&F->StatementListHead, B)) {
      Q = FORM_BROWSER_STATEMENT_FROM_LINK (B);
      if (Q->Storage != S || Q->StorageWidth == 0 || Q->Expression != NULL || F->SuppressExpression != NULL) { continue; }
      Reveal = (BOOLEAN)(ReviewScalar (Q) && ReviewAliasesSafe (S, Q));
      Offset = Q->VarStoreInfo.VarOffset;
      Width = Q->StorageWidth;
      if (Offset > S->Size || Width > S->Size - Offset) { continue; }
      Changed = FALSE;
      for (J = Offset; J < Offset + Width; J++) {
        if (Mask[J] == 0) { break; }
        if (Copy->Before[J] != Copy->After[J]) { Changed = TRUE; }
      }
      if (J != Offset + Width || !Changed) { continue; }
      if (C->Public.ItemCount == REVIEW_MAX_ITEMS) { FreePool (Mask); return EFI_OUT_OF_RESOURCES; }
      Item = &C->Items[C->Public.ItemCount++];
      CopyGuid (&Item->FormSetGuid, &Fs->Guid);
      CopyGuid (&Item->StorageGuid, &S->Guid);
      Item->FormId = F->FormId;
      Item->QuestionId = Q->QuestionId;
      Item->Flags = MODERN_SETUP_REVIEW_BASELINE_KNOWN;
      Title = HiiGetString (Fs->HiiHandle, F->FormTitle, NULL);
      Prompt = HiiGetString (Fs->HiiHandle, Q->Prompt, NULL);
      Length = (Title == NULL ? 0 : StrSize (Title)) + (Prompt == NULL ? 0 : StrSize (Prompt)) + 16;
      Item->Path = AllocateZeroPool (Length);
      if (Reveal) {
        Item->OldValue = ReviewValue (Fs, Q, Copy->Before);
        Item->NewValue = ReviewValue (Fs, Q, Copy->After);
      } else {
        Item->Flags |= MODERN_SETUP_REVIEW_REDACTED;
      }
      if (Item->Path != NULL && Title != NULL && Prompt != NULL) { UnicodeSPrint ((CHAR16 *)Item->Path, Length, L"%s / %s", Title, Prompt); }
      Status = (Title == NULL || Prompt == NULL || Item->Path == NULL || (Reveal && (Item->OldValue == NULL || Item->NewValue == NULL))) ? EFI_OUT_OF_RESOURCES : EFI_SUCCESS;
      if (Title != NULL) { FreePool (Title); }
      if (Prompt != NULL) { FreePool (Prompt); }
      if (EFI_ERROR (Status)) { FreePool (Mask); return Status; }
      SetMem (Mask + Offset, Width, 0); // aliases cannot duplicate this span
    }
  }
  for (N = 0; N < S->Size; N++) {
    if (Mask[N] != 0 && Copy->Before[N] != Copy->After[N]) {
      Status = ReviewOpaque (C, TRUE);
      break;
    }
  }
  FreePool (Mask);
  return Status;
}

STATIC EFI_STATUS
ReviewFormSet (REVIEW_CONTEXT *C, FORM_BROWSER_FORMSET *Fs, FORM_BROWSER_FORM *Form, BOOLEAN FormOnly)
{
  LIST_ENTRY *L;
  FORM_BROWSER_CONFIG_REQUEST *R;
  FORMSET_STORAGE *S;
  EFI_STATUS Status;
  if (Fs == NULL || (FormOnly && Form == NULL)) { return EFI_INVALID_PARAMETER; }
  if (FormOnly) {
    if (!IsNvUpdateRequiredForForm (Form)) { return EFI_SUCCESS; }
    for (L = GetFirstNode (&Form->ConfigRequestHead); !IsNull (&Form->ConfigRequestHead, L); L = GetNextNode (&Form->ConfigRequestHead, L)) {
      R = FORM_BROWSER_CONFIG_REQUEST_FROM_LINK (L);
      if (R->Storage->Type == EFI_HII_VARSTORE_EFI_VARIABLE || R->ElementCount == 0) { continue; }
      Status = ReviewRequest (C, Fs, R->Storage, R->ConfigRequest);
      if (EFI_ERROR (Status)) { return Status; }
    }
  } else {
    if (!IsNvUpdateRequiredForFormSet (Fs)) { return EFI_SUCCESS; }
    for (L = GetFirstNode (&Fs->StorageListHead); !IsNull (&Fs->StorageListHead, L); L = GetNextNode (&Fs->StorageListHead, L)) {
      S = FORMSET_STORAGE_FROM_LINK (L);
      if (S->BrowserStorage->Type == EFI_HII_VARSTORE_EFI_VARIABLE || S->ElementCount == 0) { continue; }
      Status = ReviewRequest (C, Fs, S->BrowserStorage, S->ConfigRequest);
      if (EFI_ERROR (Status)) { return Status; }
    }
  }
  return EFI_SUCCESS;
}

STATIC VOID
ModernReviewSaveError (VOID)
{
  EFI_GUID Guid = MODERN_SETUP_CHANGE_REVIEW_PROTOCOL_GUID;
  MODERN_SETUP_CHANGE_REVIEW_PROTOCOL *Ui;
  MODERN_SETUP_REVIEW_SNAPSHOT Snapshot;
  MODERN_SETUP_REVIEW_DECISION Decision;
  ZeroMem (&Snapshot, sizeof (Snapshot));
  Snapshot.Revision = MODERN_SETUP_CHANGE_REVIEW_REVISION;
  Snapshot.Size = sizeof (Snapshot);
  if (!EFI_ERROR (gBS->LocateProtocol (&Guid, NULL, (VOID **)&Ui)) && Ui->Revision == MODERN_SETUP_CHANGE_REVIEW_REVISION) {
    Ui->Review (Ui, &Snapshot, &Decision);
  }
}

STATIC EFI_STATUS
ModernReviewSubmit (FORM_BROWSER_FORMSET *Fs, FORM_BROWSER_FORM *Form, BROWSER_SETTING_SCOPE Scope)
{
  REVIEW_CONTEXT *C;
  MODERN_SETUP_CHANGE_REVIEW_PROTOCOL *Ui;
  MODERN_SETUP_REVIEW_DECISION Decision;
  EFI_GUID Guid = MODERN_SETUP_CHANGE_REVIEW_PROTOCOL_GUID;
  EFI_STATUS Status;
  LIST_ENTRY *L, *Next;
  FORM_BROWSER_FORMSET *Current;
  UI_MENU_SELECTION *Selection;
  UINTN I;
  // Never replace a live approval from a reentrant submit.
  if (mReviewBusy || mApprovedReview != NULL || gBrowserContextCount > 1) { return EFI_ABORTED; }
  C = AllocateZeroPool (sizeof (*C));
  if (C == NULL) { return EFI_OUT_OF_RESOURCES; }
  C->Public.Revision = MODERN_SETUP_CHANGE_REVIEW_REVISION;
  C->Public.Size = sizeof (C->Public);
  C->Public.Items = C->Items;
  C->Public.Complete = TRUE;
  mReviewBusy = TRUE;
  C->Public.ContextId = mReviewContextEpoch;
  Selection = gCurrentSelection;
  Status = EFI_SUCCESS;
  if (Scope == SystemLevel) {
    C->Public.Scope = ModernSetupReviewSystem;
    for (L = GetFirstNode (&gBrowserFormSetList); !IsNull (&gBrowserFormSetList, L); L = Next) {
      Next = GetNextNode (&gBrowserFormSetList, L);
      Current = FORM_BROWSER_FORMSET_FROM_LINK (L);
      if (!ValidateFormSet (Current)) { continue; }
      Status = ReviewFormSet (C, Current, NULL, FALSE);
      if (EFI_ERROR (Status)) { break; }
    }
  } else if (Scope == FormLevel || Scope == FormSetLevel) {
    C->Public.Scope = Scope == FormLevel ? ModernSetupReviewForm : ModernSetupReviewFormSet;
    Status = ReviewFormSet (C, Fs, Form, (BOOLEAN)(Scope == FormLevel));
  } else { Status = EFI_UNSUPPORTED; }
  if (!EFI_ERROR (Status) && C->Public.ItemCount != 0) {
    Status = gBS->LocateProtocol (&Guid, NULL, (VOID **)&Ui);
    Decision = ModernSetupContinueEditing;
    if (!EFI_ERROR (Status) && Ui->Revision == MODERN_SETUP_CHANGE_REVIEW_REVISION) {
      Status = Ui->Review (Ui, &C->Public, &Decision);
      if (!EFI_ERROR (Status) && (!C->Public.Complete || Decision != ModernSetupConfirmSave)) { Status = EFI_ABORTED; }
    } else { Status = EFI_UNSUPPORTED; }
    // Reject refresh/reentry before dereferencing borrowed browser pointers.
    if (mHiiPackageListUpdated || gCurrentSelection != Selection || mReviewContextEpoch != C->Public.ContextId) { Status = EFI_ABORTED; }
    if (!EFI_ERROR (Status)) {
      for (I = 0; I < C->StoreCount; I++) {
        if (C->Stores[I].Size == 0) {
          LIST_ENTRY *NameLink;
          NAME_VALUE_NODE *NameNode;
          BROWSER_STORAGE *NameStore;
          NameStore = C->Stores[I].Storage;
          if (!NameStore->ModernReviewBaselineKnown || NameStore->Type != EFI_HII_VARSTORE_NAME_VALUE) { Status = EFI_ABORTED; break; }
          for (NameLink = GetFirstNode (&NameStore->NameValueListHead); !IsNull (&NameStore->NameValueListHead, NameLink); NameLink = GetNextNode (&NameStore->NameValueListHead, NameLink)) {
            NameNode = NAME_VALUE_NODE_FROM_LINK (NameLink);
            if (NameNode->Value == NULL || NameNode->EditValue == NULL || StrCmp (NameNode->Value, NameNode->EditValue) != 0) { Status = EFI_ABORTED; break; }
          }
          if (EFI_ERROR (Status)) { break; }
          continue;
        }
        if (C->Stores[I].Storage->Size != C->Stores[I].Size ||
            C->Stores[I].Storage->Buffer == NULL || C->Stores[I].Storage->EditBuffer == NULL ||
            !C->Stores[I].Storage->ModernReviewBaselineKnown ||
            CompareMem (C->Stores[I].Before, C->Stores[I].Storage->Buffer, C->Stores[I].Size) != 0 ||
            CompareMem (C->Stores[I].After, C->Stores[I].Storage->EditBuffer, C->Stores[I].Size) != 0) { Status = EFI_ABORTED; break; }
      }
    }
  }
  mReviewBusy = FALSE;
  if (!EFI_ERROR (Status)) { mApprovedReview = C; }
  else { ReviewFree (C); }
  return Status;
}
