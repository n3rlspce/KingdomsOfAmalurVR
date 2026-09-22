# Immersive cinematics and dialogue

The mod hides only the native `letterbox` child of `conversation_menu` and
`cinematic_paused_win`, after their normal UI callbacks run. Conversation asset
1621413 contains `top_letterbox_sprite`, `bottom_letterbox_sprite`,
`fuzzy_edge_bottom` and `fuzzy_edge_top` in this group. Cinematic asset 1824674
has a separate letterbox group. This removes the complete backdrop, including
textured edges missed by the earlier solid-black vertex shader filter.
The native lookup requires all four arguments: `WINDOW.find_window(root,
4591092, -1, true)`. The first installed version incorrectly passed a name and
omitted required arguments; the live framework reported this failure. Version 2
corrects that call and emits installation/hide receipts. A versioned upgrade
disables only the old cosmetic callback, preserving the native callback chain.

Text, selection highlights, pause/skip controls and dialogue progression remain
native. Existing VR dialogue placement and head tracking continue to work.
Current window handles are resolved on each callback; no engine handle
is retained across destruction or save/load. No shared WINDOW function is hooked.

This is an opt-in UI mod for the existing VR installation, using its already
installed Re-Reckoning Lua framework. It also suppresses bars when playing that
installation on the desktop; it does not automatically detect headset presence.
It does not crop prerecorded video, remove bars encoded in a video, or add
stereo/head tracking to prerecorded scenes. The bridge's existing untracked
image path presents a world-anchored screen, retaining aspect ratio and its
Interface size control. This patch does not add a dedicated video detector.

The native DLL also has an experimental scripted-scene camera path. It requires
the verified CinematicSceneMgr/current CinematicScene and active SceneWin camera
identities, excludes the gameplay camera, widens the FOV for stereo, and follows
the authored camera position with live six-degree head movement. The horizontal
heading is captured at scene entry; native animated yaw, pitch and roll do not
rotate the viewer. Scene camera travel and position cuts still occur. Tracking
loss returns the camera to native behavior. Invalid or unrecognized ownership
leaves the native path alone; coverage of every cinematic is not established.

Conversation entry uses the active NPC's location instead of leftover player
movement yaw. The heading is latched at entry, so subsequent NPC movement never
locks head-look. A separate Lua callback supplies the stock noncombat look-at
target only when the NPC has no target. It respects NPC_DisableHeadTracking and
existing scripted targets, changes neither body facing nor actor position, and
leaves target lifetime to native AI. It targets the player actor's native head
attachment, not the headset's room-scale position. A one-line AMALUR_VR_GAZE
diagnostic distinguishes the applied fallback from existing/disabled tracking.
Live testing confirmed the fallback executes and installs a target, but the user
still observes no NPC head following. It is not a verified gaze fix: an accepted
target does not establish that the dialogue animation applies head tracking.
The user-requested alternative in Cinematics003 turns the NPC's body once when
a tracked conversation starts, toward the actual VR eye's horizontal position.
It uses the existing validated native `set_facing` service (absolute integer
degrees), with generation-checked NPC/location and active motion-component
guards. It does not continuously rotate the NPC or change their position. This
is entry alignment, not continuous head/eye following. Native rendering still
needs a user playtest; the diagnostic logs the NPC, old yaw and requested yaw.

Cinematics004 supersedes that one-time adjustment at the user's request with
continuous body facing toward the tracked eye position during dialogue. It uses
the validated native fractional motion accumulator behind `set_facing`, at up
to 60 degrees/second with a 0.3-degree deadband. Elapsed time is capped at 50ms
per update to prevent catching up with a large turn after a stall. Conversation
and NPC changes, invalid geometry, tracking loss and dialogue exit reset the
follower. Headset orientation does not steer NPC facing. This remains body yaw,
not independent neck/eye animation; in-game appearance is still unverified.

With Amalur closed:

```powershell
./tools/cinematics/install.ps1 -GameDirectory 'F:/SteamLibrary/steamapps/common/Kingdoms of Amalur Re-Reckoning'
```

The installer adds `mods/amalur_vr_cinematics.lua`,
`mods/amalur_vr_dialogue_gaze.lua`, their `amalur_vr_cinematics_entry.lua` loader,
and the JSON trigger file. Each module trigger loads both fixes independently.
It neither launches the game nor replaces DLLs, shaders, archives or other mods.
Use the same command with `-Remove` while the game is closed to undo it.
Use `-Update` to back up and update an earlier version. Native camera changes
require rebuilding/integrating the DLL against the current installed source;
the script installer does not replace it.

`python tools/cinematics/check.py` requires `lupa.lua51` and tests the actual Lua
scripts with mocked native UI handles and actor APIs. `cinematic_check` and
`dialogue_check` exercise head movement, horizon, entry direction, native camera
animation, invalid inputs, recentering and camera/scene lifetime transitions.
Native module/child names and look-at arguments were checked against locally
extracted game scripts/assets. Live rendering and callback
activation still require a playtest: enter/exit a conversation, change choices,
view a real-time cinematic, pause/skip it, and return to gameplay. Check both
headset eyes for border remnants and confirm text/choices remain visible.
