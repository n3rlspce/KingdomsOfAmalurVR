# Kingdoms of Amalur VR

Experimental VR mod for **Kingdoms of Amalur: Re-Reckoning**, targeting
**Quest 3 + Virtual Desktop (VDXR)**. Work in progress.

## Implemented

- Third-person stereo VR and 6DoF head tracking.
- Original gamepad and keyboard controls.
- Curved HUD with adjustable size and stereo placement; headset testing ongoing.
- In-VR settings panel (`*`), live HUD size slider and recentering (`F7`).

## Experimental prototype — needs headset validation

- First person, head tracking, Touch input and right-arm IK start enabled when the VR bridge supplies tracking. `F5` toggles first person; `F4` toggles arm IK.
- Head yaw requests native character facing in first person; initial headset validation pending.
- Right-controller arm IK was visibly tested with OpenXR Simulator; equipment and headset validation are ongoing.
- Legacy single-weapon pose override (`F3`); disabled while arm IK is active.
- Close the settings panel (`*`) to move. Hold the right controller naturally and press `F7` to recalibrate its orientation.
- Wrist alignment, body clipping, equipment coverage and animation transitions need work. Arm IK shows the body; a native wrist-socket override for held equipment is implemented but not visually verified yet. Auto-sheathing remains native.
- Camera consistency correction passed an initial headset test (`F2` compares it). Drawn/stowed weapon tracking is not reliable yet.
- Temporary first-person body/armor hiding (`F1`) is available to prevent jogging through the camera. This hides the visible body, not just the head; it defaults off to show tracked arms.

### Touch controls (experimental)

Left stick moves or selects in menus. Right trigger sends primary weapon attack;
right grip sends the native ability modifier, with A/B/X/Y selecting spells.
Left grip opens the item radial; left trigger blocks. Face buttons keep their
Xbox equivalents. Right stick sends D-pad directions; right stick click is stealth;
left stick click opens the map; left menu opens the game menu.
The native gamepad HUD and spell-bank switching still need verification.

## Not working reliably yet

- Tracking stability, performance and stereo depth/scale need more testing.
- Bottom screen edge can be visible; the action bar needs bottom anchoring.
- Dialogue views can be overly zoomed; HUD/menu handling is incomplete.
- Some settings, including depth/convergence controls, are not fully functional.
- No ready-to-install release yet.

## Not implemented

- Left-arm tracking, finger controls, swing-based combat and two-handed interactions.
- Full-body VR interaction.

**Controls:** `F10` toggles head tracking · `F7` recenters · `*` opens settings · `F12` stops VR.
