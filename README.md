# Life Engine NNUE

A modern rewrite of Life Engine: a browser-based evolutionary ecosystem where organisms inherit body plans and sparse neural-network behavior.

## What changed

- Strict TypeScript replaces the original JavaScript, jQuery, and Webpack application.
- Vite 8 provides the development server and optimized production build.
- Vitest covers the TypeScript NNUE implementation.
- A deterministic TypeScript trainer produces the initial browser model in public/trained-brain.json.
- The interface keeps the original blue, black, white, and teal visual identity in a responsive layout.

## Run the application

~~~sh
npm install
npm run dev
~~~

Production verification:

~~~sh
npm test
npm run build
~~~

Retrain and verify the NNUE:

~~~sh
npm run train
npm run verify
~~~

The trainer uses backpropagation to teach the seed NNUE to seek food, avoid killers, and flock. Evolution then clones and mutates those weights independently for each organism. The entire toolchain runs through Node.js.

## Neural inputs

Each NNUE receives a sparse 5x5 neighborhood encoded as one active feature per square:

- empty space
- food
- walls
- its own body
- other organisms
- map boundaries

Killer cells have their own danger feature rather than being grouped with ordinary organisms. Two additional one-hot pairs encode hunger/reproduction readiness and damage. Outputs represent up, down, left, right, and wait. Only changed features update the hidden accumulator.

NNUEs exist only on organisms containing a mover cell. Static organisms use the original Life Engine rules without allocating or evaluating a neural network. The founding organism is the original three-cell body: one central mouth and two diagonal producers.

## Learned flocking and neural evolution

Flocking is not implemented with Boids rules or movement overrides. The NNUE input identifies nearby mover organisms and their headings, plus the current organism's own heading. TypeScript training teaches two examples through the network weights:

- move toward more distant movers (cohesion)
- match the heading of close movers (alignment)

The same policy continues to seek food and treats killer avoidance as higher priority in mixed scenes. The trainer's verification set currently scores 24/24 food seeking, 24/24 killer avoidance, 24/24 mixed-scene avoidance, and 96/96 flocking decisions.

Mover offspring deep-copy their parent's NNUE. Neural mutation occurs independently from body mutation using a heritable, self-mutating neural mutation rate, so successful movement strategies spread through normal reproduction while new strategies continue to emerge. There is no explicit fitness function in the live ecosystem: survival and reproductive success remain the selection pressure.

## Cell palette

The original cell colors are preserved: green food, orange mouths, white producers, blue movers, red killers, purple armor, gray walls, and a dark-blue world.

## Evolution Observatory

Choose the Inspect tool and click any living organism to open its observatory record. The panel shows:

- its persistent creature ID, generation, life state, age, body size, food, and damage
- its complete ancestor chain and all recorded descendants
- body, movement, birth-distance, neural-weight, and neural-mutation-rate changes
- respiratory stress, death cause, and inherited oxygen-tolerance, recovery, and frailty genes
- the live sparse 5x5 sensory input seen by mover organisms
- all five NNUE output probabilities and the selected action

Ancestors and descendants are clickable, and lineage records remain inspectable after an organism dies. Resetting the world intentionally begins a new lineage archive.

## Rich ecology and performance

Seeded worlds now have separate terrain, organism, resource, and quantity layers. Fertile land, plains, deserts, water, and mountains interact with seasons. Plant, scavenger, and mineral mouths occupy distinct niches; fixed-point energy drives metabolism and reproduction, minerals gate combat tissue, deaths leave carrion, and killer/armor durability wears into inert tissue.

Perception radius and channel masks are heritable. Larger sensory workloads cost energy and reduce decision cadence under a hard inference budget, while movement remains exclusively NNUE-directed. The UI includes ecology editors, overlays, measured TPS/FPS, population summaries, and expanded inspection data.

Atmospheric pressure suppresses reproduction before it becomes lethal. Respiratory stress accumulates separately for each organism from its genes, age, energy, temperature, and body size; the deterministic mortality queue can remove at most one critically stressed organism every ten simulation ticks.

## Evolution Atlas

Press **A** or click **ATLAS** to open native long-run analytics without pausing the simulation. The Timeline tab preserves 24,000 sixty-tick samples with population, resource, atmosphere, physiology, and death-rate history plus deterministic season, peak, crash, recovery, shortage, speciation, extinction, and regeneration markers. The Species table selects representatives, the Tree follows species ancestry, and Ecology summarizes the current resource and population balance. Cycle the world overlay to **Species** to color living organisms by their analytical species; this never changes NNUE inputs or physics.

Species are classified every 300 ticks from rotation-normalized bodies, diet, perception, physiology, behavior genes, and cached NNUE signatures. IDs persist across scans, extinctions remain inspectable, and retained history is bounded. Reset Life confirms before clearing the run history; New Seeded World keeps the history and adds a regeneration marker.

The TypeScript worker uses shared copy-on-write genomes, batched climate rows, renderer acknowledgements, and movement fast paths. `npm run benchmark` runs its strict 10k-at-60-TPS gate. If that gate fails, the optional Nim backend compiles directly to JavaScript—without Wasm or Emscripten:

~~~sh
npm run build:nim
npm run benchmark:nim
~~~

## Native C++ application

The dependency-free Windows build uses C++23, Win32, GDI, a fixed-step 60 TPS loop, seeded terrain/resources, 10,000 neural organisms, energy metabolism, and click-to-inspect telemetry. Build it with MinGW GCC:

~~~sh
cmake -S native -B native/build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build native/build -j 4
~~~

Run `native/build/life_engine.exe` to launch the native Win32/Skia application. Keep `libSkiaSharp.dll` beside the executable when distributing it. The renderer, controls, simulation, NNUEs, Atlas, and observatory run in native code; no WebView is used. `life_engine_webview_legacy.exe` is retained only as an excluded legacy target.

The two native benchmark gates deliberately answer different questions:

~~~sh
native/build/life_engine_native_benchmark.exe 60
native/build/life_engine_ecology_benchmark.exe 12000
~~~

The first holds lifecycle population fixed and measures throughput. The second runs a fixed number of full-ecology ticks twice and checks deterministic state, births, deaths, resources, timeline bounds, and species history.
