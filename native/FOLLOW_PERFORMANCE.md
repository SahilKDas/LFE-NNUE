# Follow-camera performance validation

The completed-frame exchange removes the normal world renderer's dependency on
the simulation read lock. Three reusable buffers carry layer-specific dirty
versions; adjacent tile copies are merged into horizontal runs. Diagnostics
refresh at most ten times per second. Family boundaries invalidate on ownership
or family-membership changes, and a bounded text cache preserves exact glyphs.

All five native CTest suites pass, including concurrent frame publication,
held-frame protection, reset epochs, layer invalidation, captured decisions,
pixel-coherent rendering caches, replay, save/load, and reference parity.

## Measurements

Measured on this development laptop using headless Skia rasterization, with
10,000 initial diverse organisms, active ecology, and a large highlighted family.
These measure offscreen rendering and scripted command latency, not physical
display presentation or desktop input. The user's existing application was left
untouched. Results depend on concurrent laptop workload.

An eight-second comparison using the same scenario measured:

| Measurement | Live-state lock path | Completed-frame exchange |
| --- | ---: | ---: |
| Simulation TPS | 59.27 | 59.46 |
| Render FPS | 59.39 | 59.59 |
| Distinct simulation ticks displayed/second | 20.99 | 43.61 |
| Snapshot p95 (ms) | 7.95 | 3.48 |
| Command-to-frame p95 (ms) | 22.62 | 13.22 |

The 60-second follow scenario measured 53.95 TPS, 60.00 render FPS,
42.47 distinct displayed ticks/second, 33.85 ms p99 frame interval
(29.54 FPS 1% low), and 14.61 ms p95 command-to-frame latency.
The population changed naturally during this run; it is not a sustained fixed
10,000-population throughput test. A short regression build/test run overlapped
part of this measurement, so it is also not an isolated machine result.

**The strict performance acceptance gate remains unmet.** In particular, average
render FPS does not establish smooth frame pacing, and repeated rendering of
one completed state does not establish simulation progress. Reports retain
separate `render_fps`, `snapshot_fps`, and `unique_tick_fps` measurements and do
not relax the 58 TPS / 55 FPS / 45 FPS 1% low thresholds.

Reproduce with `native/tests/follow_performance.commands`, described in
[HEADLESS.md](HEADLESS.md). `benchmark-sync` permits direct comparison with the
old lock-based handoff. Keep builds and other benchmark processes idle for an
isolated comparison.
