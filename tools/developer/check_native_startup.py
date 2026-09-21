"""Execute staged x86 startup code offline; no game process or game API calls."""
import json
from pathlib import Path
import struct
import sys
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_ECX, UC_X86_REG_EAX

package = Path(sys.argv[1])
data = (package/'koa.exe').read_bytes()
manifest = json.loads((package/'manifest.json').read_text())
u16 = lambda o: struct.unpack_from('<H', data, o)[0]
u32 = lambda o: struct.unpack_from('<I', data, o)[0]
pe = u32(0x3c); opt = pe + 24
table = opt + u16(pe + 20)

for base in (0x400000, 0x490000, 0x10000000):
    for state, callback, context, users, connected, expected in (
        (-1, True, True, True, True, 1),
        (-1, True, True, True, False, 0),
        (-1, False, True, True, True, 0),
        (-1, True, False, True, True, 0),
        (-1, True, True, False, True, 0),
        (0, True, True, True, True, 0),
        (1, True, True, True, True, 0),
        (2, True, True, True, True, 0),
    ):
        cpu = Uc(UC_ARCH_X86, UC_MODE_32)
        cpu.mem_map(base, u32(opt + 56))
        for i in range(u16(pe + 6)):
            s = table + i * 40
            cpu.mem_write(base + u32(s + 12), data[u32(s + 20):u32(s + 20) + u32(s + 16)])
        obj = 0x50000000
        cpu.mem_map(obj, 0x4000)
        cpu.mem_map(0x70000000, 0x10000)
        put = lambda address, value: cpu.mem_write(address, struct.pack('<I', value & 0xffffffff))
        put(obj + 0x428, state)
        put(obj + 0x424, 0x12345678 if callback else 0)
        put(obj + 0x450, obj + 0x1000 if context else 0)
        put(obj + 0x101c, obj + 0x2000 if users else 0)
        # Execute the real begin-acquisition routine. Replace only its external
        # platform call with ret 8, keeping its actual state transition intact.
        platform = base + 0xc76440
        cpu.mem_write(platform, b'\xc2\x08\x00')
        probe = base + manifest['controllerProbeRva']
        cpu.mem_write(probe, b'\xb8' + struct.pack('<I', 0 if connected else 1167) + b'\xc2\x08\x00')
        calls = []
        def on_code(uc, address, size, unused):
            if address == probe:
                sp = uc.reg_read(UC_X86_REG_ESP)
                assert bytes(uc.mem_read(sp + 4, 4)) == bytes(4)
                buffer = struct.unpack('<I', bytes(uc.mem_read(sp + 8, 4)))[0]
                assert buffer == sp + 12
            if address == platform:
                assert uc.reg_read(UC_X86_REG_ECX) == obj + 0x1000
                sp = uc.reg_read(UC_X86_REG_ESP)
                assert bytes(uc.mem_read(sp + 4, 8)) == bytes(8)
                calls.append(address)
        cpu.hook_add(UC_HOOK_CODE, on_code)
        cpu.reg_write(UC_X86_REG_ESI, obj)
        cpu.reg_write(UC_X86_REG_ESP, 0x70008000)
        for frame in range(2):
            cpu.emu_start(base + manifest['hookRva'], base + manifest['hookRva'] + 6, count=100)
            assert cpu.reg_read(UC_X86_REG_ESP) == 0x70008000
            assert cpu.reg_read(UC_X86_REG_ESI) == obj
            assert len(calls) == expected, 'Unexpected or repeated platform request'
            assert cpu.reg_read(UC_X86_REG_EAX) == (0 if expected else state & 0xffffffff)
print('PASS: actual x86 guards, native state transition, balanced stack, no repeated request, three ASLR bases.')
