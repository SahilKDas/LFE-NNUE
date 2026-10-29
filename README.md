# LFE-NNUE v1.0.0

A native Windows evolutionary ecosystem built with C++23, Win32, and Skia. Organisms inherit body plans, sparse NNUE brains, perception, diets, physiology, and mutations. Movement decisions belong exclusively to their neural networks.

The supported application, tests, headless controls, and benchmarks are native. Node.js, npm, TypeScript, and a browser are not needed to build or run them. The retired TypeScript browser app, trainer, tests, npm manifests, and local Node dependencies have been removed. Native neural initialization, inference, inheritance, and mutation are unchanged.

## Build and run

Requirements: Windows x64, CMake 3.20 or newer, and MinGW-w64 GCC with C++23 support. The required Skia library and license are included under `native/vendor/skia`.

```powershell
cmake -S native -B native/build-follow -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build native/build-follow -j 4
ctest --test-dir native/build-follow --output-on-failure
./native/build-follow/life_engine.exe
```

Use `native/build-follow` to keep development builds separate from an older running executable. The renderer DLL is embedded as a verified fallback; the packaged executable can run without a sidecar DLL.

## Simulation

- Flat 1024×640 terrain with fertile land, plains, deserts, impassable water and mountains, and toroidal borders.
- Latitude-based seasons, climate pressure, energy metabolism, mineral construction costs, starvation, aging, and durable combat cells.
- Plant, scavenger, mineral, and decomposer mouths; carrion and soil nutrients connect death to future plant growth.
- Heritable NNUE weights, body mutations, perception radius/channel masks, oxygen tolerance, recovery, and frailty.
- Gradual individual respiratory stress, with atmospheric mortality capped at one organism per ten ticks.
- Region-indexed batches, sleeping inactive areas, cached sensing, SIMD inference with a scalar reference, dirty tiles, viewport culling, and immutable render snapshots.

Elevation, caves, bridges, projected weather, and downhill drainage have been removed. Older version 1/2 saves still load: cave/bridge passages become ordinary traversable land, and retired depth controls/history are cleared.

## Controls and observation

The original blue, black, white, and teal theme and Arial fonts remain. Render caps are selectable from 30–360 FPS in increments of 30; the default is 30 FPS. Requested TPS, measured TPS, and render FPS are separate.

- Mouse wheel: zoom; middle mouse or Pan: move the camera.
- Inspect: select an organism and inspect physiology, sensory inputs, neural outputs, mutations, ancestry, and decomposition totals.
- **F**: follow/release the selected organism; **G**: family highlights; **C**: compact follow card; **D**: follow a surviving descendant after death.
- **A**: open/close the Evolution Atlas, with timelines, species, ancestry, and ecological summaries.
- **F5 / F9**: quicksave/load; hold Shift to choose the archive path.
- World tools paint resources, terrain, and nutrients. Overlays expose resources, productivity, climate, species, nutrients, and decomposition.

Observation and analytical species labels never command movement or change simulation randomness. Save archives preserve world state, neural genomes, lineage, Atlas history, and follow preferences; session decision history remains transient.

Settings and bounded diagnostic logs live under `%LOCALAPPDATA%\LFE-NNUE`. Nothing is transmitted automatically.

## Headless validation and performance

[Headless controls](native/HEADLESS.md) exercise the real simulation, camera, follow UI, and Skia renderer in memory without opening windows or sending desktop input. [UI latency results](native/UI_LATENCY.md) and [regular-game performance results](native/FOLLOW_PERFORMANCE.md) report measured timings and limitations.

Build the optional benchmark executables with:

```powershell
cmake --build native/build-follow --target life_engine_native_benchmark life_engine_ecology_benchmark -j 4
```

The throughput benchmark holds population fixed; the ecology benchmark runs full lifecycle ticks twice and checks deterministic state, resources, births, deaths, species, and history. Regular-game and UI scripts additionally exercise naturally growing plants and evolving populations. Performance validation is deferred when Battery Saver is enabled or battery charge falls below 30%.

## Windows release

```powershell
powershell -ExecutionPolicy Bypass -File scripts/package-native-release.ps1
```

The packager builds and tests native code, verifies the embedded renderer headlessly, and writes a standalone ZIP and SHA-256 checksum under `dist/`. See [release notes](RELEASE_NOTES.md) for requirements and diagnostic paths.

Historical non-TypeScript assets and the excluded WebView launcher are retained outside the supported native build; this cleanup does not remove old release/reference data or generated build directories.
