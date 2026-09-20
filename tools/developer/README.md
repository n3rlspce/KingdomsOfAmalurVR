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

## F11 panel

Build `tools/developer/build.ps1`, then build the XR bridge into the same output
directory. F11 opens the headset panel; Up/Down selects one wolf or one of nine
weapon types. Left/Right chooses inventory, primary weapon, or secondary weapon.
Enter submits one action; Escape/F11 closes. The two native weapon slots are
independent of future left/right VR hand support.

The panel/transport still requires the external Lua framework console and live
validation. Select Connect before sending commands. Missing dependencies produce
an error without sending anything. Do not assume the installed game has this
framework. `reserve-f11.ps1` stages an INI with conflicting F11 bindings moved to
modified key combinations; `install-stereo.ps1` applies the same reservation.

## Lua commands

Standalone Lua commands for the **Re-Reckoning Mod framework and F2 Console**.
These dependencies are not bundled or installed by this tool. Engine integration
is pending live validation; offline tests validate command dispatch and guards.

After reviewing the installed console entrypoint, place `amalur_dev.lua` in its
`mods` folder. From the F2 console in loaded gameplay, run:

```lua
run('.\\mods\\amalur_dev.lua')
amalur_dev.probe()
amalur_dev.wolf()
amalur_dev.sword()
```

Run one command at a time. Loading the file defines commands only. `wolf()` requests
one `wolf_forest` 500 game units ahead; `sword()` requests one `sword2h_unique12f`.
For chosen internal names:

```lua
amalur_dev.spawn('wolf_forest', 500)
amalur_dev.give('sword2h_unique12f', 1)
amalur_dev.give_and_equip('sword2h_unique12f', 0) -- primary (1 = secondary)
```

The engine resolves names through `SIMTYPE_ID`. Resolution alone does not prove
that a type is a creature, a weapon, loaded, or safe for the current area. There is
no universal valid-asset catalog. Equip uses the native inventory's item-index
lookup and two-argument slot assignment. If lookup fails after a grant, the tool
reports that partial outcome instead of granting again. The commands
report submission, not verified in-game success. Distance is limited to 100–2000
game units; grants to 1–20 items. There is no retry loop or automatic action.

Use disposable save/profile copies: spawning and grants can affect autosaves and
unique-item state. Review the actual F2 entrypoint before first use: the public
reference repository's `console.lua` invokes `add_all_items.lua` by default.
Do not deploy that checkout unchanged. Existing VR D-pad bindings and DLL loader
compatibility also need checking before installing the dependencies.

API evidence: [F2 Console helper source](https://github.com/mburbea/koar-item-editor/blob/46792455aa87b9a8a6a5b0e754a893f846b6188b/lua/f2Console.lua),
[simtype catalog](https://github.com/mburbea/koar-item-editor/blob/46792455aa87b9a8a6a5b0e754a893f846b6188b/KoAR.Core/Data/simtype.csv#L4251).
This file calls the documented engine surface; it does not copy or install the
external loader, helper code, or catalogs.

Offline checks require Python and `lupa`: `python tools/developer/check.py`.
