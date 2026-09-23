# Install

**Windows 10/11 · Steam Re-Reckoning · Quest · Virtual Desktop**

1. Install the game, [Virtual Desktop Streamer](https://www.vrdesktop.net/) and [Visual C++ x86 runtime](https://aka.ms/vs/17/release/vc_redist.x86.exe).
2. Extract `KingdomsOfAmalurVR.zip`.
3. Extract the [Nexus framework](https://www.nexusmods.com/kingdomsofamalurrereckoning/mods/9) into `Dependencies`. One-time download; redistribution prohibited.
4. Close game and bridge. Connect headset. Run **Install and Launch.cmd**.
5. Choose **Steam detection** or **Pick game folder** containing `koa.exe`.

Other dependencies download automatically. Unsupported game versions stop installation.

## Play

Connect headset → **Play in Steam**. Keep game focused during startup.

Bridge runs hidden; closes with game. Alternative: **Launch VR.cmd**.

Automatic Continue and autosave enabled.

## Update / uninstall

- Update: extract newer ZIP → **Install.cmd**. Existing settings and saves retained.
- Remove: **Uninstall.cmd**. Restores originals; preserves later edits.
- Keep `.amalur-vr-installer` in the game folder for restoration.

## Bugs

**Report a Bug.cmd** → Explorer selects ZIP; [report form](https://tally.so/r/BzNXPK) opens. Attach ZIP; submit. No login.

Includes logs and optionally three recent saves. Review before sharing; no automatic upload. Limit: **10 MB**. Larger ZIP: attach separate text log and saves.

- Flat image: reconnect Virtual Desktop; close game/bridge; retry.
- Missing dependency: follow printed download link; extract into `Dependencies`.
- Startup log: `AmalurVR/logs/steam-autostart.log`.
