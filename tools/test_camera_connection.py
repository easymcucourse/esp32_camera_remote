"""Bounded UART smoke test after flashing. Does not clear pairing or change camera settings."""
import argparse
import json
from pathlib import Path
import re
import time
import serial

parser = argparse.ArgumentParser()
parser.add_argument('--port', default='COM8')
parser.add_argument('--connect-wait', type=float, default=120)
parser.add_argument('--steady', type=float, default=30)
parser.add_argument('--output', type=Path, default=Path('captures/connection-hardware-test'))
args = parser.parse_args()
args.output.parent.mkdir(parents=True, exist_ok=True)
checks = []
received = bytearray()
fatal = re.compile(rb'Guru Meditation|Backtrace:|assert failed|Task watchdog got triggered|abort\(\) was called|Brownout detector')

def check(name, passed, detail):
    item = {'name': name, 'passed': bool(passed), 'detail': detail}
    checks.append(item)
    print(json.dumps(item, ensure_ascii=False), flush=True)

with serial.Serial(port=None, baudrate=115200, timeout=0.05) as uart, args.output.with_suffix('.log').open('wb') as log:
    uart.dtr = False; uart.rts = False; uart.port = args.port; uart.open()
    uart.reset_input_buffer()
    def observe(seconds, until=None):
        start = time.monotonic(); buf = bytearray(); notice = start + 30
        while time.monotonic() - start < seconds:
            data = uart.read(uart.in_waiting or 1)
            if data:
                log.write(data); log.flush(); received.extend(data); buf.extend(data)
                if fatal.search(buf): raise RuntimeError('firmware panic/watchdog detected; see UART log')
                if until and until in buf: break
            if time.monotonic() >= notice:
                print(f'Observing UART: {time.monotonic()-start:.0f}s / {seconds:.0f}s', flush=True)
                notice += 30
        return bytes(buf), time.monotonic() - start
    def command(text):
        uart.write(text.encode('ascii') + b'\n'); uart.flush()
        print('UART command: ' + text, flush=True)
    def stop(name):
        command('s'); buf, elapsed = observe(3, b'Camera task finished')
        completed = b'Camera task finished' in buf
        check(name, completed and elapsed < 1, {'completed': completed, 'host_seconds': round(elapsed, 3)})
        return completed
    try:
        if not stop('stop_initial_task'):
            raise RuntimeError('camera task did not stop; cannot safely continue lifecycle tests')
        command('j'); observe(0.5)
        command('j'); buf, _ = observe(2, b'Camera request already active')
        check('duplicate_start_ignored', b'Camera request already active' in buf, 'single owner guard')
        # Recreate the owner once before attempting connection.
        if not stop('stop_after_resume'):
            raise RuntimeError('resumed task did not stop')
        command('j')
        buf, _ = observe(args.connect_wait, b'LIVEVIEW frames=')
        live = b'LIVEVIEW frames=' in buf
        check('camera_liveview', live, {'discovered': b'CAMERA DISCOVERED' in buf,
            'session': b'SESSION VERIFIED' in buf, 'pairing_saved': b'PAIRING SAVED' in buf,
            'init_fail': b'InitFail' in buf})
        if live:
            steady, _ = observe(args.steady)
            frames = re.findall(rb'LIVEVIEW frames=(\d+) fps=([0-9.]+)', steady)
            check('continuous_liveview', len(frames) >= 2,
                  {'reports': len(frames), 'last_frame': int(frames[-1][0]) if frames else None,
                   'fps': [float(x[1]) for x in frames]})
            if not stop('stop_liveview'):
                raise RuntimeError('live-view owner did not stop')
            command('j'); buf, _ = observe(30, b'LIVEVIEW frames=')
            check('paired_reconnect', b'LIVEVIEW frames=' in buf and b'stored_peer=1' in buf
                  and b'PAIRING SAVED' not in buf,
                {'liveview': b'LIVEVIEW frames=' in buf,
                 'stored_peer': b'stored_peer=1' in buf, 'new_pairing_write': b'PAIRING SAVED' in buf})
            # Leave the device running for the user.
        else:
            check('connection_prerequisite', False, 'No decoded frame within wait; camera setup/authorization required')
        check('no_panic', not fatal.search(received), 'bounded observation only')
    except Exception as exc:
        check('suite_error', False, str(exc))
    finally:
        args.output.with_suffix('.json').write_text(json.dumps({'port': args.port, 'checks': checks},
            indent=2, ensure_ascii=False), encoding='utf-8')

raise SystemExit(0 if checks and all(item['passed'] for item in checks) else 1)
