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
- the live sparse 5x5 sensory input seen by mover organisms
- all five NNUE output probabilities and the selected action

Ancestors and descendants are clickable, and lineage records remain inspectable after an organism dies. Resetting the world intentionally begins a new lineage archive.
