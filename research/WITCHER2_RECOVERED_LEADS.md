# VR porting reference projects

Public repository leads and engineering lessons for the Amalur adapter.

## Recovered project leads

| Project | Relevant implementation area | Relevance to Amalur |
| --- | --- | --- |
| [Mirror's Edge VR](https://github.com/letsgosportsteam/mirrors-edge-vr-mod) | D3D9 interception, duplicate-eye draws, matrix correction, restoration of ordinary rendering state, stereo evidence checks | Study camera/render hook timing and stereo verification. D3D9 implementation cannot simply replace a D3D11 adapter. Current upstream describes itself as pre-alpha. |
| [Call of Duty 4 VR](https://github.com/jplakon/CallOfDuty4_VR) | Predicted render-pose identity, same-frame views and render metadata | Strong lead for keeping submitted images, poses and frame IDs consistent. Game-specific renderer hooks remain nonportable. |
| [Witcher 3 VR](https://github.com/tig3rmast3r/witcher3-vr) | Asymmetric/canted eye geometry, OpenXR lifecycle, proxy structure and diagnostics | Study eye geometry and runtime separation; do not transfer REDengine offsets or D3D12 assumptions to Amalur. |
| [wiz3D](https://github.com/effcol/wiz3D) | Existing D3D9 shader analysis and stereo draw duplication, with a custom OpenXR bridge | Useful lower-level stereo research if engine rendering hooks fail. D3D9 implementations need a separate D3D11 adaptation. Reassess backend support and license obligations before any reuse. |
| [HaloCEVR](https://github.com/LivingFray/HaloCEVR) | D3D9 proxy reference | Secondary legacy-engine port reference, not a ready Amalur backend. |
| [6DoF Head Tracking Mods Hub](https://github.com/BerZerker96/6DOF-Head-Tracking-Mods-Hub) | Witcher 2 camera hook receiving OpenTrack-format UDP poses | Historical camera-research lead; the notes describe its Witcher release as tracking-only, not stereo. |
| [Osiris VR Viewer](https://github.com/BerZerker96/Osiris-Vr-Viewer) | External OpenXR viewer for capture/transport validation | Optional independent streaming/runtime baseline; a viewer does not satisfy full conversion. |

The first four repository pages were reopened during recovery and remain accessible. The last three entries are preserved from the historical local notes; their present capabilities were not revalidated here.

## Exact source anchors worth revisiting

The recovered port map points to:

- Mirror's Edge `src/d3d9.cpp`: `Hook_SetVSConstF` / `BuildEyeMatrix` at `b44efd8eb06f18d8d3582d82eeb18b23ce554b23`; same-frame `DuplicateDraw` at `20fac32d7506c71af86a46ed1ce5a5383b1e9b90`.
- COD4 `src/vr/vr_openxr.cpp` and `src/cgame/cg_draw.cpp`: render-pose publication and cloned same-frame views at `d8b20cac3f086fc519cbafde662f87263d367ba0`.
- Witcher 3 `src/openxr_eye_geometry.h` and `.cpp`: per-eye geometry at `ecd75b68e6caa534571e82f695a29675fe58f27f`.

These anchors were recovered from the local port map, not individually audited again in this task. Read their implementations and licenses before adapting code.

## Changes to the Amalur approach

1. Keep VRFramework as an architecture reference while comparing these concrete implementations for the missing runtime/render pieces.
2. Introduce a frame record containing predicted display time, both eye poses/FOVs, and a monotonically increasing frame ID. Carry identity alongside the rendered surfaces through submission, rather than relying solely on global counters.
3. Validate actual geometry stereo: duplicate eye images or one global horizontal shift must fail. Depth-varying disparity is supporting evidence, supplemented by scene coverage checks and headset testing.
4. Separate GPU projection correction from CPU culling. Changing a shader matrix cannot recover geometry the engine already discarded.
5. Measure scale and coordinate conventions. No default world scale should be assumed calibrated.
6. Test clearing/restoring stereo state through menus, focus loss, device reset, failed frames, and unloading.
7. Do not count an offline build, accepted OpenXR submission, or working transport as proof of comfortable, complete VR.

No historical game offsets, wrapper DLLs, or untested camera assumptions should be copied into Amalur. Select implementation reuse only after measuring Amalur's executable and renderer.
