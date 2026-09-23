# Project coordination

## Coordinator identity

- Shared registry: `F:/Kingdoms of Amalur VR/research/coordination/coordinator.json`.
- Read it before sending an install or launch handoff. It is authoritative across worktrees.
- Outgoing coordinator: `01a0be25-3625-75d1-9e06-d6acd0a8ebb3`. Replacement requested; do not assume this is the new coordinator.
- Only a task explicitly appointed coordinator by the user may claim the role. Read `CODEX_THREAD_ID` from its environment, then update the shared registry with that actual ID and its worktree. Never invent an ID.
- On takeover, read `F:/Kingdoms of Amalur VR/research/coordination/COORDINATOR-HANDOFF.md`, update this identity section, and retire the outgoing coordinator from install/launch duties.

## Work independently

- Implement and test in your own worktree. Preserve unrelated changes.
- No routine permission requests, scope handshakes or repeated status questions to the coordinator.
- Coordinate for ready-to-install changes, integration conflicts and game launches. Do not ask every agent for updates before installing.
- These rules supersede older coordination files that prohibit ready-to-install notifications or require permission for ordinary development.

## When a change is ready

- Write one ready manifest under `F:/Kingdoms of Amalur VR/research/coordination/ready/` with a unique change ID.
- Include: author task ID, source worktree/commit, base installed hashes, changed files, artifact paths and SHA-256 hashes, test results, dependencies, install destinations, and remaining headset checks.
- Supply the change delta and source, not just an old full build that could overwrite newer work.
- Notify the coordinator once with the manifest path and a short summary when task messaging is available. Otherwise, leave the manifest in the shared inbox and tell the user it is ready. Do not claim a message was delivered without a successful tool result.
- Do not install into the live game or start a competing game/bridge session from a feature task.

## Coordinator install duties

- Install validated ready changes automatically as they arrive; the user has authorized this. Do not repeatedly ask for install approval.
- Check the shared ready inbox when resuming coordinator work and before installs or launches. Use available task notifications; do not promise background monitoring without an actual running mechanism.
- Read the current installed receipt first: `F:/Kingdoms of Amalur VR/research/coordination/results/current-installed.json`.
- Verify base hashes; integrate/rebuild conflicting or stale changes against the latest installed source. Never install whichever DLL happens to be newest by timestamp.
- Preserve all already-installed changes, current VR settings, enabled autosave, original backups and saves.
- If `koa.exe` or `amalur-xr-smoke.exe` is running, defer the install until both close. Never interrupt a headset test or kill a session just to install an update.
- Back up replaced files, install, verify hashes, then atomically update the installed receipt with source/artifact provenance. Record each manifest as installed, failed or superseded so it is not installed twice.
- Install-only means leave the game closed. Launch when the user requests it; `L`/`l` means launch. Serialize launches and avoid duplicate bridges.
- No gameplay inputs, save selection, save deletion or save manipulation unless specifically requested. Bug reports may copy up to three eligible saves after the collector's prompt; exclude `svd_fmt_0_0.sav`.

## Runtime and publishing

- Game: `F:/SteamLibrary/steamapps/common/Kingdoms of Amalur Re-Reckoning`.
- Installed release bridge: `<game>/AmalurVR`. Legacy development bridge: `F:/Kingdoms of Amalur VR/build/xr-smoke-x86`. Check both before launching; preserve current settings when changing locations.
- Mod DLL installs as `amalur_camera.dll`. Never overwrite geo11's `d3d11.dll` with the diagnostic build output of the same name.
- Steam auto-start uses `AmalurVR/AutoStart.ps1`; bridge stays hidden and follows game lifetime. Verify actual live behavior separately from offline tests.
- User wants published changes on GitHub `main`, via normal non-force pushes. Stage only intended changes. Do not commit another task's unrelated work.
- Release ZIPs contain no saves, game executable, logs, captures or private backups. Preserve dependency redistribution restrictions.
- Source-only commits do not update an existing release ZIP: rebuild/publish when the downloadable package needs the change, and keep the README download link valid.
