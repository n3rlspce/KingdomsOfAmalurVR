# Repository setup

This repository records an experimental Windows x86 VR mod and its research.
It is not a ready-to-install release. Game files, saves, compiled binaries,
captures, local install receipts and downloaded reference repositories are not
included.

## Checkout and build

```powershell
git clone --recurse-submodules https://github.com/n3rlspce/KingdomsOfAmalurVR.git
cd KingdomsOfAmalurVR
.\tools\build-diagnostic.ps1
.\tools\build-xr-smoke.ps1 -OpenXrSdk 'C:\path\to\OpenXR.Loader'
```

Use Visual Studio 2022 C++ x86 tools, a Windows SDK and CMake. The OpenXR build
expects the OpenXR.Loader 1.0.10.2 NuGet package directory layout. Pass `-OpenXrSdk` with your package path or set `OPENXR_SDK_DIR`.
The MinHook submodule is pinned to its recorded commit. The copied MGS5VR core
files retain their license and provenance under `third_party/mgs5vr`.

## Local external dependencies

The stereo installer additionally expects these locally obtained packages:

- geo-11 v0.6.56 x32 files under `research/geo11/x32`.
- mikear69's KOARR_Release_Alpha_0.1 under
  `research/amalur-stereo-fix/KOARR_Release_Alpha_0.1`.

See `research/STEREO_REUSE.md` for upstream links, archive hashes and reuse
notes. These packages are not bundled in this repository. The simulator helper
also expects the separate OpenXR-Simulator checkout/build described in
`research/NEW_VR_REFERENCES.md`. Python analysis helpers use Pillow and/or
Capstone as appropriate; `tools/python-deps` is a local dependency directory.

Install scripts require `-GameDirectory` or the `AMALUR_GAME_DIR` environment
variable and verify the known executable hash. No workstation path is embedded.
Quest capture requires `-Device` and uses ADB from PATH or an explicit `-Adb` path.
Close the game before installing or removing the mod.

## Current checkpoint

Third-person stereo and head tracking have run in Quest 3 through Virtual
Desktop. Comfort, timing, stereo calibration and complete UI coverage remain
unfinished. First-person and motion controls are future work.

The latest HUD patch uses minimap draw presence to enable a curved layout.
This is a rendering heuristic, not native menu-state detection. The enlarged
HUD was observed live. A fourth shader was added to address a leftover black
background rectangle; that change still needs a restart and visual validation.
Detailed evidence and limitations are in `research/CURVED_GAME_HUD.md`.

