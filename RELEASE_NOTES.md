# LFE-NNUE v1.1.0

The native Windows simulator now lets you follow a lineage, save entire worlds, and rewind or branch a run to compare outcomes.

- **Follow and inspect.** Follow individual creatures across looping borders, highlight their families, and inspect bounded histories of sensory inputs and neural decisions. After a creature dies, follow a surviving descendant. Zoomed-out views show one copy of the world.
- **Save worlds and revisit them.** Version 3 archives retain simulation state, camera preferences, ecology, and neural runtime state. Genuine v1 and v2 archives remain readable. Older networks gain zero-filled input rows for the new senses.
- **Evolution Research Suite.** Press **R** for automatic checkpoints, cancellable rewind, branches, paired experiments, genome comparison, a searchable ecological journal, and a library of worlds and creature specimens. Automatic history uses an estimated 128 MiB capture limit and a 2 GiB disk budget. The active rewind index is session-local. Retained branch heads can be reopened from the Library.
- **New biology.** Creatures can evolve egg-laying, juvenile development, two anonymous chemical signals, and resistance to local disease. New overlays show signals, development, and infection. The Simulation panel has a saved, default-off option that lets Producer Cells on movers generate plant matter.
- **Help and presentation.** Press **F1** for the quick start guide, controls, ecosystem overview, and cell color legend. Labels and panels have dedicated space. Buttons animate on hover and press, menu tabs fade in, and the Atlas timeline animates on entry and during scroll zoom.
- **Performance and tooling.** The renderer uses layered frame exchange, cached text, and reused drawing surfaces. Full-page views skip covered world drawing, and dense charts bucket data to screen width. Headless tools exercise the regular game and UI. Timed validation checks battery state. The latest documented 10,000-creature research run averaged 60 TPS and 60 FPS but missed the strict distinct-tick smoothness target.
- **Native build cleanup.** The TypeScript browser app, Node tooling, WebView and Nim backends were removed. The experimental 2.5D habitats, weather, and drainage were retired in favor of the flat native world. Old depth saves load into the flat simulation. The README now covers controls, ecology, research, and native development.

---

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
