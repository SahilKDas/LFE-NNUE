import { performance } from "node:perf_hooks";
import { Simulation } from "../src/core/simulation";

const simulation = new Simulation({ width: 1024, height: 640, foodChance: 0, lifespan: 1_000_000 });
simulation.populateBenchmark(10_000);
simulation.step(10);
const start = performance.now(); simulation.step(60); const elapsed = performance.now()-start;
const tps = 60_000/elapsed;
console.log(`10,000 one-cell movers: ${tps.toFixed(1)} TPS (${elapsed.toFixed(1)} ms for 60 ticks)`);
if (tps < 60) throw new Error(`Benchmark below 60 TPS target: ${tps.toFixed(1)} TPS`);
