# Native camera discovery — Steam build 10619381

## Evidence

Gameplay F8 capture: [amalur-gameplay-capture.log](amalur-gameplay-capture.log).
Read-only RTTI scan: [camera-rtti.json](camera-rtti.json).
Live read-only candidate snapshot: [live-camera-candidates.json](live-camera-candidates.json).
Runtime heap addresses are session-specific and must never be hard-coded.

The gameplay capture exposes two distinct 256-byte buffers: a matrix buffer and
an unrelated material buffer. Byte width alone is insufficient to identify camera
data. Relevant matrix upload return RVAs are `0x009013ce` / `0x00901458` and
`0x00896677` / `0x008966c5` (Map / Unmap respectively).

RTTI identifies `BHG::Camera`, whose vtable RVA is `0x01335d08`. Its embedded
camera data starts at object +8. A live instance contained valid matrices, while
three other candidates contained default/unused state. The candidate scan reads
committed writable private memory using QUERY_INFORMATION and VM_READ only.

## Observed layout (relative to BHG::Camera object)

| Offset | Interpretation and evidence |
| --- | --- |
| `0x08` | Projection type; observed 1 for perspective |
| `0x0c` | Eye position XYZ; matches eye recovered from inverse view |
| `0x1c` | Look-at target XYZ candidate; separate from eye, approximately 200 units away |
| `0x2c` | Near distance, observed 26.1862 |
| `0x30` | Far distance, observed 524288 |
| `0x34` | FOV parameter in degrees, observed 87.1111; consumed by projection setup |
| `0x4c` | View matrix, row-vector convention |
| `0x8c` | Related view representation; not yet assigned definitive semantics |
| `0xcc` | Projection matrix |
| `0x10c` | View-projection matrix |
| `0x14c` | Previous view-projection/cache candidate |
| `0x366` | Flags; bit 0 requests a matrix rebuild, bit 2 controls previous matrix history |

In the captured instance, multiplying view by projection reproduces the cached VP
with maximum absolute error 0.000805 (translation entries are around 25,000).
Eye recovered from inverse view matches +0x0c to about 0.0003 world units.
These numerical checks strongly support the view/projection/eye interpretation.
World-units-per-meter and gameplay coordinate mapping are not calibrated yet.

## Native routine

`0x008e1c80`: camera-data rebuild, x86 thiscall, no explicit arguments, void return.
Its ECX points at the embedded data (`BHG::Camera +8`), not the owning object.
It checks dirty bit 0 at core+0x35e, updates projection/view/frustum helpers,
maintains prior matrices, and clears the dirty flag. Projection setup helper
`0x0085ec80` reads the FOV at core+0x2c and viewport dimensions.

## First camera-control experiment

The installed diagnostic validates the routine's prologue, then hooks it. Default
behavior forwards unchanged. **F9** toggles a 15% reduction of the FOV parameter
for the first perspective BHG::Camera with FOV between 50 and 120 degrees. It uses
the native rebuild path and restores the original scalar afterward; F9 off forces
a rebuild back to normal. No game executable or saved setting is patched.

This selector is intentionally temporary: camera replacement/load transitions,
concurrent use, portrait views and alternate cameras are not validated. The pointer
selection is fixed for the process lifetime, so restart if the camera is replaced.
This is a desktop proof of camera control, not stereo, first person, or head tracking.
Build and native hook installation succeeded. Testing confirmed clean F9 zoom toggling. Runtime projection scales changed from (1.05174, 1.86975) to (1.32598, 2.35729), then returned to the original values repeatedly. Evidence: `amalur-camera-control.log`. This validates the initial native camera-control experiment.

Next: apply tracked eye/target offsets before the native rebuild.
Separate eye rendering from simulation before claiming stereo. Shader-only shifts
would leave culling/depth/lighting problems unresolved.

