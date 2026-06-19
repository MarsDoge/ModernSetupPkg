/** @file
  ModernSetupPkg UI version string (single source of truth for display).

  Keep this in sync with the repository release tag and CHANGELOG.md heading
  (e.g. tag v1.1.0 -> "1.1.0"). It is shown in the ModernSetupApp header so a
  running UI can be matched to a specific release.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  Author: MarsDoge (Dongyan Qian)
  Open source: https://github.com/MarsDoge/ModernSetupPkg

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MODERN_UI_VERSION_H_
#define MODERN_UI_VERSION_H_

//
// Semantic version, no leading 'v'. The UI prefixes a 'v' when displaying it.
//
#define MODERN_SETUP_VERSION_STRING  L"1.1.0"

#endif
