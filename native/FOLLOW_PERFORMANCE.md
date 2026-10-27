# Regular-game performance validation

The regular startup game has its own headless test. It starts with the normal
founder and runs plants, reproduction, mutation and mortality unchanged. It does
not seed a benchmark population, inject resources, or disable lifecycle rules.
One uninterrupted thirty-second run reports three ten-second windows using the
native Skia renderer at 1920×1080 and a requested 60 TPS/FPS.

## Measured October 27, 2026

Raw results: [regular-game-2026-10-27.json](benchmarks/results/regular-game-2026-10-27.json).

| Window | TPS | Render FPS | Distinct displayed ticks/s | Frame p99 | Longest visible-tick gap |
| --- | ---: | ---: | ---: | ---: | ---: |
| 0–10 s | 59.9 | 59.9 | 58.3 | 17.218 ms | 33.801 ms |
| 10–20 s | 60.0 | 60.0 | 57.6 | 17.175 ms | 33.712 ms |
| 20–30 s | 60.0 | 60.0 | 59.0 | 17.219 ms | 50.067 ms |

Population rose naturally from one founder to 31,128 and finished at 11,595.
There were 35,918 births. The report's deaths field currently counts retained
dead lineage records, not total mortality; it reaches the 20,000 retention limit.
The overall 1% frame low was 58.115 FPS; no frame interval exceeded 20 ms.
The slowest simulation tick took 37.748 ms at tick 1200. Its classification work
took 25.316 ms and overlapped independent environment work. Overlapping phase
durations must not be added together.

**The strict acceptance gate still fails.** Smooth raster pacing does not mean
that each frame contains a new simulation state. The middle window misses the
distinct-tick threshold, and the final window contains a 50 ms world-update gap.
These failures remain visible instead of claiming success from average counters.

These measurements use offscreen rendering; physical monitor presentation and
desktop-input latency are unmeasured. No existing application window was
controlled. The regular-game test reports interaction latency as untested.

## Changes under test

Three helpers share independent analysis jobs. Coarse environment work overlaps
classification. Distance-equivalent species searches, atmosphere totals, direct
owner slots, reusable pruning buffers, sparse neural mutation plans, deferred
brain creation and runtime-selected AVX2 inference reduce repeated work. Scalar
inference remains the reference; interactions and random draws stay serial.

Rendering retains exact cell colors, caches terrain backgrounds, and uses an
exact one-pixel row-copy path. Completed immutable snapshots are presented at
paced deadlines. Telemetry separates frames, snapshots and distinct ticks.
High-resolution waits do not busy-spin or change the laptop's power plan.

Reproduce with native/tests/regular_game_performance.commands, described in
[HEADLESS.md](HEADLESS.md). Keep builds and other tests idle during measurement.
The separate restart script tests cold resumes; the diverse-population follow
benchmark remains available separately.
