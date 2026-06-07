/** @file
  Generated OEM watermark alpha coverage map. DO NOT EDIT.
  Regenerate via Scripts/gen-oem-watermark.py.

  Copyright (c) 2026, MarsDoge. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef OEM_WATERMARK_DATA_H_
#define OEM_WATERMARK_DATA_H_

#include <Uefi.h>

#define OEM_WATERMARK_WIDTH   620
#define OEM_WATERMARK_HEIGHT  92

//
// Row-major 8-bit alpha coverage (0 = transparent, 255 = solid).
//
extern CONST UINT8  gOemWatermarkAlpha[OEM_WATERMARK_WIDTH * OEM_WATERMARK_HEIGHT];

#endif // OEM_WATERMARK_DATA_H_
