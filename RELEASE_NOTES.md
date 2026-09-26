# LFE-NNUE v1.0.0

LFE-NNUE v1.0.0 is the first production release of the native Windows evolution simulator. It preserves deterministic seeded ecology and NNUE-only movement while bringing the complete simulation, renderer, Evolution Observatory, and Evolution Atlas into native C++/Skia.

## Release highlights

- Native exact-cell renderer with viewport culling, dirty tiles, density-aware work distribution, configurable 30–360 FPS cap, and independently measured TPS/FPS.
- Deterministic seasons, terrain, resources, energy metabolism, combat durability, atmosphere, gradual respiratory mortality, soil nutrients, and evolvable decomposers.
- Heritable NNUE brains, perception, diets, physiology, mutations, species classification, bounded lineage history, and long-run Atlas timelines.
- Toroidal world borders, region-indexed simulation, sleeping inactive regions, cached sensing, immutable render snapshots, and a 10,000-organism benchmark.
- Persistent validated display/simulation preferences and window placement.
- Bounded local diagnostic logs with clean-start, renderer, crash, and shutdown records.
- Embedded Skia renderer fallback, so the packaged executable does not depend on a separately installed runtime DLL.

## System requirements

- 64-bit Windows 10 or Windows 11.
- A CPU with four or more logical cores is recommended for large simulations.
- Approximately 250 MB of free memory for ordinary play; dense long-running worlds can require more.

## Controls

- Mouse wheel: zoom the world or the open Atlas timeline.
- Middle mouse, or the Pan tool: move the camera.
- `A`: open or close the Evolution Atlas.
- Inspect: select an organism for live physiology, neural output, mutation, ancestry, and decomposition telemetry.

## Diagnostics

Settings are stored in `%LOCALAPPDATA%\LFE-NNUE\settings.ini`. The bounded local log is `%LOCALAPPDATA%\LFE-NNUE\logs\life-engine.log`; the previous log is retained after rotation. No diagnostic information is transmitted.

Run `LFE-NNUE.exe --self-test` to verify the embedded renderer and deterministic simulation without opening the UI. A successful run exits with code 0 and writes `self-test.txt` beside the executable.

## Known limits

- Save/load of complete worlds is not part of v1.0.0; settings persist, simulations do not.
- Water and mountains remain universally impassable.
- The native release targets Windows x64 only.
- Species labels are analytical and never influence organism physics or NNUE inputs.
