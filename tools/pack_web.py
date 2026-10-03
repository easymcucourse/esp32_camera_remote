"""Create deterministic gzip for the embedded, offline maintenance page."""
import gzip
from pathlib import Path
import sys
data=gzip.compress(Path(sys.argv[1]).read_bytes(),mtime=0)
if len(data)>20*1024:
    raise SystemExit('maintenance page exceeds 20 KiB')
Path(sys.argv[2]).write_bytes(data)
