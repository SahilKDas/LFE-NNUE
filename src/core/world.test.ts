import { describe,expect,it } from "vitest";
import { generateWorld,terrainPassable,terrainProductivity } from "./world";
import { TerrainType } from "./types";
describe("seeded world",()=>{
  it("is deterministic and seed-sensitive",()=>{expect(generateWorld(64,40,7)).toEqual(generateWorld(64,40,7));expect(generateWorld(64,40,7).terrain).not.toEqual(generateWorld(64,40,8).terrain);});
  it("contains all biomes at representative size",()=>{const types=new Set(generateWorld(256,160,42).terrain);for(let type=0;type<=TerrainType.Mountain;type++)expect(types.has(type)).toBe(true);});
  it("makes water and mountains impassable and unproductive",()=>{expect(terrainPassable(TerrainType.Water)).toBe(false);expect(terrainPassable(TerrainType.Mountain)).toBe(false);expect(terrainProductivity(TerrainType.Water)).toBe(0);expect(terrainProductivity(TerrainType.Fertile)).toBeGreaterThan(terrainProductivity(TerrainType.Desert));});
});
