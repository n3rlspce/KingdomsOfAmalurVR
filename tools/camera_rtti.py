"""Recover candidate MSVC x86 camera vtables from PE RTTI; never execute the game."""
import json
from pathlib import Path
import re
import struct
import sys

data = Path(sys.argv[1]).read_bytes()
u32 = lambda off: struct.unpack_from('<I', data, off)[0]
pe = u32(0x3c)
opt = pe + 24
assert struct.unpack_from('<H', data, opt)[0] == 0x10b
base = u32(opt + 28)
table = opt + struct.unpack_from('<H', data, pe + 20)[0]
sections = []
for i in range(struct.unpack_from('<H', data, pe + 6)[0]):
    off = table + 40*i
    sections.append((u32(off+12), u32(off+20), u32(off+16), u32(off+36)))

def rva(raw):
    for va, start, size, flags in sections:
        if start <= raw < start+size:
            return va+raw-start
    raise ValueError('Unmapped file offset')

def executable(address):
    return any(flags & 0x20000000 and base+va <= address < base+va+size
               for va, raw, size, flags in sections)

def refs(value):
    needle = struct.pack('<I', value)
    start = 0
    while (start := data.find(needle, start)) >= 0:
        yield start
        start += 1

results = []
pattern = sys.argv[2].encode() if len(sys.argv)>2 else b'Camera'
for name in re.finditer(rb'\.\?AV[^\x00]{0,220}(?:'+pattern+rb')[^\x00]{0,180}\x00', data):
    # Ignore template allocator types and keep actual camera classes.
    if b'?$' in name.group():
        continue
    td = rva(name.start()-8)
    for reference in refs(base+td):
        col = reference-12
        if col < 0 or u32(col) != 0 or u32(col+4) > 0x10000:
            continue
        for pointer in refs(base+rva(col)):
            entries = []
            for i in range(24):
                address = u32(pointer+4+4*i)
                if not executable(address):
                    break
                entries.append(f'0x{address-base:08x}')
            if entries:
                results.append(dict(name=name.group()[:-1].decode(),
                    type_descriptor_rva=f'0x{td:08x}', col_rva=f'0x{rva(col):08x}',
                    object_offset=u32(col+4), vtable_rva=f'0x{rva(pointer+4):08x}',
                    function_rvas=entries))
print(json.dumps(results, indent=2))

