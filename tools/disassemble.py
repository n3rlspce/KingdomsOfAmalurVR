"""Read-only PE32 disassembly by RVA; use verified instruction boundaries."""
from pathlib import Path
import struct
import sys
sys.path.insert(0, str(Path(__file__).parent / 'python-deps'))
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

data = Path(sys.argv[1]).read_bytes()
start = int(sys.argv[2], 0)
size = int(sys.argv[3], 0) if len(sys.argv) > 3 else 256
pe = struct.unpack_from('<I', data, 0x3c)[0]
opt = pe + 24
assert struct.unpack_from('<H', data, opt)[0] == 0x10b
base = struct.unpack_from('<I', data, opt + 28)[0]
sections = opt + struct.unpack_from('<H', data, pe + 20)[0]
for i in range(struct.unpack_from('<H', data, pe + 6)[0]):
    off = sections + i * 40
    virtual_size, rva, raw_size, raw = struct.unpack_from('<IIII', data, off + 8)
    if rva <= start < rva + min(virtual_size, raw_size):
        position = raw + start - rva
        code = data[position:position + min(size, raw_size - (start - rva))]
        for ins in Cs(CS_ARCH_X86, CS_MODE_32).disasm(code, base + start):
            print(f'RVA {ins.address-base:08x} VA {ins.address:08x}  {ins.mnemonic:8} {ins.op_str}')
        break
else:
    raise ValueError('RVA outside file-backed section')
