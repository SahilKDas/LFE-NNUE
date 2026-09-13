import { mkdir, writeFile } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import { CLIMATE_FEATURE_OFFSET, Nnue, POSITION_COUNT } from "../src/core/nnue";

function mulberry32(seed: number): () => number {
  return () => {
    seed |= 0;
    seed = seed + 0x6d2b79f5 | 0;
    let value = Math.imul(seed ^ seed >>> 15, 1 | seed);
    value = value + Math.imul(value ^ value >>> 7, 61 | value) ^ value;
    return ((value ^ value >>> 14) >>> 0) / 4294967296;
  };
}

function sample(
  foodSquare = -1,
  killerSquare = -1,
  moverSquare = -1,
  moverDirection = 0,
  ownDirection = 0,
): number[] {
  const features: number[] = [];
  for (let square = 0; square < POSITION_COUNT; square++) {
    const category = square === killerSquare ? 5
      : square === foodSquare ? 1
      : square === moverSquare ? 7 + moverDirection
      : 0;
    features.push(square * 11 + category);
  }
  features.push(264, 266, 268 + ownDirection);
  return features;
}

function coordinates(square: number): [number, number] {
  let cursor = 0;
  for (let y = -2; y <= 2; y++) {
    for (let x = -2; x <= 2; x++) {
      if (x === 0 && y === 0) continue;
      if (cursor++ === square) return [x, y];
    }
  }
  throw new RangeError("Invalid sensor square");
}

function targetFor(square: number): number {
  const [x, y] = coordinates(square);
  return Math.abs(x) > Math.abs(y) ? (x < 0 ? 2 : 3) : (y < 0 ? 0 : 1);
}

function escapeTargetFor(square: number): number {
  return [1, 0, 3, 2][targetFor(square)]!;
}

function isAdjacent(square: number): boolean {
  const [x, y] = coordinates(square);
  return Math.max(Math.abs(x), Math.abs(y)) === 1;
}

function predicted(brain: Nnue, features: number[]): number {
  return brain.action(features);
}

const random = mulberry32(0xc0ffee);
const brain = new Nnue(undefined, random);

for (let epoch = 0; epoch < 500; epoch++) {
  for (let batch = 0; batch < 96; batch++) {
    const food = Math.floor(random() * 24);
    brain.train(sample(food), targetFor(food), 0.008);

    let killer = Math.floor(random() * 24);
    brain.train(sample(-1, killer), escapeTargetFor(killer), 0.008);
    while (killer === food) killer = Math.floor(random() * 24);
    brain.train(sample(food, killer), escapeTargetFor(killer), 0.008);

    const neighbor = Math.floor(random() * 24);
    const heading = Math.floor(random() * 4);
    const ownHeading = Math.floor(random() * 4);
    brain.train(
      sample(-1, -1, neighbor, heading, ownHeading),
      isAdjacent(neighbor) ? heading : targetFor(neighbor),
      0.008,
    );
  }
}

let foodCorrect = 0;
let escapeCorrect = 0;
let mixedCorrect = 0;
let flockCorrect = 0;
for (let square = 0; square < 24; square++) {
  foodCorrect += Number(predicted(brain, sample(square)) === targetFor(square));
  escapeCorrect += Number(predicted(brain, sample(-1, square)) === escapeTargetFor(square));
  mixedCorrect += Number(predicted(brain, sample((square + 12) % 24, square)) === escapeTargetFor(square));
  for (let heading = 0; heading < 4; heading++) {
    const target = isAdjacent(square) ? heading : targetFor(square);
    flockCorrect += Number(predicted(brain, sample(-1, -1, square, heading, (heading + 1) % 4)) === target);
  }
}

if (foodCorrect < 20 || escapeCorrect < 20 || mixedCorrect < 20 || flockCorrect < 80) {
  throw new Error(`Training missed thresholds: food ${foodCorrect}/24, escape ${escapeCorrect}/24, mixed ${mixedCorrect}/24, flock ${flockCorrect}/96`);
}

const output = resolve("public/trained-brain.json");
for (let row = CLIMATE_FEATURE_OFFSET; row < brain.inputWeights.length; row++) brain.inputWeights[row]!.fill(0);
await mkdir(dirname(output), { recursive: true });
await writeFile(output, JSON.stringify(brain.toSeed()));
console.log(`trained policy: food ${foodCorrect}/24, killer avoidance ${escapeCorrect}/24, mixed-scene avoidance ${mixedCorrect}/24, flocking ${flockCorrect}/96`);
