# Kingdoms of Amalur VR

**[Download (.zip)](https://github.com/n3rlspce/KingdomsOfAmalurVR/releases/download/v0.1.0-preview.7/KingdomsOfAmalurVR.zip)** · [Install guide](docs/INSTALL.md) · [Report a bug](https://tally.so/r/BzNXPK)

Experimental VR for **Re-Reckoning · Steam · Quest 3 · Virtual Desktop**.

Extract ZIP → add [Nexus framework](https://www.nexusmods.com/kingdomsofamalurrereckoning/mods/9) to `Dependencies` → run **Install and Launch.cmd**.

Other dependencies download automatically. Updates keep settings and saves.

![Kingdoms of Amalur VR](docs/amalur-vr-hero.png)

<sub>Unofficial fan mod · [Artwork credits](docs/ARTWORK.md)</sub>

## Features

- First-person combat with physical melee. Spells auto-target like the original game.
- VR dialogue and full-VR cutscenes, except pre-rendered videos.
- Right grip over shoulder: tap to sheathe; hold to unsheathe.
- Automatically skips the menu and continues your latest save.
- Wrist HUD.
- Snap turning, physical crouching and seated mode.

## Controls

**Click on both sticks to open VR settings.** Keep both sticks centered.

![Quest 3 controller bindings](docs/quest-3-controls.svg)

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
| Right thumbrest touch | No action; does not interrupt movement |
| Both stick clicks held + left stick | D-pad: left health, right mana, up aggressive mode |
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

Normal third-person mode uses native game attacks and disables custom physical contact damage. In first-person mode, verified regular/rusty longswords use deliberate swings at 2 m/s for 55 ms. Three linked swings select the experimental combo sequence. Hold the blade upright near shoulder height with the hand stable for about one second: gold ring fills, green indicates heavy readiness, then strike within the transition window. These controls do not apply to every weapon family.

The unified panel supports Settings/Dev tabs and hold-up/down scrolling. The health action sets roughly 10,000 health instead of invincibility. Unlock weapon moves grants the native weapon skill ranks; spell actions remain separate. Open pause menu requests the native ledger through the UI dispatcher. Automatic Continue and autosave are enabled at startup; manual saves remain available.

Latest controls: VR Settings offers **Heavy charge input: Position / Right grip**. In Right grip mode, release once after selecting it, then hold for one second anywhere and swing when ready; release to rearm. Explicit grip-plus-face-button spells cancel charging. The full attack readout defaults off and can be enabled in the Dev panel. A small vertical gauge shows heavy readiness.

Back-grip sheath/draw is experimental: use a grip gesture behind the shoulder; a tap sheathes and a hold draws, with native sound. Staff aiming attempts to align native attack facing with the tracked staff. Wrist HUD supports Left / Right / Off. Realtime cutscenes offer Full VR / Window; either stick up/down resizes cinematic screens. Physical crouch, seated mode, head height and arm thickness are adjustable in VR Settings. These additions need headset validation and do not imply full weapon-family support.
