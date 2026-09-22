/** @file
  Optional synchronous browser-owned save review contract.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  Draft revision 1 foundation; not installed or consumed by production builds.
  Browser owns all snapshot memory, valid only during Review(). The consumer must
  not retain pointers, invoke browser/config services, write variables, or pump a
  nested SendForm. Strings are NUL-terminated CHAR16 (UTF-16); sizes are elements,
  not bytes. No private browser pointers or raw config responses cross this ABI.
  Baseline means browser shadow state, NOT a fresh read of persistent storage.
  Unknown/unmapped values MUST be redacted before calling the consumer. A redacted
  item has NULL OldValue/NewValue. Never send passwords, keys or opaque raw bytes.
  Confirm is permission to attempt native validation/save, NOT proof of success.
  Browser must revalidate identity and payload after Review, before native submit.
  ContinueEditing or any error leaves edits pending and prohibits exit/reset.
**/
#ifndef MODERN_SETUP_CHANGE_REVIEW_H_
#define MODERN_SETUP_CHANGE_REVIEW_H_

#include <Uefi.h>

#define MODERN_SETUP_CHANGE_REVIEW_PROTOCOL_GUID \
  { 0x670cda24, 0xc782, 0x4efd, { 0xb7, 0x94, 0xa6, 0xeb, 0x54, 0x3a, 0x91, 0x27 } }
#define MODERN_SETUP_CHANGE_REVIEW_REVISION  1
#define MODERN_SETUP_REVIEW_REDACTED        BIT0
#define MODERN_SETUP_REVIEW_OPAQUE          BIT1
#define MODERN_SETUP_REVIEW_BASELINE_KNOWN  BIT2

// Numeric values intentionally independent of private BROWSER_SETTING_SCOPE.
typedef enum {
  ModernSetupReviewForm = 0,
  ModernSetupReviewFormSet,
  ModernSetupReviewSystem
} MODERN_SETUP_REVIEW_SCOPE;

typedef enum {
  ModernSetupContinueEditing = 0,
  ModernSetupConfirmSave
} MODERN_SETUP_REVIEW_DECISION;

typedef struct {
  EFI_GUID        FormSetGuid;
  EFI_GUID        StorageGuid;
  UINT16          FormId;
  UINT16          QuestionId; // Zero when storage-level/opaque, not mapped.
  UINT32          Flags;
  CONST CHAR16    *Path;      // Safe product label, never raw ConfigRequest.
  CONST CHAR16    *OldValue;  // NULL for redacted/unknown values.
  CONST CHAR16    *NewValue;
} MODERN_SETUP_REVIEW_ITEM;

typedef struct {
  UINT32                           Revision;
  UINT32                           Size;
  UINT64                           ContextId; // Opaque generation, not address.
  MODERN_SETUP_REVIEW_SCOPE         Scope;
  BOOLEAN                          Complete; // FALSE forbids confirmation/save.
  UINTN                            ItemCount;
  CONST MODERN_SETUP_REVIEW_ITEM    *Items;
} MODERN_SETUP_REVIEW_SNAPSHOT;

typedef struct _MODERN_SETUP_CHANGE_REVIEW_PROTOCOL MODERN_SETUP_CHANGE_REVIEW_PROTOCOL;

typedef
EFI_STATUS
(EFIAPI *MODERN_SETUP_CHANGE_REVIEW)(
  IN  MODERN_SETUP_CHANGE_REVIEW_PROTOCOL  *This,
  IN  CONST MODERN_SETUP_REVIEW_SNAPSHOT   *Snapshot,
  OUT MODERN_SETUP_REVIEW_DECISION         *Decision
  );

struct _MODERN_SETUP_CHANGE_REVIEW_PROTOCOL {
  UINT32                       Revision;
  MODERN_SETUP_CHANGE_REVIEW    Review;
};

// GUID registration in package DEC is a later integration step.
#endif
