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
- `F4`: right-controller arm IK, visibly tested with OpenXR Simulator; headset validation pending. Works in third person or alongside `F5`.
- Legacy single-weapon pose override (`F3`); disabled while arm IK is active.
- Close the settings panel (`*`) to move. Hold the right controller naturally and press `F7` to recalibrate its orientation.
- Wrist alignment, body clipping, equipment coverage and animation transitions need work. Arm IK shows the body; the staff is not attached to the solved hand yet.
- Camera consistency correction passed an initial headset test (`F2` compares it). Drawn/stowed weapon tracking is not reliable yet.
- Temporary first-person body/armor hiding (`F1`) is being tested to prevent jogging through the camera. This hides the visible body, not just the head; head yaw currently turns the view only.

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
