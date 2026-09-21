# Developer commands

## Framework direct-to-save startup

`install-startup.ps1 -GameDirectory <game> -SkipLogos` installs two owned startup
mod files and disables the Kaiko/THQ publisher videos while the game is closed.
Original videos remain alongside them with `.amalur-startup-disabled` appended;
rename them back to restore them. Story cinematics are unchanged.
The framework wraps `splash_win.on_update_event` and `main_menu.on_update_event`.
After the original callback, it advances native profile acquisition once when
the splash is visible and system UI is clear, then calls the game's normal
`main_menu.continue_last_save()` once the menu is initialized with a valid latest
slot. Save compatibility checks remain in the native Continue flow. A missing
save leaves the normal menu available. An error disables startup automation for
that process; returning to the menu does not trigger another load.

The original archive remains untouched. Remove `mods/amalur_startup.json` to
disable this feature on the next launch. `check_startup.py` exercises the real
Lua module under Lua 5.1. Process 62528 logged Continue slot 20 and loaded gameplay,
but the user confirmed they still saw logos and pressed a button first. This
validated automatic Continue only, NOT unattended startup. The splash framework
hook was installed only after main_menu loaded, too late for the initial prompt.
Renaming the loose videos did not establish a publisher-screen skip.

`build-early-startup.py` and `install-early-startup.ps1` are EXPERIMENTAL and NOT
installed. They stage an exact-revision two-byte native logo sequence bypass and
five existing splash instructions in both asset lookup paths. The live test
stopped before main_menu finished loading (last framework script reported was
pc_profile_select_win); no native Lua error was logged. Original executable,
archive, and publisher videos were restored before handing off the user's next
launch. Do not describe this candidate as working or reinstall without further
diagnosis. Evidence is in `build/early-startup-v3` (local only).

Further investigation found another splash copy in `initial_0.pak`. The builder
now accepts `--archive initial_0.pak`, checks that archive's exact revision, and
verifies the loose splash as well as the embedded batch after repacking. The
installer records which archive it owns and applies the same backup/rollback
checks to either supported archive. This is a diagnostic candidate, not a
validated fix: native code mounts both initial and patch archives, so finding
the initial copy does not establish that it wins resource lookup. No new package
was installed during the active VR playtest. Main-menu bytecode remains original;
the tested framework Continue callback remains the save-loading mechanism.

Online research: the [No Logos author's description](https://www.nexusmods.com/kingdomsofamalurrereckoning/mods/4)
explicitly excludes the splash and Press Any Button screens. A video replacement
alone therefore cannot provide unattended startup. Do not infer full startup
success from that mod or from an offline bytecode round trip.

The user tested the initial-archive candidate (v4): a long black screen remained,
followed by Press Any Button; manual input then automatically loaded the save.
That candidate failed unattended startup. Do not reuse it as a working fix.

`build-native-startup.py <original-exe> <fresh-output>` stages v5 without changing
archives. Its ASLR-safe relative trampoline runs inside native profile update
at RVA `0xb42917`. When state is -1, the callback is installed, and the platform
context/user table exist, it calls the game's begin-acquisition routine at RVA
`0xafae20`. The original asynchronous authentication, DLC readiness, and completion
callback remain in place. The original logo-sequence completion patch is retained.
This is still experimental: genuine asset loading may account for black-screen
time, and offline execution cannot verify platform initialization in the game.

`check_native_startup.py <package>` executes the actual staged machine code and
native begin routine under Unicorn, stubbing only the platform acquisition call.
It checks readiness guards, state transitions, stack preservation, no repeated
request on the next update, and three relocated image bases. To install, undo v4
first, then use `install-native-startup.ps1` with the game closed. Both archives
must match their originals. Its `-Undo` restores the original executable.

V5 reached the menu without the initial prompt, but the user reported a controller
disconnected alert and manual Continue. The framework console showed no main_menu
require trigger, so automatic Continue was never attached in this startup path.
The current candidate also checks XInputGetState(0) before starting acquisition;
the virtual pad must be connected, but no button press is required. The import
ordinal is checked against the installed 32-bit DLL (GetState=2).
The Lua module now starts from UI_State_MGR and temporarily observes window
creation to find the preloaded menu. It removes that observer once the normal
menu update wrapper is attached. Loading still occurs only during menu update,
after system UI is clear, and at most once. Offline tests cover this preloaded
menu path and disconnected-controller guarding; live results remain pending.

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

Spell actions: **Max Sorcery** (action 63) raises the 25 base Sorcery abilities to
their normal maximum ranks, using the same `character_data.grant_actual_ability`
loop as the game's built-in max-abilities cheat. It checks every ID and rank
before modifying anything, preserves already higher ranks, and verifies each
rank increase. It does not spend the player's unallocated ability points.
**Equip spell test set** (action 64) assigns Storm Bolt, Ice Barrage, Healing Surge,
and Meteor to Magic slots 1–4 using the game's spell-item and slot helpers, then
reads back each native slot. Run Max Sorcery first. This replaces the four spell
bindings, leaving primary/secondary weapon slots alone. Hold right grip and use
A/B/X/Y to cast through the existing VR ability controls; the HUD identifies each
button's spell. Other unlocked spells can be mapped from the Abilities menu.
`check_spells.py` checks dispatch restrictions, preflight validation, repeated
grants, slot assignment, and stopping after partial failures. Live validation is
still required; commands never run automatically when the module loads.

Build `tools/developer/build.ps1`, then build the XR bridge into the same output
directory. With the game closed, run `install-dispatch.ps1 -GameDirectory <game>`
to install the three owned mod files. This does not activate the framework DLLs.
The dispatcher uses the framework's `minimap_win` script trigger to wrap the
original `on_update_event`, preserving its arguments and running requests after it.

Click both thumbsticks together to open or close the headset panel immediately.
Small stick deflection while pressing is tolerated, and holding the grips does
not block opening or navigation. Release the clicks after opening. Flick the left
stick up/down to select a row and left/right to choose the weapon destination.
A or the right trigger runs the selected action once; B closes the panel.
The panel connects automatically once fresh telemetry reports loaded, unpaused
gameplay. The passive connection retries at most every five seconds until it
succeeds. No grants, equips, spawns or other mutations are retried automatically.
Reconnect remains the first row, followed by one wolf, nine weapon types, the
unique greatsword, dev character level 40, and invincibility. Close game inventory/
pause menus before sending actions.

F11 and the keyboard remain available. Up/Down selects rows; Left/Right selects Give to inventory,
Give + equip primary/secondary, or Equip existing primary/secondary. Enter sends
one action. Escape/F11 closes. Native primary/secondary slots are independent of
future VR hand assignment. `reserve-f11.ps1` moves conflicting geo-11 bindings.

Panel input consumes motion controls and suppresses hand pose publication to
gameplay while interacting. Closing or changing focus requires neutral controls
before gameplay resumes. Held confirm controls never repeat actions, and presses
while the helper is busy are discarded. The existing deflected two-stick D-pad
chord remains available outside the panel. Opening the panel does not pause the
world; enable invincibility on the dev save for uninterrupted testing.

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

Actions 60/61 prepare level 40 and verify the last equipped item (61 is helper-only).
Panel action 62 enables invincibility using `ACTOR.set_unkillable(get_player(), true)`,
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
