import { createHash } from "node:crypto";
import { Nnue, type BrainSeed } from "./nnue";
import { mulberry32 } from "./random";
import { Simulation } from "./simulation";

export const PARITY_CHECKPOINTS = [0, 1, 10, 100, 1_000] as const;

export interface ParityFrame {
  tick: number;
  stateHash: string;
  organisms: number;
  record: number;
  generation: number;
  largest: number;
  season: string;
  seasonPhase: number;
  temperature: number;
  fertility: number;
  averageStress: number;
  averageEnergy: number;
  nnueEvaluations: number;
}

export interface ParityTrace {
  schema: 1;
  randomSeed: number;
  worldSeed: number;
  width: number;
  height: number;
  frames: ParityFrame[];
  neural: {
    features: number[];
    outputs: number[];
    action: number;
  };
}

function stateHash(simulation: Simulation): string {
  const hash = createHash("sha256");
  hash.update(simulation.cells);
  hash.update(new Uint8Array(simulation.owners.buffer));
  hash.update(simulation.terrain);
  hash.update(simulation.resources);
  hash.update(new Uint8Array(simulation.resourceAmount.buffer));
  return hash.digest("hex");
}

function neuralFixture(random: () => number): ParityTrace["neural"] {
  const brain = new Nnue(undefined, random);
  const features = [0, 21, 65, 203, 1_601, 1_606, 1_612];
  const outputs = Array.from(brain.evaluate(features));
  return { features, outputs, action: brain.action(features) };
}

export function recordParity(randomSeed = 0x51a7e, worldSeed = 0x1f3d5b79): ParityTrace {
  const previousRandom = Math.random;
  const random = mulberry32(randomSeed);
  Math.random = random;
  try {
    const width = 96, height = 60;
    const simulation = new Simulation({ width, height, foodChance: 0.2, lifespan: 500, worldSeed });
    const frames: ParityFrame[] = [];
    let completed = 0;
    for (const tick of PARITY_CHECKPOINTS) {
      simulation.step(tick - completed);
      completed = tick;
      const metrics = simulation.metrics();
      frames.push({
        tick,
        stateHash: stateHash(simulation),
        organisms: metrics.organisms,
        record: metrics.record,
        generation: metrics.generation,
        largest: metrics.largest,
        season: metrics.season,
        seasonPhase: metrics.seasonPhase,
        temperature: metrics.temperature,
        fertility: metrics.fertility,
        averageStress: metrics.averageStress,
        averageEnergy: metrics.averageEnergy,
        nnueEvaluations: metrics.nnueEvaluations,
      });
    }
    return { schema: 1, randomSeed, worldSeed, width, height, frames, neural: neuralFixture(mulberry32(randomSeed ^ 0x4e4e5545)) };
  } finally {
    Math.random = previousRandom;
  }
}

export function seedTopology(seed: BrainSeed): object {
  return {
    schemaVersion: seed.schemaVersion ?? 1,
    inputRows: seed.inputWeights.length,
    hiddenWidth: seed.hiddenBias.length,
    outputRows: seed.outputWeights.length,
    outputWidth: seed.outputWeights[0]?.length ?? 0,
    outputBias: seed.outputBias.length,
  };
}
