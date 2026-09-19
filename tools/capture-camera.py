"""Read-only camera/pose telemetry; never focuses, pauses or controls the game."""
import argparse
import ctypes as c
import json
import math
from pathlib import Path
import statistics
import struct
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--seconds', type=float, default=10)
parser.add_argument('--output', default='captures/camera-timing.jsonl')
args = parser.parse_args()
if not 0 < args.seconds <= 600:
    parser.error('--seconds must be between 0 and 600')
k = c.WinDLL('kernel32', use_last_error=True)
k.OpenFileMappingW.argtypes = [c.c_uint, c.c_int, c.c_wchar_p]
k.OpenFileMappingW.restype = c.c_void_p
k.OpenMutexW.argtypes = [c.c_uint, c.c_int, c.c_wchar_p]
k.OpenMutexW.restype = c.c_void_p
k.MapViewOfFile.argtypes = [c.c_void_p, c.c_uint, c.c_uint, c.c_uint, c.c_size_t]
k.MapViewOfFile.restype = c.c_void_p
k.WaitForSingleObject.argtypes = [c.c_void_p, c.c_uint]
k.ReleaseMutex.argtypes = [c.c_void_p]
k.UnmapViewOfFile.argtypes = [c.c_void_p]
k.CloseHandle.argtypes = [c.c_void_p]
k.GetTickCount64.restype = c.c_ulonglong


class Channel:
    def __init__(self, name, lock, size):
        self.size = size
        self.name, self.lock = name, lock
        self.mapping = self.mutex = self.memory = None
        self.retry = 0

    def read(self):
        if (not self.memory or not self.mutex) and time.monotonic() >= self.retry:
            self.retry = time.monotonic() + 1
            if not self.mapping:
                self.mapping = k.OpenFileMappingW(4, 0, 'Local\\' + self.name)
            if not self.mutex:
                self.mutex = k.OpenMutexW(0x100001, 0, 'Local\\' + self.lock)
            if self.mapping and not self.memory:
                self.memory = k.MapViewOfFile(self.mapping, 4, 0, 0, self.size)
        if not self.memory or not self.mutex:
            return None
        if k.WaitForSingleObject(self.mutex, 0) not in (0, 0x80):
            return None
        try:
            return c.string_at(self.memory, self.size)
        finally:
            k.ReleaseMutex(self.mutex)

    def close(self):
        if self.memory:
            k.UnmapViewOfFile(self.memory)
        for handle in (self.mapping, self.mutex):
            if handle:
                k.CloseHandle(handle)


channels = [
    Channel('AmalurVRPoseV3', 'AmalurVRPoseMutexV3', 80),
    Channel('AmalurVRFrameV3', 'AmalurVRFrameMutexV3', 80),
    Channel('AmalurRigStatusV1', 'AmalurRigStatusMutexV1', 92),
    Channel('AmalurStereoFrameV1', 'AmalurStereoFrameMutexV1', 120),
    Channel('AmalurCameraStatusV2', 'AmalurCameraStatusMutexV2', 184),
]
rows = []
output = Path(args.output)
output.parent.mkdir(parents=True, exist_ok=True)
stream = output.open('w', buffering=1)
try:
    until = time.monotonic() + args.seconds
    while time.monotonic() < until:
        source, frame, rig, stereo, camera = [channel.read() for channel in channels]
        if source and frame and rig:
            row = dict(tick=k.GetTickCount64(), sourceTick=struct.unpack_from('<Q', source, 8)[0],
                       cameraPoseTick=struct.unpack_from('<Q', frame, 8)[0],
                       paused=struct.unpack_from('<i', rig, 48)[0],
                       focused=struct.unpack_from('<I', rig, 24)[0],
                       rigTick=struct.unpack_from('<I', rig, 8)[0])
            if ((row['tick'] & 0xffffffff) - row['rigTick']) & 0xffffffff > 1000:
                time.sleep(.01)
                continue
            if stereo:
                row.update(sequence=struct.unpack_from('<Q', stereo, 16)[0],
                           published=struct.unpack_from('<Q', stereo, 24)[0],
                           pairedPoseTick=struct.unpack_from('<Q', stereo, 48)[0])
            if camera and struct.unpack_from('<I', camera)[0] == 2:
                row.update(rebuild=struct.unpack_from('<I', camera, 12)[0],
                           cameraFrame=struct.unpack_from('<I', camera, 8)[0],
                           cameraSampled=struct.unpack_from('<Q', camera, 16)[0],
                           appliedPoseTick=struct.unpack_from('<Q', camera, 24)[0],
                           tracked=struct.unpack_from('<I', camera, 32)[0],
                           playerValid=struct.unpack_from('<I', camera, 36)[0],
                           head=struct.unpack_from('<3f', camera, 40),
                           headOrientation=struct.unpack_from('<4f', camera, 52),
                           player=struct.unpack_from('<3f', camera, 68),
                           nativeEye=struct.unpack_from('<3f', camera, 80),
                           renderedEye=struct.unpack_from('<3f', camera, 92),
                           renderedForward=struct.unpack_from('<3f', camera, 104),
                           nativeTarget=struct.unpack_from('<3f', camera, 116),
                           cachedRight=struct.unpack_from('<3f', camera, 128),
                           cachedUp=struct.unpack_from('<3f', camera, 140),
                           cachedForward=struct.unpack_from('<3f', camera, 152),
                           projection=struct.unpack_from('<4f', camera, 164))
            rows.append(row)
            stream.write(json.dumps(row) + '\n')
        time.sleep(.01)
finally:
    stream.close()
    for channel in channels:
        channel.close()
for state in sorted({(row['paused'], row['focused']) for row in rows}):
    group = [row for row in rows if (row['paused'], row['focused']) == state]
    ages = [row['tick'] - row['cameraPoseTick'] for row in group]
    summary = dict(paused=state[0], focused=state[1], samples=len(group),
                   poseAgeMedianMs=statistics.median(ages), poseAgeMaxMs=max(ages),
                   distinctCameraPoses=len({row['cameraPoseTick'] for row in group}))
    errors = []
    for row in group:
        if not row.get('tracked') or 'cachedForward' not in row:
            continue
        a, b = row['cachedForward'], row['renderedForward']
        length = math.sqrt(sum(x*x for x in a) * sum(x*x for x in b))
        if length > 1e-9:
            errors.append(math.degrees(math.acos(max(-1, min(1, sum(x*y for x, y in zip(a, b))/length)))))
    if errors:
        summary.update(cachedForwardErrorMedianDegrees=statistics.median(errors),
                       cachedForwardErrorMaxDegrees=max(errors))
    print(json.dumps(summary))
print(f'Saved {len(rows)} read-only samples to {output}')
