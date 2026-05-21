<!--
Copyright (c) 2026, MarsDoge. All rights reserved.
Author: MarsDoge (Dongyan Qian)
Open source: https://github.com/MarsDoge/ModernSetupPkg
SPDX-License-Identifier: BSD-2-Clause-Patent
-->

# Brand Assets

`logo.jpg` is a repository-provided project visual reference asset for
README and showcase material. It is not sourced from third-party firmware UIs,
commercial logos, icons, wallpapers, or vendor asset packs. It is not included
in the ArmVirt firmware image and is not referenced by the default DSC/FDF path.

Future replacements or additions should include source/provenance metadata and
license/permission notes before they are used in firmware images or public
showcases. See the repository-level `THIRD_PARTY_NOTICES.md` for consolidated
asset provenance and trademark notices.

The current firmware build has very limited FVMAIN free space, so product logo
rendering should use a small generated GOP primitive or a compact indexed/RLE
bitmap only after the image pipeline and license boundary are finalized.
