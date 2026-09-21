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
| Left grip | Reserved for support-hand grabbing |
| Left trigger + right grip | Reckoning mode |
| Right thumbrest touch + left stick | D-pad: left health, right mana, up aggressive mode |
| Both stick clicks held + left stick | D-pad fallback if thumbrest touch is unavailable |
| Left stick click, then release quickly | Map |
| Hold left stick click (350 ms) | Quick-access wheel; use left stick to select |
| Right stick click, then release | Stealth |
| Left controller menu | Game menu |

After leaving D-pad mode, center the left stick before moving. In menus, face buttons retain native gamepad actions and the right stick navigates instead of turning.

Y selects an attack slot without firing it. With staff primary and bow secondary, choose secondary with Y, then use the right trigger. Bow drawing/aiming with tracked hands is not implemented. Tracked held weapons support captured dagger, sword, staff, hammer and faeblade models; other skins may remain unsupported.

Physical contacts are disabled in default builds. Controlled diagnostics require AMALUR_OWNED_MELEE=ON and amalur-owned-melee.enable beside the game. Verified primary dagger, longsword and greatsword recipes are enabled in that build; other families remain capture-only. A deliberate swing must reach 0.9 m/s for 30 ms, opening a 450 ms contact window; accepted damage consumes it. Idle overlap does not repeatedly damage. Native trigger attacks remain available. Simulation freezes and death/reload failures remain known risks; verify enemy health loss in game.

Press `*` to open/close VR settings. Use up/down to choose a row, left/right to adjust. Grip pitch/yaw/roll tune the staff angle in 5-degree steps and are saved automatically. The panel shows the selected weapon slot. Press F7 to recenter.

Press `Ctrl+I` with the game focused to toggle Interface View for lockpicking, dispelling and other object interfaces. It shows the full image on a flat panel and keeps native gamepad menu controls. Toggle it off to return to VR gameplay. The VR settings panel also has Interface View and Interface size controls; size is saved, while Interface View starts off each session.

Open the developer panel with F11 or a simultaneous click of both sticks. It includes Weapon collisions, camera/body experiments, Max Sorcery and Equip spell test set. Select spell actions deliberately on a development save. Left grip near a supported free-offhand weapon handle engages hand-only support grip; release to detach.
