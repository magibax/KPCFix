"""Verify exact initial-enabled/disabled/stock images and rollback.
Usage: python check_modes.py /path/to/stock/object.lto
"""
from pathlib import Path
import hashlib,json,sys
root=Path(__file__).resolve().parents[2];m=json.loads((root/'patch.json').read_text())
stock=Path(sys.argv[1]).read_bytes();assert hashlib.sha256(stock).hexdigest()==m['original']
images={}
for mode,key,expected in [('Enabled','patched',m['patched']),('Disabled','disabled',m['disabled']),('Stock','stock',m['original'])]:
 b=bytearray(stock)
 for c in m['changes']:
  assert stock[c['offset']]==c['stock']
  b[c['offset']]=c[key]
 assert hashlib.sha256(b).hexdigest()==expected
 images[mode]=bytes(b)
 for c in m['changes']:b[c['offset']]=c['stock']
 assert b==stock
assert [i for i,(a,b) in enumerate(zip(images['Enabled'],images['Disabled'])) if a!=b]==[m['symbols']['initial_mode']]
print('PASS exact mode hashes, common live hooks in both startup modes, original bytes and full rollback')
