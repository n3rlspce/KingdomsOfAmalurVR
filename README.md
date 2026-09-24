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

**Click on both sticks to open VR settings.**

**Boss finishers: when the boss is down and the A prompt appears, press X + A at the same time.**

![Quest 3 controller bindings](docs/quest-3-controls.svg)

| Quest input | Action |
| --- | --- |
| Left stick | Move / navigate menus |
| Right stick left/right | 30-degree snap turn; return to center between turns |
| Y | Select primary or secondary weapon in gameplay |
| Right trigger | Attack with the selected weapon (temporary fallback while physical melee is experimental) |
| A | Dodge / confirm |
| B | Interact, sprint / back |
| X | Primary weapon attack |
| Left trigger | Block |
| Right grip + A/B/X/Y | Native ability modifier + corresponding gamepad face button |
| Left grip | Reserved for support-hand grabbing |
| Left trigger + right trigger | Reckoning mode in the prepared update; unavailable in the linked preview build |
| Right thumbrest touch | No action; does not interrupt movement |
| Both stick clicks held + left stick | D-pad: left health, right mana, up aggressive mode |
| Left stick click, then release quickly | Map |
| Hold left stick click (350 ms) | Quick-access wheel; use left stick to select |
| Right stick click, then release | Stealth |
| Left controller menu | Game menu |

**Reckoning:** the linked preview build uses the physical right trigger for a weapon attack and right grip for the game's RT input. It cannot activate Reckoning in physical melee. A paired bridge and game DLL update is prepared so **left trigger + right trigger** sends the native LT+RT chord, while right trigger alone keeps its attack fallback. The update has not been installed or added to the linked download yet.

**Long term:** once physical melee works reliably across weapon families, the right trigger attack fallback can be removed. That also makes the gameplay Y weapon selector unnecessary: Y can return to its native gamepad action, and right trigger can return to the game's RT ability input. X and Y can remain as native button fallbacks. The stick click shortcuts and grip gestures serve VR actions that have no dedicated Touch buttons.
