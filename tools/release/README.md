# Preview packaging

End users use the ZIP's Windows launchers; Python and a compiler are not required on their PCs. See [installation instructions](../../docs/INSTALL.md).

The maintainer builds the diagnostic DLL and bridge from the integrated source (Win32, `AMALUR_OWNED_MELEE=ON` for the current experimental preview), builds the developer/menu helpers, and verifies the installed baseline. The package builder consumes that verified installation and a receipt rather than guessing which of several build folders is current. It does not change the game.

```powershell
python tools/release/test_installer.py
python tools/release/build_package.py --game "<game directory>" --bridge "<bridge directory>" --receipt "<current-installed.json>" --output "build/release-vX/package" --version "X" --source-commit "<published source commit>"
```

Use a fresh output directory. The game directory must contain the checksummed original executable backup `koa.exe.amalur-native-startup-original`. Only changed bytes are packaged, and reconstruction must exactly match the installed patched executable. The original and patched executable themselves are never archived.

The payload is an explicit list: mod DLL, bridge/helpers, approved fresh-install VR preset, current integration configuration, selected authored Lua scripts/behavior markers, authored shader changes, and the licensed OpenXR loader. Dependency files are imported only by exact name and SHA-256. geo11 v0.6.56 and Mike_ar69's stereo archive are downloaded from the original hosts with pinned archive hashes. The framework is not redistributed or automatically downloaded from behind Nexus login. Its original DLLs must be supplied by the user.

The builder deliberately excludes saves, game archives, debug logs, native shader bytecode caches, runtime requests, game executable, original-file backups, and capture markers/data. `manifest.json` records the release source commit and every payload/dependency hash. Publish the ZIP and its SHA-256 sidecar as a GitHub **prerelease**, with a version-specific README download link (`releases/latest` does not select prereleases).

The installer checks all inputs before mutations, backs up originals, rolls back failed copies, retains existing user settings during updates, and preserves edited files during uninstall. Tests cover these behaviors in a disposable fixture, including path traversal and checksum rejection. The real-package test additionally uses the exact supported original executable, imports framework files locally, downloads the two upstream archives, and verifies all installed files. Never launch fixture executables or use live saves for installer tests.

Packaging validation is not proof of fresh-machine headset compatibility. The current preview targets the tested Steam/Quest/Virtual Desktop setup only.
