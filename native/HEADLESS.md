# Headless native controls

The native executable can run the same simulation, camera, follow card, and Skia renderer without creating a window. It never sends keyboard or mouse input, activates another application, reads desktop screenshots, or changes the ordinary app's settings. Exported PNGs are frames rendered into memory, not desktop captures.

Build into `native/build-follow` when another copy of the app is open:

```powershell
cmake -S native -B native/build-follow -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build native/build-follow --target life_engine life_engine_watch_tests life_engine_depth_tests life_engine_native_tests -j 4
ctest --test-dir native/build-follow --output-on-failure
```

Run a command script with `--headless-script=PATH`. Quote the value if the path contains spaces. Relative command paths resolve against the process working directory. For example:

```powershell
$scriptPath = (Resolve-Path native/tests/follow_smoke.commands).Path
Start-Process -FilePath native/build-follow/life_engine.exe -ArgumentList "--headless-script=`"$scriptPath`"" -WorkingDirectory native/build-follow -WindowStyle Hidden -Wait
```

Results are written into the working directory's `headless/` folder. The final `<script filename>.report.json` records functional success, the failing script line if any, and performance success separately. Exit codes are 0 for success, 1 for a failed command/assertion, 2 for missed performance thresholds, and 3 for renderer initialization failure. No headless operation overwrites a normal quicksave unless its script explicitly names that file.

## Commands

Use one command per line; lines starting with `#` are comments. Paths can be quoted; forward slashes are recommended inside quoted Windows paths.

| Command | Behavior |
| --- | --- |
| `seed N` | Create a fixed-seed, diverse benchmark population of 1–10,000 organisms. Includes a synthetic large genealogy for family-rendering stress while retaining independent neural families. This is a test fixture, not a change to normal world generation. |
| `select ID` / `select family` | Select an organism or the fixture's family root, clearing previous follow state. |
| `follow` / `release` | Use the same camera start/release controls as the app. |
| `descendant` | After death, follow the oldest living direct child, otherwise the oldest living descendant; ID breaks ties. |
| `family on/off`, `compact on/off`, `atlas on/off`, `weather on/off` | Change local view controls. |
| `exchange on/off` | Exercise the production three-buffer frame handoff during scripted, non-realtime checks. `benchmark` enables it. |
| `zoom Z` / `pan DX DY` | Set zoom (0.2–12), or pan in screen pixels and release following. |
| `overlay NAME` | Choose terrain, resources, productivity, climate, species, nutrients, decomposition, or habitat. |
| `resize W H` | Set the offscreen viewport, within the app's supported desktop size range. |
| `step N` | Advance exactly N simulation ticks, independently of the saved pause setting. |
| `frame [N] [milliseconds]` | Render frames and advance camera smoothing by the specified frame duration; default is one frame at 60 FPS. Simulation does not advance. |
| `history N` | Inspect an older captured decision, 0–127 decisions back; zero is the latest. |
| `paint-selected-mountain` | Exercise the actual terrain-painting operation at the watched creature's position. Useful for testing the ordinary terrain death and continuation path. |
| `inspect-screen X Y` | Inspect using the same wrapped screen-to-world coordinates as the app. |
| `save PATH` / `load PATH` | Save/load the complete world and view using the production archive implementation. |
| `export PATH.png` | Render and export a PNG. |
| `state PATH.json` | Export camera, population, family counts, death cause, captured decision pose/tick, exact feature IDs, and neural outputs. |
| `mark-camera` | Record camera offsets for a later hold assertion. |
| `benchmark SECONDS PATH.json` | Run concurrent simulation and offscreen rendering, targeting 60 TPS/FPS for 1–120 seconds; report frame percentiles and command-to-frame latency. |
| `benchmark-sync SECONDS PATH.json` | Run the same workload through the legacy live-state read-lock path for comparison. |

Assertions: `assert following on/off`, `alive on/off`, `wrapped on/off`, `compact on/off`, `target ID/family`, `decisions-min N`, `family-min N`, `camera-held`, `snapshot-coherent`, `palette-coherent`, `family-pixels-coherent`, `family-edges-coherent`, and `text-cache-coherent`. Prefix each property with `assert`. These checks cover exact captured decisions, dirty-tile and family-mask invalidation, and pixel-identical cached versus uncached text rendering.

The checked-in smoke script covers save/load, camera release, Atlas suspension, responsive layout, exact captured decisions, death holding, and descendant continuation. `follow_performance.commands` starts with 10,000 diverse organisms and keeps reproduction, mortality, mutation, terrain, resources, and climate active while highlighting a large family. Population may change naturally; the report includes initial, minimum, and final population. Throughput, smoothness, and interaction results are reported separately, retaining the 58 TPS, 55 FPS, 45 FPS 1% low, and under-100 ms latency targets. Its benchmark reports offscreen raster performance; it does not claim to measure monitor presentation or physical desktop-input latency.

The frame handoff has exactly three reusable buffers. Rendering consumes completed ticks and never needs the live physics read lock; old queued frames can be replaced rather than accumulating a backlog. The benchmark separately reports `render_fps`, `snapshot_fps`, and `unique_tick_fps` so drawing an unchanged frame cannot masquerade as visible simulation progress. Fonts and text blobs are bounded caches; pixel-coherence assertions preserve the original typography.

Headless tests use their own process and working-directory files. Below-normal process priority is useful when sharing the laptop, but its scheduling delays should not be mistaken for an isolated normal-priority throughput measurement. The suite never closes or controls an already-running Life Engine window.
