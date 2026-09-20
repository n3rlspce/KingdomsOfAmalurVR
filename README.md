# Kingdoms of Amalur VR

Experimental VR mod for **Kingdoms of Amalur: Re-Reckoning**, targeting **Quest 3 + Virtual Desktop (VDXR)**. Work in progress; no ready-to-install release.

## Current work

- Stereo VR, head tracking, first-person camera and Touch controls.
- Both-arm IK, wrist alignment, extended reach and tracked held weapons for supported rigs.
- Adjustable HUD, tracked menus, first-person dialogue and flat Interface View.
- Upright fallback menu panels and separate-eye filtering in the VR bridge.
- The VR bridge closes when the game exits.
- Torso stabilization, visual controller smoothing and CPU arm tracing for movement-jitter investigation.
- Reduced first-person near plane and an adjusted forward camera position; headset validation remains ongoing.

## Known limitations

Arm/body shake during movement, body clipping, menu edge artifacts, weapon coverage and stereo scale still need work. CPU arm tracing does not capture final GPU vertices. Finger tracking, two-handed interactions and full-body interaction are not implemented.

**Experimental physical contacts have caused simulation freezes and can stop after death/reload.** They are compiled out by default. The opt-in build and marker are for controlled diagnostics, not a stable gameplay mode. Current diagnostics remove minimum swing speed and record native contact phases; accepted contact logs alone do not prove reliable health damage.

## Development

Build the diagnostic DLL with CMake and Visual Studio using `-A Win32`. Keep `AMALUR_OWNED_MELEE=OFF` for normal builds. Generated binaries, game files, research dumps and workstation settings stay outside version control.

See [Quest controls](CONTROLS.md) and [developer tools](tools/developer/README.md). Developer commands can alter a save; use a dedicated development save.
