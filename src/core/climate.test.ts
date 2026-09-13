import { describe, expect, it } from "vitest";
import { climateAt, CLIMATE_CYCLE_TICKS, thermalStressDelta } from "./climate";
import { CLIMATE_FEATURE_OFFSET, HIDDEN_SIZE, INPUT_SIZE, LEGACY_INPUT_SIZE, MAX_INFERENCE_BUDGET, Nnue, SenseChannel, clampPerception } from "./nnue";

describe("seasonal climate",()=>{
  it("has opposite hemispheres, a stable equator, bounded fertility, and exact wraparound",()=>{
    const north=climateAt(0,101,6000), south=climateAt(100,101,6000), equator=climateAt(50,101,6000);
    expect(north.temperature).toBeGreaterThan(south.temperature); expect(equator.temperature).toBeCloseTo(.5);
    expect(north.fertility).toBeLessThanOrEqual(1.35); expect(south.fertility).toBeGreaterThanOrEqual(.35);
    expect(climateAt(10,101,0)).toEqual(climateAt(10,101,CLIMATE_CYCLE_TICKS));
  });
  it("accumulates stress outside comfort and recovers inside it",()=>{expect(thermalStressDelta(.05)).toBeGreaterThan(0);expect(thermalStressDelta(.5)).toBeLessThan(0);});
});
describe("evolvable perception",()=>{
  it("bounds radius and clamps sensory work to the inference budget",()=>{const perception=clampPerception(99,255);expect(perception.radius).toBeGreaterThanOrEqual(1);expect(perception.radius).toBeLessThanOrEqual(4);expect(perception.cost).toBeLessThanOrEqual(MAX_INFERENCE_BUDGET);});
  it("preserves requested channels when they fit",()=>{const mask=SenseChannel.Resources|SenseChannel.Terrain;expect(clampPerception(2,mask)).toMatchObject({radius:2,channels:mask,cost:48});});
});

describe("brain schema migration",()=>{
  it("pads legacy seeds with zeroed climate rows",()=>{
    const current=new Nnue().toSeed(); const legacy={...current,schemaVersion:1,inputWeights:current.inputWeights.slice(0,LEGACY_INPUT_SIZE)};
    const migrated=new Nnue(legacy).toSeed(); expect(migrated.inputWeights).toHaveLength(INPUT_SIZE);
    for(const row of migrated.inputWeights.slice(CLIMATE_FEATURE_OFFSET)) expect(row).toEqual(Array(HIDDEN_SIZE).fill(0));
  });
  it("allows mutation of inherited climate weights without changing the parent",()=>{
    const parent=new Nnue(); const child=parent.clone(); child.mutate(1,.1);
    expect(parent.inputWeights[CLIMATE_FEATURE_OFFSET]).toEqual(new Float32Array(HIDDEN_SIZE));
    expect(child.inputWeights[CLIMATE_FEATURE_OFFSET]).not.toEqual(new Float32Array(HIDDEN_SIZE));
  });
  it("rejects malformed topology",()=>{const seed=new Nnue().toSeed();seed.hiddenBias.pop();expect(()=>new Nnue(seed)).toThrow(/dimensions/);});
});
