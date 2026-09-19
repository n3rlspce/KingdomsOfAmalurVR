# Live renderer discovery — 19 September 2026

Target: Steam build 10619381, executable SHA256
`16a400f6e8fc10dbe446a9e57717de9fbe975ef17c004f4ca405e2e4e09cb314`.

## Verified in Amalur

- Diagnostic x86 D3D11 proxy loads and forwards to the Windows system runtime.
- Device: D3D11-capable AMD GPU, feature level 11.0.
- Swapchain: 2560×1440, format 29 (RGBA8 sRGB), single sample, one buffer,
  windowed, discard swap effect. Present receives sync interval 1.
- MinHook hooks installed successfully for device shader creation, factory
  CreateSwapChain, Present, ResizeBuffers, Map, and Unmap.
- Several thousand presentations completed successfully during startup/menu runs.
- No save was loaded or gameplay modified. This does not establish gameplay stability.

## Build-specific addresses

All addresses below are **RVAs**, added to the loaded `koa.exe` base, not absolute addresses.

| Address | Evidence |
| --- | --- |
| `0x00c6032e` | Return address after game calls D3D11CreateDevice |
| `0x00c6f244` | Return address immediately after game's Present call |
| `0x00c6f1d0` | Disassembled function boundary containing Present call |

The Present function takes a pointer from its stack into ESI. It accesses a context-like
pointer at `ESI+0x2c8` and a presentation record at `ESI+0x30348`; that record holds
the swapchain at `+0x184` and sync interval at `+0x1d8`. These are disassembly
interpretations, not stable engine SDK definitions. Do not patch them blindly.

## Shader reflection findings

The first 256 vertex shader creation calls retain constant-buffer reflection names.
Struct members are sampled once per binding slot; layouts sharing a slot are not
exhaustively covered.

| Buffer | Slot | Bytes | Useful fields |
| --- | --- | --- | --- |
| `g_dynamicbuffer` | 4 | 256 | `world` offset 0; `worldVector` 64; `worldViewProjection` 128 |
| `g_globalbuffer` | 1 | 1376 | Fog, lighting, game time, vegetation and material constants |
| `g_skinningbuffer` | 5 | 3744 | `weightMatrices`, 78 matrices of 3×4 |
| `g_terrainbuffer` | 3 | 64 | Terrain data; member detail not captured because slot reused |

Reflection reports the dynamic matrices as row-major 4×4. Matrix meaning, handedness,
world scale, camera separation, and main-view versus shadow usage still require runtime
validation. Finding WVP does not prove stereo: the renderer must handle visibility,
depth reconstruction, lighting, shadows, and UI consistently for both eyes.

Evidence: `amalur-first-render-hook.log`, `amalur-shader-reflection.log`, and
`amalur-shader-layouts.log` in this directory. Local logs are for research, not release assets.

## Fast next experiment

The installed diagnostic now observes Map/Unmap and UpdateSubresource writes of
256-byte constant buffers and records eight candidate world/WVP samples and stacks.
Size alone is a candidate filter, not proof of binding at slot 4. Press **F8** to rearm
sampling after loading a playable scene and after moving the camera. Map/Unmap
sampling tracks one outstanding candidate per thread; nested maps can omit samples.
UpdateSubresource instrumentation compiled but awaits a live candidate write.

Next: correlate those writes with camera movement, trace the WVP producer, then
test camera control and same-simulation stereo. Packaging, alternate runtimes,
full lifecycle testing, and broad compatibility are deferred until that works.

## Build and rollback

```powershell
.\tools\build-diagnostic.ps1
.\tools\install-diagnostic.ps1
# Exit the game before removing:
.\tools\install-diagnostic.ps1 -Remove
```

The installer checks the game hash, refuses an existing d3d11.dll, and records its
own DLL hash for removal. The proxy currently exports only the two D3D11 creation
functions, sufficient for this inspected build/startup; arbitrary overlays and
other mods may need additional exports. No hot unload is supported.

Dependency: [MinHook](https://github.com/TsudaKageyu/minhook), tag v1.3.4,
commit `c3fcafdc10146beb5919319d0683e44e3c30d537`, license retained in
`third_party/minhook/LICENSE.txt`. Capstone 5.0.6 installed under `tools/python-deps`
for read-only executable disassembly.
