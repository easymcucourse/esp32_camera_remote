import importlib.util
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
