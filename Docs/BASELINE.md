# edk2 Baseline

ModernSetupPkg carries a pinned edk2 baseline as a Git submodule so build and QEMU validation can start from a reproducible firmware tree.

## Baseline identity

- Submodule path: `External/edk2`
- Upstream URL: `https://github.com/tianocore/edk2.git`
- Pinned commit: `b03a21a63e3bd001f52c527e5a57feddb53a690b`

This baseline is a reproducible build/QEMU reference for ModernSetupPkg development and validation. It is not a claim of product source ownership over edk2, nor is it the place where ModernSetupPkg product source should be developed. ModernSetupPkg source remains in this repository; edk2 remains an upstream dependency pinned for repeatable integration checks.

## Bootstrap

Initialize the pinned baseline with:

```sh
Scripts/bootstrap-edk2.sh
```

The bootstrap helper validates the submodule entry, initializes `External/edk2`, updates edk2 first-level submodules, initializes the required nested MbedTLS framework submodule, reports the checked-out edk2 commit, and can optionally build BaseTools.

This is the preferred flow for ModernSetupPkg build validation. It intentionally avoids OpenSSL optional nested test submodules that are not required for the BaseTools, ModernSetupPkg app, or OVMF baseline and can make validation slow or dirty. If full upstream edk2 test dependencies are needed, run deeper recursive submodule updates explicitly outside normal ModernSetupPkg build validation.

Optional BaseTools build during bootstrap:

```sh
BUILD_BASETOOLS=1 Scripts/bootstrap-edk2.sh
```

## Validation levels

Use the fastest level that answers the question being tested, then move upward when changing build, firmware, or QEMU integration behavior:

1. Smoke validation: `python3 Tests/Smoke/smoke_validate.py`
2. Standalone app build: build the ModernSetupApp against the pinned workspace.
3. OVMF build: build the OVMF X64 firmware overlay against the pinned workspace.
4. Headless QEMU: boot the generated firmware in a non-graphical QEMU run where supported.
5. Graphical screenshot: capture or inspect the setup UI in graphical QEMU.

Some environments may block QEMU execution because KVM, graphics, firmware variables, or display backends are unavailable. In those cases, keep smoke and build validation reproducible and document the blocked QEMU level in the test notes.

## Updating the baseline

Future edk2 baseline changes should be deliberate and reviewable:

1. Choose the target upstream edk2 commit and record why it is needed.
2. Update `External/edk2` to the new commit without adding local edk2 source changes.
3. Ensure `.gitmodules` still points `External/edk2` at `https://github.com/tianocore/edk2.git`.
4. Update the pinned commit in this document.
5. Re-run smoke validation and the highest feasible build/QEMU validation level.
6. Note any QEMU execution limits if the current environment cannot run headless or graphical QEMU.
7. Keep ModernSetupPkg product changes separate from the edk2 submodule pointer update when practical.
