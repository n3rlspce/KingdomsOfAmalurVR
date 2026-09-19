# Kingdoms of Amalur VR

Experimental VR mod for **Kingdoms of Amalur: Re-Reckoning**, targeting
**Quest 3 + Virtual Desktop (VDXR)**. Work in progress.

## Implemented

- Third-person stereo VR and 6DoF head tracking.
- Original gamepad and keyboard controls.
- Curved HUD with adjustable size and stereo placement; headset testing ongoing.
- In-VR settings panel (`*`), live HUD size slider and recentering (`F7`).

## Experimental prototype — needs headset validation

- `F5`: first-person camera and analog Touch left-stick movement (enables head tracking).
- Right-controller pose override for a single equipped weapon; `F3` toggles it separately.
- Close the settings panel (`*`) to move. Hold the right controller naturally and press `F7` to recalibrate its orientation.
- Grip placement, body clipping and animation transitions still need testing. Dual weapons are skipped; there is no swing-based damage or arm IK yet.

## Not working reliably yet

- Tracking stability, performance and stereo depth/scale need more testing.
- Bottom screen edge can be visible; the action bar needs bottom anchoring.
- Dialogue views can be overly zoomed; HUD/menu handling is incomplete.
- Some settings, including depth/convergence controls, are not fully functional.
- No ready-to-install release yet.

## Not implemented

- Motion-controlled hands, swing-based combat and two-handed interactions.
- Full-body VR interaction.

**Controls:** `F10` toggles head tracking · `F7` recenters · `*` opens settings · `F12` stops VR.
