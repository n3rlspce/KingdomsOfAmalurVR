"""Read-only rig census for verified Re-Reckoning build 10619381.

Usage: python tools/inspect-player-rig.py PID BASE PLAYER > captures/rig.json
Addresses are session inputs, never persistent anchors. No process writes.
"""
import argparse
import ctypes as c
import json
import math
import struct


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('pid', 'base', 'player'):
        parser.add_argument(name, type=lambda value: int(value, 0))
    args = parser.parse_args()
    kernel = c.WinDLL('kernel32', use_last_error=True)
    kernel.OpenProcess.argtypes = [c.c_uint, c.c_int, c.c_uint]
    kernel.OpenProcess.restype = c.c_void_p
    kernel.ReadProcessMemory.argtypes = [c.c_void_p, c.c_void_p, c.c_void_p,
                                        c.c_size_t, c.POINTER(c.c_size_t)]
    kernel.CloseHandle.argtypes = [c.c_void_p]
    handle = kernel.OpenProcess(0x410, False, args.pid)
    if not handle:
        raise c.WinError(c.get_last_error())

    def read(address, size):
        if not 0x10000 <= address < 0x100000000 or not 0 < size <= 65536:
            raise ValueError('Invalid bounded x86 read')
        buf, got = c.create_string_buffer(size), c.c_size_t()
        if not kernel.ReadProcessMemory(handle, address, buf, size, c.byref(got)) or got.value != size:
            raise ValueError(f'Unreadable memory at {address:#x}')
        return buf.raw

    def word(address):
        return struct.unpack('<I', read(address, 4))[0]

    def entity(owner):
        pool = word(args.base + 0x15fec38) + 0x2238
        count = word(pool + 0x20)
        index = owner & 0xffff
        if not owner & 0x0fff0000 or not index < count <= 65536:
            raise ValueError('Invalid entity handle')
        if word(word(pool + 0x1c) + index * 4) != owner:
            raise ValueError('Stale entity generation')
        result = word(word(pool + 0xc) + index * 4)
        if word(result + 0x38) != owner:
            raise ValueError('Entity owner mismatch')
        return result

    def bones(address, count):
        raw = read(address, count * 48)
        result = []
        for i in range(count):
            chunk = raw[i * 48:(i + 1) * 48]
            value = struct.unpack('<8f', chunk[:32])
            norm = sum(v * v for v in value[4:8])
            valid = all(math.isfinite(v) for v in value) and abs(norm - 1) < .02
            result.append(dict(index=i, pose_valid=valid,
                               position=value[:3] if valid else None,
                               orientation=value[4:8] if valid else None,
                               opaque_tail_hex=chunk[32:].hex(), raw_hex=chunk.hex()))
        return result

    try:
        if word(args.base) & 0xffff != 0x5a4d:
            raise ValueError('Invalid PE base')
        if word(args.player) - args.base not in (0x1359f14, 0x1359e94):
            raise ValueError('Player type mismatch')
        owner = word(args.player + 0x1ec)
        player_entity = entity(owner)
        render = word(player_entity + 0x3c + 7 * 4)
        if (not word(player_entity + 0x10c) & 1 or word(render) != args.base + 0x13560e4
                or word(render + 0x18) != owner or word(render + 0x1c) != 7):
            raise ValueError('Player rendering ownership mismatch')
        manager = word(args.base + 0x15fdf54)
        table, total = word(manager + 0xc4), word(manager + 0xc8)
        if not 2 < total < 100000:
            raise ValueError('Invalid Fab table size')
        seen, records = set(), []

        def visit(index, slot, depth):
            if index < 2 or index >= total or index in seen:
                return
            if depth > 8 or len(seen) >= 128:
                raise ValueError('Rig census bound exceeded')
            seen.add(index)
            obj = word(table + index * 4)
            if not obj:
                return
            if word(obj) != args.base + 0x1340f3c or word(obj + 0x194) != index:
                raise ValueError('Fab type/index mismatch')
            record = dict(slot=slot, index=index, address=hex(obj), owner=hex(word(obj + 0xf8)))
            records.append(record)
            count, buffer = word(obj + 0x38), word(obj + 0x34)
            record.update(bone_count=count, bone_buffer=hex(buffer), asset=word(obj + 0x198))
            if 0 < count <= 512:
                try:
                    record['bones'] = bones(buffer, count)
                except ValueError as error:
                    record['bone_error'] = str(error)
                    record['bone_bytes'] = read(buffer, count * 48).hex()
            # 0x90fe20: animation member at Fab+0x44 resolves asset ID +0xac.
            # 0x89a9b0: asset+4 -> blob+0x18 -> relative int16 parent table +0x1c.
            try:
                asset_id = word(obj + 0xf0)
                if asset_id < 100000:
                    flags = read(word(manager + 0x28) + asset_id, 1)[0]
                    if flags & 4 and not flags & 0x10:
                        asset = word(word(manager + 0x18) + asset_id * 4)
                        blob = word(asset + 0x1c)
                        record['skeleton_blob'] = hex(blob)
                        record['blob_header'] = read(blob, 64).hex()
                        name_offset = struct.unpack('<i', read(blob + 0x20, 4))[0]
                        if name_offset and 0 < count <= 512:
                            record['bone_ids'] = struct.unpack('<' + 'I' * count, read(blob + 0x20 + name_offset, count * 4))
                        offset = struct.unpack('<i', read(blob + 0x1c, 4))[0]
                        if offset and 0 < count <= 512:
                            parents = struct.unpack('<' + 'h' * count, read(blob + 0x1c + offset, count * 2))
                            record['parents'] = parents
                            record['parent_order_valid'] = all(-1 <= p < i for i, p in enumerate(parents))
            except ValueError as error:
                record['skeleton_error'] = str(error)
            child_count, children = word(obj + 0x28), word(obj + 0x24)
            if child_count > 32:
                raise ValueError('Invalid child count')
            for child in range(child_count):
                visit(word(children + child * 4), f'{slot}/{child}', depth + 1)
            if word(obj + 0x34) != buffer or word(obj + 0x38) != count or word(table + index * 4) != obj:
                raise ValueError('Rig changed during census; retry in stable gameplay')

        visit(word(render + 0x9c), 'player', 0)
        if not records or int(records[0]['owner'], 16) != owner or word(args.player + 0x1ec) != owner:
            raise ValueError('Player changed during census')
        print(json.dumps(dict(pid=args.pid, base=hex(args.base), owner=hex(owner),
                              evidence='read-only snapshot; render consumer not proven',
                              instances=records), indent=2, allow_nan=False))
    finally:
        kernel.CloseHandle(handle)


if __name__ == '__main__':
    main()
