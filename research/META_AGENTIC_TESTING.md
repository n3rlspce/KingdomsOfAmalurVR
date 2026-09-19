# Meta tools for automated VR testing

Researched 19 September 2026 from external references and Meta's current documentation. No tools were installed, no headset settings changed, and no runtime compatibility test was performed.

## Findings

**Meta VR CLI (`metavr`)** is the current name for Horizon Debug Bridge. The older `hzdb` command remains an alias. It offers CLI and MCP access to device/app operations, logs, screenshots, documentation, and performance traces. See [Meta's overview, updated 31 August 2026](https://developers.meta.com/horizon/essentials/metavr-overview/).

**Meta XR Operator** is a different component: an experimental OpenXR API layer exposing runtime state and interaction to an agent. It supports pose/controller-input control and inspection. Unity supplies additional scene/UI integration. See [Operator overview, updated 9 September 2026](https://developers.meta.com/horizon/documentation/unity/meta-xr-operator/).

Crucially, the [standalone guide](https://developers.meta.com/horizon/documentation/unity/meta-xr-operator/connecting-ai-agents/) explicitly supports OpenXR apps without Unity. On Windows, the loader discovers the layer through `XR_API_LAYER_PATH` and enables `XR_APILAYER_METAX_operator` through `XR_ENABLE_API_LAYERS`. The app must inherit that environment. An accompanying MCP proxy connects to the server in the app process. The bundle's process architecture, graphics compatibility, and behavior with VDXR must still be checked locally.

The [Simulator workflow](https://developers.meta.com/horizon/documentation/unity/meta-xr-operator/xr-simulator/) supports repeatable desktop testing without a physical headset. Simulation is a separate test route from the actual VDXR/Virtual Desktop delivery path.

## Applicability to Amalur

Proposed Windows test path:

```text
Amalur + our conversion DLL
    -> OpenXR loader -> XR Operator API layer -> selected OpenXR runtime
                           ^                       |-- Meta XR Simulator (test)
                           |                       `-- VDXR (compatibility to prove)
                     local MCP proxy
                           ^
                       test agent
```

This tool operates after the conversion creates an OpenXR session. It does not discover Amalur's camera, generate stereo rendering, or replace the engine/combat adapter. Unity hierarchy and C# inspection tools cannot be assumed to understand Big Huge Engine objects.

For PCVR, the game simulation and rendering live on Windows, while Virtual Desktop's streaming client lives on Quest. Consequently, Quest-side CLI diagnostics may help investigate the device/client, but cannot by themselves attribute a Windows game's CPU/GPU cost to its functions. Keep Windows timing instrumentation and Virtual Desktop's streaming metrics alongside headset diagnostics. This division is an inference from our architecture, not a verified integration claim.

## Addition to the implementation plan

1. **Phase 0 test application:** once graphics API/process architecture are known, create the minimal OpenXR test app already planned. Verify two eyes and controller actions without Operator first.
2. **Bounded Operator compatibility test:** use the standalone layer in that app, first with Simulator, then with VDXR. Verify layer discovery, session query, pose changes, button/trigger actions, and available screenshot tools. Record tool/package versions and unsupported capabilities.
3. **Use process-local configuration:** enable the layer only in the developer test launch environment. Record the active runtime and layers. Do not change the default player installation or silently switch the user's runtime.
4. **Deterministic integration tests:** after Amalur's hooks work, use a fixed save/scene to test recentering, head lean near walls, menu opening/closing, hand selection, and aim at stationary targets. Compare before/after state and images with frame IDs.
5. **Fast movement tests:** replay timestamped controller trajectories inside a test adapter for swing thresholds and hit deduplication. Use an agent to start the scenario and inspect results; do not depend on sequential MCP calls to synthesize accurately timed swordplay.
6. **Native game diagnostics:** expose read-only test state from our own adapter, such as menu mode, actor position, hit count, simulation tick, and submitted eye-frame IDs. Choose a supported custom-tool or separate local diagnostics route only after inspecting the available native integration API.
7. **Physical validation:** repeat accepted scenarios through Quest 3 + Virtual Desktop. Measure timing with automation/capture disabled as well, and retain human evaluation of comfort, motion, audio and haptics.

Proposed acceptance for the tooling experiment: the agent can query a real session, move a simulated pose, set/release an action, and obtain evidence of the expected response, with an explicit runtime/version record. If VDXR fails, keep Operator confined to Simulator and use our own diagnostics for physical testing. Neither result replaces the stereo or combat feasibility gates.

## Limits to preserve

Meta's [announcement](https://developers.meta.com/horizon/blog/meta-xr-operator-close-the-build-test-verify-loop-for-vr/) identifies screenshot-based observation, no audio access, no per-finger input, difficulty with real-time moving interactions, and possible missed subtle visual defects. That makes menus and stationary interactions sensible initial targets. It cannot certify stereo comfort or the feel of blocking and swinging.

Keep Operator a development dependency. Its experimental API and tool names can change; simulator success must not be presented as successful on-headset play. Do not reduce the schedule solely from the articles' general claims of faster development.

## User-provided coverage

- [VR.org: Horizon Debug Bridge](https://vr.org/articles/meta-quest-agentic-tools-hzdb-native-vr-development), 3 June 2026 — discovery context; newer Meta docs supply the current name and scope.
- [Meta VR CLI overview](https://developers.meta.com/horizon/essentials/metavr-overview/) — primary CLI reference.
- [Road to VR: XR Operator](https://roadtovr.com/meta-ai-vr-dev-tool-test-quest-games/), 26 August 2026 — discovery context; standalone applicability verified in Meta's documentation above.
