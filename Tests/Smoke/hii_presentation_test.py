#!/usr/bin/env python3
"""Host source guards only; firmware rendering/idle acceptance needs QEMU."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
PATH = "Library/ModernUiCustomizedDisplayLib/CustomizedDisplayLibInternal.c"
SOURCE = (ROOT / PATH).read_text()


def function(source, name):
    match = re.search(r"\n" + name + r"\s*\([^)]*\)\s*\{", source)
    assert match, name
    start = match.end() - 1
    depth = 1
    end = start + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


class HiiPresentation(unittest.TestCase):
    def test_clock_is_partial_and_does_not_initialize(self):
        body = function(SOURCE, "ModernDisplayRefreshClock")
        self.assertIn("mModernRenderReady", body)
        self.assertIn("mModernRenderContext.Gop != NULL", body)
        self.assertIn("ModernUiEngineRefreshClock (&mModernRenderContext)", body)
        for token in ("EnsureRenderer", "ModernUiClear", "DrawPage", "ReadKeyStroke"):
            self.assertNotIn(token, body)

    def test_modal_wait_owns_optional_timer(self):
        body = function(SOURCE, "WaitForKeyStroke")
        self.assertIn("EventCount  = 1", body)
        self.assertIn("EVT_TIMER, TPL_CALLBACK, NULL, NULL", body)
        self.assertIn("TimerPeriodic, 10000000", body)
        self.assertIn("ModernDisplayRefreshClock ();", body)
        self.assertEqual(body.count("CloseEvent (Timer)"), 2)
        self.assertNotIn("FormRefreshEvent", body)

    def test_private_grid_and_popup_use_same_viewport(self):
        body = function(SOURCE, "ScreenDimensionInfoValidate")
        self.assertIn("FormData->ScreenDimensions == NULL", body)
        self.assertIn("mModernRenderContext.Height / 32", body)
        self.assertIn("gScreenDimensions.BottomRow = mModernGridRows", body)
        self.assertIn("mModernGridRows", function(SOURCE, "ModernDisplayRows"))
        self.assertNotIn("SetMode", body)
        popup = (ROOT / "Universal/ModernDisplayEngineDxe/Popup.c").read_text()
        self.assertIn("Rows = MIN (Rows, gScreenDimensions.BottomRow)", popup)
        layout = function(SOURCE, "ModernDisplayCalculateLayout")
        self.assertIn("gClassOfVfr == FORMSET_CLASS_FRONT_PAGE", layout)
        self.assertIn("Layout->Statement.BottomRow = Layout->ContentBottomRow", layout)

    def test_title_is_not_inferred_navigation(self):
        body = function(SOURCE, "ModernDisplayDrawFormIdentity")
        self.assertIn("PrintableTitle", body)
        self.assertNotIn("Category", body)
        self.assertNotIn("Prefix", body)
        self.assertNotIn("ModernDisplayDrawFormBreadcrumb", SOURCE)

    def test_readonly_widgets_and_paint_scope(self):
        body = function(SOURCE, "ModernDisplayDrawValueWidget")
        self.assertIn("ModernDisplayFormRowStateReadOnly", body)
        self.assertIn("ModernDisplayFormRowStateDisabled", body)
        self.assertIn("mModernRowPaintActive = FALSE", function(SOURCE, "ModernDisplayDrawStatementRowCue"))
        self.assertIn("mModernRowPaintActive", function(SOURCE, "ModernDisplayResetHighlightRowTracking"))
        display = (ROOT / "Universal/ModernDisplayEngineDxe/FormDisplay.c").read_text()
        self.assertRegex(
            display,
            r"ProcessOptions \(MenuOption, FALSE, &OptionString, FALSE\);\s*"
            r"if \(EFI_ERROR \(Status\)\) \{\s*"
            r"ModernDisplayResetHighlightRowTracking \(\);\s*return Status;",
        )


if __name__ == "__main__":
    unittest.main()
