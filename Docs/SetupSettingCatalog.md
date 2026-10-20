# Universal Setup setting catalog

Language: English | [简体中文](SetupSettingCatalog.zh-CN.md)

## What this delivers

The product catalog is browsable under Advanced > Setting Catalog: 21 categories
and 203 read-only N/A entries. Enter opens a category or item help; Up/Down
browses and Esc returns. Catalog categories, labels, help, navigation and status
follow the active App language: Simplified Chinese or English, with English
fallback for Russian. It does not connect hardware backends. Selected Boot and Secure Boot help pages offer a separate `Open native setup`
action using the existing handoffs. Enter or click opens the owner page, not an
individual question. Missing owners do not fall back to unrelated tools; return
preserves catalog position. This does not bind catalog values.
A known intended owner is not a resolved, working binding.

`Config/SetupSettings.json` is the canonical setting registry. Each setting has a
stable ID, bilingual text, category, control/value metadata, applicability,
access intent, default source, owner/binding information, effect and risk.
`Include/ModernUi/ModernSetupSetting.h` is the C metadata contract; the generator
produces immutable metadata rather than a second hand-maintained registry.

The scope follows `IbvAndPlatformSetupSurvey.md`: AMI Aptio, InsydeH2O,
Phoenix firmware families and Byosoft ByoCore are IBV references; motherboard,
client and server OEMs are workflow references. Coverage is a product catalog,
not a claim that every platform supports every setting.

## Missing backends stay visible

An unbound setting stays in its category with `N/A`. Users can inspect its help,
but cannot edit, invoke or submit it. It must not enter a save-change review.
Unbound, unsupported, not reported and read error are distinct reasons. A real
value of zero or Disabled is not an unavailable value.

This rule does not override native security, permission, suppressif, grayoutif
or disableif conditions. An unavailable catalog entry is not permission to
expose a protected native question or bypass a driver decision.

Intended access and current availability are separate: a setting designed as a
switch may still be unbound. A supported writable backend must be explicitly
resolved before enabling any editor. Metadata alone cannot grant write access.

## Backend connection rules

- Native HII: register exact owner/context and FormSet/Form/Question identifiers;
  never guess ownership from display text. Preserve browser validation,
  callbacks, default handling and actual submission scope.
- Platform services: connect through a typed, reviewed adapter/owner. Do not
  give ModernSetupApp raw ConfigAccess or arbitrary variable-write access.
- Read-only providers: provide real values plus availability, not demo numbers.
- App preferences: only values owned and consumed by the App belong here.
  System date/time is not an App preference.
- Unbound: record the missing connection without inventing GUIDs, offsets,
  enum choices, hardware limits or defaults.

A variable layout must identify its GUID/name, storage/schema version, byte or
bit layout, encoding, attributes, validation owner and actual consumer. Writing
an unused variable is not implementation. HII does not require DynamicHii PCDs:
varstores, ConfigAccess and callback-based actions are also relevant mechanisms.
Passwords, TPM clear, key enrollment and firmware update are not generic byte
writes; their native workflows and sensitive-data rules remain in force.

## Validation and use

From the package root:

```sh
python3 Scripts/setup-setting-catalog.py --check
python3 Scripts/setup-setting-catalog.py --output /tmp/ModernSetupSettings.generated.h
python3 Tests/Smoke/setting_catalog_test.py
python3 Tests/Smoke/catalog_localization_test.py
python3 Tests/Smoke/catalog_routing_test.py
python3 Tests/Smoke/smoke_validate.py
```

Schema version 1 deliberately accepts only unbound records: `binding.hii`,
`binding.variable` and `binding.service` must be null. This is not yet a usable
platform binding ABI. A later reviewed schema adds concrete mappings and their
validation; this batch cannot activate a backend by filling in a guessed GUID.
The complete item metadata is retained in `DescriptorJson` (UTF-8); commonly
used display fields are exposed directly in the generated C descriptor. The
firmware consumes the checked-in generated header and selects bilingual display
fields using the active App language. Metadata remains UTF-8 and is decoded
with bounded UTF-16 conversion; malformed sequences become `?`, and surrogate
pairs are never partially written. Parity tests guard against stale generated
data and exercise every label/help translation. Glyph coverage is checked
against the committed font table; host tests do not replace rendered QEMU review.

To export a readable inventory without a second manually maintained list:

```sh
python3 Scripts/setup-setting-catalog.py --markdown-output Build/Reports/SetupSettingCatalog.zh-CN.md --language zh-CN
```

The generated header is a build/integration input, not proof of runtime wiring.
Host validation must cover duplicate IDs, invalid metadata, unsafe availability,
malformed bindings and deterministic generation, including a real C compile.
New controls need later QEMU visual and keyboard/pointer acceptance.

## Next integration step

The catalog uses the existing Advanced detail hierarchy, not new top-level tabs. Keep repeated entry points tied to one
SettingId. Bind real Boot/Secure Boot owners first, leave other rows N/A, then
connect the browser-owned change-review model. Backend integration should not
require redesigning each page or copying platform policy into the App.
