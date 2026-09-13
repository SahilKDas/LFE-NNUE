import type { BrainSeed } from "./nnue";
import type { OrganismInspection } from "./simulation";
import type { CellType, Metrics } from "./types";

export type WorkerCommand =
  | { type: "initialize"; seed?: BrainSeed; width?: number; height?: number }
  | { type: "step-control"; running: boolean; ticksPerSecond?: number }
  | { type: "paint"; x: number; y: number; cellType: CellType }
  | { type: "reset" }
  | { type: "settings"; foodChance?: number; lifespan?: number }
  | { type: "select"; x: number; y: number }
  | { type: "inspect"; id: number }
  | { type: "render-ack" };

export type WorkerResponse =
  | { type: "ready"; width: number; height: number }
  | { type: "full-snapshot"; width: number; height: number; cells: Uint8Array; owners: Int32Array }
  | { type: "cell-delta"; indices: Uint32Array; cells: Uint8Array; owners: Int32Array }
  | { type: "metrics"; metrics: Metrics }
  | { type: "selection"; id?: number; inspection?: OrganismInspection }
  | { type: "error"; message: string };
