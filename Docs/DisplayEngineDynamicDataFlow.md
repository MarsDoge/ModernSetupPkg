# DisplayEngine Dynamic Data Flow Contract

This document defines the intended product direction for ModernSetupPkg dynamic platform data, setup configuration, and refresh UX.

## Goal

ModernSetupApp and the Modern DisplayEngine must be able to show current platform information, reflect configuration changes, and update dynamic data such as time or sensor-like values without turning the renderer into a policy or hardware owner.

## Intended pipeline

```text
PEI platform discovery / defaults
  -> HOB / PCD / Variable / protocol handoff
    -> DXE platform services
      -> ModernSetupApp / DisplayEngine view model
        -> Modern UI display + dynamic refresh
        -> app writes selected config through platform-owned PCD / Variable / protocol paths
          -> immediate effect where safe, or reboot-required effect
            -> next PEI consumes non-default config and republishes latest state
```

## Responsibilities

### PEI / platform discovery

- Collect early platform information.
- Read default and non-default configuration inputs.
- Publish handoff data through platform-owned mechanisms.
- Consume reboot-persistent configuration on the next boot.

### DXE platform services

- Normalize platform data for applications.
- Own policy, validation, persistence, and reset requirements.
- Expose current data and update notifications to the app/display layer.

### ModernSetupApp

- Presents product-level setup pages.
- Initiates configuration changes through approved platform services or existing setup mechanisms.
- Shows whether changes are live, unsaved, or require reboot.

### Modern DisplayEngine

- Renders FormBrowser-owned form state and ModernSetup page state.
- Shows live/refresh/unsaved/reboot-required/status affordances.
- Supports redraw-friendly dynamic fields such as time.
- Must not own hardware probing, policy decisions, or storage writes.

## DisplayEngine constraints

Allowed in DisplayEngine/UI code:

- Consume already-materialized FormBrowser display data.
- Consume future app/platform view-model state.
- Render row kind/state, status chips, refresh indicators, and dynamic values.
- Repaint on FormBrowser refresh events or app-driven redraws.

Forbidden in DisplayEngine/UI renderer code:

- Direct hardware probing.
- Independent IFR parsing.
- ConfigAccess semantics.
- Direct `SetVariable`, `RouteConfig`, `ExtractConfig`, or `HiiSetBrowserData` ownership.
- Treating generic unsaved changes as reboot-required without a platform/FormBrowser source.

## UX states

Current private DisplayEngine status slots:

```text
LIVE VIEW        default live page surface
LIVE REFRESH     FormBrowser/page has a refresh event or equivalent update source
UNSAVED CHANGES  changed state exists but is not committed
REBOOT REQUIRED  future platform/FormBrowser source says reboot is required
MODAL VIEW       modal FormBrowser state
```

`REBOOT REQUIRED` is intentionally a reserved UX state until a real source is wired. The UI must not infer it from generic changed state.

## Validation expectation

Routine UX iteration should stay Modern DisplayEngine focused:

```bash
python3 Tests/Smoke/smoke_validate.py
git diff --check
TARGET=RELEASE MODERN_SETUP_DISPLAY_ENGINE=modern MODERN_SETUP_REPLACE_UIAPP=1 Scripts/build-ovmf-x64.sh
```

Use native-vs-modern capture only for milestones or PR review baselines, not every iteration.
