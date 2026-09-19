# Stereo frame/pose pairing — 19 September 2026

Testing showed constant micro-jitter even after reducing source
resolution to 4K and correcting transient pose-mutex contention. VR was stopped.
The previous bridge labelled an asynchronously acquired game image with the newest
predicted XR pose, which was not necessarily the pose used to render that image.

## Implemented

- Sample one physical pose for camera rebuilds in each game presentation interval.
- Preserve the last rendered camera pose when the engine reuses cached matrices.
- After geo-11 Present assembles its Katanga image, copy it on the underlying
  game D3D11 immediate context into our own keyed shared texture.
- Publish that texture's generation, sequence and rendered pose together under a
  CPU mutex. GPU ownership alternates producer key 0 / consumer key 1.
- The bridge copies a complete image into its private texture before returning GPU
  ownership. On contention it keeps the previous complete image AND previous pose.
- Projection-layer poses use the rendered head pose while preserving runtime
  eye-to-head transforms. No arbitrary head-motion smoothing is introduced.
- Restart/resize replaces the resource generation. A paused/dead consumer cannot
  block the game indefinitely. A source older than one second is not displayed.
- The game bridge requires the paired feed; it cannot silently fall back to the
  original unsynchronized Katanga feed. The latter remains available for capture
  and legacy shader/color tests only.

GPU protocol follows Microsoft's [AcquireSync documentation](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgikeyedmutex-acquiresync)
and [ReleaseSync documentation](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgikeyedmutex-releasesync).
AcquireSync must return S_OK, not merely pass SUCCEEDED: WAIT_TIMEOUT and
WAIT_ABANDONED are not ownership grants.

## Evidence and limits

`tools/test-stereo-transport.ps1` runs GPU tests outside the directory containing
our diagnostic D3D11 proxy. Two independent devices transferred 240 alternating
pixel patterns with matching pose tags; an unconsumed image could not be overwritten.
Expiry, pause recovery, tracking-invalid metadata and resource resize passed.
The eye-pose math test preserves a canted eye's orientation and 32 mm eye offset
across a 90-degree head rotation. Existing gamma/stereo routing and pose IPC tests
are included.

An initial live game capture-only run consumed 293 paired menu frames in five
seconds at 1280×720 per eye, without starting OpenXR. A later generated-camera
test found zero frames because the game had exited; this is not a passing camera
test. The current build is installed and the game relaunched, but tracked gameplay
pairing and headset comfort are still awaiting verification. The bridge remains off.

Remaining assumptions: the selected BHG camera is the displayed gameplay camera;
geo-11 packs the current render before its Present wrapper returns; all relevant
camera rebuilds occur in the presentation interval. These need validation in
gameplay, menus, cinematics and loading transitions. Stereo eye spacing remains
provisional. Passing transport tests does not prove comfortable VR or fix low FPS.

## User-proposed display-mode comparison
The user linked jitter onset to changing from borderless to windowed. Restored
personal.ini Display Fullscreen=2 from 0 with the same paired-frame build and
3840x2160 request. Native client resize reports 3840x2160, but Katanga capture
actually remains 5120x1440 SBS (2560x1440 per eye). This is NOT an isolated
same-resolution mode comparison: both mode and effective render load changed.
If improvement is reported, compare windowed at 2560x1440 next to separate them.
Evidence: captures/borderless-comparison.bmp. Quest outcome pending.
