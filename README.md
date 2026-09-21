# Kingdoms of Amalur VR

![Kingdoms of Amalur: Re-Reckoning VR — gold title lettering over fantasy battle artwork](docs/amalur-vr-hero.png)

**[Quest controls guide](CONTROLS.md)**

Experimental VR mod for **Kingdoms of Amalur: Re-Reckoning**, targeting **Quest 3 + Virtual Desktop (VDXR)**. Work in progress; no ready-to-install release.

<sub>Unofficial fan mod. Banner adapted from THQ Nordic's official artwork and logo; [artwork credits](docs/ARTWORK.md).</sub>

![Quest 3 Touch Plus controller diagram: left stick moves; Y selects weapon; X attacks primary; left trigger blocks; left grip supports. Right stick snap turns; A dodges; B interacts or sprints; right trigger attacks selected weapon; right grip modifies abilities.](docs/quest-3-controls.svg)

| Combination / gesture | Action |
| --- | --- |
| Right grip held + A / B / X / Y | Native ability modifier + corresponding face-button ability slot. |
| Left trigger + right grip | Reckoning mode. |
| Right thumbrest touch + left stick | D-pad: **left** health, **right** mana, **up** aggressive mode. Center the stick after leaving D-pad mode. |
| Both stick clicks held + left stick | D-pad fallback. Deflect the left stick **before** clicking both; clicking both while centered opens the developer panel instead. |
| Hold left stick click for 350 ms | Quick-access wheel; use the left stick to select. A quick click and release opens the map. |
| Left grip near a supported free-offhand weapon handle | Hand-only support grab; release to detach. |
| Both sticks clicked while centered | Open / close the developer panel (also F11). Release controls before navigating. |
| Physical weapon swings / raised-blade charge | **Experimental, disabled in default builds.** Verified first-person longswords support linked swings and a stable, upright shoulder-height hold of about one second to charge; green signals readiness. See the guide for supported weapons, opt-in requirements and limitations. |

In menus, use the game's gamepad UI: face buttons keep native actions and the right stick navigates. Release controls after opening or closing menus/panels. Y selects a weapon without attacking; the right trigger attacks that selection. Tracked bow drawing/aiming is not implemented. **[Full controls, keyboard shortcuts and gesture details →](CONTROLS.md)**

## Current work

- Stereo VR, head tracking, first-person camera and Touch controls.
- Both-arm IK, tracked held weapons, body/render-root pairing and weapon orientation corrections.
- HUD and menu adjustments, first-person dialogue, and developer camera/animation isolation toggles.
- Free-offhand support grip for captured weapon models, plus primary/secondary held-weapon visibility.
- Experimental physical damage paths for verified primary daggers, longsword and greatsword; deliberate swing gates and shared collision previews.
- Longsword V3: physical three-strike sequence, raised-hand charge, captured native audio and custom trails for verified regular/rusty models; scoped physical hit-stop suppression.
- Native third-person mode, hold-to-scroll settings, health/weapon-move cheats, and native attack/effect diagnostics.
- Cinematic camera and dialogue-gaze experiments with Lua letterbox removal.
- Developer panel inventory tools, Sorcery unlock and spell test loadout, with bounded labels fixing the spell-row bridge crash.
- Revision-checked startup tools, automatic Continue with autosave disabled before loading, and a UI pause-recovery action.
- The VR bridge closes when the game exits.

## Known limitations

Weapon jitter and VR session loss remain under investigation. Body stabilization has improved in live tests, but this is not complete visual or stability acceptance. Collision shapes are provisional; faeblades use fitting guides and thrown chakrams remain unsupported. Staff magic is observed for diagnostics, not enabled as physical contact damage. Longsword audio and custom trails are experimental; native trail ownership and broader weapon combo/effect playback remain unfinished.

**Experimental physical contacts have caused simulation freezes and can stop after death/reload.** They are compiled out by default. The opt-in build and marker are for controlled diagnostics. Native contact acceptance logs are not proof of enemy health loss. Finger curl, broad weapon-model coverage and startup/pause reliability are unfinished. Cinematic scene coverage, dialogue gaze and the latest hit-stop change need headset validation.

## Development

Build the diagnostic DLL with CMake and Visual Studio using -A Win32. Keep AMALUR_OWNED_MELEE=OFF for normal builds. Build the bridge using tools/build-xr-smoke.ps1 with -OpenXrSdk pointing to an OpenXR.Loader package; build the developer helper with tools/developer/build.ps1.

The source under src/ is the integrated baseline. Diagnostic checks are CMake targets; run D3D/WARP checks from a directory without the proxy d3d11.dll to avoid DLL shadowing. Generated binaries, original game files, research dumps and workstation settings are excluded from version control. Never replace the game's geo11 d3d11.dll with the diagnostic output; the established installation name is amalur_camera.dll.

See [developer tools](tools/developer/README.md). Developer commands can alter a save; use a dedicated development save.
