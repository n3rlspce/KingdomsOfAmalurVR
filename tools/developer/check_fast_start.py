"""Exercise the emitted startup tail with a small offline instruction interpreter."""
import struct
from fast_start import patch_menu, abc


def truth(value):
    return value is not None and value is not False


def execute(code, constants, globals_):
    values = []
    for raw in constants:
        values.append(raw[5:-1].decode() if raw[0] == 4 else struct.unpack('>i', raw[1:])[0])
    registers = {}
    pc = 0
    while pc < len(code):
        w = code[pc]; pc += 1
        op, a, b, c, bx = w >> 25, w & 255, (w >> 17) & 255, (w >> 8) & 511, (w >> 8) & 131071
        if op == 6:
            registers[a] = globals_.get(values[bx])
        elif op == 0x43:
            registers[a] = registers[b][values[c]]
            pc += 2  # field cache DATA words
        elif op == 1:
            if truth(registers[a]) != bool(c):
                pc += 1
        elif op == 28:
            pc += bx - 65535
        elif op == 9:
            return
        elif op == 2:
            result = registers[a](*(registers[i] for i in range(a + 1, a + b)))
            if c == 2:
                registers[a] = result
        elif op == 4:
            right = values[c - 256] if c >= 256 else registers[c]
            if (registers[b] == right) != bool(a):
                pc += 1
        elif op == 13:
            registers[a] = bool(b)
        elif op == 27:
            globals_[values[bx]] = registers[a]
        else:
            raise AssertionError('Unexpected opcode: ' + str(op))


def main():
    # Same emitter as production; no proprietary assets are required for this test.
    class FakeChunk:
        root = [None, None, None, [None] * 58]
    f = [b'', [0] * 148 + [abc(9, 0, 1)], [], [], b'']
    FakeChunk.root[3][57] = f
    tail = patch_menu(FakeChunk())
    for visible, disabled, slot in [(False, False, 0), (True, True, 0), (True, False, -1), (True, False, 0), (True, False, 7)]:
        calls = []
        env = {'WINDOW': {'is_visible': lambda _: visible}, 'm_window': 42,
               'SAVE_RESTORE': {'is_saving_disabled_for_user': lambda: disabled,
                                'get_most_recent_save_slot': lambda: slot},
               'continue_last_save': lambda: calls.append('continue')}
        execute(tail.code, f[2], env)
        execute(tail.code, f[2], env)
        assert len(calls) == int(visible and not disabled and slot != -1)
    # Mark before dispatch so a failed load cannot cause an automatic retry loop.
    env.pop('amalur_fast_start_attempted', None)
    def fail():
        raise RuntimeError('load failed')
    env['continue_last_save'] = fail
    try:
        execute(tail.code, f[2], env)
    except RuntimeError:
        pass
    else:
        raise AssertionError('Expected simulated failure')
    execute(tail.code, f[2], env)
    print('PASS: startup visibility, save availability, slot zero, one attempt, failed-load no retry.')


if __name__ == '__main__':
    main()
