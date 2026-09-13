import type { BrainSeed } from "./nnue";
import type { OrganismInspection } from "./simulation";
import type { CellType, Metrics, ResourceType, TerrainType } from "./types";

export type WorkerCommand =
  | { type: "initialize"; seed?: BrainSeed; width?: number; height?: number }
  | { type: "step-control"; running: boolean; ticksPerSecond?: number }
  | { type: "paint"; x: number; y: number; cellType: CellType }
  | { type: "reset" }
  | { type: "settings"; foodChance?: number; lifespan?: number }
  | { type: "select"; x: number; y: number }
  | { type: "inspect"; id: number }
  | { type: "render-ack" }
  | { type: "regenerate"; seed: number }
  | { type: "paint-terrain"; x:number; y:number; terrainType:TerrainType }
  | { type: "paint-resource"; x:number; y:number; resourceType:ResourceType; amount?:number };

export type WorkerResponse =
  | { type: "ready"; width: number; height: number }
  | { type: "full-snapshot"; width: number; height: number; cells: Uint8Array; owners: Int32Array; terrain:Uint8Array; resources:Uint8Array; resourceAmount:Uint16Array }
  | { type: "cell-delta"; indices: Uint32Array; cells: Uint8Array; owners: Int32Array; terrain:Uint8Array; resources:Uint8Array; resourceAmount:Uint16Array }
  | { type: "metrics"; metrics: Metrics }
  | { type: "selection"; id?: number; inspection?: OrganismInspection }
  | { type: "error"; message: string };
