"""Extract one complete Sony 0x9209 dataset from a TCP capture for local QA.

Output contains raw camera properties: keep it in ignored captures/, not Git.
"""
import argparse
import shutil
import struct
import subprocess
from pathlib import Path


def packets(data):
    at = 0
    while at + 8 <= len(data):
        length, kind = struct.unpack_from('<II', data, at)
        if length < 8 or at + length > len(data):
            break  # Capture may begin/end mid-session.
        yield kind, data[at + 8:at + length]
        at += length


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--stream', type=int, required=True)
    parser.add_argument('--transaction', type=int)
    parser.add_argument('--max-packets', type=int, default=50000)
    parser.add_argument('--tshark', default=shutil.which('tshark'))
    args = parser.parse_args()
    if not args.capture.is_file():
        parser.error(f'capture not found: {args.capture}')
    if not args.tshark:
        parser.error('tshark not found; supply --tshark')
    raw = subprocess.check_output([
        args.tshark, '-r', str(args.capture), '-c', str(args.max_packets),
        '-q', '-z', f'follow,tcp,raw,{args.stream}'], text=True)
    directions = [bytearray(), bytearray()]
    for line in raw.splitlines():
        value = line.strip()
        if value and len(value) % 2 == 0 and all(c in '0123456789abcdefABCDEF' for c in value):
            directions[int(line.startswith('\t'))].extend(bytes.fromhex(value))
    request_direction, tids = None, set()
    for direction, data in enumerate(directions):
        for kind, body in packets(data):
            if kind == 6 and len(body) >= 10 and struct.unpack_from('<H', body, 4)[0] == 0x9209:
                tid = struct.unpack_from('<I', body, 6)[0]
                if args.transaction is None or args.transaction == tid:
                    request_direction = direction
                    tids.add(tid)
    if request_direction is None:
        parser.error('no matching 0x9209 request; check stream or increase --max-packets')
    objects, totals = {}, {}
    for kind, body in packets(directions[1 - request_direction]):
        if kind not in (7, 9, 10, 12) or len(body) < 4:
            continue
        tid = struct.unpack_from('<I', body, 2 if kind == 7 else 0)[0] if len(body) >= 6 else -1
        if tid not in tids:
            continue
        if kind == 9 and len(body) == 12:
            total = struct.unpack_from('<Q', body, 4)[0]
            if total <= 1024 * 1024:
                totals[tid], objects[tid] = total, bytearray()
        elif kind in (10, 12) and tid in objects:
            objects[tid].extend(body[4:])
        elif kind == 7 and len(body) >= 6 and struct.unpack_from('<H', body)[0] == 0x2001:
            if tid in objects and len(objects[tid]) == totals[tid]:
                args.output.parent.mkdir(parents=True, exist_ok=True)
                args.output.write_bytes(objects[tid])
                print(f'{args.output}: stream={args.stream} transaction={tid} bytes={totals[tid]}')
                return
    parser.error('no complete successful dataset within capture limit')


if __name__ == '__main__':
    main()
