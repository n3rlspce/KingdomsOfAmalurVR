"""Stage an opt-in startup patch from the user's own game assets.

No game process, input simulation, timer, or external Lua loader is used.
Only the two audited asset revisions are accepted. Proprietary assets stay local.
"""
from pathlib import Path
import argparse
import hashlib
import struct

HASHES = {
    '13493.lua_bxml': '661b3f507fab09e74af81e0f3e75f0848b8fba158e626c8c73ae61dad3955305',
    '1645799.lua_bxml': 'e60c520d25d267b26ec9e6bec779b0d61c4476fd09c9eed4116c95156a8b565a',
}
BATCH_MENU_HASH = 'c7d6afebb94fcc8b601ceae122f9379e91b8fcac8d6fe24ef040235a53fdf97e'


def word(n):
    return struct.pack('>I', n)


def abc(op, a=0, b=0, c=0):
    assert 0 <= a < 256 and 0 <= b < 256 and 0 <= c < 512
    return (op << 25) | (b << 17) | (c << 8) | a


def abx(op, a, bx):
    assert 0 <= bx < 131072
    return (op << 25) | (bx << 8) | a


class Chunk:
    """Lossless reader/writer for this game's big-endian Kore format 8."""
    def __init__(self, data):
        self.data = data
        self.start = data.index(b'\x1bLua')
        assert data[self.start:self.start + 14] == bytes.fromhex('1b4c756151080004040404010100')
        assert struct.unpack_from('<I', data, self.start - 4)[0] == len(data) - self.start
        self.pos = self.start + 14
        for _ in range(self.u32()):
            self.u32()
            self.string()
        self.prefix = data[:self.pos]
        self.root = self.function()
        self.suffix = data[self.pos:]

    def take(self, count):
        assert 0 <= count <= len(self.data) - self.pos
        result = self.data[self.pos:self.pos + count]
        self.pos += count
        return result

    def u32(self):
        return struct.unpack('>I', self.take(4))[0]

    def string(self):
        return self.take(self.u32())

    def function(self):
        start = self.pos
        self.string(); self.string()
        self.take(4 * 4 + 1 + 4)  # lines, upvalues, params, vararg, stack
        header = self.data[start:self.pos]
        code = [self.u32() for _ in range(self.u32())]
        constants = []
        for _ in range(self.u32()):
            start = self.pos
            tag = self.take(1)[0]
            if tag == 4:
                self.string()
            elif tag in (2, 3):
                self.take(4)
            elif tag == 11:
                self.take(8)
            elif tag == 1:
                self.take(1)
            else:
                assert tag == 0
            constants.append(self.data[start:self.pos])
        children = [self.function() for _ in range(self.u32())]
        start = self.pos
        self.take(4 * self.u32())
        for _ in range(self.u32()):
            self.string(); self.take(8)
        for _ in range(self.u32()):
            self.string()
        return [header, code, constants, children, self.data[start:self.pos]]

    @staticmethod
    def encode(f):
        header, code, constants, children, debug = f
        return (header + word(len(code)) + b''.join(word(w) for w in code) +
                word(len(constants)) + b''.join(constants) + word(len(children)) +
                b''.join(Chunk.encode(c) for c in children) + debug)

    def bytes(self):
        result = bytearray(self.prefix + self.encode(self.root) + self.suffix)
        struct.pack_into('<I', result, self.start - 4, len(result) - self.start)
        return bytes(result)


class Tail:
    def __init__(self, function):
        self.f = function
        self.code = []

    def constant(self, value):
        if isinstance(value, str):
            raw = value.encode() + b'\0'
            raw = b'\x04' + word(len(raw)) + raw
        else:
            raw = b'\x03' + struct.pack('>i', value)
        if raw not in self.f[2]:
            self.f[2].append(raw)
        return self.f[2].index(raw)

    def global_(self, register, name):
        # Uncached GETGLOBAL avoids adding mutable memoization slots.
        self.code.append(abx(6, register, self.constant(name)))

    def method(self, register, table, name):
        self.global_(register, table)
        self.code.extend([abc(0x43, register, register, self.constant(name)),
                          abc(0x46), abc(0x46)])

    def return_if(self, register, truth):
        self.code.extend([abc(1, register, 0, 0 if truth else 1),
                          abx(0x1c, 0, 65536), abc(9, 0, 1)])


def patch_menu(chunk, bundled=False):
    # The installed patch inserts another function before on_update_event.
    f = chunk.root[3][58 if bundled else 57]
    assert len(f[1]) == (152 if bundled else 149) and f[1][-1] == abc(9, 0, 1)
    t = Tail(f)
    t.global_(2, 'amalur_fast_start_attempted')
    t.return_if(2, True)
    t.method(2, 'WINDOW', 'is_visible')
    t.global_(3, 'm_window')
    t.code.append(abc(2, 2, 2, 2))
    t.return_if(2, False)
    t.method(2, 'SAVE_RESTORE', 'is_saving_disabled_for_user')
    t.code.append(abc(2, 2, 1, 2))
    t.return_if(2, True)
    t.method(2, 'SAVE_RESTORE', 'get_most_recent_save_slot')
    t.code.append(abc(2, 2, 1, 2))
    # No save: leave the ordinary menu available, never start a new game.
    t.code.extend([abc(4, 0, 2, 256 + t.constant(-1)),
                   abx(0x1c, 0, 65536), abc(9, 0, 1)])
    t.code.extend([abc(13, 2, 1),
                   abx(27, 2, t.constant('amalur_fast_start_attempted'))])
    t.global_(2, 'continue_last_save')
    t.code.extend([abc(2, 2, 1, 1), abc(9, 0, 1)])
    # Earlier returns keep their meaning; branches to the final return enter here.
    f[1][-1:] = t.code
    return t


def patch_splash(chunk):
    code = chunk.root[3][7][1]  # on_update_event
    # Take the existing profile-acquisition route without requiring key presses.
    for index in (48, 84, 146):
        assert code[index] == abc(0x3f, 2, 1, 2)
        code[index] = abc(13, 2, 1)
    # Remove only the debounce delay in those same startup-input paths.
    for index in (59, 157):
        assert code[index] == abc(50, 0, 3, 2)
        code[index] = abx(0x1c, 0, 65536)


def stage(source, output):
    if source.resolve() == output.resolve():
        raise ValueError('Output must differ from source')
    staged = {}
    for name, expected in HASHES.items():
        data = (source / name).read_bytes()
        if hashlib.sha256(data).hexdigest() != expected:
            raise ValueError('Unsupported asset revision: ' + name)
        chunk = Chunk(data)
        assert chunk.bytes() == data, 'Lossless round-trip failed'
        (patch_menu if name.startswith('13493') else patch_splash)(chunk)
        patched = chunk.bytes()
        assert Chunk(patched).bytes() == patched
        staged[name] = patched
    output.mkdir(parents=True, exist_ok=True)
    for name, data in staged.items():
        (output / name).write_bytes(data)
    print('Staged direct startup scripts; live game validation still required.')


def batch_entries(data):
    count = struct.unpack_from('<I', data)[0]
    if count > 100000 or 4 + count * 8 > len(data):
        raise ValueError('Invalid script batch directory')
    pos = 4 + count * 8
    entries = []
    for i in range(count):
        ident, size = struct.unpack_from('<II', data, 4 + i * 8)
        if pos + size > len(data):
            raise ValueError('Truncated script batch')
        entries.append((ident, data[pos:pos + size]))
        pos += size
    if pos != len(data):
        raise ValueError('Unexpected script batch trailer')
    return entries


def patch_batch(data):
    entries = batch_entries(data)
    changed = set()
    patched = []
    for ident, payload in entries:
        if ident in (13493, 1645799):
            expected = BATCH_MENU_HASH if ident == 13493 else HASHES['1645799.lua_bxml']
            if ident in changed or hashlib.sha256(payload).hexdigest() != expected:
                raise ValueError('Unsupported or duplicate active script: ' + str(ident))
            chunk = Chunk(payload)
            assert chunk.bytes() == payload
            if ident == 13493:
                patch_menu(chunk, bundled=True)
            else:
                patch_splash(chunk)
            payload = chunk.bytes()
            assert Chunk(payload).bytes() == payload
            changed.add(ident)
        patched.append((ident, payload))
    if changed != {13493, 1645799}:
        raise ValueError('Active batch does not contain both startup scripts')
    result = (struct.pack('<I', len(patched)) +
              b''.join(struct.pack('<II', ident, len(payload)) for ident, payload in patched) +
              b''.join(payload for _, payload in patched))
    rebuilt = batch_entries(result)
    assert rebuilt == patched
    assert all(before == after for before, after in zip(entries, rebuilt) if before[0] not in changed)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--batch', action='store_true', help='Patch the active klua.batch archive entry')
    args = parser.parse_args()
    if args.batch:
        patched = patch_batch(args.source.read_bytes())
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes(patched)
        print('Patched active script batch; all other embedded scripts preserved byte-for-byte.')
    else:
        stage(args.source, args.output)
