# Evolution Research Suite

Open Research with **R**. History supports checkpointing, cancellable seeks, return to present, and forks. The Experiments page runs equal-duration baseline and intervention branches sequentially and shows population by ticks since the fork. Genomes compares retained specimens, Journal indexes recorded events, and Library stores checksummed creature genomes and world archives.

New and migrated worlds run development, chemical signals, and local disease. Save version 3 preserves the new biology and both deterministic RNG streams. The v1/v2 reader preserves the existing neural prefix and initializes appended schema 5 weights to zero. Migration tests use genuine old archives, including one with a schema 4 movement network.

Automatic history captures every 600 ticks. Capture checks the 128 MiB additional memory estimate before cloning. Managed checkpoint files and shared brain blobs use a 2 GiB disk budget across sessions. Explicit library files have a separate retention policy. Protected branch heads from earlier sessions can be opened from the Library. The active rewind index is session-local.

The native Research I/O worker also handles ordinary save/load work. Replay and experiments use that worker while the live coordinator is paused. The renderer reads published snapshots. The original theme, exact cells, looping borders, and five neural movement outputs remain in use.

## Verification

Use CTest for the native regression suites. The headless Research script exercises the real workspace controller and new overlays. The headless regular and diverse workloads use the same native renderer and only run timed measurements with eligible battery status. See [HEADLESS.md](HEADLESS.md).

The isolated 1080p headless check passed all 13 functional suites. At 100% charge with Battery Saver off, the regular 10-second workload measured 59.86 TPS, 59.96 FPS, 19.73 ms frame p99, and 53.52 ms input-to-visible p95. Its report does not measure distinct displayed ticks. The 12-second diverse workload started with 10,000 organisms and measured 59.98 TPS, 59.98 FPS, 55.90 distinct displayed ticks/s, 17.16 ms frame p99, and 17.01 ms command-to-frame p95. The longest visual gap was 50.02 ms and checkpoint capture took 28.85 ms. The diverse run missed the smoothness gate.
