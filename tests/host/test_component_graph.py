"""Failure fixtures for the actual-build dependency verifier."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('component_graph', ROOT / 'tools/check_component_graph.py')
graph = importlib.util.module_from_spec(spec)
spec.loader.exec_module(graph)


class GraphContract(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.build = Path(self.directory.name)
        self.root = self.build / 'project'
        self.metadata = {}
        for name in graph.ALLOWED:
            path = self.root / 'main' if name == 'main' else self.root / 'components' / name
            self.metadata[name] = {'dir': str(path), 'reqs': [],
                                   'priv_reqs': sorted(graph.ALLOWED[name]), 'sources': []}
        self.http = str(self.build / 'idf/httpd.c')
        self.sim = str(self.root / 'components/app_input_sim/input_sim.c')
        self.metadata['app_input_sim']['sources'] = [self.sim]
        self.metadata['esp_http_server'] = {'dir': str(self.build / 'idf/http'), 'sources': [self.http]}
        self.commands = [{'file': self.http, 'command': 'cc -Dlwip_bind=wifi_esp32_http_bind'},
                         {'file': self.sim, 'command': 'cc'}]
        (self.build / 'config').mkdir()
        (self.build / 'config/sdkconfig.h').write_text('#define CONFIG_REMOTE_DBG_SIM 1\n')
        (self.build / 'module-http-links.txt').write_text('__idf_lwip;__idf_wifi_esp32\n')

    def inspect(self):
        (self.build / 'project_description.json').write_text(json.dumps({
            'project_path': str(self.root), 'build_component_info': self.metadata}))
        (self.build / 'compile_commands.json').write_text(json.dumps(self.commands))
        return graph.inspect(self.build)

    def rejects(self, text):
        with self.assertRaisesRegex(ValueError, text):
            self.inspect()

    def test_debug(self):
        self.assertFalse(self.inspect()['release'])

    def test_release_empty_registration(self):
        (self.build / 'config/sdkconfig.h').write_text('')
        self.commands.pop()
        self.metadata['app_input_sim']['sources'] = []
        self.metadata['app_core']['priv_reqs'].remove('app_input_sim')
        self.assertIn('app_input_sim', self.inspect()['empty_components'])

    def test_core_public_dependency(self):
        self.metadata['app_core']['reqs'] = ['app_wifi']
        self.rejects('public startup')

    def test_functional_bypass(self):
        self.metadata['app_ui']['priv_reqs'].append('app_camera')
        self.rejects('functional dependency bypass')

    def test_public_backend(self):
        self.metadata['app_camera']['reqs'] = ['camera_backend_sony']
        self.rejects('backend dependency is public')

    def test_cycle_through_http_adapter(self):
        self.metadata['common_runtime']['priv_reqs'] = ['esp_http_server']
        self.rejects('dependency cycle')

    def test_missing_actual_link(self):
        (self.build / 'module-http-links.txt').write_text('__idf_lwip')
        self.rejects('post-project HTTP link')

    def test_missing_actual_compile_adapter(self):
        self.commands[0]['command'] = 'cc'
        self.rejects('HTTP compilation')

    def test_release_sim_implementation(self):
        (self.build / 'config/sdkconfig.h').write_text('')
        self.rejects('Release compiles SIM')

    def test_missing_required_component(self):
        del self.metadata['app_camera']
        self.rejects('missing production')


if __name__ == '__main__':
    unittest.main()
