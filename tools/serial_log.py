"""Capture a bounded UART boot log (requires pyserial, included in ESP-IDF)."""
import argparse
from pathlib import Path
import time
import serial

parser = argparse.ArgumentParser()
parser.add_argument('--port', default='COM8')
parser.add_argument('--seconds', type=float, default=25)
parser.add_argument('--output', type=Path, default=Path('build/serial-boot.log'))
parser.add_argument('--reset', action='store_true')
parser.add_argument('--command', help='Send an ASCII command without resetting the board')
parser.add_argument('--until', help='Stop when this ASCII text appears in the UART log')
args = parser.parse_args()
args.output.parent.mkdir(parents=True, exist_ok=True)
with serial.Serial(port=None, baudrate=115200, timeout=0.5) as uart:
    uart.dtr = False
    uart.rts = False
    uart.port = args.port
    uart.open()
    if args.reset:
        uart.rts = True
        time.sleep(0.1)
        uart.rts = False
    if args.command:
        uart.write((args.command + '\n').encode('ascii'))
        uart.flush()
    deadline = time.monotonic() + args.seconds
    stop_text = args.until.encode('ascii') if args.until else None
    recent = b''
    with args.output.open('wb') as output:
        while time.monotonic() < deadline:
            data = uart.read(uart.in_waiting or 1)
            output.write(data)
            output.flush()
            print(data.decode('utf-8', errors='replace'), end='', flush=True)
            if stop_text:
                recent += data
                if stop_text in recent:
                    break
                recent = recent[-len(stop_text):]
