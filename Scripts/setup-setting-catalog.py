#!/usr/bin/env python3
"""Validate the product catalog and generate immutable, unbound C descriptors.

No hardware discovery or firmware variable I/O. Runtime binding is a later step.
"""
import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CATEGORIES = ('system', 'boot', 'security', 'cpu', 'memory', 'virtualization',
              'pcie', 'storage', 'peripherals', 'graphics', 'network', 'power',
              'thermal', 'management', 'ras', 'firmware', 'diagnostics',
              'embedded', 'accelerator', 'preferences', 'exit')
# Presentation names are separate from stable category IDs and hardware values.
CATEGORY_LABELS = {
    'system': ('System', '系统信息'), 'boot': ('Boot', '启动'),
    'security': ('Security', '安全'), 'cpu': ('Processor', '处理器'),
    'memory': ('Memory', '内存'), 'virtualization': ('Virtualization', '虚拟化'),
    'pcie': ('PCIe', 'PCIe'), 'storage': ('Storage', '存储'),
    'peripherals': ('Peripherals', '外设'), 'graphics': ('Graphics', '显示'),
    'network': ('Network', '网络'), 'power': ('Power', '电源'),
    'thermal': ('Thermal', '散热'), 'management': ('Management', '管理'),
    'ras': ('RAS', '可靠性与维护'), 'firmware': ('Firmware', '固件'),
    'diagnostics': ('Diagnostics', '诊断'), 'embedded': ('Embedded', '嵌入式'),
    'accelerator': ('Accelerators', '加速器'), 'preferences': ('Preferences', '偏好设置'),
    'exit': ('Save and exit', '保存与退出'),
}
TYPES = {'text': 'display', 'number': 'integer', 'toggle': 'boolean',
         'select': 'enum', 'string': 'string', 'ordered_list': 'ordered_list',
         'action': 'action'}
FIELDS = {'id', 'category', 'topnav', 'label', 'help', 'control', 'value_type',
          'unit', 'constraints', 'access_intent', 'applicability', 'dependencies',
          'default', 'intended_owner', 'binding', 'availability', 'visible',
          'editable', 'submittable', 'apply', 'scope', 'risk', 'sensitive'}


class CatalogError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise CatalogError(message)


def keys(value, expected, label):
    require(isinstance(value, dict) and set(value) == set(expected),
            f'{label}: expected fields {sorted(expected)}')


def text(value, label):
    require(isinstance(value, str) and value.strip() and
            not any(ord(c) < 32 or 0xD800 <= ord(c) <= 0xDFFF for c in value), f'{label}: invalid text')


def validate(catalog):
    keys(catalog, {'schema_version', 'categories', 'settings'}, 'catalog')
    require(type(catalog['schema_version']) is int and catalog['schema_version'] == 1,
            'unsupported schema_version')
    require(catalog['categories'] == list(CATEGORIES), 'category contract mismatch')
    require(isinstance(catalog['settings'], list) and catalog['settings'], 'empty settings')
    seen = set()
    domains = set()
    for item in catalog['settings']:
        keys(item, FIELDS, 'setting')
        ident = item['id']
        require(isinstance(ident, str) and re.fullmatch(r'[a-z][a-z0-9_]*\.[a-z][a-z0-9_]*', ident),
                'invalid setting ID')
        require(ident not in seen, f'duplicate ID: {ident}')
        seen.add(ident)
        require(item['category'] in CATEGORIES and ident.split('.')[0] == item['category'],
                f'{ident}: wrong category')
        domains.add(item['category'])
        require(item['topnav'] in ('Main', 'Advanced', 'Boot', 'Security', 'Exit'),
                f'{ident}: invalid topnav')
        for field in ('label', 'help'):
            keys(item[field], {'en', 'zh-CN'}, f'{ident}.{field}')
            for lang, value in item[field].items():
                text(value, f'{ident}.{field}.{lang}')
        control = item['control']
        require(isinstance(control, str) and control in TYPES and item['value_type'] == TYPES[control],
                f'{ident}: control/value type mismatch')
        access = 'read' if control == 'text' else 'action' if control == 'action' else 'edit'
        require(item['access_intent'] == access, f'{ident}: invalid access intent')
        if item['unit'] is not None:
            text(item['unit'], f'{ident}.unit')
        constraints = item['constraints']
        keys(constraints, {'source', 'min', 'max', 'step', 'options'}, f'{ident}.constraints')
        # Product catalog has no platform limits, enum values or made-up defaults.
        require(constraints['source'] == 'platform' and
                all(constraints[k] is None for k in ('min', 'max', 'step', 'options')),
                f'{ident}: unbound constraints must be platform-defined, not guessed')
        keys(item['applicability'], {'source', 'architectures', 'products'}, f'{ident}.applicability')
        require(item['applicability'] == {'source': 'platform', 'architectures': [], 'products': []},
                f'{ident}: platform applicability unresolved')
        keys(item['dependencies'], {'source', 'ids'}, f'{ident}.dependencies')
        require(item['dependencies']['source'] == 'native_owner' and
                isinstance(item['dependencies']['ids'], list) and
                all(isinstance(x, str) for x in item['dependencies']['ids']),
                f'{ident}: invalid dependencies')
        keys(item['default'], {'source', 'value'}, f'{ident}.default')
        require(item['default']['source'] in ('native_owner', 'app_owner') and
                item['default']['value'] is None, f'{ident}: unbound default must not invent a value')
        text(item['intended_owner'], f'{ident}.intended_owner')
        keys(item['binding'], {'kind', 'hii', 'variable', 'service'}, f'{ident}.binding')
        require(item['binding'] == {'kind': 'unbound', 'hii': None, 'variable': None, 'service': None},
                f'{ident}: this catalog version supports unbound descriptors only')
        require(item['availability'] == 'unbound', f'{ident}: metadata cannot claim a live binding')
        require(item['visible'] is True and item['editable'] is False and item['submittable'] is False,
                f'{ident}: unbound must stay visible, noneditable and nonsubmittable')
        require(item['apply'] == ('not_applicable' if access == 'read' else 'owner_defined'),
                f'{ident}: apply semantics must be owner-defined')
        require(item['scope'] == 'owner_defined', f'{ident}: invalid scope')
        require(item['risk'] == ('none' if access == 'read' else 'owner_defined'), f'{ident}: invalid risk')
        require(type(item['sensitive']) is bool, f'{ident}: invalid sensitive flag')
        if 'password' in ident or ident in ('security.key_management', 'management.users'):
            require(item['sensitive'], f'{ident}: secrets must be marked sensitive')
    require(domains == set(CATEGORIES), 'missing product domain')
    for item in catalog['settings']:
        deps = item['dependencies']['ids']
        require(len(deps) == len(set(deps)) and all(d in seen and d != item['id'] for d in deps),
                f"{item['id']}: invalid dependency reference")
    return catalog


def no_duplicates(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, f'duplicate JSON key: {key}')
        result[key] = value
    return result


def load(path):
    return validate(json.loads(Path(path).read_text(encoding='utf-8'), object_pairs_hook=no_duplicates))


def c_string(value):
    # Octal UTF-8 bytes keep generated source ASCII and avoid compiler charset
    # dependence; octal escapes cannot absorb following hexadecimal characters.
    output = '"'
    for byte in value.encode('utf-8'):
        if 32 <= byte <= 126 and byte not in (34, 92):
            output += chr(byte)
        elif byte in (34, 92):
            output += '\\' + chr(byte)
        else:
            output += f'\\{byte:03o}'
    return output + '"'


def generate(catalog):
    validate(catalog)
    lines = ['// Generated from SetupSettings.json. Do not edit.',
             '#ifndef MODERN_SETUP_SETTINGS_GENERATED_H_',
             '#define MODERN_SETUP_SETTINGS_GENERATED_H_',
             '#include <ModernUi/ModernSetupSetting.h>',
             'STATIC CONST MODERN_SETUP_SETTING_DESCRIPTOR mModernSetupSettings[] = {']
    for s in sorted(catalog['settings'], key=lambda x: x['id']):
        fields = [s['id'], s['category'], s['topnav'], s['label']['en'], s['label']['zh-CN'],
                  s['help']['en'], s['help']['zh-CN'], s['control'], s['value_type'],
                  s['access_intent'], s['binding']['kind'], s['intended_owner'], s['apply'], s['risk']]
        descriptor = json.dumps(s, ensure_ascii=False, sort_keys=True, separators=(',', ':'))
        lines.append('  {' + ', '.join(map(c_string, fields)) + ', ' +
                     ('TRUE' if s['sensitive'] else 'FALSE') + ', ' + c_string(descriptor) + '},')
    lines += ['};',
              'typedef struct { CONST CHAR8 *Id; CONST CHAR8 *LabelEn; CONST CHAR8 *LabelZh; } MODERN_SETUP_CATEGORY_TEXT;',
              'STATIC CONST MODERN_SETUP_CATEGORY_TEXT mModernSetupCategories[] = {']
    for category in CATEGORIES:
        lines.append('  {' + ', '.join(map(c_string, (category, *CATEGORY_LABELS[category]))) + '},')
    lines += ['};', '#define MODERN_SETUP_SETTING_COUNT (sizeof(mModernSetupSettings) / sizeof(mModernSetupSettings[0]))', '#endif', '']
    return '\n'.join(lines)


def markdown(catalog, language):
    validate(catalog)
    zh = language == 'zh-CN'
    lines = ['# ' + ('通用配置项清单' if zh else 'Universal setting inventory'), '',
             ('自动生成；所有项目当前未绑定后端。N/A 不可编辑或提交。' if zh else
              'Generated; every descriptor is currently unbound. N/A is not editable or submittable.'), '',
             '| SettingId | ' + ('名称 | 控件 | 接入方（预期） | 状态 |' if zh else 'Name | Control | Intended owner | Status |'),
             '| --- | --- | --- | --- | --- |']
    def escape(s):
        return s.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;').replace('|', '&#124;')
    for s in sorted(catalog['settings'], key=lambda x: x['id']):
        lines.append('| ' + ' | '.join(escape(v) for v in
                     (s['id'], s['label'][language], s['control'], s['intended_owner'], 'N/A')) + ' |')
    return '\n'.join(lines) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--catalog', type=Path, default=ROOT / 'Config/SetupSettings.json')
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--markdown-output', type=Path)
    parser.add_argument('--language', choices=('en', 'zh-CN'), default='en')
    args = parser.parse_args()
    try:
        catalog = load(args.catalog)
        targets = [x for x in (args.output, args.markdown_output) if x is not None]
        require(len({x.resolve() for x in targets}) == len(targets), 'output paths must differ')
        for target in targets:
            require(target.resolve() != args.catalog.resolve(), 'refusing to overwrite source catalog')
        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(generate(catalog), encoding='ascii')
        if args.markdown_output:
            args.markdown_output.parent.mkdir(parents=True, exist_ok=True)
            args.markdown_output.write_text(markdown(catalog, args.language), encoding='utf-8')
        print(f"PASS {len(catalog['settings'])} settings / {len(catalog['categories'])} domains; all unbound, visible N/A")
        return 0
    except (CatalogError, ValueError, TypeError, OSError) as exc:
        print(f'FAIL catalog: {exc}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
