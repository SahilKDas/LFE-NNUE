# Flat world and responsive native controls

The active elevation/habitat/moisture layers, uphill costs, caves, bridges, rain/cloud rendering, and downhill drainage are removed. Terrain, seasons, nutrients, physiology, resources, NNUE movement, and the original theme/fonts remain. Version 1/2 save columns are retained only for compatibility; legacy cave/bridge passages load as plains. Old depth controls and depth-specific history are cleared.

## UI changes

- The presentation scheduler waits on its own thread; Win32 painting never sleeps for a frame deadline.
- Input invalidation repaints immediately instead of waiting for the next scheduled paint.
- Painting, selection, follow, reset, regeneration, and species selection enqueue coordinator work. UI callbacks receive copied results; they never wait for the world write lock. Commands wake the coordinator even at low requested TPS, and execute while paused.
- Save/load completion carries restored view state, so the UI does not reacquire the simulation lock.
- Atlas data and membership arrive through immutable snapshots. The renderer no longer scans the organism collection or copies the entire selected lineage every frame.
- TPS slider dragging works continuously. Painting is clipped to the world viewport. The default cap remains 30 FPS; buttons still cover 30–360 FPS.

## Validation

`ctest --test-dir native/build-follow --output-on-failure` includes command ordering/thread ownership, controls submitted while the physics lock is held, frame acknowledgment propagation, deterministic replay, flat-world passability, native parity, both legacy save versions, save recovery, and follow rendering.

`native/tests/ui_latency.commands` starts the ordinary game and runs 10 seconds of offscreen native rendering at requested 60 TPS/FPS with plants, evolving creatures, births and deaths enabled. Interactions cycle overlays and Atlas tabs, paint resources/nutrients, inspect/follow organisms, pan, and zoom. Command acknowledgments are carried by the completed world frame; an unrelated snapshot cannot acknowledge pending work.

Runs on the development laptop, plugged in at 100%, Battery Saver off:

| Duration | TPS / FPS | Peak population | Feedback p95 | Command-to-visible p95 | Frame p99 / maximum |
| --- | --- | --- | --- | --- | --- |
| 10 seconds | 60.0 / 60.0 | 2,314 | 15.9 ms | 53.8 ms | 22.0 / 29.8 ms |
| 30 seconds | 60.0 / 60.0 | 9,297 | 6.4 ms | 53.7 ms | 20.0 / 35.6 ms |

Both runs finished with no pending commands. The 30-second run recorded 11,326 births and 1,967 retained dead records. The maximum frame is reported deliberately: average 60 FPS does not mean every frame took 16.7 ms.

These tests render in memory and exercise shared controls, not actual Windows mouse delivery or compositor presentation. Inputs are injected at frame boundaries. They do not create windows or interact with another app. Reports live in `native/benchmarks/results/flat-ui-2026-10-28-*.json`. Performance scripts defer before warm-up and during execution when Battery Saver is enabled or battery falls below 30%.
