import { describe,expect,it } from "vitest";
import { generateWorld,terrainPassable,terrainProductivity } from "./world";
import { TerrainType } from "./types";
import { ResourceType } from "./types";
import { Simulation } from "./simulation";
describe("seeded world",()=>{
  it("is deterministic and seed-sensitive",()=>{expect(generateWorld(64,40,7)).toEqual(generateWorld(64,40,7));expect(generateWorld(64,40,7).terrain).not.toEqual(generateWorld(64,40,8).terrain);});
  it("contains all biomes at representative size",()=>{const types=new Set(generateWorld(256,160,42).terrain);for(let type=0;type<=TerrainType.Mountain;type++)expect(types.has(type)).toBe(true);});
  it("makes water and mountains impassable and unproductive",()=>{expect(terrainPassable(TerrainType.Water)).toBe(false);expect(terrainPassable(TerrainType.Mountain)).toBe(false);expect(terrainProductivity(TerrainType.Water)).toBe(0);expect(terrainProductivity(TerrainType.Fertile)).toBeGreaterThan(terrainProductivity(TerrainType.Desert));});
});
describe("layered ecology",()=>{
  it("converts adjacent plant resources into fixed-point energy",()=>{const simulation=new Simulation({width:21,height:21,foodChance:0,lifespan:100,worldSeed:2});const id=simulation.organismIdAt(10,10)!;const before=simulation.inspect(id)!.energy;simulation.paintResource(11,10,ResourceType.Plant,100);simulation.step();expect(simulation.inspect(id)!.energy).toBeGreaterThan(before);expect(simulation.resourceAmount[10*21+11]).toBe(0);});
  it("kills organisms painted beneath an impassable biome",()=>{const simulation=new Simulation({width:21,height:21,foodChance:0,lifespan:100});const id=simulation.organismIdAt(10,10)!;simulation.paintTerrain(10,10,TerrainType.Mountain);expect(simulation.inspect(id)!.alive).toBe(false);});
});
