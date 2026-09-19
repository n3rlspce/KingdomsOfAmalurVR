# Amalur VR v0.1 — third-person 6DoF

## Concise plan

Keep Amalur's third-person character and normal controls. Add real stereo rendering,
6DoF head tracking, recentering, and usable menus on Quest 3 through Virtual Desktop.
Reuse existing OpenXR/D3D11 components; write the Amalur-specific camera/render adapter.
First person and motion controls move to a later release.

## What v0.1 includes

- Distinct left/right views with correct eye separation, projection and depth.
- Head rotation and physical translation relative to the third-person camera rig.
  Leaning changes perspective; it does not move the character or orbit around them.
- Original gamepad/keyboard/mouse movement, camera orbit, aiming, attacks and interactions.
  Game aiming remains tied to the original controls; no gaze aiming in this release.
- Recenter for seated or standing play, with calibrated world scale.
- Readable HUD and menus operated with existing controls. Menus/cinematics may use
  a comfortable virtual panel where necessary; playable world rendering remains stereo.
- Tracking-loss handling, basic camera comfort (suppress camera shake where possible),
  and reversible installation for the verified Steam build.

No tracked hands, controller gestures, physical weapons, first-person body changes,
or combat-system rewrites. Quest controller-to-gamepad emulation is also deferred;
use a conventional gamepad or keyboard/mouse.

## Implementation order

1. **Reuse audit and runtime integration.** Extract the useful D3D11/OpenXR session,
   pose, swapchain and submission components from MGS5VR, preserving MIT attribution.
   Reuse the existing x86 OpenXR setup and MinHook. Keep controls/IK/combat code out.
2. **Tracked third-person camera.** Use the verified native camera rebuild hook to
   apply calibrated head rotation/translation around the game's camera position.
   Combine gamepad orbit and recentered headset pose without accumulating drift.
   The native eye/look-at representation must update consistently with matrices/culling.
3. **Stereo feasibility gate.** Find the render-only boundary and render both eyes
   from one simulation state. Resolve eye targets, asymmetric projection, visibility,
   lighting, shadows and screen-space effects. Hooking Present alone is insufficient.
4. **Playable alpha.** Implement HUD/menu treatment, recenter, tracking recovery,
   cutscene handling and basic comfort. Check load/save/death and representative combat.
5. **Package v0.1.** Record tested resolution/refresh/performance on D3D11-capable AMD GPU,
   publish known limitations and provide install/remove instructions. Target 72 Hz
   initially; measured results determine settings, not an assumed guarantee.

## Reuse decisions

| Source | Use | Constraint |
| --- | --- | --- |
| MGS5VR, commit e20d92223a0a6dfccd8d75eac9d4c7841e635b75 | D3D11/OpenXR runtime and stereo transaction patterns | MIT license confirmed locally. Runtime currently imports many game-specific modules; extract a minimal subset. Its FOX projection helper assumes negative X scale, unlike the positive X scale observed in Amalur; do not copy that conversion unchanged. |
| Recovered Witcher 2 project | Existing x86 OpenXR dependencies and integration lessons | D3D9/game-specific code is not the Amalur render adapter. |
| MinHook v1.3.4 | Existing native/D3D11 hooks | Already compiled and working in-game; retain license. |
| VRFramework | Selected architecture/reference code | Inspected D3D11 hook is unfinished. |
| OutwardVR | Future motion-combat reference | Not needed for v0.1. |

MGS5VR's `runTheatre` is explicitly a 2D theatre presenter. It must not be shipped
as a substitute for v0.1's stereo world. Runtime reuse has been inspected, not yet ported.

## Current evidence and release gate

**Stereo update:** geo-11 and mikear69's Amalur shaders now provide experimental
stereo. Both DLLs load, the shared 5120×1440 full-SBS texture is readable, and an
ADB capture shows Amalur gameplay in both Quest eyes with head tracking active.
Depth/alignment, timing and comfort remain unvalidated. See
[stereo integration](research/STEREO_REUSE.md) for limitations and provenance.

Passed: standalone x86 OpenXR headset test; in-game D3D11 interception; gameplay
matrix capture; native camera F9 zoom toggle confirmed visually and in logs.

Implemented and initially confirmed by the user: F10 headset-driven camera with F7
recenter, shared pose bridge, and reused MGS5VR pose math. Automated math/channel
checks and a live Quest pose read passed; world scale remains provisional.
Testing showed lingering roll. A heading-only recenter correction passes its
regression test and awaits installation/live verification.

Pending: roll correction validation, same-state stereo world rendering, VR UI,
comfort/performance and playable-session validation. The experimental stereo
prototype is not a validated v0.1 release yet.

Call it v0.1 only when the user can load a scene, see actual stereo depth, turn and
lean naturally, play using normal controls, recenter, open menus and exit cleanly.
Keep campaign/DLC coverage explicit; a working alpha does not imply a full playthrough.

The original full-conversion roadmap remains the long-term backlog. Its 22–43 week
estimate does not apply directly to this smaller scope; estimate v0.1 after the
stereo feasibility gate, which remains the largest unknown.
