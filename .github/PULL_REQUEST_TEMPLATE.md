<!-- SPDX-License-Identifier: BSD-2-Clause-Patent -->

# Pull request checklist

Thanks for contributing. Use N/A for sections that do not apply; docs-only and small maintainer-file PRs should stay lightweight.

## Summary

- What changed:
- Affected area(s) from `Docs/AGENT_OWNERSHIP.md`:
- Cross-review needed, if any:

## Compatibility and boundaries

- [ ] N/A - no public API/DEC change.
- [ ] Public `Include/ModernUi/*.h` or `ModernSetupPkg.dec` changes follow `Docs/API_COMPATIBILITY.md`.
- [ ] Changes stay within `Docs/MODULE_BOUNDARIES.md`, or the boundary change is explained.
- [ ] Struct/enum changes are append-only, versioned, or explicitly justified.

## Firmware safety checklist

- [ ] N/A - docs/tooling only.
- [ ] App entries for real setup pages still use `EFI_FORM_BROWSER2_PROTOCOL.SendForm()`.
- [ ] No direct ConfigAccess calls, IFR parsing, or HII varstore writes were added outside approved owners.
- [ ] DisplayEngine/FormBrowser compatibility and fallback behavior are preserved.
- [ ] Experimental HII bridge changes remain isolated and fail closed/read-only for unsupported constructs.

## Validation

- [ ] Docs-only review / link or formatting sanity.
- [ ] Build, script, or focused smoke validation noted below.
- [ ] QEMU/manual validation noted below, or unavailable with reason.
- [ ] Changelog updated, or N/A because the change has no user-visible/API/build impact.

Validation notes:
