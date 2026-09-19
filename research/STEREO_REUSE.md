# Experimental stereo integration — 19 September 2026

## Result

geo-11 + the native Amalur camera adapter are installed and both DLLs load.
The x86 OpenXR bridge reads a live **5120×1440 full-SBS** shared surface.
ADB captured **Amalur gameplay in both Quest eyes**, at 4128×2208, with native
head tracking active in the game log. Capture: `captures/quest-stereo-menu.png`
(the filename predates the gameplay appearing). This proves end-to-end image
transport; depth, alignment, comfort and game coverage are not validated.

Testing showed over-under on the PC monitor. Direct capture verified that the
separate Katanga surface is full SBS, so the presenter keeps its left/right split.

## Reused components

- [mikear69's Re-Reckoning stereo fix](https://www.mtbs3d.com/forum/viewtopic.php?t=25373),
  KOARR_Release_Alpha_0.1. Existing game-specific shaders; author reports lighting
  and decal limitations, and incomplete campaign testing. Archive SHA256:
  `5621841809338B38B8A576CE9A3152312CB0DC103455C603C4E2C41DCF163B9C`.
- [geo-11 v0.6.56](https://github.com/ThreeDeeJay/geo-11/releases/tag/v0.6.56),
  x32 binaries from the SBS package, configured for `katanga_vr`. Geometric stereo
  on AMD. ZIP SHA256:
  `F01ED160B0FE822868969530ECB8846E17BACD5D5DA6B7BEB333600B19D1D659`.
- [VRScreenCap](https://github.com/artumino/VRScreenCap), commit
  `a2866bd353f237b9f65969a879f78366b72c6a57`: the Katanga shared-handle/full-SBS
  protocol informed our C++ D3D11 reader. Rust/Vulkan presenter not ported wholesale.
  MIT license retained at `third_party/VRScreenCap/LICENSE`.
- Existing MGS5VR pose math and MinHook; existing provenance retained.

geo-11 binaries and the Amalur shader fix remain local external dependencies.
Resolve their redistribution rights before publishing a combined package; the
combined package must not be described as entirely MIT source.

## Path and controls

Game → geo-11 d3d11.dll → amalur_camera.dll → system D3D11.
geo-11 publishes stereo; our OpenXR bridge snapshots the surface once for both eyes.
With tracked camera metadata it samples each half using runtime eye FOV and the
game projection scale. The native camera requests 130° horizontal FOV in game mode,
then restores the original scalar after matrix rebuilding.

Without fresh camera metadata, the bridge displays a stereo menu panel. **F10**
toggles tracked gameplay, **F7** recenters position/heading, **F12** exits the bridge.
The inherited geo-11 F10 reload binding overlaps our toggle; reassign before release.

## Checks and limits

### Depth investigation

Testing showed little/no perceived depth after the image-quality fix. The
captured full-SBS frame (`captures/depth-check.bmp`) has different eye images:
horizontal correspondence is -53 px for sky, -52 for a distant arch, -42 for the
player and -35 for nearby ground. These are image-matching estimates, not depth
ground truth. Reproduce with `py tools/analyze-stereo.py`.

The measured sign relative to infinity suggests reversed depth, plus a constant
screen-convergence offset unsuitable for parallel VR projections. A provisional
presenter correction reverses source order and subtracts the measured infinity
offset (26.5/2560 UV per source eye) in tracked gameplay. Predicted residual shifts
are 0 for sky, -11 for the player and -18 for near ground. No head-pose code changed.
This **does** change source-eye order, unlike the earlier yaw-only fix below.
The correction is tied to current separation=20 and this capture, not a general
calibration. It must be rechecked if geo-11 settings change. Human depth/fusion
confirmation and physical IPD calibration are still pending.

Image-quality correction: game-mode XR swapchains now use runtime-recommended
dimensions instead of the smoke test's 1024×1024 cap. The geo-11 final-color
UNORM surface is copied into a compatible typeless texture and sampled through
an sRGB view before the XR sRGB target encodes it. This prevents extra brightening.
GPU readback verifies display-encoded gray 128 remains 128 through the round trip.
The game source resolution and wide-FOV crop still limit effective detail; this
change cannot recover detail absent from that source. Headset visual comparison
and performance at the higher resolution remain to be checked.

Testing confirmed the headset feed works and clarified that left/right **head
rotation**, not eye order, was reversed. No eye swap was applied. Disassembly of
the native basis builder (RVA 0x836a00; cross-product helper 0x6c7950) confirms
screen-right = up × forward. The adapter previously used forward × up. Corrected
the basis and its yaw/roll/lateral-motion tests; math tests pass. Live direction
verification remains pending after reinstall.

Passed: x86 builds, camera/channel tests including tilted-recenter regression,
synthetic GPU test (left red/right green), live shared-texture readback, Quest ADB
capture showing gameplay in both eyes. The corrected recenter behavior still needs
human confirmation. A screenshot cannot establish comfortable binocular fusion.

Provisional geo-11 separation=20, convergence=100; not calibrated to physical IPD.
World scale remains 100 game units/metre. HUD is still embedded in world images.
The Katanga protocol has no pose/frame fence; camera metadata and texture may not
match exactly. Current runtime poses are submitted, so latency can cause incorrect
reprojection. Rendering gamma, culling, screen-space effects and lifecycle also need
verification. The native scene-replay path is not implemented; geo-11 supplies stereo.

```powershell
.\tools\install-stereo.ps1
.\tools\run-xr-smoke.ps1 -Mode game
.\build\diagnostic-x86\RelWithDebInfo\stereo_check.exe
.\build\diagnostic-x86\RelWithDebInfo\stereo_check.exe --capture captures/geo11-stereo.bmp
# Exit game before removal:
.\tools\install-stereo.ps1 -Remove
```

Installer records all 77 installed files and hashes, replacing only the known
diagnostic DLL. Removal refuses changed files; generated logs/caches are retained.
