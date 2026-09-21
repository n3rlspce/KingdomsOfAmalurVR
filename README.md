# Kingdoms of Amalur VR

Experimental VR mod for **Kingdoms of Amalur: Re-Reckoning**, targeting **Quest 3 + Virtual Desktop (VDXR)**. Work in progress; no ready-to-install release.

## Current work

- Stereo VR, head tracking, first-person camera and Touch controls.
- Both-arm IK, tracked held weapons, body/render-root pairing and weapon orientation corrections.
- HUD and menu adjustments, first-person dialogue, and developer camera/animation isolation toggles.
- Free-offhand support grip for captured weapon models, plus primary/secondary held-weapon visibility.
- Experimental physical damage paths for verified primary daggers, longsword and greatsword; deliberate swing gates and shared collision previews.
- Native attack, combo, sound/FX and render-publication diagnostics.
- Developer panel inventory tools, Sorcery unlock and spell test loadout, with bounded labels fixing the spell-row bridge crash.
- Revision-checked experimental startup tools and automatic Continue support.
- The VR bridge closes when the game exits.

## Known limitations

Weapon jitter and VR session loss remain under investigation. Body stabilization has improved in live tests, but this is not complete visual or stability acceptance. Collision shapes are provisional; faeblades use fitting guides and thrown chakrams remain unsupported. Staff magic is observed for diagnostics, not enabled as physical contact damage. Physical-swing sound/VFX playback and native combo moves are not enabled.

**Experimental physical contacts have caused simulation freezes and can stop after death/reload.** They are compiled out by default. The opt-in build and marker are for controlled diagnostics. Native contact acceptance logs are not proof of enemy health loss. Finger curl, broad weapon-model coverage and fully unattended startup are unfinished.

## Development

Build the diagnostic DLL with CMake and Visual Studio using -A Win32. Keep AMALUR_OWNED_MELEE=OFF for normal builds. Build the bridge using tools/build-xr-smoke.ps1 with -OpenXrSdk pointing to an OpenXR.Loader package; build the developer helper with tools/developer/build.ps1.

The source under src/ is the integrated baseline. Diagnostic checks are CMake targets; run D3D/WARP checks from a directory without the proxy d3d11.dll to avoid DLL shadowing. Generated binaries, original game files, research dumps and workstation settings are excluded from version control. Never replace the game's geo11 d3d11.dll with the diagnostic output; the established installation name is amalur_camera.dll.

See [Quest controls](CONTROLS.md) and [developer tools](tools/developer/README.md). Developer commands can alter a save; use a dedicated development save.
