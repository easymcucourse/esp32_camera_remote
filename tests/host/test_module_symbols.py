"""Negative direct-reference fixtures, independent of target archive contents."""
import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from check_module_symbols import validate


class SymbolContract(unittest.TestCase):
    def edge(self, caller, provider, symbol):
        return [{'caller': caller, 'provider': provider, 'symbols': [symbol]}]

    def test_core_public_lifecycle(self):
        validate(self.edge('app_core', 'app_ui', 'app_ui_init'),
                 {('app_core', 'app_ui')}, {'app_ui': {'app_ui_init'}})

    def test_undeclared_formatter_provider(self):
        with self.assertRaisesRegex(ValueError, 'undeclared'):
            validate(self.edge('app_console', 'app_input_atom', 'i2c_monitor_result_name'), set(), {})

    def test_private_ui_symbol(self):
        with self.assertRaisesRegex(ValueError, 'private direct call'):
            validate(self.edge('app_core', 'app_ui', 'app_ui_show_jpeg'),
                     {('app_core', 'app_ui')}, {'app_ui': {'app_ui_init'}})

    def test_noncore_camera(self):
        with self.assertRaisesRegex(ValueError, 'private direct call'):
            validate(self.edge('app_console', 'app_camera', 'app_camera_start'),
                     {('app_console', 'app_camera')}, {'app_camera': {'app_camera_start'}})

    def test_provider_report_only(self):
        validate(self.edge('app_input_atom', 'app_input', 'input_provider_publish'),
                 {('app_input_atom', 'app_input')}, {})
        with self.assertRaisesRegex(ValueError, 'Input report'):
            validate(self.edge('app_input_atom', 'app_input', 'app_input_start'),
                     {('app_input_atom', 'app_input')}, {})

    def test_http_single_adapter(self):
        validate(self.edge('esp_http_server', 'wifi_esp32', 'wifi_esp32_http_bind'),
                 {('esp_http_server', 'wifi_esp32')}, {})
        with self.assertRaisesRegex(ValueError, 'single backend'):
            validate(self.edge('esp_http_server', 'wifi_esp32', 'wifi_esp32_create'),
                     {('esp_http_server', 'wifi_esp32')}, {})


if __name__ == '__main__':
    unittest.main()
