# Developer commands

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
Use the normal inventory menu to equip it. For chosen internal names:

```lua
amalur_dev.spawn('wolf_forest', 500)
amalur_dev.give('sword2h_unique12f', 1)
```

The engine resolves names through `SIMTYPE_ID`. Resolution alone does not prove
that a type is a creature, a weapon, loaded, or safe for the current area. There is
no universal valid-asset catalog or automatic equip support yet. The commands
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
