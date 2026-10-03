import importlib.util
import io
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('uart_script', Path(__file__).parents[2] / 'tools/uart_script.py')
module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)

class Clock:
    def __init__(self): self.now = 0
    def time(self): return self.now
    def sleep(self, duration): self.now += duration

class Port:
    def __init__(self, response='OK'):
        self.chunks = [b'[dbg] #1 OK stale\n']; self.response = response; self.sent = []
    @property
    def in_waiting(self): return len(self.chunks[0]) if self.chunks else 0
    def read(self, count): return self.chunks.pop(0) if count and self.chunks else b''
    def flush(self): pass
    def write(self, data):
        self.sent.append(data)
        if self.response:
            number = data.split(b' ')[0]
            self.chunks.extend([b'[db', b'g] ' + number + b' ' + self.response.encode() + b' response\n'])

class Tests(unittest.TestCase):
    def runner(self, port):
        clock = Clock()
        return module.Runner({'lcd': port}, io.StringIO(), .03, clock.time, clock.sleep)
    def test_fragmented_and_numbered(self):
        port = Port(); runner = self.runner(port)
        runner.execute(['version', 'expect "response"', 'wait 10', 'status # comment'])
        self.assertEqual(port.sent, [b'#1 version\n', b'#2 status\n'])
    def test_history_does_not_satisfy_command(self):
        with self.assertRaises(module.ScriptError): self.runner(Port(None)).execute(['status'])
    def test_error_requires_negative_prefix(self):
        self.runner(Port('ERR')).execute(['!lcd unknown'])
        with self.assertRaises(module.ScriptError): self.runner(Port('ERR')).execute(['unknown'])
    def test_syntax_and_line_limit(self):
        for script in ['@absent status', 'expect', 'wait -1', '"unterminated', 'x' * 256]:
            with self.assertRaises(ValueError): self.runner(Port()).execute([script])
    def test_expect_scoped_to_latest_command(self):
        port = Port(); runner = self.runner(port)
        with self.assertRaises(module.ScriptError): runner.execute(['version', 'expect "stale" .02'])
    def test_async_waits_for_matching_terminal_token(self):
        class Async(Port):
            def write(self, data):
                super().write(data)
                self.chunks.extend([b'[dbg] FAIL wifi token=8 error\n', b'[dbg] DONE wifi token=7\n'])
        runner = self.runner(Async('OK wifi queued token=7'))
        runner.execute(['wifi display on'])
        self.assertIn(b'DONE wifi token=7', runner.fresh('lcd'))
    def test_pad_duration_extends_async_budget(self):
        clock = Clock()
        class Slow(Port):
            def write(self, data):
                super().write(data)
                self.finish = False
            @property
            def in_waiting(self):
                if not self.chunks and not getattr(self, 'finish', True) and clock.now > 11:
                    self.chunks.append(b'[dbg] DONE SIM pad token=7 elapsed=12000ms\n')
                    self.finish = True
                return super().in_waiting
        port = Slow('OK SIM queued token=7 duration=12000ms')
        runner = module.Runner({'atom': port}, io.StringIO(), .03, clock.time, clock.sleep)
        runner.execute(['tap start 10000'])
        self.assertGreater(clock.now, 11)

if __name__ == '__main__': unittest.main()
