"""Reassemble PTP/IP with tshark, then decode packet boundaries independently."""
import argparse
import csv
import io
import pathlib
import struct
import subprocess
from collections import Counter


def decode(data):
    offset = 0
    while offset + 8 <= len(data):
        length, kind = struct.unpack_from('<II', data, offset)
        if length < 8 or offset + length > len(data):
            raise ValueError(f'invalid/incomplete PTP/IP packet at {offset}, length={length}')
        body = data[offset + 8:offset + length]
        code = transaction = detail = ''
        if kind == 6 and len(body) >= 10:
            phase, op, transaction = struct.unpack_from('<IHI', body)
            code = f'0x{op:04x}'
            detail = f'phase={phase}; params=' + body[10:].hex()
        elif kind in (7, 8) and len(body) >= 6:
            op, transaction = struct.unpack_from('<HI', body)
            code = f'0x{op:04x}'
            detail = body[6:].hex()
        elif kind in (9, 10, 12) and len(body) >= 4:
            transaction = struct.unpack_from('<I', body)[0]
            if kind == 9 and len(body) >= 12:
                detail = f'data_length={struct.unpack_from("<Q", body, 4)[0]}'
            elif kind in (10, 12) and len(body) <= 68:
                detail = 'data=' + body[4:].hex()
        yield offset, length, kind, code, transaction, detail
        offset += length
    if offset != len(data):
        raise ValueError(f'trailing {len(data) - offset} bytes')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('capture', type=pathlib.Path)
    parser.add_argument('--tshark', default=r'C:\Program Files\Wireshark\tshark.exe')
    args = parser.parse_args()
    def run(*flags):
        return subprocess.check_output([args.tshark, '-r', str(args.capture), *flags], text=True)
    streams = sorted(set(run('-Y', 'tcp.port == 15740', '-T', 'fields', '-e', 'tcp.stream').split()))
    output = args.capture.with_suffix('.ptpip.csv')
    counts = Counter()
    with output.open('w', newline='', encoding='utf-8') as handle:
        writer = csv.writer(handle)
        writer.writerow(['stream', 'direction', 'offset', 'length', 'type', 'code', 'transaction', 'detail'])
        for stream in streams:
            # Follow TCP removes retransmissions and reassembles by sequence number.
            result = run('-q', '-z', f'follow,tcp,raw,{stream}')
            buffers = [io.BytesIO(), io.BytesIO()]
            for line in result.splitlines():
                value = line.strip()
                if value and all(c in '0123456789abcdefABCDEF' for c in value) and len(value) % 2 == 0:
                    buffers[int(line.startswith('\t'))].write(bytes.fromhex(value))
            for direction, buffer in enumerate(buffers):
                try:
                    for row in decode(buffer.getvalue()):
                        writer.writerow([stream, direction, *row])
                        if row[3]:
                            counts[(direction, row[3])] += 1
                except ValueError as error:
                    print(f'WARNING stream={stream} direction={direction}: {error}')
    print(f'Saved {output}')
    for (direction, code), count in counts.most_common():
        print(f'direction={direction} code={code} count={count}')


if __name__ == '__main__':
    main()
