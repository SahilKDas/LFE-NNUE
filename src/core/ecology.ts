export const KILLER_DURABILITY=12, ARMOR_DURABILITY=18, ATTACK_COST=120, MAX_ATTACK_GAIN=240;
export function constructionMinerals(cellType:number):number { return cellType===6?250:cellType===7?180:0; }
export function predatorGain(damageDealt:boolean):number { return damageDealt?MAX_ATTACK_GAIN:0; }
export function climateMetabolicCost(temperature:number,isDesert:boolean):number { const discomfort=Math.max(0,Math.abs(temperature-.5)-.15);return Math.round(discomfort*30)+(isDesert?4:0); }
