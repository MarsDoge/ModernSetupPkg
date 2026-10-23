# Optional Setup background

Language: English | [简体中文](HomeBackground.zh-CN.md)

Version 1.2.0 adds opt-in build-time artwork to the App and native HII pages
rendered by ModernDisplayEngine. The full viewport uses an opaque image with a
centered, aspect-preserving cover crop. Dashboard sections/cards are moderately
translucent (140/255, 150/255, selected 180/255); opted-in settings panel surfaces
use 205/255 opacity. Text backplates, active controls, input/password editors,
errors and save confirmation stay solid. Not every row is translucent.

## Build

```sh
MODERN_SETUP_HOME_BACKGROUND_PNG=/absolute/path/background.png \
MODERN_SETUP_ASSET_PYTHON=/usr/bin/python3 \
MODERN_SETUP_REPLACE_UIAPP=1 \
MODERN_SETUP_DISPLAY_ENGINE=lvgl \
TARGET=RELEASE Scripts/build-loongarchvirt.sh
```

Use the normal WORKSPACE/toolchain configuration. Install Pillow only in the
asset-conversion interpreter. Set one explicit SOURCE_DATE_EPOCH for a reproducible
build batch; the App shows that frozen build label separately from product version
and the live clock. Native forms do not display this App build label.
Omit MODERN_SETUP_HOME_BACKGROUND_PNG for the unchanged solid appearance; Pillow
is then unnecessary. This build switch is currently LoongArchVirt-only.

## Input and rendering contract

- Accept PNG files decodable by Pillow, not just fixed 1280×800 or 800×600 inputs.
  The original is unchanged. The selected blue source is 1263×705 RGB.
- Convert to RGB (alpha is discarded, not blended), thumbnail within 1280×800
  without enlarging small sources, then encode little-endian RGB565. The resource
  is at most 2,048,000 bytes plus its string terminator per linked image.
- Generate the resource in the build overlay; do not commit user artwork,
  generated resource headers or firmware binaries. Redistribute only authorized
  artwork. There is no runtime PNG decoder, file loading or wallpaper picker.
- Bilinear sampling produces a full-viewport cover crop. RGB565 limits colour
  precision and upscaling cannot recover missing detail. Runtime regions larger
  than 4096×2160 are rejected; invalid bounds, allocation failure or a reported
  restore failure fall back to solid drawing. The existing LVGL flush path does
  not report every underlying GOP failure.
- App and native DisplayEngine each own their resource/cache. No pointer is
  borrowed across image unload. Native reentry restores the immutable cache;
  each App paint prepares and releases its cache. Each surface blends from
  pristine artwork, never previously drawn text or panels.
- Only explicit presentation surfaces opt in. Global FillRect, VFR suppression,
  disabled state, validation, callbacks, ConfigAccess and variable writes are
  unchanged. Native refresh still belongs to FormBrowser. There is no new
  dirty-control/local-refresh subsystem or unused geometry experiment in this PR.

## Validation

```sh
python3 Tests/Smoke/home_background_test.py
python3 Tests/Smoke/native_backdrop_frame_test.py
python3 Tests/Smoke/hii_presentation_test.py
python3 Tests/Smoke/product_version_test.py
python3 Tests/Smoke/smoke_validate.py
```

The background tests execute real C with mocked rendering under ASan/UBSan:
bounds, crop, bilinear sampling, repeated/overlapping surfaces, ownership, failure
fallback, OFF behavior and native framework console/cursor state. The
`--legacy-header-clear` negative control must fail. These are not firmware visual
or callback-transaction acceptance; release evidence must identify the exact FD,
source hashes, QEMU framebuffer sizes and exercised interactions. No physical
board acceptance or measured performance gain is claimed.
