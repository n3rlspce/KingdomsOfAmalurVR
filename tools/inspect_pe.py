"""Read PE architecture and normal imports without loading the executable."""
import hashlib
import json
from pathlib import Path
import struct
import sys


def inspect(path):
    data = Path(path).read_bytes()
    def u16(offset):
        return struct.unpack_from('<H', data, offset)[0]
    def u32(offset):
        return struct.unpack_from('<I', data, offset)[0]
    if data[:2] != b'MZ':
        raise ValueError('Not a PE file')
    pe = u32(0x3c)
    if data[pe:pe + 4] != b'PE\0\0':
        raise ValueError('Invalid PE signature')
    opt = pe + 24
    magic = u16(opt)
    if magic not in (0x10b, 0x20b):
        raise ValueError('Unsupported optional header')
    sections = []
    start = opt + u16(pe + 20)
    for i in range(u16(pe + 6)):
        offset = start + 40 * i
        sections.append((u32(offset + 12), max(u32(offset + 8), u32(offset + 16)), u32(offset + 20)))
    def rva_to_offset(rva):
        if rva < u32(opt + 60):
            return rva
        for va, size, raw in sections:
            if va <= rva < va + size:
                return raw + rva - va
        raise ValueError(f'Unmapped RVA {rva:x}')
    def string_at(offset):
        return data[offset:data.index(b'\0', offset)].decode('ascii', errors='replace')
    directories = opt + (96 if magic == 0x10b else 112)
    imports = {}
    import_rva = u32(directories + 8)
    if import_rva:
        desc = rva_to_offset(import_rva)
        for _ in range(4096):
            fields = struct.unpack_from('<5I', data, desc)
            if not any(fields):
                break
            original, _, _, name, thunk = fields
            dll = string_at(rva_to_offset(name))
            cursor = rva_to_offset(original or thunk)
            width = 4 if magic == 0x10b else 8
            symbols = []
            for _ in range(65536):
                value = int.from_bytes(data[cursor:cursor + width], 'little')
                if not value:
                    break
                if value & (1 << (width * 8 - 1)):
                    symbols.append(f'ordinal:{value & 0xffff}')
                else:
                    symbols.append(string_at(rva_to_offset(value) + 2))
                cursor += width
            imports[dll] = symbols
            desc += 20
    return {'path': str(Path(path).resolve()), 'bytes': len(data),
            'sha256': hashlib.sha256(data).hexdigest(),
            'machine': hex(u16(pe + 4)),
            'architecture': {0x14c: 'x86', 0x8664: 'x64'}.get(u16(pe + 4), 'other'),
            'pe_format': 'PE32' if magic == 0x10b else 'PE32+',
            'imports': imports,
            'scope': 'Static normal import table only; not runtime device verification.'}


if __name__ == '__main__':
    print(json.dumps(inspect(sys.argv[1]), indent=2))
