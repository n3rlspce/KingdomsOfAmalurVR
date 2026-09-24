# Preview packaging

Every push to `main` runs `.github/workflows/release.yml`: build Win32 mod, bridge and helpers; run gameplay and installer checks; publish a complete experimental release. The README uses the permanent `releases/latest/download/KingdomsOfAmalurVR.zip` link. Failed runs retain the last successful download. `workflow_dispatch` allows a retry.

CI uses `build_ci_package.py` and the checked-in `baseline/` manifest, sparse executable patch and integration defaults. It needs no game installation, saves or personal token. Source-owned scripts, shaders, settings and binaries are rebuilt/copied from the current commit. External dependencies remain pinned. When changing patch/default baseline files, refresh their matching hashes in the baseline manifest. The original `build_package.py` remains available for validating an updated baseline against a local installation.

End users use the ZIP's Windows launchers; Python and a compiler are not required on their PCs. See [installation instructions](../../docs/INSTALL.md).

The maintainer builds the diagnostic DLL and bridge from the integrated source (Win32, `AMALUR_OWNED_MELEE=ON` for the current experimental preview), builds the developer/menu helpers, and verifies the installed baseline. The package builder consumes that verified installation and a receipt rather than guessing which of several build folders is current. It does not change the game.

```powershell
python tools/release/test_installer.py
python tools/release/test_report.py
python tools/release/build_package.py --game "<game directory>" --bridge "<bridge directory>" --receipt "<current-installed.json>" --output "build/release-vX/package" --version "X" --source-commit "<published source commit>"
```

Use a fresh output directory. The game directory must contain the checksummed original executable backup `koa.exe.amalur-native-startup-original`. Only changed bytes are packaged, and reconstruction must exactly match the installed patched executable. The original and patched executable themselves are never archived.

The payload is an explicit list: mod DLL, bridge/helpers, approved fresh-install VR preset, current integration configuration, selected authored Lua scripts/behavior markers, authored shader changes, and the licensed OpenXR loader. Dependency files are imported only by exact name and SHA-256. geo11 v0.6.56 and Mike_ar69's stereo archive are downloaded from the original hosts with pinned archive hashes. The framework is not redistributed or automatically downloaded from behind Nexus login. Its original DLLs must be supplied by the user.

The builder deliberately excludes saves, game archives, debug logs, native shader bytecode caches, runtime requests, game executable, original-file backups, and capture markers/data. `manifest.json` records the release source commit and every payload/dependency hash. CI uploads the ZIP and SHA-256 sidecar into a draft before publishing it as the latest release. Releases are explicitly titled experimental; GitHub prerelease status is not used because `releases/latest` excludes prereleases.

The installer checks all inputs before mutations, backs up originals, rolls back failed copies, retains existing user settings during updates, and preserves edited files during uninstall. Tests cover these behaviors in a disposable fixture, including path traversal and checksum rejection. The real-package test additionally uses the exact supported original executable, imports framework files locally, downloads the two upstream archives, and verifies all installed files. Never launch fixture executables or use live saves for installer tests.

Packaging validation is not proof of fresh-machine headset compatibility. The current preview targets the tested Steam/Quest/Virtual Desktop setup only.

For a validated source change that must stay separate from the live development installation, pass `--diagnostic "<newly built diagnostic DLL>"`. The builder still verifies the base installation receipt, while the manifest records the packaged override hash and supplied source commit. Never substitute an untested DLL. The release's `AmalurVR/AutoStart.ps1` opts into hidden bridge startup at the game's first Present; absence of this file leaves development startup unchanged. The script and manual launcher share a startup mutex.

The report collector only writes local artifacts. Text logs have bounded tails and common personal paths redacted. Saves are optional unmodified binary copies, limited to the newest three distinct `.sav` files. Never publish collected reports as release assets.
