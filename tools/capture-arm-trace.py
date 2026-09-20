"""Read-only, bounded capture of same-call arm diagnostics. No game inputs."""
import argparse
import ctypes as c
import json
import math
from pathlib import Path
import struct
import time

HEADER = struct.Struct('<6I')
RECORD = struct.Struct('<8I6Q105f')
POSES = ['rootWorld', 'previousObjectWorld', 'headAnchor', 'rawRight', 'rawLeft',
         'targetRight', 'targetLeft', 'solvedRight', 'solvedLeft',
         'remappedRight', 'remappedLeft', 'shoulderRight', 'shoulderLeft', 'elbowRight', 'elbowLeft']


def decode(data):
    values = RECORD.unpack(data)
    result = dict(zip(['sequence', 'frame', 'stage', 'flags', 'root', 'object', 'owner', 'slot',
                       'tick', 'rightTick', 'leftTick', 'headTick', 'rawRightTick', 'rawLeftTick'], values[:14]))
    for i, name in enumerate(POSES):
        pose = values[14 + i * 7:21 + i * 7]
        result[name] = {'q': pose[:4], 'p': pose[4:]}
    return result


def summarize(rows):
    errors = {'target_to_solved': [], 'solved_to_remapped': []}
    failed = {}
    objects = set()
    previous = {}
    residual_steps = []
    within_frame_changes = 0
    for row in rows:
        objects.add(row['object'])
        if not row['flags'] & 8:
            key = str(row['stage'])
            failed[key] = failed.get(key, 0) + 1
            continue
        for side, fresh, mapped in [('Right', 1, 16), ('Left', 2, 32)]:
            if not row['flags'] & fresh:
                continue
            errors['target_to_solved'].append(math.dist(row['target' + side]['p'], row['solved' + side]['p']))
            if row['flags'] & mapped:
                errors['solved_to_remapped'].append(math.dist(row['solved' + side]['p'], row['remapped' + side]['p']))
                key = (row['object'], side)
                residual = [a - b for a, b in zip(row['remapped' + side]['p'], row['target' + side]['p'])]
                if key in previous:
                    old_frame, old_target, old_residual = previous[key]
                    residual_steps.append(math.dist(residual, old_residual))
                    if row['frame'] == old_frame and math.dist(row['target' + side]['p'], old_target) > .001:
                        within_frame_changes += 1
                previous[key] = (row['frame'], row['target' + side]['p'], residual)
    return {'records': len(rows), 'objects': len(objects), 'failed_stages': failed,
            'within_frame_target_changes': within_frame_changes,
            'max_target_to_mesh_residual_step_game_units': max(residual_steps, default=0),
            'position_errors_game_units': {key: {'samples': len(vals), 'max': max(vals, default=0),
                'mean': sum(vals) / max(1, len(vals))} for key, vals in errors.items()}}


def self_test():
    poses = []
    for i in range(15):
        poses.extend([0, 0, 0, 1, float(i), 0, 0])
    row = decode(RECORD.pack(1, 2, 8, 1 | 8 | 16, 3, 4, 5, 7, 1000, 999, 998, 997, 1001, 1002, *poses))
    assert RECORD.size == 500 and row['rightTick'] == 999 and row['rawRightTick'] == 1001
    assert row['targetRight']['p'] == (5, 0, 0) and row['elbowLeft']['p'] == (14, 0, 0)
    result = summarize([row])
    assert result['position_errors_game_units']['target_to_solved']['max'] == 2
    assert result['position_errors_game_units']['solved_to_remapped']['max'] == 2
    row['flags'] = 1
    assert summarize([row])['failed_stages'] == {'8': 1}
    print('PASS: 500-byte trace schema, pose ordering, stage and error analysis')


def capture(seconds, output):
    kernel = c.WinDLL('kernel32', use_last_error=True)
    kernel.OpenFileMappingW.argtypes = [c.c_uint, c.c_int, c.c_wchar_p]
    kernel.OpenFileMappingW.restype = c.c_void_p
    kernel.OpenMutexW.argtypes = [c.c_uint, c.c_int, c.c_wchar_p]
    kernel.OpenMutexW.restype = c.c_void_p
    kernel.MapViewOfFile.argtypes = [c.c_void_p, c.c_uint, c.c_uint, c.c_uint, c.c_size_t]
    kernel.MapViewOfFile.restype = c.c_void_p
    kernel.WaitForSingleObject.argtypes = [c.c_void_p, c.c_uint]
    kernel.ReleaseMutex.argtypes = [c.c_void_p]
    kernel.UnmapViewOfFile.argtypes = [c.c_void_p]
    kernel.CloseHandle.argtypes = [c.c_void_p]
    mapping = kernel.OpenFileMappingW(4, False, 'Local\\AmalurArmTraceV1')
    mutex = kernel.OpenMutexW(0x100001, False, 'Local\\AmalurArmTraceMutexV1')
    memory = kernel.MapViewOfFile(mapping, 4, 0, 0, 0) if mapping else None
    rows, previous, missed = [], 0, 0
    header = None
    try:
        if not memory or not mutex:
            raise RuntimeError('No live arm trace. The diagnostic DLL must be running in gameplay.')
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            chunks = []
            if kernel.WaitForSingleObject(mutex, 0) in (0, 128):
                try:
                    header = HEADER.unpack(c.string_at(memory, HEADER.size))
                    version, pid, capacity, stride, published, dropped = header
                    if (version, capacity, stride) != (1, 2048, RECORD.size):
                        raise RuntimeError('Unexpected trace schema')
                    if previous == 0:
                        previous = published  # Start now, not with old ring contents.
                    begin = max(previous + 1, published - capacity + 1)
                    missed += max(0, begin - previous - 1)
                    for sequence in range(begin, published + 1):
                        chunk = c.string_at(memory + HEADER.size + ((sequence - 1) % capacity) * stride, stride)
                        if struct.unpack_from('<I', chunk)[0] == sequence:
                            chunks.append(chunk)
                    previous = published
                finally:
                    kernel.ReleaseMutex(mutex)
            rows.extend(decode(chunk) for chunk in chunks)
            time.sleep(.02)
    finally:
        if memory:
            kernel.UnmapViewOfFile(memory)
        if mapping:
            kernel.CloseHandle(mapping)
        if mutex:
            kernel.CloseHandle(mutex)
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open('w', encoding='utf-8') as stream:
        stream.write(json.dumps({'header': header, 'reader_missed': missed}) + '\n')
        for row in rows:
            stream.write(json.dumps(row) + '\n')
    print(json.dumps(summarize(rows), indent=2))
    print(f'Capture: {output.resolve()}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seconds', type=float, default=15)
    parser.add_argument('--output', type=Path, default=Path('build/arm-trace.jsonl'))
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    if args.self_test:
        self_test()
    elif not 0 < args.seconds <= 60:
        parser.error('--seconds must be between 0 and 60')
    else:
        capture(args.seconds, args.output)
