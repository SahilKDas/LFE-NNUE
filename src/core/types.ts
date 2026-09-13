export const enum CellType {
  Empty,
  Food,
  Wall,
  Mouth,
  Producer,
  Mover,
  Killer,
  Armor,
  PlantMouth,
  ScavengerMouth,
  MineralMouth,
  Inert,
}

export const enum TerrainType { Plains, Fertile, Desert, Water, Mountain }
export const enum ResourceType { None, Plant, Carrion, Mineral }

export interface LocalCell {
  type: CellType;
  x: number;
  y: number;
  durability?: number;
}

export interface Metrics {
  organisms: number;
  record: number;
  generation: number;
  ticks: number;
  largest: number;
  averageMutation: number;
  season: string;
  seasonPhase: number;
  temperature: number;
  fertility: number;
  averageStress: number;
  averageEnergy:number;
  plantEaters:number;
  scavengers:number;
  mineralEaters:number;
  predators:number;
  nnueEvaluations:number;
  measuredTps:number;
  renderBacklog:number;
  memoryEstimate:number;
}

export const DIRECTIONS = [[0, -1], [0, 1], [-1, 0], [1, 0]] as const;

export const CELL_COLORS: Record<CellType, string> = {
  [CellType.Empty]: "#121d29",
  [CellType.Food]: "green",
  [CellType.Wall]: "gray",
  [CellType.Mouth]: "orange",
  [CellType.Producer]: "white",
  [CellType.Mover]: "#3493eb",
  [CellType.Killer]: "red",
  [CellType.Armor]: "purple",
  [CellType.PlantMouth]: "#ff9f43",
  [CellType.ScavengerMouth]: "#d67c4a",
  [CellType.MineralMouth]: "#d6c36a",
  [CellType.Inert]: "#59636f",
};
