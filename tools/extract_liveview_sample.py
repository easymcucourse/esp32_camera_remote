"""Extract the first live-view GetObject transaction from the recorded TCP stream."""
import io
from pathlib import Path
import struct
import subprocess

capture = Path('captures/remote-20260927-193841.pcapng')
result = subprocess.check_output([
    r'C:\Program Files\Wireshark\tshark.exe', '-r', str(capture),
    '-c', '1000', '-q', '-z', 'follow,tcp,raw,0'], text=True)
data = bytearray()
for line in result.splitlines():
    value = line.strip()
    if line.startswith('\t') and value and all(c in '0123456789abcdefABCDEF' for c in value):
        data.extend(bytes.fromhex(value))
offset = 0
objects = {}
while offset + 8 <= len(data):
    length, kind = struct.unpack_from('<II', data, offset)
    if length < 8 or offset + length > len(data):
        break
    body = data[offset + 8:offset + length]
    if kind in (9, 10, 12) and len(body) >= 4:
        transaction = struct.unpack_from('<I', body)[0]
        if kind == 9:
            total = struct.unpack_from('<Q', body, 4)[0]
            print(f'transaction={transaction} total={total}')
            objects[transaction] = bytearray()
        else:
            objects[transaction].extend(body[4:])
    offset += length
payload = objects[11]
Path('captures/liveview-object.bin').write_bytes(payload)
start = payload.find(b'\xff\xd8\xff')
end = payload.find(b'\xff\xd9', start) + 2
if start < 0 or end <= start:
    raise ValueError('No complete JPEG in first GetObject')
jpeg = bytes(payload[start:end])
Path('captures/liveview-sample.jpg').write_bytes(jpeg)
print(f'object={len(payload)} jpeg_offset={start} jpeg_bytes={len(jpeg)} prefix={payload[:min(start,64)].hex()}')
try:
    from PIL import Image
    image = Image.open(io.BytesIO(jpeg))
    image.load()
    print(f'JPEG {image.size} mode={image.mode}')
except ImportError:
    print('Pillow not installed; sample saved for inspection')
