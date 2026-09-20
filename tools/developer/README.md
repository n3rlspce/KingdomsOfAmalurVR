# Developer commands

## Direct startup (independent of the Lua framework)

**Runtime failure: disabled.** A menu crash with `function expected instead of
table` occurred with version 2 and Mod Framework installed together. Both were
rolled back; the cause has not been isolated. Do not reinstall either startup
package as a working fix. Offline emitter tests did not establish engine validity.

`build-fast-start.ps1 -GameDirectory <game> -Python <python>` stages a rebuilt
`patch_0.pak`, preserving the installed patch's other assets. The patch bypasses
the splash input/debounce gates through the existing profile-acquisition path.
Once the main menu is initialized and visible with a latest save, it calls the
game's own `continue_last_save` once. Save compatibility checks and asynchronous
load completion remain in the original game code. No sleeps or key presses are
used. No save leaves the ordinary menu available; failed loads are not retried.

The active startup scripts are embedded in `134230570_klua.batch`. Version 2 of
this tool patches that batch as well as the loose assets; the first package
changed only loose assets and did not activate the direct-save path in testing.
The batch writer updates entry sizes and verifies every unrelated script remains
byte-identical. Run `check_fast_start.py <original klua.batch>` to exercise the
actual emitted menu tail and verify batch preservation. To rebuild while version
1 is installed, pass its original archive backup as `-BaseArchive`. Restore the
old installation with its own manifest and `-Undo` before installing the new one.

`install-fast-start.ps1 -GameDirectory <game> -PackageDirectory <staged folder>`
installs only while the game is closed. It backs up the original archive and
renames the two publisher-logo videos so they are not opened at startup. Run the
same command with `-Undo` to restore all three original files. Hash checks refuse
unexpected revisions or overwriting subsequent archive changes. A live startup
test is still required; offline checks cannot establish engine loading behavior.
Do not publish the generated archive or extracted game assets.

`python tools/developer/check_fast_start.py` tests the emitted startup logic.
The builder also verifies lossless asset parsing and archive round trips.

## F11 panel and game-update dispatch

Build `tools/developer/build.ps1`, then build the XR bridge into the same output
directory. With the game closed, run `install-dispatch.ps1 -GameDirectory <game>`
to install the three owned mod files. This does not activate the framework DLLs.
The dispatcher uses the framework's `minimap_win` script trigger to wrap the
original `on_update_event`, preserving its arguments and running requests after it.

F11 opens the headset panel. Up/Down selects Connect, one wolf, any of the nine
weapon types, or a unique greatsword. Left/Right selects Give to inventory,
Give + equip primary/secondary, or Equip existing primary/secondary. Enter sends
one action. Escape/F11 closes. Native primary/secondary slots are independent of
future VR hand assignment. `reserve-f11.ps1` moves conflicting geo-11 bindings.

The helper checks fresh telemetry for loaded, unpaused gameplay, atomically
publishes `mods/amalur_request.lua`, and reads the nonce acknowledgement from the
framework output. It never writes console input or runs Lua on the console thread.
Connect establishes a process-local dispatcher session. The dispatcher consumes
nonces before execution, rejects stale sessions and paused gameplay, and clears
its execution flag even when Lua returns an error. Direct mutation calls from the
console are disabled. Pending requests are removed on helper completion/timeout;
unknown outcomes are never automatically retried.

The module checks API presence without invoking UI notifications. Engine calls
are made only inside the owned dispatch callback. `equip_existing(name, slot)`
never grants an item. `give_and_equip` reports a failed inventory lookup after a
grant without repeating it. Slot 0 is primary; slot 1 is secondary. Distances are
100–2000 game units and quantities are 1–20.

## Validation and known failures

The old console transport caused native errors: its notification probe failed in
`WINDOW.create_window`, and a unique-greatsword grant while inventory was open
left the session frozen. A common longsword grant returned an acknowledgement.
Those results did not establish reliable mutation safety. The replacement moves
execution into the game update callback and adds pause checks.

Live validation on the dev save succeeded: level 40 read back after the native
level setter, all nine weapon types were granted and individually verified in the
primary slot, and the unique greatsword/longbow were verified in primary/secondary.
The wolf spawn was acknowledged and the user confirmed gameplay, then died.
No new native runtime errors were observed during this dispatcher session.
The earlier level-2 character could not equip the unique sword; a Lua-only
requirement override did not bypass the native gate and has been removed.

Helper-only actions 60/61 prepare level 40 and verify the last equipped item.
Action 62 enables invincibility using `ACTOR.set_unkillable(get_player(), true)`,
matching the extracted game's cheat script. The live dispatcher acknowledged
this setter in restarted process 49908, and level 40 was verified again.
Damage prevention still needs observation in combat. These actions require the same
unpaused dispatcher session and do not grant items or run at module load.

Run `check.py` and `check_dispatch.py` with Python and Lupa. The C++ build runs the
helper and F11 input tests. Dispatcher tests cover session isolation, consuming
requests once, rejecting paused requests without executing them on unpause,
reload behavior, and exception cleanup. Preserve current body/UI bridge changes
when staging this branch's F11 integration; do not install an older bridge over a
newer one.

API evidence: the user's extracted inventory scripts call
`PLAYER.get_item_index(SIMTYPE_ID(name))` and `PLAYER.equip(index, slot)`;
the autosave script calls `GAME.is_game_paused()` without arguments.
[Public F2 helper source](https://github.com/mburbea/koar-item-editor/blob/46792455aa87b9a8a6a5b0e754a893f846b6188b/lua/f2Console.lua)
and [simtype catalog](https://github.com/mburbea/koar-item-editor/blob/46792455aa87b9a8a6a5b0e754a893f846b6188b/KoAR.Core/Data/simtype.csv)
provide grant/spawn and item-name references. No external helper code is bundled.
