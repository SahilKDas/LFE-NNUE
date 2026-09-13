import { describe,expect,it } from "vitest";
import { ATTACK_COST,climateMetabolicCost,constructionMinerals,MAX_ATTACK_GAIN,predatorGain } from "./ecology";
import { CellType } from "./types";
describe("combat economy",()=>{
  it("requires minerals for combat tissue",()=>{expect(constructionMinerals(CellType.Killer)).toBeGreaterThan(0);expect(constructionMinerals(CellType.Armor)).toBeGreaterThan(0);expect(constructionMinerals(CellType.Mover)).toBe(0);});
  it("caps predator return without generating energy on blocked hits",()=>{expect(predatorGain(false)).toBe(0);expect(predatorGain(true)).toBe(MAX_ATTACK_GAIN);expect(MAX_ATTACK_GAIN).toBeLessThanOrEqual(ATTACK_COST*2);});
});
it("charges gradual thermal and desert metabolic costs",()=>{expect(climateMetabolicCost(.5,false)).toBe(0);expect(climateMetabolicCost(.9,false)).toBeGreaterThan(0);expect(climateMetabolicCost(.5,true)).toBeGreaterThan(0);});
