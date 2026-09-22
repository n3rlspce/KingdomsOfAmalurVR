"""Read-only, bounded native dialogue snapshot; does not inject or send input."""
import argparse
import ctypes as c
import json
import struct
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('pid', type=int)
p.add_argument('base', type=lambda x: int(x, 0))
p.add_argument('output', type=Path)
a = p.parse_args()
k = c.WinDLL('kernel32', use_last_error=True)
k.OpenProcess.argtypes = [c.c_uint, c.c_int, c.c_uint]
k.OpenProcess.restype = c.c_void_p
k.ReadProcessMemory.argtypes = [c.c_void_p, c.c_void_p, c.c_void_p, c.c_size_t, c.POINTER(c.c_size_t)]
k.CloseHandle.argtypes = [c.c_void_p]
h = k.OpenProcess(0x410, 0, a.pid)
if not h:
    raise OSError(c.get_last_error(), 'Cannot read game process')

def read(address, count):
    data, size = c.create_string_buffer(count), c.c_size_t()
    if not k.ReadProcessMemory(h, address, data, count, c.byref(size)) or size.value != count:
        raise OSError(c.get_last_error(), hex(address))
    return data.raw

def word(address):
    return struct.unpack('<I', read(address, 4))[0]

def vector(address):
    return struct.unpack('<3f', read(address, 12))

try:
    assert read(a.base, 2) == b'MZ'
    manager = word(a.base + 0x15fec38)
    dialogs = manager + 0x2df8
    assert word(dialogs) == a.base + 0x134e074
    dialog = word(dialogs + 0x148)
    result = {'pid': a.pid, 'base': hex(a.base), 'dialog': hex(dialog)}
    if dialog:
        assert word(dialog) == a.base + 0x1343210
        result['flags'] = read(dialog + 0x6c, 1)[0]
        result['participants'] = []
        for offset in (0x5c, 0x60):
            handle = word(dialog + offset)
            pool, index = manager + 0x2238, handle & 65535
            assert index < word(pool + 0x20)
            assert word(word(pool + 0x1c) + index * 4) == handle
            entity = word(word(pool + 0xc) + index * 4)
            assert word(entity + 0x38) == handle
            loc = word(entity + 0x3c + 6 * 4)
            item = {'offset': hex(offset), 'handle': hex(handle),
                    'entity': hex(entity), 'entityFlags': word(entity + 0x10c),
                    'location': hex(loc), 'locationVtable': hex(word(loc) - a.base),
                    'locationOwner': hex(word(loc + 0x18)), 'locationIndex': word(loc + 0x1c),
                    'position': vector(loc + 0x24),
                    'yaw': word(loc + 0xb0) * 360 / 4294967296}
            result['participants'].append(item)
            animation = word(entity + 0x3c + 7 * 4)
            if animation and word(animation + 0x18) == handle and word(animation + 0x1c) == 7:
                model_id = word(animation + 0x9c)
                if model_id < 65536:
                    model_pool = word(a.base + 0x15fdf54)
                    model = word(word(model_pool + 0xc4) + model_id * 4)
                    tracker = word(model + 0x21c)
                    item['lookAt'] = {'part': hex(animation), 'modelId': model_id,
                                      'targetMode': word(animation + 0xc0),
                                      'tracker': hex(tracker)}
                    if tracker:
                        item['lookAt']['trackerState10'] = read(tracker + 0x10, 16).hex()
    globals_ = word(a.base + 0x15fe9c4)
    windows = globals_ + 0x397c
    assert word(windows) == a.base + 0x134e60c and word(windows + 8) == 1
    game = word(word(windows + 4) + 4)
    assert word(game) in (a.base + 0x1328914, a.base + 0x132ae7c)
    scene = word(game + 0x40c)
    assert word(scene) == a.base + 0x1326e9c
    camera = word(scene + 0x410)
    assert word(camera) == a.base + 0x1335d08
    result['camera'] = {'address': hex(camera), 'eye': vector(camera + 12),
                        'target': vector(camera + 28)}
    a.output.write_text(json.dumps(result, indent=2))
    print(json.dumps(result))
finally:
    k.CloseHandle(h)
