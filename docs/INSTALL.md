# Install the experimental VR preview

Supported starting point: Windows 10/11, the Steam version of **Kingdoms of Amalur: Re-Reckoning**, Quest headset and Virtual Desktop. Other stores, executable versions and OpenXR runtimes are not validated.

1. Install the game through Steam. Install Virtual Desktop Streamer on the PC and connect the headset using Virtual Desktop. Install the Microsoft Visual C++ 2015–2022 **x86** runtime if needed: <https://aka.ms/vs/17/release/vc_redist.x86.exe>.
2. Extract `KingdomsOfAmalurVR.zip` into a normal writable folder. Do not run from inside the ZIP.
3. For a fresh setup, download the Re-Reckoning Mod framework from Nexus (linked in `DEPENDENCIES.md`) and extract it into `Dependencies`. Its author prohibits redistribution. The installer downloads geo11 and the stereo shader pack from their original hosts automatically; OpenXR is included. Existing matching dependencies are reused.
4. Close the game and bridge, then double-click **Install and Launch.cmd**. Steam's game folder is detected, or select `koa.exe` when prompted. Installation checks the supported executable and every payload/dependency hash before changing the game.
5. For later sessions, connect the headset and double-click **Launch VR.cmd**. Keep the game focused during startup. **Install.cmd** installs without launching.

The approved VR panel preset is installed on the first installation only. Updates retain the player's existing panel settings and automatic-Continue preference. The preview includes the current experimental physical-melee settings, startup behavior (automatic Continue with autosave disabled), and save-anywhere script. Manual saves remain available; use a separate test save. No save files, save selection, captured logs, game archives or game executable are included. The installer generates the patched executable from the player's exact supported original and retains a backup.

## Restore or update

Extract a newer package and run Install again. The game-local `.amalur-vr-installer` folder stores checksummed originals and the installation receipt. Keep it until you no longer need rollback. Never copy this folder into a release.

**Uninstall.cmd** restores unchanged installed files from their originals and removes files this installer added. It preserves files edited since installation, reports them, and keeps the backup history. Saves are untouched. Reinstalling the game through Steam may replace its executable; rerun the installer afterward.

## Troubleshooting

- **Missing dependencies:** read the printed source links and `DEPENDENCIES.md`, extract the original downloads, and retry. A similar filename is not sufficient; hashes must match the tested version. No game files change if preflight fails.
- **Unsupported executable:** this preview supports the specific Steam revision in `manifest.json`. Do not disable the check. GOG/Epic or later revisions need a separately validated build.
- **Permission denied:** select the correct game folder and ensure your account can write there. The installer does not silently elevate or disable Windows security.
- **Bridge exits / flat image:** confirm Virtual Desktop Streamer and headset connection, close both game and bridge, then use Launch VR again. Logs are under the game's `AmalurVR/logs` folder.
- **HUD caches:** the installer backs up and invalidates only four known automatically converted HUD shader-cache pairs, allowing geo11 to regenerate them.

Installation/restore can be verified with `powershell -NoProfile -File .\Install.ps1 -GameDirectory "..." -DependencyDirectory "..." -CheckOnly`. This does not launch the game or modify its files.

This is an unsigned experimental preview. Automated installation checks do not establish headset comfort, full playthrough stability, or compatibility with other mods.
