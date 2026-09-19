# Kingdoms of Amalur VR

Experimental PCVR conversion of **Kingdoms of Amalur: Re-Reckoning**, targeting
Quest 3 through Virtual Desktop / VDXR. This is a development prototype, not a
validated playable release.

The immediate target is **third-person stereo and 6DoF head tracking**, using
normal gamepad or keyboard controls. First-person and motion controls are future
work. See the [v0.1 plan](V0_1_PLAN.md) and [conversion roadmap](VR_CONVERSION_PLAN.md).

## Build and dependencies

Follow [repository setup](REPOSITORY_SETUP.md) for checkout, x86 build tools,
OpenXR.Loader and external stereo dependencies. The game, downloaded mod binaries,
saves, captures and workstation configuration are not included.

```powershell
.\tools\build-diagnostic.ps1
.\tools\build-xr-smoke.ps1 -OpenXrSdk 'C:\path\to\OpenXR.Loader'
.\tools\run-xr-smoke.ps1 -Mode probe
```

Builds require Visual Studio C++ x86 tools, CMake and a Windows SDK. Install
scripts require `-GameDirectory` or `AMALUR_GAME_DIR`; the OpenXR build accepts
`-OpenXrSdk` or `OPENXR_SDK_DIR`. Close the game before installation or removal.

## Current status

- The x86 D3D11/OpenXR smoke test has rendered stereo images and exercised
  controller poses and haptics.
- geo-11, existing Amalur stereo shaders and a native camera adapter have
  delivered stereo gameplay and head tracking to Quest.
- Shared-texture transport associates rendered imagery with camera metadata;
  isolated transport tests pass. Headset comfort and timing remain unverified.
- HUD curvature uses minimap draw presence as an experimental gate. Complete
  menu isolation and the latest background-layer correction remain unverified.
- Stereo calibration, source resolution, performance and lifecycle handling
  still need work. Existing reports include jitter and incorrect UI placement.

Research records describe individual experiments, not guaranteed capabilities.
See [camera discovery](research/CAMERA_DISCOVERY.md),
[stereo integration](research/STEREO_REUSE.md),
[frame pairing](research/FRAME_PAIRING.md) and
[HUD work](research/CURVED_GAME_HUD.md).

## Development controls

| Key | Action |
| --- | --- |
| `*` | Toggle the translucent settings panel |
| Arrow keys | Adjust panel settings; intercepted while the game is focused |
| Ctrl+Home | Reset selected setting |
| Ctrl+R / F7 | Recenter tracking |
| F10 | Toggle native head tracking |
| F11 | Reload geo-11 configuration |
| F6 | Toggle experimental auto-gated HUD curvature |
| F4 | Cycle experimental HUD disparity |
| F8 | Capture diagnostic samples |
| F9 | Toggle desktop FOV probe |
| F12 | Stop the OpenXR bridge |

Depth/convergence controls still require backend validation. HUD disparity is
not a calibrated physical distance. Rendering a stereo frame does not establish
correct scale, comfortable tracking or full game coverage.
