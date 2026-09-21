"""Bounded upload/draw capture; writes only diagnostic enable control, no game inputs.

Output is a 96-byte header followed by consecutive 4620-byte records (not a ring).
CPU pose in each record is the latest remap, NOT a proven same-object association.
"""
import argparse
import collections
import ctypes as c
import hashlib
import json
from pathlib import Path
import struct
import time

HEADER = struct.Struct('<6IQ16I')
PREFIX = struct.Struct('<8I3Q')
STRIDE = 4620
CAPACITY = 128
CPU_OFFSET = PREFIX.size
WORLD_OFFSET = CPU_OFFSET + 500
SKIN_OFFSET = WORLD_OFFSET + 256


def summarize(records):
    groups = collections.defaultdict(lambda: {'draws': 0, 'camera_matches': 0,
        'near_root_draws': 0, 'fresh_cpu_draws': 0, 'world_age_max_frames': 0, 'skin_age_max_frames': 0,
        'consecutive_frame_pairs': 0, 'unchanged_palette_pairs': 0})
    previous = {}
    for raw in records:
        frame, wf, sf, vb, ib, stride, count, flags, tick, camera_tick, serial = PREFIX.unpack_from(raw)
        key = f'{vb:08x}/{ib:08x}/{stride}/{count}'
        group = groups[key]
        group['draws'] += 1
        group['near_root_draws'] += bool(flags & 2)
        group['fresh_cpu_draws'] += bool(flags & 4)
        group['camera_matches'] += bool(flags & 1)
        group['world_age_max_frames'] = max(group['world_age_max_frames'], (frame-wf) & 0xffffffff)
        group['skin_age_max_frames'] = max(group['skin_age_max_frames'], (frame-sf) & 0xffffffff)
        digest = hashlib.sha256(raw[SKIN_OFFSET:SKIN_OFFSET+3744]).digest()
        old = previous.get(key)
        if old and ((frame-old[0]) & 0xffffffff) == 1:
            group['consecutive_frame_pairs'] += 1
            group['unchanged_palette_pairs'] += digest == old[1]
        previous[key] = frame, digest
    return {'records': len(records), 'candidate_draw_groups': dict(groups),
        'limitations': 'All captured skinned draws are candidates, not proven player draws. Proximity is tagged, not filtered. CPU pose is latest remap, not matched to this draw. Unchanged palettes can be valid when still. Draw cap is 24 candidates/frame. Counter deltas expose rejected draws and cache eviction. Capture overhead may affect timing.'}


def self_test():
    assert HEADER.size == 96 and PREFIX.size == 56
    assert SKIN_OFFSET + 3744 + 64 == STRIDE
    raw = bytearray(STRIDE)
    PREFIX.pack_into(raw, 0, 10, 9, 8, 1, 2, 32, 100, 1, 1000, 990, 1)
    next_raw = bytearray(raw)
    struct.pack_into('<I', next_raw, 0, 11)
    result = summarize([raw, next_raw])['candidate_draw_groups']['00000001/00000002/32/100']
    assert result['skin_age_max_frames'] == 3
    assert result['camera_matches'] == 2 and result['unchanged_palette_pairs'] == 1
    next_raw[SKIN_OFFSET] = 1
    assert summarize([raw, next_raw])['candidate_draw_groups']['00000001/00000002/32/100']['unchanged_palette_pairs'] == 0
    tagged = bytearray(raw)
    struct.pack_into('<I', tagged, 28, 6)  # near root and fresh CPU, no camera match
    group = summarize([tagged])['candidate_draw_groups']['00000001/00000002/32/100']
    assert group['near_root_draws'] == 1 and group['fresh_cpu_draws'] == 1 and group['camera_matches'] == 0
    far = summarize([raw])['candidate_draw_groups']['00000001/00000002/32/100']
    assert far['draws'] == 1 and far['near_root_draws'] == 0  # far draws remain analyzable
    header = HEADER.pack(2, 42, STRIDE, CAPACITY, 1, 0, 12345, *range(16))
    assert struct.unpack_from('<Q', header, 24)[0] == 12345 and HEADER.unpack(header)[7:] == tuple(range(16))
    print('PASS: V2 header/control offset, counters, proximity tags, far-draw retention, upload ages and palette changes')


def capture(seconds, output):
    kernel = c.WinDLL('kernel32', use_last_error=True)
    for name, args, result in [
        ('OpenFileMappingW', [c.c_uint, c.c_int, c.c_wchar_p], c.c_void_p),
        ('OpenMutexW', [c.c_uint, c.c_int, c.c_wchar_p], c.c_void_p),
        ('MapViewOfFile', [c.c_void_p, c.c_uint, c.c_uint, c.c_uint, c.c_size_t], c.c_void_p),
        ('WaitForSingleObject', [c.c_void_p, c.c_uint], c.c_uint),
        ('ReleaseMutex', [c.c_void_p], c.c_int),
        ('UnmapViewOfFile', [c.c_void_p], c.c_int),
        ('CloseHandle', [c.c_void_p], c.c_int),
        ('GetTickCount64', [], c.c_uint64)]:
        fn = getattr(kernel, name)
        fn.argtypes, fn.restype = args, result
    mapping = kernel.OpenFileMappingW(6, False, 'Local\\AmalurSkinTraceV2')
    mutex = kernel.OpenMutexW(0x100001, False, 'Local\\AmalurSkinTraceMutexV2')
    memory = kernel.MapViewOfFile(mapping, 6, 0, 0, HEADER.size+CAPACITY*STRIDE) if mapping else None
    records, missed, enabled = [], 0, False
    try:
        if not memory or not mutex:
            raise RuntimeError('No live skin trace. Install body-021 on the next authorized game launch.')
        if kernel.WaitForSingleObject(mutex, 1000) not in (0, 128):
            raise RuntimeError('Skin trace mutex unavailable')
        try:
            header = HEADER.unpack(c.string_at(memory, HEADER.size))
            version, pid, stride, capacity, previous, dropped_start, until = header[:7]
            counters_start = header[7:]
            if (version, stride, capacity) != (2, STRIDE, CAPACITY):
                raise RuntimeError(f'Unsupported trace schema: {header}')
            current = header
            now = kernel.GetTickCount64()
            if until > now:
                raise RuntimeError('Another skin capture is active')
            c.c_uint64.from_address(memory+24).value = now + int(seconds*1000) + 500
            enabled = True
        finally:
            kernel.ReleaseMutex(mutex)
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline and len(records) < 12000:
            chunks = []
            if kernel.WaitForSingleObject(mutex, 0) in (0, 128):
                try:
                    current = HEADER.unpack(c.string_at(memory, HEADER.size))
                    if current[1] != pid:
                        raise RuntimeError('Game process changed during capture')
                    published = current[4]
                    delta = (published-previous) & 0xffffffff
                    missed += max(0, delta-CAPACITY)
                    for age in range(min(delta, CAPACITY)-1, -1, -1):
                        index = ((published-1-age) & 0xffffffff) % CAPACITY
                        chunks.append(c.string_at(memory+HEADER.size+index*STRIDE, STRIDE))
                    previous = published
                finally:
                    kernel.ReleaseMutex(mutex)
            records.extend(chunks)
            time.sleep(.02)
    finally:
        if enabled and kernel.WaitForSingleObject(mutex, 1000) in (0, 128):
            c.c_uint64.from_address(memory+24).value = 0
            kernel.ReleaseMutex(mutex)
        if memory:
            kernel.UnmapViewOfFile(memory)
        if mapping:
            kernel.CloseHandle(mapping)
        if mutex:
            kernel.CloseHandle(mutex)
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open('wb') as stream:
        stream.write(HEADER.pack(2, pid, STRIDE, len(records), len(records), missed, 0, *current[7:]))
        stream.writelines(records)
    result = summarize(records)
    result['capture_counters'] = dict(zip(('draws','deferred','missing_world','missing_skin','no_cpu','stale_cpu','far_world','published','evicted','world_uploads','skin_uploads','pending_full','draw_cap','read_miss','reserved14','reserved15'), ((b-a)&0xffffffff for a,b in zip(counters_start,current[7:]))))
    result.update(pid=pid, missed_ring_records=missed, writer_drops=(current[5]-dropped_start) & 0xffffffff)
    output.with_suffix('.json').write_text(json.dumps(result, indent=2))
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seconds', type=float, default=10)
    parser.add_argument('--output', type=Path, default=Path('skin-trace.bin'))
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    if args.self_test:
        self_test()
    elif not 0.1 <= args.seconds <= 59:
        parser.error('--seconds must be between 0.1 and 59')
    else:
        capture(args.seconds, args.output)
