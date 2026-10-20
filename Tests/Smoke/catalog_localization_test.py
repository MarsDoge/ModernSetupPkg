#!/usr/bin/env python3
"""Exercise the firmware UTF-8/language path and every generated translation."""
import importlib.util
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest
from smoke_validate import extract_c_function_body
from setting_catalog_test import tool

ROOT = Path(__file__).resolve().parents[2]
APP = ROOT / 'Application/ModernSetupApp'


class LocalizationTests(unittest.TestCase):
    def test_real_c_unicode_language_and_metadata(self):
        source = (APP / 'ModernSetupAppCatalog.c').read_text()
        funcs = source[source.index('STATIC BOOLEAN CatalogChinese'):source.index('STATIC VOID CatalogInit')]
        prelude = r'''
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include "ModernSetupSettings.generated.h"
typedef size_t UINTN;
typedef uint8_t UINT8;
typedef uint32_t UINT32;
typedef wchar_t CHAR16;
#define VOID void
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#define AsciiStrCmp strcmp
static const char *language;
static const char *ModernUiGetLanguage(void) { return language; }
'''
        tests = r'''
static void check(const char *utf8, const uint16_t *expected, size_t count) {
  CHAR16 out[4096]; size_t i;
  CatalogText(utf8, out, ARRAY_SIZE(out));
  for (i=0; i<count; i++) assert(out[i] == expected[i]);
  assert(out[count] == 0);
}
int main(void) {
  CHAR16 out[8] = {0x1111,0x2222,0x3333,0x4444};
  const CHAR16 *wide;
  language="zh-CN";
  assert(CatalogChinese());
  assert(strcmp(CatalogLocalized("English", "Chinese"), "Chinese") == 0);
  assert(strcmp(CatalogLocalized("fallback", ""), "fallback") == 0);
  assert(strcmp(CatalogLocalized("fallback", NULL), "fallback") == 0);
  wide=ModernSetupCatalogUi(L"E", L"Z"); assert(wide[0]=='Z');
  language="en-US"; assert(!CatalogChinese());
  wide=ModernSetupCatalogUi(L"E", L"Z"); assert(wide[0]=='E');
  language="ru-RU"; assert(!CatalogChinese());
  assert(strcmp(CatalogLocalized("English", "Chinese"), "English") == 0);
  language=NULL; assert(!CatalogChinese());
  language=""; assert(!CatalogChinese());
  language="z"; assert(!CatalogChinese());
  CatalogText("abc", out, 0); assert(out[0]==0x1111);
  CatalogText(NULL, out, 1); assert(out[0]==0 && out[1]==0x2222);
  CatalogText("\xe4\xb8\xad", out, 2); assert(out[0]==0x4e2d && out[1]==0 && out[2]==0x3333);
  CatalogText("\xf0\x9f\x98\x80", out, 2); assert(out[0]==0);
  CatalogText("\xf0\x9f\x98\x80", out, 3); assert(out[0]==0xd83d && out[1]==0xde00 && out[2]==0);
  check("\xc3\xa9", (uint16_t[]){0xe9}, 1);
  check("\xe4\xb8", (uint16_t[]){'?', '?'}, 2);
  check("\xc0\xaf", (uint16_t[]){'?', '?'}, 2);
  check("\xed\xa0\x80", (uint16_t[]){'?', '?', '?'}, 3);
  check("\xf4\x90\x80\x80", (uint16_t[]){'?', '?', '?', '?'}, 4);
  check("\xe4" "A", (uint16_t[]){'?', 'A'}, 2);
  assert(strcmp(CatalogCategoryText("unknown"), "unknown")==0);
'''
        def units(text):
            raw = text.encode('utf-16-le')
            return ','.join(str(int.from_bytes(raw[i:i+2], 'little')) for i in range(0, len(raw), 2))
        catalog = tool.load(ROOT / 'Config/SetupSettings.json')
        for index, item in enumerate(sorted(catalog['settings'], key=lambda x: x['id'])):
            for lang, suffix in [('en-US', 'En'), ('zh-CN', 'Zh'), ('ru-RU', 'En')]:
                tests += f'language="{lang}";\n'
                for field in ('Label', 'Help'):
                    value = item[field.lower()]['zh-CN' if suffix == 'Zh' else 'en']
                    tests += f'check(CatalogLocalized(mModernSetupSettings[{index}].{field}En,mModernSetupSettings[{index}].{field}Zh), (uint16_t[]){{{units(value)}}}, {len(value.encode("utf-16-le"))//2});\n'
        for category, names in tool.CATEGORY_LABELS.items():
            for lang, value in [('en', names[0]), ('zh-CN', names[1]), ('ru', names[0])]:
                tests += f'language="{lang}"; check(CatalogCategoryText("{category}"), (uint16_t[]){{{units(value)}}}, {len(value)});\n'
        tests += 'return 0; }\n'
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / 'Uefi.h').write_text('#include <stdint.h>\n#define STATIC static\n#define CONST const\n#define IN\n#define TRUE 1\n#define FALSE 0\ntypedef char CHAR8;\ntypedef uint8_t BOOLEAN;\n')
            (path / 'test.c').write_text(prelude + funcs + tests)
            subprocess.run(shlex.split(os.environ.get('CC', 'cc')) + ['-std=c11', '-fshort-wchar', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', '-I', str(path), '-I', str(APP), '-I', str(ROOT / 'Include'), str(path / 'test.c'), '-o', str(path / 'test')], check=True)
            subprocess.run([str(path / 'test')], check=True)

    def test_glyph_coverage_and_navigation(self):
        catalog = tool.load(ROOT / 'Config/SetupSettings.json')
        source = (APP / 'ModernSetupAppCatalog.c').read_text()
        texts = [v for item in catalog['settings'] for field in ('label', 'help') for v in item[field].values()]
        texts += [v for names in tool.CATEGORY_LABELS.values() for v in names]
        texts += re.findall(r'L"([^"\n]*)"', source)
        chrome = (APP / 'ModernSetupAppChrome.c').read_text()
        texts += re.findall(r'ModernSetupCatalogUi \(L"[^"\n]*", L"([^"\n]*)"\)', chrome)
        for title in ('Universal Setting Catalog', 'Advanced > Setting Catalog', 'Setting Catalog >'):
            self.assertIn('ModernSetupCatalogUi (L"' + title + '"', chrome)
        needed = {ord(c) for text in texts for c in text if ord(c) > 127}
        glyphs = {int(c, 16) for c in re.findall(r'\{ 0x([0-9A-Fa-f]+),', (ROOT / 'Library/ModernUiRendererLib/ModernUiGlyphs.c').read_text())}
        self.assertFalse(needed - glyphs, ''.join(map(chr, sorted(needed - glyphs))))
        self.assertIn('mHelpChinese != CatalogChinese ()', source)
        self.assertEqual(source.count('/ (CatalogChinese () ? 18 : 8)'), 2)
        self.assertIn(': L"N/A";', source)
        self.assertNotIn('AsciiStrToUnicodeStrS', source)
        for text in ('< Back', 'Open native setup', 'Native owner unavailable', '< Previous', 'Scroll down', 'Next >'):
            self.assertRegex(source, r'ModernSetupCatalogUi \(L"[^"\n]*' + re.escape(text))
        self.assertEqual(set(tool.CATEGORY_LABELS), set(catalog['categories']))

    def test_generated_utf8_glyph_collection(self):
        # Exercise collection without requiring Pillow in the smoke environment.
        import ast
        tree = ast.parse((ROOT / 'Scripts/generate-font-glyphs.py').read_text())
        tree.body = [node for node in tree.body if isinstance(node, ast.FunctionDef)
                     and node.name in ('collect_chars', 'collect_c_chars', 'collect_uni_chars')]
        namespace = {'Path': Path, 're': re}
        exec(compile(tree, 'generate-font-glyphs.py', 'exec'), namespace)
        catalog = tool.load(ROOT / 'Config/SetupSettings.json')
        needed = {c for item in catalog['settings'] for field in ('label', 'help')
                  for text in item[field].values() for c in text if ord(c) > 127}
        needed.update(c for names in tool.CATEGORY_LABELS.values()
                      for text in names for c in text if ord(c) > 127)
        collected = set(namespace['collect_chars']([APP / 'ModernSetupSettings.generated.h']))
        self.assertFalse(needed - collected)

    def test_invalid_unicode_metadata_rejected(self):
        import copy
        catalog = copy.deepcopy(tool.load(ROOT / 'Config/SetupSettings.json'))
        catalog['settings'][0]['label']['zh-CN'] = '\ud800'
        with self.assertRaises(tool.CatalogError):
            tool.validate(catalog)


if __name__ == '__main__':
    unittest.main()
