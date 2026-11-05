# Evolution Research Suite implementation

The accepted implementation covers History/rewind, single-active-branch experiments, genome comparison, a creature/world library, an evidence journal, evolvable eggs/juveniles, anonymous chemical signals, and gradual local disease.

Defaults: all biology active; egg-laying evolves alongside immediate offspring; two signal channels; one active branch; 128 MiB automatic-capture RAM and 2 GiB managed cache; carrion-origin disease; paced disease mortality. Preserve native C++/Skia, deterministic replay, single-world rendering, looping physics, and five movement outputs.

## Checkpoints

| Checkpoint | Status |
| --- | --- |
| Archive v3; immutable neural weights, independent runtime caches | Complete; ten native tests passed; commit f43bb48 |
| Typed commands; genome IO/comparison; frozen states; external neural blobs | Complete; eleven native tests pass |
| Bounded checkpoints, cancellation, preview/return, branches, paired experiments | Backend complete; exact replay, branch isolation, paired-arm and limit tests pass |
| Research UI, journal and library | Native five-page workspace and headless workflows pass; previews/persistence polish remains |
| Eggs, juvenile growth, signals, neural v5, disease/immunity | Pending |
| Integrated regression, replay, memory and eligible-power performance validation | Pending |

Build isolated from the running app in `native/build-follow/research`. Use headless controls only. Do not run timed performance validation with Battery Saver on or battery below 30%. Commit buildable checkpoints; push validated milestones. Preserve all user saves and genuine v1/v2 archive fixtures.

## Validation notes / remaining work

- Latest whole suite: 12 tests passed, including research backend and native Research UI workflows.
- Build uses a shared optimized static simulation library (55ca947), rather than recompiling the core separately for every executable.
- Remaining hardening: global cache quota across sessions, durable branch/catalog metadata and preview thumbnails, bounded metadata size, off-thread import reads, typed replay validation for missing/pruned command history, and staged biology validation.
- UI entry: Research button / R; History, Experiments, Genomes, Journal, Library. Existing persistence work shares the Research I/O worker to keep the total within twelve threads.
