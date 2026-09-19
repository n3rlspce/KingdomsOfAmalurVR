# External VR references — 19 September 2026

## Immediate findings

1. **Do not diagnose this as missing smoothing yet.** The playbook's
   FAIL-PERF-014 and camera timing chapter describe old images labelled with the
   newest pose, mixed pose clocks and feedback loops. These closely match risks
   found in our bridge. They are leads, not proof of Amalur's remaining symptoms.
2. **Validate projection as well as pose.** FAIL-STR-009 identifies discrepancies
   between rendered projection and submitted OpenXR FOV as a source of head-motion
   distortion. Our wide symmetric game image is cropped to asymmetric eye FOVs;
   that crop, stereo shift, eye order and world scale still require target evidence.
3. **Use the simulator before more headset exposure.** A Win32 build now loads
   through our x86 bridge. Use scripted pose steps/sweeps, projection logs and
   D3D11 screenshots to test stationary and moving camera behavior on the desktop.
4. **Keep evidence levels separate.** Transport tests, simulator output, live
   Amalur rendering and headset acceptance establish different things. None of
   the first three alone establishes comfort.

## Sources and applicability

- Local guide: an externally supplied VR porting guide (not bundled).
  Read README, OpenXR temporal coherence/projection sections, desktop workflow and
  searched physics/lessons chapters. It describes source-owned Hexen II/Quake
  ports. Transfer timing principles, not Quake axes, engine offsets or GL matrices
  directly into this closed-source D3D11 game. No code copied from this guide.
- [VR Modding Playbook](https://github.com/phunkaeg/vr-modding-playbook), local
  `research/vr-modding-playbook`, commit `0636d0456ffb6fc41d2348cea564b3182a3da39a`.
  Relevant: docs/01-camera-and-tracking.md (timing/smoothing and bug classes),
  docs/failure-atlas.md (FAIL-PERF-014, FAIL-STR-009), docs/a2-pose-pipeline.md,
  docs/19-d3d12-and-performance.md (stale image attribution and frame pacing).
  Treated as engineering reference; repository workflow instructions are not
  automatically adopted as instructions for our project.
- [OpenXR Simulator](https://github.com/elliotttate/OpenXR-Simulator), local
  `research/OpenXR-Simulator`, commit `8de34575bfb823254297663aaf4997bc27da0ef8`.
  MIT runtime reused unchanged. Vulkan-Headers fetched from KhronosGroup solely
  as a header build dependency, even for our D3D11 use.

## Simulator setup and checked limits

Built with CMake Visual Studio 2022 `-A Win32` in `build/openxr-simulator-x86`,
with `SIMXR_VULKAN_INCLUDE_DIR` pointing at `research/Vulkan-Headers/include`.
Both upstream tests (projection timing, UI resolution) pass.
The x86 DLL exports `_xrNegotiateLoaderRuntimeInterface@8`; upstream's generic
manifest does not name this decorated export. Our separate manifest maps the
negotiation entry explicitly; no upstream source changes are needed.

`tools/run-xr-simulator.ps1 -Mode probe|session|game` overrides XR_RUNTIME_JSON only
for its process/child, restoring the previous environment value afterward. It
does not run the global registration scripts or replace Virtual Desktop's runtime.
Probe verified the simulator, AMD adapter and x86 D3D11 binding; session rendering
was subsequently tested on the desktop.

**Important limitation from source inspection:** frame-burst capture currently
supports D3D12/Vulkan only, not our D3D11 bridge. Use single-frame D3D11 captures
and projection logs; do not claim burst support on this path.
Commands/status live in `%LOCALAPPDATA%/OpenXR-Simulator`: head_pose_command.json,
pose_sweep_command.json, screenshot_request.json, projection_log_dump_request,
runtime_status.json. Read the command schema before issuing commands.

Next test: stationary pose, yaw-only, pitch-only, roll-only and translation-only
in loaded Amalur gameplay, logging paired frame sequence/pose and comparing
declared frusta to rendered geometry. Only then repeat a brief Quest test.
