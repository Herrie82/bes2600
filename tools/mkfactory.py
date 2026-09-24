#!/usr/bin/env python3
"""Build bes2600/factory.bin from bes2600_factory.txt.

megi's cw1200-based BES2600 driver requests a 72-byte binary blob, while the
firmware package only ships the text form our own driver parses at runtime.
The two describe the same struct factory_t (8-byte head + 64-byte data), so
this converts one to the other.  Layout from bes2600/bes2600_factory.h; CRC is
crc32_le(0xffffffff, data) ^ 0xffffffff over the data, per factory_crc32().
"""
import struct, sys, zlib

src, dst = sys.argv[1], sys.argv[2]
kv = {}
for line in open(src):
    line = line.strip()
    if not line or line.startswith('#') or line.startswith('%'):
        continue
    if ':' in line:
        k, v = line.split(':', 1)
        kv[k.strip()] = int(v.strip(), 0)

g = lambda k, d=0: kv.get(k, d)
ch5 = ["ch36-40","ch44-48","ch52-56","ch60-64","ch100-104","ch108-112",
       "ch116-120","ch124-128","ch132-136","ch140-144","ch149-153",
       "ch157-161","ch165-169"]

data = struct.pack('<I',  g('iQ_offset'))
data += struct.pack('<H', g('freq_cal'))
data += struct.pack('<3H', g('ch1'), g('ch7'), g('ch13'))
data += struct.pack('<BB', g('freq_cal_flags'), g('tx_power_type', 0xff))
data += struct.pack('<H',  g('temperature'))
data += struct.pack('<13H', *[g(c) for c in ch5])
data += struct.pack('<H',  g('tx_power_flags_5G'))
data += struct.pack('<4I', g('bdr_div'), g('bdr_power'), g('edr_div'), g('edr_power'))
data += struct.pack('<H',  g('temperature_5G'))
data += struct.pack('<H',  g('select_efuse'))

assert len(data) == 64, f"data is {len(data)} bytes, expected 64"
crc = zlib.crc32(data) & 0xffffffff          # == crc32_le(~0, d) ^ ~0
head = struct.pack('<HHI', g('magic', 0xba80), g('version', 2), crc)
blob = head + data
assert len(blob) == 72, f"blob is {len(blob)} bytes, expected 72"
open(dst, 'wb').write(blob)
print(f"wrote {dst}: {len(blob)} bytes, crc 0x{crc:08x}")
