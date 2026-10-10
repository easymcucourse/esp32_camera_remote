import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

TOOL = Path(__file__).parents[2] / 'tools/check_doc_links.py'
spec = importlib.util.spec_from_file_location('doc_links', TOOL)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class Tests(unittest.TestCase):
    def test_locale_coverage_and_order_failures(self):
        locale_spec = importlib.util.spec_from_file_location('doc_locales', TOOL.with_name('check_doc_locales.py'))
        locales = importlib.util.module_from_spec(locale_spec)
        locale_spec.loader.exec_module(locales)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            names = ['README.md', 'README.zh-CN.md', 'README.ja.md',
                     'm5_atom_matrix/README.md', 'm5_atom_matrix/README.zh-CN.md', 'm5_atom_matrix/README.ja.md',
                     'docs/README.md', 'docs/README.zh-CN.md', 'docs/en/README.md', 'docs/ja/README.md',
                     'docs/user-guide/README.md', 'docs/en/user-guide/README.md', 'docs/ja/user-guide/README.md']
            valid = '# Guide\n\nEnglish · 简体中文 · 日本語\n\n' + 'Documented behavior. ' * 8
            for name in names:
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(valid, encoding='utf-8')
            manifest = {'default_language': 'en', 'language_order': ['en', 'zh-CN', 'ja'],
                        'topics': ['user-guide/README.md']}
            manifest_path = root / 'docs/locales.json'
            manifest_path.write_text(json.dumps(manifest), encoding='utf-8')
            self.assertEqual(locales.check(root), (1, []))
            (root / 'docs/ja/user-guide/README.md').unlink()
            (root / 'README.ja.md').write_text(valid.replace('English · 简体中文 · 日本語',
                                                         '日本語 · English · 简体中文'), encoding='utf-8')
            issues = locales.check(root)[1]
            self.assertEqual(len(issues), 2)
            self.assertTrue(any('missing language document' in issue for issue in issues))
            self.assertTrue(any('unordered' in issue for issue in issues))
            manifest['topics'] = []
            manifest_path.write_text(json.dumps(manifest), encoding='utf-8')
            self.assertTrue(any('every current topic' in issue for issue in locales.check(root)[1]))
            result = subprocess.run([sys.executable, str(TOOL.with_name('check_doc_locales.py')),
                                     '--root', str(root)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 1)

    def test_locale_readmes_are_checked(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'docs').mkdir()
            (root / 'm5_atom_matrix').mkdir()
            for name in ('README.md', 'm5_atom_matrix/README.md'):
                (root / name).write_text('# Overview\n', encoding='utf-8')
            (root / 'README.ja.md').write_text('[missing](absent.md)', encoding='utf-8')
            (root / 'm5_atom_matrix/README.zh-CN.md').write_text('[missing](absent.md)', encoding='utf-8')
            documents, links, issues = module.check(root)
            self.assertEqual((documents, links, len(issues)), (4, 2, 2))

    def test_links_and_failure_exit(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'docs').mkdir()
            (root / 'm5_atom_matrix').mkdir()
            (root / 'm5_atom_matrix/README.md').write_text('# ATOM\n', encoding='utf-8')
            (root / 'docs/a b.md').write_text('# 中文标题\n# Same\n# Same\n<a id="custom"></a>\n', encoding='utf-8')
            readme = root / 'README.md'
            readme.write_text('[中文](docs/a%20b.md#中文标题)\n[repeat](<docs/a%20b.md#same-1>)\n'
                              '[html](docs/a%20b.md#custom)\n[web](https://example.com)\n'
                              '```md\n[ignore](missing.md)\n```\n~~~\n[ignore](missing.md)\n~~~\n', encoding='utf-8')
            self.assertEqual(module.check(root), (3, 3, []))
            readme.write_text('[bad file](missing.md)\n[bad heading](docs/a%20b.md#absent)', encoding='utf-8')
            self.assertEqual(len(module.check(root)[2]), 2)
            result = subprocess.run([sys.executable, str(TOOL), '--root', str(root)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 1)
            self.assertIn('2 issues', result.stdout)


if __name__ == '__main__':
    unittest.main()
