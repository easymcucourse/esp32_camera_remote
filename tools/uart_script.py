"""Replay numbered UART commands; every expect is scoped to fresh output.

python tools/uart_script.py --port lcd=COM8 --script tools/uart_scripts/console-smoke.uart
"""
import argparse
from collections import deque
from pathlib import Path
import re
import shlex
import sys
import time


class ScriptError(Exception):
    pass


class Runner:
    def __init__(self, ports, log, timeout=3, clock=time.monotonic, sleep=time.sleep):
        self.ports, self.log, self.timeout = ports, log, timeout
        self.clock, self.sleep = clock, sleep
        self.started = clock()
        self.history = deque(maxlen=20)
        self.data = {name: bytearray() for name in ports}
        self.partial = {name: bytearray() for name in ports}
        self.base = {name: 0 for name in ports}
        self.mark = {name: 0 for name in ports}
        self.target = next(iter(ports))
        self.request = 0

    def note(self, name, text):
        record = f"{self.clock() - self.started:8.3f} [{name}] {text}"
        self.history.append(record)
        self.log.write(record + "\n")
        self.log.flush()

    def pump(self):
        for name, uart in self.ports.items():
            chunk = uart.read(uart.in_waiting or 0)
            if not chunk:
                continue
            self.data[name].extend(chunk)
            self.partial[name].extend(chunk)
            while b'\n' in self.partial[name]:
                line, tail = self.partial[name].split(b'\n', 1)
                self.partial[name] = tail
                self.note(name, line.decode('utf-8', errors='replace').rstrip('\r'))
            if len(self.partial[name]) > 4096:
                self.note(name, self.partial[name].decode('utf-8', errors='replace'))
                self.partial[name].clear()
            if len(self.data[name]) > 1024 * 1024:
                removed = len(self.data[name]) - 1024 * 1024
                del self.data[name][:removed]
                self.base[name] += removed

    def finish_log(self):
        self.pump()
        for name, partial in self.partial.items():
            if partial:
                self.note(name, partial.decode('utf-8', errors='replace'))
                partial.clear()

    def fresh(self, name):
        start = self.mark[name] - self.base[name]
        if start < 0:
            raise ScriptError("expect output exceeded 1 MiB retention; use shorter windows")
        return bytes(self.data[name][start:])

    def until(self, predicate, seconds, message):
        deadline = self.clock() + seconds
        while True:
            self.pump()
            value = predicate()
            if value:
                return value
            if self.clock() >= deadline:
                raise ScriptError(message)
            self.sleep(0.01)

    def command(self, command, expect_error=False):
        self.pump()  # Drain older replies before defining the command's output window.
        self.mark = {name: self.base[name] + len(self.data[name]) for name in self.ports}
        self.request += 1
        packet = f"#{self.request} {command}\n".encode('ascii')
        if len(packet) - 1 > 255:
            raise ValueError("numbered command exceeds firmware's 255-byte line limit")
        self.note(self.target, "TX " + packet.decode('ascii').rstrip())
        self.ports[self.target].write(packet)
        self.ports[self.target].flush()
        pattern = re.compile(rb"\[dbg\] #" + str(self.request).encode() + rb" (OK|ERR)([^\r\n]*)\r?\n")
        def response():
            found = pattern.search(self.fresh(self.target))
            return found.groups() if found else None
        result = self.until(response, self.timeout, f"request #{self.request} reply timeout")
        wanted = b'ERR' if expect_error else b'OK'
        if result[0] != wanted:
            raise ScriptError(f"request #{self.request}: expected {wanted.decode()}, got {result[0].decode()}")
        queued = re.search(rb'queued token=(\d+)', result[1])
        if queued and not expect_error:
            token = queued.group(1)
            terminal = re.compile(rb'\[dbg\] (DONE|FAIL) [^\r\n]*token=' + token + rb'(?:\D|$)')
            def completed():
                found = terminal.search(self.fresh(self.target))
                return found.group(1) if found else None
            duration = re.search(rb'duration=(\d+)ms', result[1])
            allowance = int(duration.group(1)) / 1000 if duration else 0
            state = self.until(completed, allowance + max(10, self.timeout), f'async token {token.decode()} timeout')
            if state == b'FAIL':
                raise ScriptError(f'async token {token.decode()} failed')

    def execute(self, lines):
        for number, raw in enumerate(lines, 1):
            try:
                words = shlex.split(raw, comments=True)
                if not words:
                    continue
                if words[0] in ('expect', 'expect-any'):
                    if len(words) not in (2, 3):
                        raise ValueError('usage: expect[-any] "text" [seconds]')
                    seconds = float(words[2]) if len(words) == 3 else self.timeout
                    if not 0 < seconds <= 120:
                        raise ValueError('expect timeout must be in (0,120] seconds')
                    needle = words[1].encode('utf-8')
                    names = self.ports if words[0] == 'expect-any' else (self.target,)
                    self.until(lambda: any(needle in self.fresh(name) for name in names), seconds,
                               f"expect timeout: {words[1]}")
                elif words[0] == 'wait':
                    if len(words) != 2 or not 0 <= int(words[1]) <= 120000:
                        raise ValueError('usage: wait <0..120000 ms>')
                    deadline = self.clock() + int(words[1]) / 1000
                    while self.clock() < deadline:
                        self.pump(); self.sleep(min(0.01, max(0, deadline - self.clock())))
                else:
                    negative = words[0].startswith('!')
                    if words[0].startswith('@') or negative:
                        target = words.pop(0)[1:]
                        if negative and not target:
                            target = self.target
                        if target not in self.ports:
                            raise ValueError(f'unknown device: {target}')
                        self.target = target
                    if not words:
                        raise ValueError('missing device command')
                    self.command(shlex.join(words), negative)
            except (ScriptError, ValueError, UnicodeEncodeError) as exc:
                raise type(exc)(f'line {number}: {exc}') from exc


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', action='append', required=True, help='COM8 or lcd=COM8; repeat for multiple devices')
    parser.add_argument('--script', required=True, type=Path)
    parser.add_argument('--log', default=Path('build/uart-script.log'), type=Path)
    parser.add_argument('--timeout', default=3.0, type=float)
    parser.add_argument('--reset', action='append', default=[], help='Device name to reset before script')
    args = parser.parse_args()
    opened = {}
    runner = None
    try:
        import serial
        if not 0 < args.timeout <= 120:
            raise ValueError('timeout must be in (0,120] seconds')
        lines = args.script.read_text(encoding='utf-8-sig').splitlines()
        for spec in args.port:
            name, port = spec.split('=', 1) if '=' in spec else ('lcd', spec)
            if not name or not port or name in opened or port in [u.port for u in opened.values()]:
                raise ValueError('device names and physical ports must be unique')
            uart = serial.Serial(port=None, baudrate=115200, timeout=0)
            uart.dtr = uart.rts = False
            uart.port = port; uart.open(); opened[name] = uart
        for name in args.reset:
            if name not in opened:
                raise ValueError(f'unknown reset target: {name}')
            opened[name].rts = True; time.sleep(0.1); opened[name].rts = False
        args.log.parent.mkdir(parents=True, exist_ok=True)
        with args.log.open('w', encoding='utf-8', newline='\n') as log:
            runner = Runner(opened, log, args.timeout)
            try:
                runner.execute(lines)
            finally:
                runner.finish_log()
        print(f'PASS: {args.script} ({runner.request} commands), log={args.log}')
        return 0
    except ScriptError as exc:
        print(f'FAIL: {exc}', file=sys.stderr)
        if runner:
            print('\n'.join(runner.history), file=sys.stderr)
        return 1
    except (ValueError, OSError, ImportError) as exc:
        print(f'ERROR: {exc}', file=sys.stderr)
        return 2
    finally:
        for uart in opened.values():
            uart.close()


if __name__ == '__main__':
    sys.exit(main())
