import { hash2d } from "./random";
import { ResourceType, TerrainType } from "./types";

export interface WorldLayers { terrain: Uint8Array; resources: Uint8Array; resourceAmount: Uint16Array; }
export function terrainProductivity(type:TerrainType):number { return type===TerrainType.Fertile?1.35:type===TerrainType.Desert?.3:type===TerrainType.Plains?1:0; }
export function generateWorld(width:number,height:number,seed:number):WorldLayers {
  const size=width*height, terrain=new Uint8Array(size), resources=new Uint8Array(size), resourceAmount=new Uint16Array(size);
  for(let y=0;y<height;y++) for(let x=0;x<width;x++) {
    const index=y*width+x, coarse=hash2d(Math.floor(x/24),Math.floor(y/24),seed), detail=hash2d(x,y,seed^0x9e3779b9), value=coarse*.78+detail*.22;
    const type=value<.13?TerrainType.Water:value>.9?TerrainType.Mountain:value<.3?TerrainType.Desert:value>.67?TerrainType.Fertile:TerrainType.Plains;
    terrain[index]=type;
    if(type===TerrainType.Fertile&&detail>.7){resources[index]=ResourceType.Plant;resourceAmount[index]=600;}
    else if(type!==TerrainType.Water&&type!==TerrainType.Mountain&&detail<.018){resources[index]=ResourceType.Mineral;resourceAmount[index]=1000;}
  }
  return {terrain,resources,resourceAmount};
}
export const terrainPassable=(type:TerrainType):boolean=>type!==TerrainType.Water&&type!==TerrainType.Mountain;
