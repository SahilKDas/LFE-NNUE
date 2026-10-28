# Regular-game smoothness validation

The normal startup world now passes the strict smoothness gate in independent
30-second and 60-second headless runs. Both start with one founder and retain
plants, evolution, reproduction, mortality, terrain, resources and climate.
No benchmark population is inserted and no lifecycle rules are disabled.

## Measured October 27, 2026

Power eligibility was confirmed at 48% battery with Battery Saver off before
measurement. The runner checks eligibility before warm-up, during stepping and
at completion. These measurements use native Skia offscreen rendering at
1920×1080, requested 60 TPS and 60 FPS. Physical monitor presentation and desktop
input latency are unmeasured.

Raw reports:

- [30-second regular game](benchmarks/results/regular-game-2026-10-27-optimized-30.json)
- [60-second confirmation](benchmarks/results/regular-game-2026-10-27-optimized-60.json)

| Measurement | 30-second run | 60-second confirmation |
| --- | ---: | ---: |
| Measured TPS | 59.985 | 59.998 |
| Render FPS | 59.985 | 59.998 |
| Distinct displayed ticks/second | 59.885 | 59.948 |
| Frame interval p99 | 17.208 ms | 17.262 ms |
| 1% frame low | 58.111 FPS | 57.929 FPS |
| Longest frame / visible-update gap | 18.246 ms | 20.422 ms |
| First visible update after start | 67.099 ms | 66.853 ms |
| Every ten-second window passes | Yes | Yes |

The reported 1% frame low is the reciprocal of the p99 frame interval.

The demanding 10–20 and 20–30 second windows independently maintained 60 TPS,
60 render FPS and 60 distinct displayed ticks per second. Observed population
peaked above 31,000. This verifies visible simulation progression as well as
frame presentation cadence.

Population changes naturally. It reaches zero late in the 60-second run; the
last portion therefore does not establish sustained dense-population throughput.
The dense 10–30 second windows and the independent 30-second run provide that
stress coverage. The report's legacy `deaths` field counts retained dead lineage
records, which reach the 20,000 retention limit; it is not total mortality.

## Implementation

At matching simulation/render rates, three fixed snapshot buffers deliver
completed ticks in publication order. This adds a short bounded presentation
cushion. The longest internal tick was 39.801 ms in the 30-second run; buffered
completed states absorbed that variation while every busy-window frame advanced
the visible world. Exact cells, original colors, fonts and physics remain intact.
Other render rates continue taking the newest completed state.

Owner IDs are transferred when family or species rendering needs them. Enabling
those views hydrates the complete ownership layer before it is used. Ordinary
terrain rendering avoids that large recurring copy. Terrain palette generation
also avoids unused climate calculations and reuses row values for climate views.

Historical strings and counters live outside the hot organism record. Cold data
retains value semantics and the archive field order is unchanged. Compact owner
sensory metadata, cached body descriptors, persistent species queries and an
exact body/signature search bound reduce scattered reads and repeated work.
Metadata buffers are reserved before play. Helpers use HighQoS scheduling hints;
thread priority and the laptop power plan remain unchanged.

## Correctness and reproduction

All six native CTest suites, 22 TypeScript tests, and the production TypeScript build pass. Additional compatibility checks compare original
regular-game saves against the optimized engine at ticks 600 and 1,200, including
world layers, randomness, organism fields, neural weights/runtime and species.
Tests cover ownership hydration, ordered frame delivery, immutable leased frames,
lineage copying/pruning, save/load, exact raster caches and captured decisions.

Run `native/tests/regular_game_performance.commands` using the controls in
[HEADLESS.md](HEADLESS.md). The default game FPS setting remains 30; select 60 in
the FPS buttons for the tested presentation rate. Performance validation defers
when Battery Saver is enabled, battery charge is below 30%, or power status is
unknown. Deferrals contain no accepted performance score.
