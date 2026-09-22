#!/usr/bin/env python3
"""Executable validation/generation/state tests; no runtime backend claimed."""
import copy
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / 'Scripts/setup-setting-catalog.py'
spec = importlib.util.spec_from_file_location('catalog_tool', SCRIPT)
assert spec is not None and spec.loader is not None
tool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tool)


class CatalogTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.catalog = tool.load(ROOT / 'Config/SetupSettings.json')

    def changed(self, mutate):
        data = copy.deepcopy(self.catalog)
        mutate(data['settings'][0])
        with self.assertRaises(tool.CatalogError):
            tool.validate(data)

    def test_all_domains_and_stable_ids(self):
        self.assertEqual(set(tool.CATEGORIES), {x['category'] for x in self.catalog['settings']})
        self.assertEqual(len(self.catalog['settings']), len({x['id'] for x in self.catalog['settings']}))

    def test_duplicate_id_rejected(self):
        data = copy.deepcopy(self.catalog)
        data['settings'].append(copy.deepcopy(data['settings'][0]))
        with self.assertRaisesRegex(tool.CatalogError, 'duplicate ID'):
            tool.validate(data)

    def test_malformed_metadata_rejected(self):
        for field, value in [('control', 'unknown'), ('value_type', 'wrong'),
                             ('topnav', 'Unknown'), ('apply', 'immediate'),
                             ('risk', 'safe'), ('sensitive', 0), ('access_intent', 'edit')]:
            with self.subTest(field=field):
                self.changed(lambda s: s.update({field: value}))

    def test_na_stays_visible_and_readonly(self):
        for field, value in [('visible', False), ('editable', True), ('submittable', True),
                             ('availability', 'available')]:
            self.changed(lambda s: s.update({field: value}))

    def test_invented_bindings_rejected(self):
        for field, value in [('kind', 'native_hii'), ('hii', {'question': 1}),
                             ('variable', {'guid': 'guess', 'offset': -1}),
                             ('service', 'some-backend')]:
            self.changed(lambda s: s['binding'].update({field: value}))

    def test_unbound_constraints_and_defaults(self):
        for field, value in [('min', 10), ('max', -1), ('step', 0), ('options', ['on', 'off'])]:
            self.changed(lambda s: s['constraints'].update({field: value}))
        self.changed(lambda s: s['default'].update(value=0))

    def test_dependencies_and_secrets(self):
        self.changed(lambda s: s['dependencies'].update(ids=['missing.setting']))
        data = copy.deepcopy(self.catalog)
        next(s for s in data['settings'] if s['id'] == 'security.admin_password')['sensitive'] = False
        with self.assertRaises(tool.CatalogError):
            tool.validate(data)

    def test_duplicate_json_keys(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / 'bad.json'
            p.write_text('{"schema_version":1,"schema_version":1}')
            with self.assertRaisesRegex(tool.CatalogError, 'duplicate JSON key'):
                tool.load(p)

    def test_deterministic_generation(self):
        a = tool.generate(self.catalog)
        reordered = copy.deepcopy(self.catalog)
        reordered['settings'].reverse()
        self.assertEqual(a, tool.generate(reordered))
        self.assertTrue(a.isascii())
        # Rich data is retained, not silently reduced to a list of labels.
        for key in ('constraints', 'dependencies', 'applicability', 'binding'):
            self.assertIn(key, a)

    def test_checked_in_firmware_metadata_matches(self):
        generated = ROOT / 'Application/ModernSetupApp/ModernSetupSettings.generated.h'
        self.assertEqual(generated.read_text(encoding='ascii'), tool.generate(self.catalog),
                         'Regenerate firmware metadata from Config/SetupSettings.json')

    def test_markdown_inventory(self):
        for language in ('en', 'zh-CN'):
            out = tool.markdown(self.catalog, language)
            for s in self.catalog['settings']:
                self.assertIn(s['id'], out)
            self.assertEqual(out.count('| N/A |'), len(self.catalog['settings']))

    def test_cli_and_source_protection(self):
        with tempfile.TemporaryDirectory() as d:
            source, output = Path(d) / 'catalog.json', Path(d) / 'out.h'
            source.write_text(json.dumps(self.catalog))
            result = subprocess.run([sys.executable, str(SCRIPT), '--catalog', str(source),
                                     '--check', '--output', str(output)], capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(output.read_text(), tool.generate(self.catalog))
            before = source.read_bytes()
            denied = subprocess.run([sys.executable, str(SCRIPT), '--catalog', str(source),
                                     '--output', str(source)], capture_output=True)
            self.assertNotEqual(denied.returncode, 0)
            self.assertEqual(source.read_bytes(), before)

    def test_generated_c_and_state_matrix(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d)
            (p / 'generated.h').write_text(tool.generate(self.catalog), encoding='ascii')
            (p / 'Uefi.h').write_text('''#include <stdint.h>
#define STATIC static
#define CONST const
#define IN
#define TRUE 1
#define FALSE 0
typedef char CHAR8;
typedef uint8_t BOOLEAN;
''')
            test = r'''
#include <assert.h>
#include <string.h>
#include "generated.h"
int main(void) {
  MODERN_SETUP_SETTING_PRESENTATION p;
  unsigned int i;
  assert(MODERN_SETUP_SETTING_COUNT > 0);
  for (i=0; i<MODERN_SETUP_SETTING_COUNT; ++i) {
    assert(strcmp(mModernSetupSettings[i].BindingKind,"unbound")==0);
    assert(mModernSetupSettings[i].LabelZh[0]);
    assert(strstr(mModernSetupSettings[i].DescriptorJson,"\"editable\":false"));
  }
  for (i=ModernSettingUnbound; i<ModernSettingAvailable; ++i) {
    p=ModernSetupSettingPresentation((MODERN_SETTING_STATUS)i,FALSE,TRUE,FALSE,TRUE);
    assert(p.Visible && p.ShowNa && !p.CanEdit && !p.CanInvoke && !p.CanSubmit);
    p=ModernSetupSettingPresentation((MODERN_SETTING_STATUS)i,FALSE,TRUE,TRUE,TRUE);
    assert(!p.CanEdit && !p.CanInvoke && !p.CanSubmit);
  }
  // Availability is separate from the value: real zero/Disabled stays real.
  p=ModernSetupSettingPresentation(ModernSettingAvailable,FALSE,FALSE,FALSE,TRUE);
  assert(p.Visible && !p.ShowNa && !p.CanEdit && !p.CanSubmit);
  p=ModernSetupSettingPresentation(ModernSettingAvailable,FALSE,TRUE,FALSE,TRUE);
  assert(p.CanEdit && p.CanSubmit && !p.CanInvoke);
  p=ModernSetupSettingPresentation(ModernSettingAvailable,FALSE,TRUE,TRUE,TRUE);
  assert(p.CanInvoke && !p.CanEdit && !p.CanSubmit);
  p=ModernSetupSettingPresentation(ModernSettingAvailable,FALSE,TRUE,FALSE,FALSE);
  assert(p.CanEdit && !p.CanSubmit);
  p=ModernSetupSettingPresentation(ModernSettingAvailable,TRUE,TRUE,FALSE,TRUE);
  assert(!p.Visible && !p.ShowNa && !p.CanEdit && !p.CanSubmit);
  return 0;
}
'''
            (p / 'test.c').write_text(test)
            subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-I', str(p),
                            '-I', str(ROOT / 'Include'), str(p / 'test.c'), '-o', str(p / 'test')], check=True)
            subprocess.run([str(p / 'test')], check=True)


if __name__ == '__main__':
    unittest.main()
