<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# Screenshots

This directory holds ModernSetupPkg screenshots used for the GitHub repository
presentation and design review.

Capture rules:

- Use ModernSetupPkg output only. Do not copy commercial firmware screenshots,
  vendor icons, fonts, wallpapers, or other closed assets into this repository.
- Prefer ArmVirtQemu captures at 1024x768 or 1280x800 so text, panels, and
  drop-downs are readable in GitHub.
- Keep filenames stable and descriptive, for example
  `armvirt-dashboard.png`, `armvirt-hii-driver-sample.png`, and
  `armvirt-exit-language-dropdown.png`.
- Update the README showcase section when adding or replacing presentation
  screenshots.
- Use `before-` and `after-` prefixes for native DisplayEngine versus
  ModernDisplayEngine comparisons captured from the same HII page.

Current captures:

- `before-armvirt-frontpage.png` / `after-armvirt-frontpage.png` - native edk2
  DisplayEngine versus ModernDisplayEngine FrontPage comparison.
- `before-armvirt-device-manager.png` / `after-armvirt-device-manager.png` -
  Device Manager comparison from the same native FormBrowser path.
- `before-armvirt-driver-sample-first-page.png` /
  `after-armvirt-driver-sample-first-page.png` - DriverSample first page
  comparison.
- `before-armvirt-driver-sample-oneof-popup.png` /
  `after-armvirt-driver-sample-oneof-popup.png` - DriverSample one-of popup
  comparison.
- `modern-loongarch-dashboard.png` - LoongArchVirtQemu ModernSetupApp dashboard
  with Simplified Chinese UI, platform summary, quick access cards, and
  hardware monitor placeholders.
- `modern-loongarch-displayengine-device.png` - LoongArchVirtQemu native
  FormBrowser page rendered by ModernDisplayEngine.
- `setup-v0.4-dashboard.png` - ModernSetupApp v0.4 dashboard with Chinese UI,
  Quick Access cards, and hardware monitor layout.
- `modern-app-dashboard.png` - experimental ModernSetupApp dashboard view.
- `modern-app-en-exit.png` - experimental ModernSetupApp exit page in English.
- `modern-app-zh-exit.png` - experimental ModernSetupApp exit page in
  Simplified Chinese.
