"""Keep partial/missing/reconnected captures from appearing continuously healthy."""
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('analyze_liveview', Path(__file__).resolve().parents[2] / 'tools/analyze_liveview.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def frame(ms, count, fps, read=120):
    return f'I ({ms}) app_ui: LIVEVIEW frames={count} fps={fps} JPEG=180000 read={read}ms display=150ms stack_free=2048'


class AnalysisTests(unittest.TestCase):
    def test_empty_is_unknown(self):
        data = module.analyze('Waiting for paired camera...')
        self.assertIsNone(data['fps']['weighted_mean'])
        self.assertIsNone(data['heap_bytes']['min_internal'])

    def test_initial_window_and_weighting(self):
        data = module.analyze('\n'.join([frame(100, 1, 100, 900), frame(5100, 26, 5), frame(15100, 86, 6)]))
        self.assertAlmostEqual(data['fps']['weighted_mean'], 17 / 3)
        self.assertAlmostEqual(data['fps']['frame_count_mean'], 17 / 3)
        self.assertEqual(data['sampled_frames']['read_ms']['max'], 120)

    def test_reconnect_does_not_bridge_sessions(self):
        data = module.analyze('\n'.join([frame(100, 1, 10), frame(5100, 26, 5),
                                        'Live-view ended: backend=12 frame_failed=0 stop=0 shown=26 dropped=0 bad=0', frame(10000, 50, 5), frame(15000, 75, 5),
                                        frame(100, 1, 10), frame(5100, 26, 5)]))
        self.assertEqual(len(data['sessions']), 3)
        self.assertEqual(data['fps']['longest_session_seconds'], 5)
        self.assertEqual(data['incident_log_lines']['stream_exit'], 1)

    def test_missing_timestamp_breaks_coverage(self):
        data = module.analyze('\n'.join([frame(100, 1, 10), frame(5100, 26, 5).split('app_ui: ')[1], frame(10100, 51, 5)]))
        self.assertEqual(data['fps']['untimed_reports'], 1)
        self.assertEqual(data['fps']['covered_seconds'], 0)

    def test_heap_phases_and_known_failures(self):
        data = module.analyze('\n'.join(['\x1b[32mI (10) remote: free_internal=70000 min_internal=40000 min_psram=1100000 largest_internal=16384\x1b[0m',
            'JPEG phases us: wait=1 decode=75000 publish=51000 settings=0',
            'JPEG phases us: decode=160000 publish=51000 settings=1',
            'ESP_ERR_NO_MEM', 'response=0x200F', 'SESSION VERIFIED', 'display_failed=1']))
        self.assertEqual(data['heap_bytes']['min_internal']['min'], 40000)
        self.assertEqual(data['jpeg_phases_us_by_settings']['1']['decode']['p95'], 160000)
        self.assertEqual(data['incident_log_lines']['liveview_200f'], 1)
        self.assertEqual(data['incident_log_lines']['no_mem'], 1)
        self.assertEqual(data['incident_log_lines']['display_failure'], 1)
        self.assertTrue(data['mixed_settings'])
        self.assertEqual(data['observed_settings_phase_reports'], {'0': 1, '1': 1})

    def test_small_sample_p95_preserves_outlier(self):
        self.assertEqual(module.stats([120, 1100])['p95'], 1100)

    def test_cli_page_consistency_and_summary_retention(self):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / 'capture.log'
            output = Path(directory) / 'summary.json'
            for content, expected in [('', 1),
                    ('JPEG phases us: decode=1 settings=0', 0),
                    ('JPEG phases us: decode=1 settings=0\nJPEG phases us: decode=1 settings=1', 1)]:
                log.write_text(content, encoding='utf-8')
                result = subprocess.run([sys.executable, str(Path(module.__file__)),
                    str(log), '--output', str(output), '--expect-settings', '0'], capture_output=True)
                self.assertEqual(result.returncode, expected)
                self.assertTrue(output.is_file())


# Both synchronous publication failures and deferred scan failures must be counted.
class DisplayFailureLogs(unittest.TestCase):
    def test_sync_and_async_scan_errors(self):
        data = module.analyze("display_failed=0\nLCD publication failed: ESP_ERR_TIMEOUT\n"
                       "LCD completion timed out; framebuffer writes suspended\n")
        self.assertEqual(data['incident_log_lines']['display_failure'], 2)

if __name__ == '__main__':
    unittest.main()
