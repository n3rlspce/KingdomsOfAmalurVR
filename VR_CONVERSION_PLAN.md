# Kingdoms of Amalur VR — full conversion plan

## Concise roadmap

**Current release target: [v0.1 — third-person 6DoF VR](V0_1_PLAN.md).**
Deliver stereo head-tracked gameplay with original controls first. First person,
tracked hands and motion combat below are deferred to later releases.

**Latest implementation:** in-game D3D11 diagnostic hooks work; thousands of frames
presented successfully. Shader reflection exposed `world` and `worldViewProjection`
matrix layouts. A native camera rebuild hook now passes the F9 zoom test in gameplay, confirmed by the user and projection values. Next is tracked eye/target control and stereo rendering. See [renderer findings](research/RENDER_DISCOVERY.md). Prioritize this path;
defer packaging, alternate runtimes, and broad compatibility until stereo works.

**Goal:** full first-person Re-Reckoning VR on **Quest 3 + Virtual Desktop**, with tracked hands/weapons, physical combat, VR menus, and campaign/DLC support.

**Verified foundation:** Steam build `10619381`, Big Huge Engine, **x86 executable with D3D11 imports**. The independent x86 OpenXR test now builds, and its live probe detected Quest 3 through VDXR and created a D3D11 device on the D3D11-capable AMD GPU. An awake-headset session completed with 2,682 stereo pairs, both controller poses, and trigger/haptic API calls after fixing the render-target format. Testing confirmed stable visuals during head movement and vibration on both hands. Initial smoke test passed; lifecycle stress tests and game integration remain pending. See [prototype instructions and status](README.md).

1. **Prove the VR connection:** build a small x86 D3D11/OpenXR test for stereo, head/controller tracking, haptics, and VDXR lifecycle.
2. **Prove the game hooks:** identify camera and render functions; render both eyes from one simulation update with correct scale and 6DoF. This is the first feasibility gate.
3. **Make exploration playable:** stable first-person camera, hands, collision, comfortable movement, recentering, and usable menus.
4. **Prove motion combat:** one sword, shield, bow, and spell using the game's damage/progression rules. Prevent duplicate hits and animation-driven headset motion. This is the second feasibility gate.
5. **Complete the conversion:** all weapon families, skills, interactions, UI, cinematics, accessibility, campaign, and DLC exceptions.
6. **Validate and package:** Quest 3 playthroughs, performance tuning toward 72/90 Hz, compatibility checks, installation/rollback, and release documentation.

**Reuse:** study MGS5VR for D3D11/OpenXR, OutwardVR for melee-RPG controls, and the recovered Witcher 2 references for x86 runtime integration. Use VRFramework selectively. Evaluate Meta XR Operator for automated tests only after checking x86/runtime compatibility.

**Main uncertainty:** access to Amalur's renderer and combat internals. Keep work staged around the two feasibility gates. Initial allowance: **22–43 full-time developer-weeks plus contingency**, to be revised after the prototypes.

Research date: 19 September 2026. Status: implementation started with an independent runtime smoke test. The installed executable has been inspected statically; a standalone headset session now runs; game renderer integration remains pending.

**Installed baseline update:** The executable has now been inspected statically: Steam build `10619381`, **32-bit x86**, with a `D3D11CreateDevice` import. VDXR is registered for both process architectures. See [verified installed baseline](research/INSTALLED_BASELINE.md). No live game/headset test has occurred; this supersedes earlier pending-install and architecture assumptions.

## 1. Direction and scope

Build a Windows PCVR mod for **Kingdoms of Amalur: Re-Reckoning**, using a custom game integration layer and OpenXR, streamed to **Quest 3 through Virtual Desktop**, with **VDXR as the primary runtime**. Start with one verified Steam executable build. Treat the original 2012 Reckoning, GOG, and other storefront builds as separate compatibility targets.

Static inspection confirmed build `10619381` and x86 architecture; see the installed baseline for the executable hash. Verify owned DLC separately.

The intended release is a complete first-person conversion: stereoscopic 6DoF head tracking, tracked hands and weapons, motion-driven melee and ranged combat, VR interfaces, comfortable locomotion, and campaign progression. A camera moved inside the character, a stereoscopic virtual screen, or controller gestures that only press an attack button are intermediate experiments, not completion.

This is a substantial reverse-engineering project with unresolved feasibility. First fund a bounded investigation, then expand only when stereo rendering and combat integration are demonstrated. Do not promise a finished conversion on the strength of archive tools or an existing stereo profile.

### Definition of done

- Both eyes receive correct geometry, projection, scale, and occlusion from the same simulation state; head translation and rotation work independently of the character's animation.
- Quest controllers position visible hands/weapons. Physical melee sweeps, directional blocking, bow drawing, and controller-directed magic affect the game through its combat rules.
- All weapon families, skills, progression, interactions, and mandatory campaign sequences have an implemented and tested VR treatment.
- New game, character creation, dialogue, inventory, map, crafting, trading, quests, saving, loading, death, and ending are usable entirely from the headset.
- Seated/standing play, recentering, left-handed bindings, snap turning, and tracking-loss recovery work.
- A complete base-game playthrough is verified. Teeth of Naros, Legend of Dead Kel, and Fatesworn are tracked separately; the “full package” milestone includes all owned target DLC after separate validation.
- A documented reference PC sustains the selected refresh target in representative demanding scenes, with explicit resolution and streaming settings.

Scope excludes a standalone Quest APK, multiplayer, a recreation of the world in another engine, and fully simulated physics for every prop. Preserve the game's RPG systems and content while adapting their presentation and inputs.

## 2. Research findings and their implications

| Finding | Evidence and confidence | Consequence |
| --- | --- | --- |
| Re-Reckoning lists DirectX 11 and 64-bit Windows requirements. | Publisher's [Steam listing](https://store.steampowered.com/app/1041720/), high confidence for requirements. | D3D11 is the initial integration hypothesis. Requirements alone do not establish the actual device API, process architecture, or render path: inspect the installed executable and capture a frame. |
| The original game has Big Huge Games engine technology and a specifically engineered third-person camera. | [Original developer's camera article](https://media.gdcvault.com/GD_Mag_Archives/GDM_September_2011.pdf), plus [original asset sale documentation](https://www.hgpauction.com/wp-content/uploads/2016/08/38-Studios.pdf). | Plan for game-specific integration. Do not confuse Amalur with Project Copernicus or assume Unreal facilities exist in the remaster. |
| UEVR targets Unreal Engine 4/5. | [UEVR repository](https://github.com/praydog/UEVR), high confidence. | UEVR is not an established drop-in solution for this game. Its existence does not remove the need to investigate Amalur's renderer. |
| A Re-Reckoning mod framework supports custom Lua scripts and console commands. | [Framework author's page](https://www.nexusmods.com/kingdomsofamalurrereckoning/mods/9), [files/history](https://www.nexusmods.com/kingdomsofamalurrereckoning/mods/9?tab=files). Documented files date to 2020. | Investigate scripting and gameplay entry points, but do not assume compatibility with the installed build or access to rendering, animation, or collision. Original-game mods are explicitly incompatible with this framework. |
| Archive and texture tooling exists. | [Toolbox author's documentation](https://www.nexusmods.com/kingdomsofamalurrereckoning/mods/15) describes wrappers around developer-provided pack/unpack tools. | Useful for asset identification and packaging. This is not evidence of a complete engine SDK or working skeletal exporter. |
| A community vorpX profile reports geometry stereo and head tracking. | [Profile author's post](https://www.vorpx.com/forums/topic/kingdoms-of-amalur-re-reckoning/), May 2023; untested here. | An encouraging rendering lead and optional comparison baseline, not proof of physical combat or full first-person compatibility. |
| VDXR runs OpenXR applications through Virtual Desktop without requiring SteamVR. | [VDXR repository](https://github.com/mbucchia/VirtualDesktop-OpenXR), [maintainer's setup wiki](https://github.com/mbucchia/VirtualDesktop-OpenXR/wiki). | Use OpenXR directly; validate SteamVR's OpenXR runtime as a secondary path. Virtual Desktop streaming alone does not convert a flat game into VR. |

Research did not establish an existing complete first-person motion-control conversion. That is a search result, not proof none exists. Recheck community resources before duplicating substantial work. The [Amalur resource index](https://github.com/thunderer/Amalur) is a discovery aid; verify each linked project's edition, build compatibility, and permissions at its own source.

### Unresolved questions that must become experiments

1. What are the installed executable's architecture, graphics API, feature level, build hash, and loaded graphics wrappers?
2. Can camera transforms and projection be controlled before visibility determination and rendering?
3. Can the world render twice without advancing simulation, animations, particles, or combat twice?
4. Can head geometry be hidden locally while preserving equipment, shadow, and body behavior?
5. Can weapon transforms, collision queries, damage events, attack state, and animation hit events be intercepted independently?
6. Can HUD rendering be separated from the world and its pointer coordinates remapped?
7. Which Lua and native interfaces survive loading, respawn, cinematics, and DLC transitions?

## 3. Architecture

### Expanded reference survey

The external installer hub yielded 180 distinct GitHub repository references, including tools and dependencies. See [PCVR hub survey](research/PCVR_HUB_SURVEY.md) for the ranked shortlist and evidence. **MGS5VR now takes priority as a concrete D3D11/OpenXR source candidate**, conditional on verifying Amalur's API; **OutwardVR** is the leading available melee-RPG behavior/source reference. Ilya's Elden Ring VR advertises a particularly close feature set, but its linked source repository was inaccessible during inspection. These findings broaden the options beyond completing VRFramework. No existing Big Huge Engine VR adapter was established by the survey.

### VRFramework evaluation — added after source inspection

Reference supplied: [elliotttate/vrframework](https://github.com/elliotttate/vrframework). A read-only research checkout is stored at `research/vrframework`, pinned by this inspection to commit `751863e2076c9c142553c69a73d159f483b2ab5e`. No framework code was built, executed, or installed into the game.

**Decision:** use its guides and adapter design as references, and evaluate selected code reuse during Phase 0/1. Do not yet adopt the whole repository as a working Amalur runtime. Source inspection reveals more implementation than the scaffold-oriented README suggests, but also critical gaps for our likely graphics path.

| Inspected source | Finding | Amalur action |
| --- | --- | --- |
| [`IEngineAdapter.hpp`](https://github.com/elliotttate/vrframework/blob/751863e2076c9c142553c69a73d159f483b2ab5e/include/spi/IEngineAdapter.hpp) | Separates hook installation, frame timeline, stereo transforms, and HUD/input callbacks. | Model an `AmalurAdapter` on these boundaries; camera, actor, UI, and combat discovery remain game-specific. |
| [`D3D11Hook.cpp`](https://github.com/elliotttate/vrframework/blob/751863e2076c9c142553c69a73d159f483b2ab5e/src/hooks/D3D11Hook.cpp) | Hook/unhook return false; presentation and resize bodies are placeholders. | If Amalur is confirmed D3D11, implement or port a real hook and resource lifecycle before integration. |
| [`VR.cpp`](https://github.com/elliotttate/vrframework/blob/751863e2076c9c142553c69a73d159f483b2ab5e/src/mods/VR.cpp) | OpenXR initialization rejects non-D3D12; submission uses its D3D12 component. Eye selection alternates across engine presents. | Add a native D3D11 binding/submission path if needed, and replace the AFR schedule with synchronized per-eye rendering for the final target. Merely setting a capability enum is insufficient. |
| [`OpenXR.cpp`](https://github.com/elliotttate/vrframework/blob/751863e2076c9c142553c69a73d159f483b2ab5e/src/vr/runtimes/OpenXR.cpp) | Contains runtime/pose implementation, but explicitly excludes action/binding/input machinery. Projection code assumes reverse-Z. | Implement controller actions, pose spaces, bindings and haptics; measure Amalur's depth convention rather than inheriting projection assumptions. |
| [`CMakeLists.txt`](https://github.com/elliotttate/vrframework/blob/751863e2076c9c142553c69a73d159f483b2ab5e/CMakeLists.txt) | C++23/CMake 3.27; dependencies default to author-local `E:/Github` paths; FH5 target defaults on. | Supply pinned dependencies through our own build, disable the FH5 target, and exclude game-specific code/offsets. No clean local build has been verified. |
| [`LICENSE`](https://github.com/elliotttate/vrframework/blob/751863e2076c9c142553c69a73d159f483b2ab5e/LICENSE) | MIT notices for praydog and Elliott Tate/contributors. | Preserve notices when reusing code and audit separately acquired dependency licenses. |

Immediate follow-up: compare the cost of completing its D3D11/OpenXR core with a smaller purpose-built D3D11/OpenXR implementation using audited upstream components. Select the path after a minimal graphics/runtime smoke test, keeping the game adapter independent. This repository can reduce design/discovery work; it does not yet justify reducing the project estimate or removing either feasibility gate.

```text
Re-Reckoning executable
  -> Build-specific game adapter: camera, simulation, rendering, actors, combat, UI
  -> VR conversion DLL: tracking, stereo, hands, gameplay translation, comfort
  -> OpenXR loader -> VDXR -> Virtual Desktop Streamer -> Quest 3
                     \-> SteamVR OpenXR (secondary validation route)
```

Use C++ with MSVC and CMake for the injected module; match the game's measured process architecture. Use a small loader/proxy only after identifying a reliable loading mechanism. Keep hooks behind typed interfaces so the rest of the mod does not depend directly on scattered offsets. Lua may help with discovery or high-level features; native timing-sensitive integration must not depend on unverified scripting capabilities.

Suggested components:

| Component | Responsibility |
| --- | --- |
| `loader` / `compat` | Exact build identification, validated signatures, startup diagnostics, reversible hook installation |
| `game_adapter` | Typed access to player, camera, render entry points, input, combat, UI, and game modes |
| `xr` | Session lifecycle, actions, poses, swapchains, frame timing, haptics, runtime diagnostics |
| `renderer` | Eye rendering, culling, effect corrections, UI capture, mirror view, graphics-state restoration |
| `avatar` | First-person visibility, hands, equipment alignment, optional arm/body IK |
| `combat` | Swing recognition, swept contact queries, attack authorization, hit deduplication, skills |
| `ui` / `comfort` | Menus, pointers, HUD, locomotion, transitions, calibration, accessibility |
| `diagnostics` | Timing traces, collision visualization, compatibility report, crash context |

Unknown builds must decline to apply unverified hooks and explain why. Pattern matches require expected bytes, valid ranges, and structural checks; a matching byte sequence alone is insufficient. Reacquire scene-dependent pointers after transitions. Serialize configuration separately from game saves and avoid permanent executable edits.

### Stereo rendering is the first critical dependency

Additional reference work was recovered from earlier VR porting research. See [Recovered Witcher 2 leads](research/WITCHER2_RECOVERED_LEADS.md) for Mirror's Edge VR, Call of Duty 4 VR, Witcher 3 VR, wiz3D, and exact historical source anchors. Compare these with VRFramework when choosing reusable components. Incorporate explicit frame identity alongside eye surfaces, stereo image validation, culling checks, and state-restoration tests. The recovered project had offline validation but no passing current-build headset proof according to its README; it is a reference, not a validated Amalur foundation.

Prefer a hook into the game's scene rendering: run simulation once, then render two views using runtime-provided eye poses and asymmetric FOVs. Hooking presentation alone exposes a finished image, not all the machinery needed for correct stereo.

For an actual D3D11 device, validate the adapter LUID and feature level against OpenXR graphics requirements before session creation. Use the existing device only if it satisfies those requirements. If inspection finds a different API, revise the graphics bridge before proceeding; do not silently assume texture sharing will solve it. The normative reference is the [OpenXR specification](https://registry.khronos.org/OpenXR/specs/1.0-khr/html/xrspec.html).

The VR frame coordinator must respect OpenXR frame/session lifecycle, sample poses for predicted display time, acquire/wait/release swapchain images correctly, and submit the stereo projection layer. Handle focus loss, session stopping, resize, device failure, and headset reconnection. Do not block the game unpredictably inside an arbitrary presentation hook.

Investigate camera-dependent culling, skyboxes, shadows, water, particles, transparency, decals, occlusion queries, screen-space lighting, and temporal histories. Each eye needs valid view-dependent data. Share expensive work only after correctness is proven. Use a union view frustum or per-eye visibility so geometry does not disappear at one eye's edge.

If scene re-entry fails, time-box a lower-level graphics interception prototype. Draw-call replay is a research fallback with substantial shader/resource-state work, not a guaranteed solution. Depth-based stereo and alternating simulation frames are not acceptable substitutes for the final synchronized geometry stereo requirement.

## 4. First-person embodiment and movement

Separate the gameplay actor, locomotion heading, tracking origin, HMD pose, and rendered hands. Define coordinate conventions, handedness, units-per-meter, and transform order explicitly. Calibrate scale using both measured controller travel and plausible environmental dimensions; FOV changes are not scale calibration.

- Anchor the tracking origin to a stable player reference, not a bobbing head bone. Preserve independent HMD look during attacks, movement, targeting, and scripted sequences.
- Hide head/hair/helmet geometry from the local view, retaining shadows where possible. Test armor sets individually. Start with tracked hands and weapons; add arms/body IK after animation hooks are proven.
- Separate visual hand poses from authoritative reach and hit checks. Tracking loss must cancel pending attacks and stop haptic loops without launching weapons or dealing damage on recovery.
- Integrate room-scale displacement with the player collision capsule. Resolve world collision through existing queries; use a fade for obstructed head movement instead of allowing wall peeking or forcibly rotating the view.
- Support configurable stick movement relative to head or controller, snap turning by default, optional smooth turning, seated height calibration, physical crouch where gameplay supports it, and accessible button alternatives.
- Convert dodge rolls, knockback, blink movement, climbing/context traversal, and forced camera transitions into stable-heading movement or brief fades. Never inherit animation roll directly into the headset.
- Treat teleport locomotion as a later feature requiring traversal, encounter, and quest-boundary validation; smooth movement is the initial parity route.

## 5. Motion combat without losing Amalur's RPG systems

Use an incremental integration path, while keeping the final target explicit:

1. **Input prototype:** tracked weapons plus buttons/gestures invoking existing attacks. Establish state transitions and controller usability. Label this limited combat clearly.
2. **Physical combat slice:** one sword, one shield, and one enemy. Swept weapon contact determines the valid target while the game retains damage calculation, stamina/mana costs, armor, effects, and progression.
3. **Full combat adapter:** replace or suppress the original animation hit events for physical attacks, add skills/combos and every weapon family, and prove no duplicated damage or broken ability triggers.

Track weapon segments across simulation steps and sweep them through collision space, rather than sampling a single tip point. Gate attacks using calibrated velocity, travel distance, attack state, cooldown, and rearm rules. Use per-swing/per-target hit records. Reject stationary overlaps, rapid wrist jitter, wall penetration, and out-of-reach hits. Define how valid skill-related multi-hits differ from accidental repeated contacts.

Preserve original stats and rules where possible; do not invent a second independent damage system. If target selection is inseparable from original animations, record that as a major blocker rather than calling button emulation physical combat.

| Family/system | Planned VR behavior and validation |
| --- | --- |
| Longswords, greatswords, hammers | Swept strikes; equipment-specific reach and recovery; two-hand grip for suitable weapons; retain heavy attacks and unlockable moves |
| Daggers and faeblades | Dual-wield tracking with controlled off-hand contribution; explicit treatment for spin attacks and finishers |
| Shields | Off-hand pose defines facing/coverage; route block and parry through existing rules; test attacks from sides and behind |
| Longbows | Grip to hold, other hand to draw/release; controller aim and game-authoritative projectile spawn; retain draw, ammo, and skill rules |
| Staves and sceptres | Tracked aim/origin, grip handling, and existing melee/projectile/spell behavior as applicable |
| Chakrams | Gesture-directed launch with preserved return behavior and game rules; reliable virtual reattachment to hands |
| Magic | Controller aiming, radial selection, clear targeting feedback, existing mana/cooldown/area rules |
| Reckoning/Fateshift | Preserve game time scaling and rewards while keeping head tracking responsive; comfortable finisher treatment and accessible QTE inputs |
| Stealth, backstabs, contextual kills | Preserve eligibility and quest/combat triggers; provide stable first-person or explicitly chosen cinematic presentation |

Provide dominant-hand selection, adjustable gesture thresholds, reduced-motion play, and button alternatives. Haptics should distinguish contact, parry, bow draw, and spell release. Tune using actual Quest controllers and multiple users, not desktop pose emulation alone.

## 6. Interfaces, interactions, and presentation

Capture the original UI into a readable VR panel as the first complete usability route. A controller ray must map panel coordinates back to the game's actual hit regions; rendering a panel without reliable selection is insufficient. Use controller navigation when mouse emulation is unavailable.

Move persistent health, mana, quest indicators, and notifications to adjustable stable panels or a wrist display. Avoid fixing all HUD content tightly to the face. Provide font/panel scaling, depth adjustment, subtitles, and consistent focus ownership between gameplay and menus.

Support dialogue choices, character creation, inventory comparisons, map navigation, shops, crafting, alchemy, blacksmithing, sagecraft, lockpicking, dispelling, tutorials, and all modal prompts. Use ray-based interaction first; add near-hand interaction only where reliable world queries exist. Preserve locked-door, distance, and quest requirements.

Define a mode/state table covering gameplay, menus, dialogue, loading, cutscene, death, and paused/unfocused runtime. Each state specifies camera ownership, input ownership, simulation policy, and transition behavior. Cinematics may use a stable virtual screen where necessary; document these exceptions rather than silently forcing a moving cinematic camera onto the headset.

Verify audio listener orientation follows the VR view where controllable, correct headset output, subtitle visibility, and positional cues. Keep VR calibration/settings outside character saves.

## 7. Quest 3 + Virtual Desktop validation

The PC renders the game; Quest 3 receives the stream and supplies tracking/input. Install the wireless Quest version of Virtual Desktop and its Windows Streamer. The vendor recommends a wired PC connection to an appropriate 5 GHz AC/AX router; validate the actual network rather than assuming a particular router guarantees results. See [Virtual Desktop's requirements](https://www.vrdesktop.net/).

Select VDXR explicitly for the primary test configuration and confirm the runtime in the Virtual Desktop performance overlay and mod logs. The [VDXR setup documentation](https://github.com/mbucchia/VirtualDesktop-OpenXR/wiki) describes the runtime selection and overlay check. Use SteamVR OpenXR as a diagnostic comparison, not an extra required layer in the VDXR route.

Begin at 72 Hz with a conservative render resolution and spacewarp disabled to reveal actual application cost; target 90 Hz for the recommended preset. These are proposed test targets, not performance claims about an unmeasured PC. Refresh intervals are approximately 13.89 ms at 72 Hz, 12.50 ms at 80 Hz, 11.11 ms at 90 Hz, and 8.33 ms at 120 Hz. Leave runtime/streaming headroom rather than consuming the entire interval in game rendering.

Record CPU/GPU frame times and missed submissions separately from encoding, network, and decoding latency. Log runtime, GPU/driver, game build, resolution per eye, refresh rate, codec, bitrate, and spacewarp setting. Compare supported codecs on the actual GPU; do not assume AV1 encoding is available. Higher bitrate is not automatically a better latency configuration.

Provisional performance gate: a repeatable ten-minute demanding route with p95 application CPU and GPU times each below 90% of the selected frame interval, p99 below that interval, and less than 1% missed application submissions outside separately reported loading transitions. Follow with a two-hour play session. Revise budgets using measured runtime overhead, and publish failures rather than hiding them behind interpolation.

## 8. Milestones and decision gates

Estimates below are planning allowances for an experienced developer working full time, not research-derived delivery promises. They assume access to the game and headset. Reverse engineering can invalidate them.

| Phase | Indicative effort | Deliverable and exit gate |
| --- | --- | --- |
| 0. Baseline | 3–5 days | Edition/build/hash, process/API capture, backup saves, flat performance route, known-good OpenXR headset smoke test |
| 1. Feasibility | 2–4 weeks | Repeatable camera override and synchronized two-eye geometry rendering of a small area on Quest 3; preliminary native combat/input hook inventory |
| 2. VR foundation | 3–6 weeks | Stable first-person tracking, collision/scale, locomotion, lifecycle recovery, readable operable menus; 30-minute exploration test |
| 3. Combat vertical slice | 4–8 weeks | Sword/shield physical combat, bow, one spell, one dungeon/boss loop, loot/save/load; no duplicate hit events |
| 4. Full conversion | 8–16 weeks | Remaining weapons/skills, UI/minigames, cinematics, avatar treatment, accessibility, campaign and DLC test coverage |
| 5. Release hardening | 4–8 weeks | Performance presets, full playthroughs, installer/uninstaller, runtime/build matrix, issue triage and documentation |

The sum is roughly **22–43 developer-weeks**, before contingency or additional storefront support. Reserve another 30–50% for unknown hooks, content exceptions, and regression repair; a solo full release may take most of a year or longer. Part-time development expands calendar time. A renderer or combat dead end may require redesign rather than more time on the same implementation.

**Gate A — after feasibility:** Continue only with evidence that camera control and synchronized stereo are repeatable. Otherwise produce a blocker report identifying failed approaches and the next bounded experiment. A vorpX profile is not a substitute for this gate.

**Gate B — after combat slice:** Continue toward the full package only if physical contact integrates with original combat, skill state, and rewards without duplicated hits or forced headset motion. If only gesture-to-button combat is possible, record it as reduced scope requiring a deliberate product decision.

**Gate C — before release:** All advertised content is controller-operable, validated on the stated builds, and within published stability/performance limits. Keep experimental content clearly marked until it passes.

## 9. Validation and risk register

Evaluate **Meta XR Operator standalone + Meta XR Simulator** early as an optional developer test workflow. Meta documents non-Unity OpenXR support, making our native mod a plausible integration target once it creates an OpenXR session. Prove process-architecture, graphics, capture/input, and VDXR compatibility in the small OpenXR test app first. Use **Meta VR CLI (`metavr`, formerly `hzdb`)** for appropriate Quest-side diagnostics. See [Meta agentic testing assessment](research/META_AGENTIC_TESTING.md) for the compatibility experiment, source links and limitations. Keep deterministic simulated checks distinct from actual Quest 3/Virtual Desktop performance and comfort validation; neither tool supplies Amalur's missing engine hooks.

Use unit tests for transform math, coordinate conversion, collision sweep subdivision, swing rearming/hit deduplication, and input focus transitions. These are deterministic failure-prone parts. Use in-game and headset tests for visual quality, comfort, combat feel, and progression; those cannot be established by unit tests.

Maintain a save-based regression route spanning the tutorial, outdoor vistas, dense settlements, interiors, water, heavy spell effects, boss combat, inventory, cinematics, death, fast travel, and DLC transitions. Include left/right dominance, seated/standing play, controller occlusion, headset removal, recentering, and runtime reconnect. Check per-eye screenshots and headset perception; a correct desktop mirror is insufficient evidence.

| Risk | Early detection | Response |
| --- | --- | --- |
| Renderer cannot safely render twice | Phase 1 instrumentation; verify one simulation tick and two views | Time-box deeper rendering hooks; re-estimate or stop the full-conversion branch |
| Camera fix happens after culling | Lean/turn near frustum edges and doorways | Move hook earlier or correct visibility for both eyes |
| Animated hits cannot be separated | Phase 3 event/damage tracing | Locate authoritative hit path before expanding weapons |
| Framework or offsets are obsolete | Exact-build loading tests | Implement independent adapter; never reuse unsupported offsets blindly |
| UI is inseparable from scene effects | Early menu capture and selection tests | Isolate UI passes or provide an explicit alternative panel path |
| Gameplay updates occur twice | Combat timers/particle/time instrumentation | Separate simulation from rendering before more features |
| Streaming masks renderer issues | Compare app metrics, VD metrics, and alternate runtime | Fix the responsible stage; keep reproducible settings |
| Saves or progression regress | Backup-based campaign tests and clean uninstall/load tests | Keep mod state separate and investigate authoritative game mutations |
| Third-party DLL collisions | Clean baseline, then one additional mod at a time | Publish compatibility exclusions and supported chaining only after testing |

Distribute original mod code/configuration and any independently created assets. Keep extracted game content and executables out of the package. The toolbox page restricts redistribution of its executables, and the framework page restricts modification/re-upload; treat these projects as research references or user-installed dependencies unless suitable permission is obtained. Verify each reused dependency's actual license before including code. This is a packaging requirement, not an assumption that publicly downloadable tools are freely redistributable.

## 10. First implementation backlog

- [x] Confirm edition/storefront and intended installation path: Steam Re-Reckoning at the path recorded above.
- [ ] Verify installation completion, DLC, Windows version, CPU/GPU/RAM, and network setup.
- [x] Inspect external VRFramework source and record its exact commit and integration gaps.
- [ ] Compare a completed VRFramework D3D11 port against a minimal custom OpenXR core after graphics API identification.
- [x] Record installed executable hash/architecture and graphics imports in `research/INSTALLED_BASELINE.md`.
- [ ] Capture the live graphics device, adapter, feature level and frame layout.
- [ ] Establish backup saves and a reproducible benchmark route.
- [ ] Create a minimal **x86 D3D11/OpenXR** test application to verify VDXR, both controllers, haptics, and headset lifecycle independently of the game.
- [ ] Inventory official archive tools and test community utilities individually on the exact build.
- [ ] Locate camera/projection writes, scene render entry, and simulation boundaries; document evidence in `docs/reverse-engineering.md`.
- [ ] Prototype reversible first-person camera control on the monitor.
- [ ] Demonstrate synchronized stereo with correct head translation on Quest 3.
- [ ] Produce `docs/feasibility-report.md` with captures, timings, hook limitations, and Gate A decision.
- [ ] Only after Gate A: build the sword/shield combat slice and full VR interface path.

Suggested future repository layout: `src/{loader,compat,game_adapter,xr,renderer,avatar,combat,ui,comfort,diagnostics}`, `tests/`, `config/`, `docs/`, and `tools/`. Keep build-specific addresses/signatures and compatibility evidence together, with no proprietary binaries committed.

## 11. Research source register

Sources were consulted on 19 September 2026. Author descriptions establish documented capability, not local compatibility. Engineering architecture, thresholds, estimates, and milestone gates in this document are proposed project decisions.

1. [Publisher's Re-Reckoning Steam page](https://store.steampowered.com/app/1041720/) — product scope, DLC description, system requirements.
2. [Eric Undersander's camera article, Game Developer September 2011](https://media.gdcvault.com/GD_Mag_Archives/GDM_September_2011.pdf) — developer account of Reckoning's camera smoothing and obstacle handling.
3. [38 Studios asset documentation](https://www.hgpauction.com/wp-content/uploads/2016/08/38-Studios.pdf) — historical Big Huge Engine context.
4. [Re-Reckoning Mod Framework](https://www.nexusmods.com/kingdomsofamalurrereckoning/mods/9) and [file history](https://www.nexusmods.com/kingdomsofamalurrereckoning/mods/9?tab=files) — Lua features, edition incompatibility, permissions, release dates.
5. [Re-Reckoning Modding Utility Toolbox](https://www.nexusmods.com/kingdomsofamalurrereckoning/mods/15) — archive/texture tooling and distribution restrictions.
6. [vorpX Re-Reckoning profile announcement](https://www.vorpx.com/forums/topic/kingdoms-of-amalur-re-reckoning/) — firsthand profile claims; not independently reproduced.
7. [UEVR repository](https://github.com/praydog/UEVR) — Unreal-specific supported scope.
8. [VDXR repository](https://github.com/mbucchia/VirtualDesktop-OpenXR) and [setup wiki](https://github.com/mbucchia/VirtualDesktop-OpenXR/wiki) — OpenXR runtime, Quest support, setup and diagnostics.
9. [Virtual Desktop official site](https://www.vrdesktop.net/) — Streamer and wireless PC requirements.
10. [Khronos OpenXR specification](https://registry.khronos.org/OpenXR/specs/1.0-khr/html/xrspec.html) — graphics binding, frame lifecycle, tracking/input integration reference.
11. [Amalur resource index](https://github.com/thunderer/Amalur) — leads for further investigation, not evidence of VR readiness.
12. [VRFramework](https://github.com/elliotttate/vrframework) — external reference; source reviewed at commit `751863e2076c9c142553c69a73d159f483b2ab5e`. See the source-linked evaluation in section 3 for verified implementation gaps.



