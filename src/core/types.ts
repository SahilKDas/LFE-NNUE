export const enum CellType {
  Empty,
  Food,
  Wall,
  Mouth,
  Producer,
  Mover,
  Killer,
  Armor,
}

export interface LocalCell {
  type: CellType;
  x: number;
  y: number;
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
};
