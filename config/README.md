# VR release preset

`amalur-vr.ini` captures the user's approved saved VR panel values on
2026-09-23. Ship it beside `amalur-xr-smoke.exe` for fresh installations.
The bridge build script seeds it only when the output has no existing INI.
Updates must preserve the user's existing INI.

The preset includes all current active saved fields, including camera depth,
head height, cinematic scale, wrist HUD, seated/crouch mode and heavy input.
Historical `PreviousGrip*` / `PreviousWeapon*` migration values are omitted;
`GripBasisVersion=1` and the current grip/weapon offsets are retained.

This is a frozen release baseline, not a link to the live settings file.
Session-only dev switches and the separate automatic-Continue/autosave settings
are not part of this INI. The original byte-for-byte snapshot is also backed up
outside the build directory under the local installation's `settings-backups`.
