import { describe, expect, it } from "vitest";
import { HIDDEN_SIZE, INPUT_SIZE, Nnue, OUTPUT_SIZE } from "./nnue";
import { Simulation } from "./simulation";
import { CellType } from "./types";

describe("Nnue", () => {
  it("returns stable output when sparse features do not change", () => {
    const brain = new Nnue();
    expect(Array.from(brain.evaluate([0, 7, 18]))).toEqual(Array.from(brain.evaluate([0, 7, 18])));
  });

  it("deep clones heritable weights", () => {
    const parent = new Nnue();
    const child = parent.clone();
    expect(child).not.toBe(parent);
    expect(child.toSeed()).toEqual(parent.toSeed());
  });

  it("can evolve an inherited brain independently of its parent", () => {
    const parent = new Nnue();
    const child = parent.clone();
    child.mutate(1, 0.1);
    expect(child.toSeed()).not.toEqual(parent.toSeed());
  });

  it("has the expected compact topology", () => {
    const seed = new Nnue().toSeed();
    expect(seed.hiddenBias).toHaveLength(HIDDEN_SIZE);
    expect(seed.outputBias).toHaveLength(OUTPUT_SIZE);
    expect(seed.inputWeights).toHaveLength(INPUT_SIZE);
  });
});

describe("original Life Engine physics", () => {
  it("restores the original static founding organism without an NNUE", () => {
    const simulation = new Simulation({ width: 21, height: 21, foodChance: 0.04, lifespan: 100 });
    const center = 10 + 10 * simulation.width;
    expect(simulation.cells[center]).toBe(CellType.Mouth);
    expect(simulation.cells[center - simulation.width - 1]).toBe(CellType.Producer);
    expect(simulation.cells[center + simulation.width + 1]).toBe(CellType.Producer);
    expect(simulation.neuralOrganismCount()).toBe(0);
  });

  it("exposes the founding creature through the lineage observatory", () => {
    const simulation = new Simulation({ width: 21, height: 21, foodChance: 0, lifespan: 100 });
    const id = simulation.organismIdAt(10, 10);
    expect(id).toBeDefined();
    const inspection = simulation.inspect(id!);
    expect(inspection).toMatchObject({
      id,
      generation: 0,
      alive: true,
      cells: 3,
      isMover: false,
      ancestors: [],
      descendants: [],
      mutations: [],
    });
  });
});
