"""Stage an exact-revision native startup experiment; never installs or runs."""
import hashlib
import json
from pathlib import Path
import struct
import sys

ORIGINAL = '16a400f6e8fc10dbe446a9e57717de9fbe975ef17c004f4ca405e2e4e09cb314'


def build(data):
    assert hashlib.sha256(data).hexdigest() == ORIGINAL, 'Unsupported original executable'
    result = bytearray(data)
    u16 = lambda o: struct.unpack_from('<H', data, o)[0]
    u32 = lambda o: struct.unpack_from('<I', data, o)[0]
    pe = u32(0x3c)
    opt = pe + 24
    assert u16(opt) == 0x10b
    table = opt + u16(pe + 20)
    count = u16(pe + 6)
    section = table + count * 40
    assert section + 40 <= u32(opt + 60)
    assert not any(data[section:section + 40]), 'Section header space is occupied'
    align = lambda n, a: (n + a - 1) // a * a
    rva = align(u32(opt + 56), u32(opt + 32))
    raw = align(len(data), u32(opt + 36))
    hook = 0xb42917
    original = bytes.fromhex('8b8628040000')
    assert data[hook - 0xc00:hook - 0xc00 + 6] == original
    def offset(address):
        for i in range(count):
            s = table + i * 40
            if u32(s + 12) <= address < u32(s + 12) + u32(s + 16):
                return u32(s + 20) + address - u32(s + 12)
        raise AssertionError('Unmapped import RVA')
    descriptor = offset(u32(opt + 104))
    get_state = None
    while u32(descriptor + 12):
        name = offset(u32(descriptor + 12))
        if data[name:data.index(0, name)].lower() == b'xinput1_3.dll':
            names = offset(u32(descriptor))
            index = 0
            while u32(names + index * 4):
                # Verified against the installed 32-bit xinput1_3 export table.
                if u32(names + index * 4) == 0x80000002:
                    get_state = u32(descriptor + 16) + index * 4
                index += 1
        descriptor += 20
    assert get_state is not None, 'XInputGetState import missing'
    stub = b'\xff\x25' + struct.pack('<I', u32(opt + 28) + get_state)
    stub_offset = data.find(stub, 0x400, 0xf61000)
    assert stub_offset >= 0, 'XInput import thunk missing'
    probe_rva = stub_offset + 0xc00
    # Profile update: completion guard already ran. State -1 awaits input.
    # Call the normal begin-acquisition routine, retaining its asynchronous
    # initialization and the original update's authentication/DLC checks.
    code = bytearray()
    branches = []
    def guard(instructions, jump):
        code.extend(bytes.fromhex(instructions))
        branches.append(len(code))
        code.extend(bytes([jump, 0]))
    guard('83be28040000ff', 0x75)  # state != -1
    guard('83be2404000000', 0x74)  # callback not installed
    code.extend(bytes.fromhex('8b8e50040000'))  # platform profile context
    guard('85c9', 0x74)
    guard('83791c00', 0x74)  # input/user acquisition table not initialized
    # Native sign-in can run before the VR bridge exposes its virtual pad.
    # Probe through the game's existing, relocated import thunk (and VR hook).
    # No synthetic input is sent; disconnected controllers keep state at -1.
    code.extend(bytes.fromhex('83ec108bc4506a00'))
    probe = len(code)
    code.extend(b'\xe8' + struct.pack('<i', probe_rva - (rva + probe + 5)))
    code.extend(bytes.fromhex('83c410'))
    guard('85c0', 0x75)
    code.extend(bytes.fromhex('8bce'))  # this = profile manager
    call = len(code)
    code.extend(b'\xe8' + struct.pack('<i', 0xafae20 - (rva + call + 5)))
    resume = len(code)
    for pos in branches:
        code[pos + 1] = resume - (pos + 2)
    code.extend(original)
    code.extend(b'\xe9' + struct.pack('<i', hook + 6 - (rva + len(code) + 5)))
    result[hook - 0xc00:hook - 0xc00 + 6] = b'\xe9' + struct.pack('<i', rva - (hook + 5)) + b'\x90'
    # Finish the publisher/legal sequence through its normal completion path.
    assert data[0x664e6f:0x664e74] == bytes.fromhex('83f803721a')
    result[0x664e72:0x664e74] = b'\x90\x90'
    raw_size = align(len(code), u32(opt + 36))
    result.extend(bytes(raw - len(result)))
    result.extend(code)
    result.extend(bytes(raw_size - len(code)))
    struct.pack_into('<8sIIIIIIHHI', result, section, b'.amstart', len(code), rva,
                     raw_size, raw, 0, 0, 0, 0, 0x60000020)  # read/execute only
    struct.pack_into('<H', result, pe + 6, count + 1)
    struct.pack_into('<I', result, opt + 4, u32(opt + 4) + raw_size)
    struct.pack_into('<I', result, opt + 56, align(rva + len(code), u32(opt + 32)))
    struct.pack_into('<I', result, opt + 64, 0)  # user-mode PE checksum optional
    # All references in the new section are relative, so ASLR remains valid.
    return result, {'hookRva': hook, 'thunkRva': rva, 'thunkSize': len(code),
                    'callRva': rva + call, 'resumeRva': rva + resume,
                    'controllerProbeRva': probe_rva}


if __name__ == '__main__':
    source, out = map(Path, sys.argv[1:])
    assert not out.exists(), 'Use a fresh output directory'
    payload, details = build(source.read_bytes())
    out.mkdir(parents=True)
    (out/'koa.exe').write_bytes(payload)
    (out/'manifest.json').write_text(json.dumps(dict(details,
        original=ORIGINAL, patched=hashlib.sha256(payload).hexdigest(),
        status='experimental; live no-input startup test required'), indent=2))
    print(json.dumps(details))
