# Kingdoms of Amalur VR

**[⬇ Download experimental preview (.zip)](https://github.com/n3rlspce/KingdomsOfAmalurVR/releases/download/v0.1.0-preview.5/KingdomsOfAmalurVR.zip)** — [Release notes](https://github.com/n3rlspce/KingdomsOfAmalurVR/releases/tag/v0.1.0-preview.5) · [Installation guide](docs/INSTALL.md)

Download and extract the ZIP, obtain the one-time Nexus framework dependency, then run **Install and Launch.cmd**. The installer finds Steam, downloads verified geo11/stereo dependencies, backs up changed files and installs the approved VR preset. Updates preserve your settings and saves.

![Kingdoms of Amalur: Re-Reckoning VR — square battle poster with matching gold VR lettering and a gold-trimmed headset on the warrior](docs/amalur-vr-hero.png)

**[Quest controls guide](CONTROLS.md)**

Experimental VR mod for **Kingdoms of Amalur: Re-Reckoning**, targeting **Quest 3 + Virtual Desktop (VDXR)**. Work in progress; experimental Windows installer preview available.

<sub>Unofficial fan mod. Banner adapted from THQ Nordic's official artwork and logo; [artwork credits](docs/ARTWORK.md).</sub>

![Quest 3 Touch Plus controller diagram: left stick moves; Y selects weapon; X attacks primary; left trigger blocks; left grip supports. Right stick snap turns; A dodges; B interacts or sprints; right trigger attacks selected weapon; right grip modifies abilities.](docs/quest-3-controls.svg)

| Combination / gesture | Action |
| --- | --- |
| Right grip held + A / B / X / Y | Native ability modifier + corresponding face-button ability slot. |
| Left trigger + right grip | Reckoning mode. |
| Both stick clicks held + left stick | D-pad: **left** health, **right** mana, **up** aggressive mode. Deflect the left stick **before** clicking both; clicking both while centered opens the developer panel instead. |
| Hold left stick click for 350 ms | Quick-access wheel; use the left stick to select. A quick click and release opens the map. |
| Left grip near a supported free-offhand weapon handle | Hand-only support grab; release to detach. |
| Both sticks clicked while centered | Open / close the developer panel (also F11). Release controls before navigating. |
| Physical weapon swings / raised-blade charge | **Experimental, disabled in default builds.** Verified first-person longswords support linked swings and a one-second shoulder/overhead or right-grip charge; green signals readiness. See the guide for supported weapons, opt-in requirements and limitations. |

In menus, use the game's gamepad UI: face buttons keep native actions and the right stick navigates. Release controls after opening or closing menus/panels. Y selects a weapon without attacking; the right trigger attacks that selection. Tracked bow drawing/aiming is not implemented. **[Full controls, keyboard shortcuts and gesture details →](CONTROLS.md)**

## Current work

- Stereo VR and Touch input, first/third-person modes, snap turning, physical crouch and seated settings.
- Tracked arms and weapons, head-height/arm-thickness controls, and pelvis/body attachment corrections.
- Experimental physical melee with native weapon selection and activation recovery, family feedback, and back-grip sheath/draw.
- Longsword charge by raised hand or right grip, a compact charge gauge, optional debug readout, and measured rusty-longsword scale preservation.
- Experimental staff attack-facing alignment.
- Transparent native UI capture, controller menu handling, wrist HUD and independent bottom-HUD placement.
- Realtime cutscenes in Full VR or Window mode, resizable cinematic screens, dialogue facing and letterbox adjustments.
- Developer inventory, health, spell and weapon-skill tools, native pause recovery, and process-once automatic Continue with autosave disabled.
- VR bridge lifecycle follows the game, with focus recovery and source-resolution control.
- Experimental native melee effects, wrist/map presentation, cursor/menu handling, and finisher/save-anywhere scripts from the current local build.

## Known limitations

Weapon jitter and VR session loss remain under investigation. Body stabilization has improved in live tests, but this is not complete visual or stability acceptance. Collision shapes are provisional; faeblades use fitting guides and thrown chakrams remain unsupported. Staff magic is observed for diagnostics, not enabled as physical contact damage. Family feedback and custom trails are experimental; full native combos/heavies for every weapon, bow-arrow offset and general weapon-scale preservation remain unfinished. Latest staff aiming, wrist capture and body changes still require headset validation.

**Experimental physical contacts have caused simulation freezes and can stop after death/reload.** They are compiled out by default in source builds; this experimental preview enables the tested physical-melee path. The opt-in build and marker are for controlled diagnostics. Native contact acceptance logs are not proof of enemy health loss. Finger curl, broad weapon-model coverage and startup/pause reliability are unfinished. Cinematic scene coverage, dialogue gaze and the latest hit-stop change need headset validation.

## Development

Build the diagnostic DLL with CMake and Visual Studio using -A Win32. Keep AMALUR_OWNED_MELEE=OFF for normal builds. Build the bridge using tools/build-xr-smoke.ps1 with -OpenXrSdk pointing to an OpenXR.Loader package; build the developer helper with tools/developer/build.ps1.

The source under `src/diagnostic` and `src/tracking` is the integrated game baseline. The bridge uses `src/xr_smoke` and its separately preserved `src/bridge_tracking` headers; these snapshots currently differ, so do not substitute one for the other. HUD replacement sources are under `shaders/ShaderFixes` (see `shaders/README.md`). Diagnostic checks are CMake targets; run D3D/WARP checks from a directory without the proxy d3d11.dll to avoid DLL shadowing. Generated binaries, original game files, research dumps and workstation settings are excluded from version control. Never replace the game's geo11 d3d11.dll with the diagnostic output; the established installation name is amalur_camera.dll.

See [developer tools](tools/developer/README.md). Developer commands can alter a save; use a dedicated development save.

The release ZIP ships the current installed binaries with checksums and source-commit provenance. Maintainer packaging and restore tests are documented in [release tooling](tools/release/README.md).
