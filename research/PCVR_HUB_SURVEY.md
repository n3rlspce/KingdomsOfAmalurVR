# PCVR hub survey and Amalur reference shortlist

Research date: 19 September 2026. Purpose: identify existing implementations that reduce duplicate work on a first-person, motion-controlled Amalur conversion.

## Engine and local status

Amalur belongs to the **Big Huge Engine** lineage, an in-house engine. Historical [asset-sale documentation](https://www.hgpauction.com/wp-content/uploads/2016/08/38-Studios.pdf) associates Reckoning with Big Huge Engine code, and the remaster's [transcribed game credits](https://www.mobygames.com/game/150309/kingdoms-of-amalur-re-reckoning/credits/windows/) explicitly identify Big Huge Engine (visible in search results; direct page fetch was blocked). The publisher's [remaster FAQ](https://thqnordic.com/news/faq-kingdoms-of-amalur-re-reckoning) says KAIKO improved the rendering engine. There is no basis here for treating Re-Reckoning as an Unreal or Unity conversion.

**Engine and graphics API are different facts.** Big Huge Engine identifies the game technology; Direct3D identifies a graphics interface. The publisher lists DirectX 11 requirements, but the executable/device still needs local inspection before choosing a backend.

During this survey the intended game directory still returned no files. Steam's app manifest recorded installed build `0`, no installed depots, and target build `10619381`. This is an incomplete-install snapshot, not a verified installed build or a live estimate of download progress. No game was launched or modified.

## Catalog extraction

Inspected [PCVR Mods Installer Hub](https://github.com/Mr-Nlce/PCVR-Mods-Installer-Hub) at commit `c52ea87323fefde9730d511e7ee22ab6763ebba3`; local checkout: `research/PCVR-Mods-Installer-Hub`.

Extracted GitHub owner/repository references from its Markdown, PowerShell, and JSON files outside `Assets`, normalized GitHub/API URLs, deduplicated them, and removed `sponsors` and `user-attachments` routes. Result: **180 distinct repository references**, saved in [hub-repository-index.txt](hub-repository-index.txt). This includes dependencies, forks, release repositories, and tools; it is not a count of independent working VR conversions. Nexus-only destinations can lead to additional projects.

No Amalur-specific entry was found in the searched README/install scripts. That does not establish that no such mod exists anywhere. Installer scripts were read as text, never executed.

## Ranked references

This ranking is our engineering assessment. Author feature claims and source inspection are distinguished from headset validation; none of these mods was run here.

### 1. MGS5VR — strongest newly found native graphics reference

[Repository](https://github.com/nikamigaming-create/MGS5VR), locally inspected at `e20d92223a0a6dfccd8d75eac9d4c7841e635b75` in `research/MGS5VR`.

The build explicitly selects D3D11/OpenXR. Source includes eye-pair capture, runtime, camera, arm IK, wrist UI, and motion-melee modules. Its README describes Quest 3/Touch feedback and experimental tracked weapons; the status document describes drawing the scene twice within one transaction. This is substantially closer to our proposed native graphics architecture than VRFramework's currently implemented D3D12 route.

Read first:

- `src/xr_runtime.cpp`, `src/mailbox.cpp`, `src/scene_capture.cpp`: runtime and eye-image handoff.
- `src/render_camera.cpp`, `src/head_camera.cpp`, `src/stereo.cpp`: camera and projection integration.
- `src/controller_rig.cpp`, `src/arm_ik.cpp`, `src/motion_melee.cpp`: tracked presentation and authoritative contacts.
- `docs/STATUS.md`, `docs/QUEST3_TESTER_FEEDBACK.md`: separate simulated results from physical headset feedback.

A spot-check of motion melee found stroke identity, contact filtering and duplicate-hit suppression around native hooks. Those patterns are useful; the hard-coded game addresses and object layouts are specific to MGSV. The project is experimental, and UI/effect/interaction coverage remains incomplete. MIT license found in the checkout; preserve notices for any selected reuse. Full source audit and build validation remain future work.

### 2. OutwardVR — strongest available melee-RPG design reference

[Repository](https://github.com/cybensis/OutwardVR), locally inspected at `c9c610f8f5b06b629973c09d7cf8758b9e583d3d` in `research/OutwardVR`.

It converts a third-person RPG into first-person tracked VR. Its source uses Unity/BepInEx, so its integration mechanism does not transfer to Amalur. Inspect `combat/VRMeleeHandler.cs`, `VRShieldHandler.cs`, `WeaponPatches.cs`, and the `body`, `camera`, and `UI` folders.

The melee source reads hand velocity, checks hitboxes and stamina, and invokes original combat functions including `AttackInput` and `HasHit`. This is a concrete example of joining physical input to RPG rules. It uses raycasts in inspected hit paths; do not assume its collision algorithm already satisfies our continuous swept-blade requirement. The README acknowledges missing secondary attacks and tuning work. GPL-3.0 is declared; distinguish learning from the design from copying implementation into the mod.

### 3. Ilya's Elden Ring VR — closest claimed feature set, source inaccessible

[Author's Nexus page](https://www.nexusmods.com/eldenring/mods/10711) records an original upload on 26 August 2026 and an update on 10 September 2026. It describes physical melee through original hitboxes, native combat events, tracked arms, two-handing, hand-directed spells, and two-eye rendering with simulation frozen for the second pass. It also identifies incomplete skills and costly rendering.

The page links `https://github.com/IlyaMez/EldenringVR`, but both browsing and an actual clone attempt failed; Git reported repository not found. We therefore have author-reported features, not an available source dependency. It is a valuable behavior reference and follow-up lead. Resolve source availability and the conflicting Nexus permission panel/source-license description before considering reuse. No binaries were downloaded.

### 4. AnvilEngine2VR — architecture for adapting a different proprietary engine

[Repository](https://github.com/mutars/anvilengine2vr) documents an REFramework-derived OpenXR port for Assassin's Creed Odyssey, Valhalla, and Mirage. It provides a concrete example of moving framework infrastructure into another engine, with per-game integration. Its documented features include 6DoF/head aiming and HUD scaling; that is not evidence of Amalur-style physical weapon combat. The README identifies water/effect issues when looking away from character facing, a useful regression case for decoupled head tracking. MIT is declared. README inspected; source was not audited here.

### 5. Dishonored VR — relevant hands, sword, powers; current renderer caveat

The hub links the [original repository](https://github.com/GingasVRFO/Dishonored-VR). Its current README marks it discontinued and points to the [VR-Stereo-Hub continuation](https://github.com/VR-Stereo-Hub/Dishonored-VR).

The continuation's README says version 41.0 has returned to a head-locked screen while rebuilding stereo and disables motion controls by default. The previous DXVK stereo approach was abandoned in that line. Study the engine/body/power interfaces and failure reports, but do not adopt it as proof of a stable complete port. This is exactly why installer catalog labels need source-level verification.

### 6. Crysis VR — established interaction and lifecycle reference

[Repository](https://github.com/fholger/crysis_vrmod) documents OpenXR, tracked controls, two-hand grips, holsters, and VR settings. It is useful for interaction design and a legacy-engine integration comparison. Its engine access and CryENGINE 2 Mod SDK license differ from our project; do not assume its code is a permissive generic runtime. The author also documents CPU/stereo performance costs. README inspected; no source audit or runtime test here.

## Other leads retained

The catalog also exposes F.E.A.R., BioShock, Starfield, Halo, and many Unity ports. Those remain indexed candidates, not all individually audited projects. The prior Witcher 2 recovery already supplies [Mirror's Edge VR, COD4 VR, Witcher 3 VR, and wiz3D](WITCHER2_RECOVERED_LEADS.md).

## Resulting implementation strategy

1. Confirm Amalur's process architecture and graphics device when installation completes.
2. If D3D11 is confirmed, prioritize an audit of MGS5VR's D3D11/OpenXR lifecycle and image handoff against upstream OpenXR/REFramework implementations; compare this with finishing VRFramework's D3D11 port.
3. Keep an independent `AmalurAdapter` for camera, rendering, input, actors, animation, hit events and UI. Similar graphics APIs do not imply shared game structures.
4. Use Outward's available combat source to specify behavior and tests; use Elden Ring's advertised design as an additional comparison pending source access.
5. Extend the feasibility demo to record both-eye frame identity and detect visibility/effect errors while the headset looks away from body facing.
6. Re-estimate after a minimal in-game stereo/pose prototype. Available examples reduce uncertainty about patterns; they do not yet establish the effort needed to find Amalur's hooks.

The expanded ecosystem materially improves our starting references. This survey does not quantify whether AI caused its growth, nor infer implementation quality from a recent upload date or a long feature list.
