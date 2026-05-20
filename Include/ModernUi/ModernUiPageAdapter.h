/** @file
  GUID-bound page adapter interface for ModernSetupPkg.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MODERN_UI_PAGE_ADAPTER_H_
#define MODERN_UI_PAGE_ADAPTER_H_

#include <Uefi.h>

#include <ModernUi/ModernUiHiiBridge.h>
#include <ModernUi/ModernUiInput.h>
#include <ModernUi/ModernUiRenderer.h>
#include <ModernUi/ModernUiTheme.h>

#define MODERN_UI_PAGE_ADAPTER_VERSION  1

typedef struct _MODERN_UI_PAGE_ADAPTER MODERN_UI_PAGE_ADAPTER;

typedef struct {
  UINT32                    Version;
  MODERN_UI_RENDER_CONTEXT  *Ui;
  CONST MODERN_UI_THEME     *Theme;
  MODERN_UI_HII_VIEW        *HiiView;
  MODERN_UI_HII_FORMSET     *FormSet;
  MODERN_UI_HII_PAGE        *Page;
  EFI_FORM_ID               FormId;
  UINTN                     Selection;
  UINTN                     Scroll;
  BOOLEAN                   HasFocus;
  CHAR16                    *StatusMessage;
  UINTN                     StatusMessageSize;
} MODERN_UI_PAGE_CONTEXT;

/**
  Return whether an adapter supports one HII formset.

  @param[in] Adapter  Adapter being queried. Must not be NULL.
  @param[in] FormSet  HII formset candidate. Must not be NULL.

  @retval TRUE   Adapter can render or handle the formset.
  @retval FALSE  Adapter does not support the formset.
**/
typedef
BOOLEAN
(EFIAPI *MODERN_UI_PAGE_ADAPTER_SUPPORTED)(
  IN CONST MODERN_UI_PAGE_ADAPTER  *Adapter,
  IN CONST MODERN_UI_HII_FORMSET   *FormSet
  );

/**
  Draw a GUID-bound page.

  @param[in] Adapter  Adapter selected for the page. Must not be NULL.
  @param[in] Context  Draw context. Must not be NULL.

  @retval EFI_SUCCESS            Page was drawn.
  @retval EFI_INVALID_PARAMETER  A required parameter is NULL.
  @retval EFI_UNSUPPORTED        Adapter declined this context.
  @retval others                 Rendering failure from lower layers.
**/
typedef
EFI_STATUS
(EFIAPI *MODERN_UI_PAGE_ADAPTER_DRAW)(
  IN CONST MODERN_UI_PAGE_ADAPTER  *Adapter,
  IN MODERN_UI_PAGE_CONTEXT        *Context
  );

/**
  Handle input for a GUID-bound page.

  @param[in]     Adapter  Adapter selected for the page. Must not be NULL.
  @param[in]     Event    Input event to handle. Must not be NULL.
  @param[in,out] Context  Page context. Must not be NULL.

  @retval EFI_SUCCESS            Event was consumed.
  @retval EFI_NOT_READY          Event was not consumed by this adapter.
  @retval EFI_INVALID_PARAMETER  A required parameter is NULL.
  @retval others                 Adapter-specific failure.
**/
typedef
EFI_STATUS
(EFIAPI *MODERN_UI_PAGE_ADAPTER_HANDLE_INPUT)(
  IN     CONST MODERN_UI_PAGE_ADAPTER  *Adapter,
  IN     CONST MODERN_UI_INPUT_EVENT   *Event,
  IN OUT MODERN_UI_PAGE_CONTEXT        *Context
  );

struct _MODERN_UI_PAGE_ADAPTER {
  UINT32                              Version;
  EFI_GUID                            AdapterGuid;
  EFI_GUID                            TargetFormSetGuid;
  INTN                                Priority;
  CONST CHAR16                        *Name;
  MODERN_UI_PAGE_ADAPTER_SUPPORTED    Supported;
  MODERN_UI_PAGE_ADAPTER_DRAW         Draw;
  MODERN_UI_PAGE_ADAPTER_HANDLE_INPUT HandleInput;
};

/**
  Return the compiled-in page adapter registry.

  @param[out] Count  Receives the number of adapters. Must not be NULL.

  @return Pointer to an array of adapter pointers. NULL is valid when Count is 0.
**/
CONST MODERN_UI_PAGE_ADAPTER *CONST *
EFIAPI
ModernUiGetStaticPageAdapters (
  OUT UINTN  *Count
  );

/**
  Find the best adapter for a HII formset GUID.

  @param[in] FormSetGuid  Formset GUID to match. Must not be NULL.
  @param[in] FormSet      Optional formset metadata used by Supported().

  @return Highest-priority matching adapter, or NULL when no adapter exists.
**/
CONST MODERN_UI_PAGE_ADAPTER *
EFIAPI
ModernUiFindPageAdapterByGuid (
  IN CONST EFI_GUID                *FormSetGuid,
  IN CONST MODERN_UI_HII_FORMSET   *FormSet OPTIONAL
  );

#endif
