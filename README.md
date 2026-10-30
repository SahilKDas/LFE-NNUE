# LFE-NNUE

**Build an ecosystem. Follow a creature. Watch evolution unfold.**

LFE-NNUE is a native Windows artificial-life simulator. Creatures inherit bodies, diets, senses, physiology, and sparse neural networks. They reproduce, mutate, compete for resources, and leave descendants behind in a changing world.

The Evolution Observatory shows what one creature senses and decides. The Evolution Atlas shows how entire populations change over time.

**Windows x64 · C++23 · Skia · Deterministic seeded simulation**

[Play](#play) · [Controls](#controls) · [Ecosystem](#the-ecosystem) · [Build](#build-from-source) · [Testing](#testing-and-performance)

## Play

Download a Windows package from [GitHub Releases](https://github.com/SahilKDas/LFE-NNUE/releases), extract it, and launch **LFE-NNUE.exe**. The packaged app includes its renderer; no Node.js, browser, WebView, or separate runtime installation is required.

For the latest source build, follow [Build from source](#build-from-source).

### Your first few minutes

1. Let the initial population grow. Watch plants, creatures, resources, and seasons change.
2. Choose **Inspect** and click a creature to open its Evolution Observatory.
3. Press **F** to follow it. Turn on family highlights to see its descendants, or use the compact card for an unobstructed view.
4. Press **A** to explore population history, species, ancestry, and ecological summaries.
5. Paint terrain or resources to change the environment, then observe which traits persist. Save a world before experimenting if you want to return to it.

The world is flat, **1024 × 640 cells**, and wraps at its borders. Water and mountains are impassable.

## Controls

| Action | Control |
| --- | --- |
| Inspect a creature | Select **Inspect**, then click it |
| Pan | Middle-mouse drag, or select **Pan** and drag |
| Zoom | Mouse wheel |
| Open / close Evolution Atlas | **A** or the **Atlas** button |
| Follow / release the selected creature | **F** or **Follow** |
| Toggle family highlights | **G** |
| Toggle compact follow view | **C** |
| Follow a surviving descendant after death | **D** or **Descendant**, when available |
| Browse captured neural decisions | **[** for older, **]** for newer |
| Quicksave / quickload | **F5** / **F9** |
| Choose a save / load file | **Shift + F5** / **Shift + F9** |
| Pause / resume | **Pause / Resume** button |
| Change simulation speed | **Requested simulation TPS** slider |
| Change rendering cap | **FPS** buttons: **30–360**, in steps of 30 |

Rendering defaults to **30 FPS**. Choose **60** for a higher cap. Requested TPS controls simulation speed; FPS controls how often the world is drawn. The interface reports measured TPS and render FPS separately.

World tools paint plant matter, carrion, minerals, nutrients, and terrain. Overlays show resources, productivity, climate, species, soil nutrients, and decomposition.

## The ecosystem

### Bodies, brains, and inheritance

Organisms evolve body cells, perception radius, sensory channels, physiology, mutation rates, and NNUE weights. Different bodies have different energy and mineral costs. Combat cells and armor wear down; larger bodies and more expensive senses carry metabolic tradeoffs.

Moving creatures choose between **up, down, left, right, and wait** through their neural networks. Terrain limits passability, and physiology affects survival and reproduction. There is no terrain, resource, or climate steering fallback.

### Resources and ecological niches

| Resource or process | Role |
| --- | --- |
| Plant matter | Food for plant-specialized mouths |
| Carrion | Food for scavengers and decomposers |
| Minerals | Construction resources for growth and combat tissue |
| Decomposition | Trades some immediate food energy for nutrients returned to nearby soil |
| Soil nutrients | Support plant productivity, diffuse, decay, and recover gradually |
| Seasons and latitude | Change local temperature and fertility over time |
| Atmosphere | Creates individual respiratory stress and reproductive pressure |

Energy, aging, starvation, temperature, combat, and respiration remain distinct pressures. Respiratory vulnerability is heritable; atmospheric deaths are paced at no more than one organism every ten simulation ticks.

### Follow one life

The follow camera tracks a creature without changing its zoom, including across world borders. Manual panning releases it. Gold, cyan, and violet outlines distinguish the followed creature, direct offspring, and later descendants.

The follow card shows captured sensory inputs, five neural output scores, and the chosen action. Decisions are labeled with their inference tick, with a session history of up to 128 entries. Outputs are scores, not probabilities. Stationary organisms have no movement network to display.

When a creature dies, the camera holds its final position. Its death cause and family remain inspectable, and a living descendant can continue the story.

### Read the bigger picture

The **Evolution Observatory** exposes an individual's energy, physiology, diet, perception, neural decisions, mutations, and lineage. The **Evolution Atlas** provides Timeline, Species, Tree, and Ecology views for population history and emerging or extinct species.

Species are analytical labels. Observation, family highlighting, and historical charts never become neural inputs or alter simulation randomness.

## Saves and local data

Save archives preserve the world, organisms, neural genomes, lineage, Atlas history, and follow preferences. Captured decision history is session-only. Version 1 and version 2 archives are supported; older depth passages are flattened to traversable land when loaded.

| Data | Location |
| --- | --- |
| Quicksave | `%LOCALAPPDATA%\LFE-NNUE\world.lfesave` |
| Display / simulation settings | `%LOCALAPPDATA%\LFE-NNUE\settings.ini` |
| Bounded diagnostic log | `%LOCALAPPDATA%\LFE-NNUE\logs\life-engine.log` |
| Save chosen through the file dialog | Your selected path |

A previous save is retained as a `.bak` when an existing archive is replaced. No telemetry is transmitted automatically.

## Build from source

Requirements:

- **Windows 10 or 11, x64**
- **CMake 3.20+**
- **MinGW-w64 GCC with C++23 support**, available on `PATH`

Skia's library and license are included under `native/vendor/skia`. No npm install, JavaScript build, Nim compiler, WebAssembly, or Emscripten step is involved.

Run from the repository root in PowerShell:

```powershell
cmake -S native -B native/build-follow -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build native/build-follow -j 4
ctest --test-dir native/build-follow --output-on-failure
./native/build-follow/life_engine.exe
```

Close an executable before rebuilding that same path. Use a separate CMake build directory if you want to develop while another build is running.

## Testing and performance

The native CTest suites cover deterministic replay, neural behavior, ecology, gradual mortality, frame exchange, nonblocking UI commands, follow rendering, lineage, and save/load compatibility and recovery.

[Headless controls](native/HEADLESS.md) exercise the simulation and Skia renderer in memory without creating a window or sending desktop input.

Optional benchmark targets:

```powershell
cmake --build native/build-follow --target life_engine_native_benchmark life_engine_ecology_benchmark -j 4
```

- **Throughput benchmark:** measures simulation speed with population held fixed.
- **Ecology benchmark:** runs full lifecycle ticks twice and checks deterministic state and ecological accounting.
- **Regular-game / UI scripts:** exercise naturally growing plants, evolving populations, and UI interactions.

An average counter does not tell the whole story. Reports distinguish TPS, render FPS, distinct displayed ticks, frame-time percentiles, worst-frame gaps, and command-to-visible latency. Offscreen measurements exclude Windows input delivery and compositor latency.

See [UI latency measurements](native/UI_LATENCY.md), [regular-game measurements](native/FOLLOW_PERFORMANCE.md), and [benchmark history](native/benchmarks/RESULTS.md). These are development-machine measurements, not universal hardware guarantees. Performance validation is deferred when Battery Saver is on or charge is below 30%.

## Package a Windows release

```powershell
powershell -ExecutionPolicy Bypass -File scripts/package-native-release.ps1
```

The packager builds native code, runs tests, verifies the embedded renderer headlessly, and writes a standalone ZIP plus a SHA-256 checksum under `dist/`. Its staging directory includes the README, release notes, and Skia license.

## Repository guide

| Path | Contents |
| --- | --- |
| `native/src/` | Simulation, neural networks, native UI, rendering, persistence, and Atlas |
| `native/tests/` | Native regression tests, archive fixtures, and headless command scripts |
| `native/benchmarks/` | Throughput/ecology benchmarks and recorded results |
| `native/vendor/skia/` | Renderer dependency and its license |
| `reference/` | Historical parity fixtures |
| `scripts/package-native-release.ps1` | Windows release packaging |

The supported app is native C++/Skia. The retired TypeScript browser app, WebView launcher, and Nim fallback are no longer part of the project.
