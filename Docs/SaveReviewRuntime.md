# Native staged-save review runtime

Modern/LVGL builds replace the one pinned SetupBrowserDxe component; the SDK is not edited. The browser owns extraction, shadow/edit buffers, scope, validation and persistence. DisplayEngine receives a synchronous read-only snapshot, not raw storage or write authority.

`MODERN_SETUP_SAVE_REVIEW` defaults to `1`; only `0` and `1` are accepted. Set `0` explicitly for synthetic build fixtures or unsupported/custom SDKs. Native-engine builds do not install this overlay. Malformed default-enabled overlays are errors, not silent opt-outs.

## Baseline and values

`Initialized` is not provenance: it is set before extraction in upstream. `ModernReviewBaselineKnown` requires successful native extraction/conversion and synchronization. Defaults following extraction failure are unknown, and cannot authorize saving.

Form, formset and system requests compare copied shadow/edit bytes within native request spans. Reverted edits disappear. Numeric, checkbox and one-of values are supported. Non-password strings are bounded by their native storage width; ordered lists use option labels in stored order (numeric fallback for missing labels), terminate on the native zero element, and fail closed on output overflow. Password/bitfield/suppression/expression aliases force redaction. All value strings and copied buffers are cleared before release.

Known unmapped changed bytes appear as a protected aggregate warning, not a fabricated setting/value. Consequently `Complete` means the save scope is accounted for, not that every opaque byte has an exact question identity. Unknown baseline, malformed request, unsupported changed name/value storage, unavailable receiver, allocation failure or stale snapshot blocks the native submit. Unchanged known name/value storage does not block unrelated buffer edits and is rechecked after review. Name/value loading validates the native ConfigHdr and exact requested name tokens before destructive conversion: the response must contain every requested name once, in request order, with hex byte values and no extra suffix. Every storage node must occur exactly once in the request. Prefix matches, partial responses and duplicate names are not coverage. The native converter runs on a temporary copy (fully wiped before release), then every resulting EditValue is compared to the validated response because native setters can silently fail. Successful synchronization must also leave every Value equal to its EditValue. Failed name/value conversion is not synchronized, since a failed native allocation can leave a NULL edit value. Reordered or otherwise ambiguous responses conservatively remain unknown; changed name/value saving remains unsupported.

The original CancelGuards transform remains in place. Canceled review does not discard, exit or reset. Native route failures can represent partial persistence and must not be described as rollback.

## Reproducible host validation

```
python3 Tests/Smoke/save_review_runtime_test.py \
  --workspace /home/qdy/modernsetup-pr \
  --sdk /home/qdy/modernsetup-validation-edk2
```

The runner verifies pinned inputs, generates only build-output copies, compiles the shipped runtime against actual generated native browser types, and executes real edited buffers. It additionally extracts and executes the transformed native `LoadStorage`, `ProcessAction`, `SubmitForForm`, `SubmitForFormSet`, `ConfigRespToStorage` and `SetValueByName` functions. Name/value regressions obtain baseline provenance only through generated `LoadStorage`, then exercise review and native submission alongside a changed buffer, including exact-prefix collisions and a real native setter allocation failure. Synchronization and routing services remain fault-injected host doubles. Firmware services are host stubs; this is not a QEMU/persistence test.

Coverage includes two BootNext/Timeout-like buffer changes, form/formset/system scopes, cancellation, revert, snapshot mutation and package-refresh rejection, missing/incompatible receiver, password aliases, bounded public strings, ordered option-label ordering, opaque changed bytes, unchanged/changed name/value handling, extraction/conversion/synchronization failure provenance, and cancel containment for combined save/exit/reset/discard actions.

Still require firmware acceptance against the final revision: ordered-list UI layout, long strings, callback submit/reconnect cancellation, native partial route failure, and reboot persistence. Host tests do not claim all these paths are covered.
