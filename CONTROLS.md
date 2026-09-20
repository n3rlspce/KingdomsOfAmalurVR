# Quest controls

Use the game's gamepad UI. Release the controls after opening/closing menus or the VR settings panel to resume input.

| Quest input | Action |
| --- | --- |
| Left stick | Move / navigate menus |
| Right stick left/right | 30-degree snap turn; return to center between turns |
| Y | Select primary or secondary weapon in gameplay |
| Right trigger | Attack with the selected weapon |
| A | Dodge / confirm |
| B | Interact, sprint / back |
| X | Primary weapon attack |
| Left trigger | Block |
| Right grip + A/B/X/Y | Native ability modifier + corresponding gamepad face button |
| Left grip | Item radial |
| Left trigger + right grip | Reckoning mode |
| Right thumbrest touch + left stick | D-pad: left health, right mana, up aggressive mode |
| Both stick clicks held + left stick | D-pad fallback if thumbrest touch is unavailable |
| Left stick click, then release | Map |
| Right stick click, then release | Stealth |
| Left controller menu | Game menu |

After leaving D-pad mode, center the left stick before moving. In menus, face buttons retain native gamepad actions and the right stick navigates instead of turning.

Y selects an attack slot without firing it. With staff primary and bow secondary, choose secondary with Y, then use the right trigger. Bow drawing/aiming with tracked hands is not implemented. Tracked held weapons currently support the staff and the verified Scalding Daggers rig.

Physical blade contacts are disabled in default builds. The Scalding Daggers diagnostic requires both an `AMALUR_OWNED_MELEE=ON` build and `amalur-owned-melee.enable` beside the game executable. It has caused simulation freezes and can stop after death/reload; it is not a reliable gameplay feature. One normal basic trigger attack prepares the source. The current diagnostic bypasses minimum speed and retries contacts at 250 ms intervals, with phase and clock logging around native calls. Explicit trigger attacks retain native handling. Other melee weapon rigs are not supported.

Press `*` to open/close VR settings. Use up/down to choose a row, left/right to adjust. Grip pitch/yaw/roll tune the staff angle in 5-degree steps and are saved automatically. The panel shows the selected weapon slot. Press F7 to recenter.

Press `Ctrl+I` with the game focused to toggle Interface View for lockpicking, dispelling and other object interfaces. It shows the full image on a flat panel and keeps native gamepad menu controls. Toggle it off to return to VR gameplay. The VR settings panel also has Interface View and Interface size controls; size is saved, while Interface View starts off each session.
