import { CLIMATE_FEATURE_OFFSET, FEATURE_CATEGORIES, Nnue, POSITION_COUNT, SENSOR_RADIUS, type BrainSeed } from "./nnue";
import { climateAt, thermalStressDelta, type ClimateSample } from "./climate";
import { CellType, DIRECTIONS, type LocalCell, type Metrics } from "./types";

const enum Direction { Up, Down, Left, Right }

export interface LineageSummary {
  id: number;
  generation: number;
  alive: boolean;
}

export interface SensoryInput {
  x: number;
  y: number;
  category: number;
  label: string;
}

export interface OrganismInspection {
  id: number;
  parentId?: number;
  generation: number;
  alive: boolean;
  birthTick: number;
  deathTick?: number;
  age: number;
  cells: number;
  food: number;
  damage: number;
  mutability: number;
  neuralMutability: number;
  isMover: boolean;
  mutations: string[];
  ancestors: LineageSummary[];
  descendants: LineageSummary[];
  senses: SensoryInput[];
  outputs?: number[];
  action?: string;
  season: string;
  seasonPhase: number;
  temperature: number;
  fertility: number;
  thermalStress: number;
  totalDescendants: number;
}

interface LineageRecord {
  id: number;
  parentId?: number;
  generation: number;
  birthTick: number;
  deathTick?: number;
  mutations: string[];
  children: number[];
  cells: number;
  food: number;
  damage: number;
  mutability: number;
  neuralMutability: number;
  isMover: boolean;
  lastFeatures?: number[];
  lastOutputs?: number[];
  lastAction?: number;
  thermalStress: number;
  totalDescendants: number;
}

interface Organism {
  id: number;
  parentId?: number;
  generation: number;
  birthTick: number;
  x: number;
  y: number;
  cells: LocalCell[];
  brain?: Nnue;
  food: number;
  lifetime: number;
  damage: number;
  mutability: number;
  neuralMutability: number;
  birthDistance: number;
  moveRange: number;
  moveCount: number;
  direction: Direction;
  rotation: Direction;
  living: boolean;
  isProducer: boolean;
  isMover: boolean;
  mutations: string[];
  lastFeatures?: number[];
  lastOutputs?: number[];
  lastAction?: number;
  thermalStress: number;
  featureBuffer: number[];
}

export interface SimulationOptions {
  width: number;
  height: number;
  foodChance: number;
  lifespan: number;
  lineageLimit?: number;
}

export interface CellDelta { indices: Uint32Array; cells: Uint8Array; owners: Int32Array; }

export class Simulation {
  readonly width: number;
  readonly height: number;
  readonly cells: Uint8Array;
  readonly owners: Int32Array;
  foodChance: number;
  lifespan: number;
  foodBlocksReproduction = true;
  moversCanProduce = false;
  moversCanRotate = true;
  offspringRotate = true;
  instaKill = false;
  private organisms = new Map<number, Organism>();
  private activeIds: number[] = [];
  private nextId = 1;
  private ticks = 0;
  private resets = 0;
  private record = 0;
  private largest = 0;
  private lineage = new Map<number, LineageRecord>();
  private deadOrder: number[] = [];
  private selectedId?: number;
  private dirty: number[] = [];
  private dirtyFlags: Uint8Array;
  private readonly lineageLimit: number;

  constructor(options: SimulationOptions, private readonly seed?: BrainSeed) {
    this.width = options.width;
    this.height = options.height;
    this.foodChance = options.foodChance;
    this.lifespan = options.lifespan;
    this.lineageLimit = options.lineageLimit ?? 20_000;
    this.cells = new Uint8Array(this.width * this.height);
    this.owners = new Int32Array(this.width * this.height);
    this.dirtyFlags = new Uint8Array(this.width * this.height);
    this.reset();
  }

  reset(): void {
    this.cells.fill(CellType.Empty);
    this.owners.fill(-1);
    this.organisms.clear();
    this.activeIds = [];
    this.lineage.clear();
    this.deadOrder = [];
    this.dirty.length = 0; this.dirtyFlags.fill(0);
    this.ticks = 0;
    this.resets++;
    const organism = this.createOrganism(Math.floor(this.width / 2), Math.floor(this.height / 2));
    this.addCell(organism, CellType.Mouth, 0, 0);
    this.addCell(organism, CellType.Producer, -1, -1);
    this.addCell(organism, CellType.Producer, 1, 1);
    this.addOrganism(organism);
  }

  step(count = 1): void {
    for (let iteration = 0; iteration < count; iteration++) {
      this.ticks++;
      const count = this.activeIds.length;
      for (let index = 0; index < count; index++) { const organism = this.organisms.get(this.activeIds[index]!); if (organism) this.updateOrganism(organism); }
      this.activeIds = this.activeIds.filter((id) => { const organism = this.organisms.get(id); if (organism?.living) return true; this.organisms.delete(id); return false; });
      if (this.activeIds.length === 0) this.reset();
      this.record = Math.max(this.record, this.activeIds.length);
    }
  }

  paint(x: number, y: number, type: CellType): void {
    const index = this.safeIndex(x, y);
    if (index < 0) return;
    const owner = this.ownerAt(index);
    if (owner) this.die(owner);
    this.write(index, type, -1);
  }

  metrics(): Metrics {
    let mutation = 0; let stress = 0;
    for (const organism of this.organisms.values()) { mutation += organism.mutability; stress += organism.thermalStress; }
    const climate = climateAt(Math.floor(this.height / 2), this.height, this.ticks);
    return {
      organisms: this.activeIds.length,
      record: this.record,
      generation: this.resets,
      ticks: this.ticks,
      largest: this.largest,
      averageMutation: this.activeIds.length ? mutation / this.activeIds.length : 0,
      season: climate.season, seasonPhase: climate.phase, temperature: climate.temperature,
      fertility: climate.fertility, averageStress: this.activeIds.length ? stress / this.activeIds.length : 0,
    };
  }

  neuralOrganismCount(): number {
    let count = 0; for (const organism of this.organisms.values()) if (organism.isMover && organism.brain) count++; return count;
  }

  organismIdAt(x: number, y: number): number | undefined {
    const index = this.safeIndex(x, y);
    const id = index < 0 ? -1 : (this.owners[index] ?? -1);
    return id >= 0 ? id : undefined;
  }

  inspect(id: number): OrganismInspection | undefined {
    this.selectedId = id;
    const record = this.lineage.get(id);
    if (!record) return undefined;
    const organism = this.organisms.get(id);
    const ancestors: LineageSummary[] = [];
    let parentId = record.parentId;
    while (parentId !== undefined) {
      const parent = this.lineage.get(parentId);
      if (!parent) break;
      ancestors.unshift(this.lineageSummary(parent));
      parentId = parent.parentId;
    }
    const descendants: LineageSummary[] = [];
    const visit = (childId: number): void => {
      const child = this.lineage.get(childId);
      if (!child) return;
      descendants.push(this.lineageSummary(child));
      child.children.forEach(visit);
    };
    record.children.forEach(visit);
    const localClimate = climateAt(organism?.y ?? Math.floor(this.height / 2), this.height, this.ticks);
    return {
      id,
      parentId: record.parentId,
      generation: record.generation,
      alive: organism?.living ?? false,
      birthTick: record.birthTick,
      deathTick: record.deathTick,
      age: organism?.lifetime ?? Math.max(0, (record.deathTick ?? this.ticks) - record.birthTick),
      cells: organism?.cells.length ?? record.cells,
      food: organism?.food ?? record.food,
      damage: organism?.damage ?? record.damage,
      mutability: organism?.mutability ?? record.mutability,
      neuralMutability: organism?.neuralMutability ?? record.neuralMutability,
      isMover: organism?.isMover ?? record.isMover,
      mutations: [...record.mutations],
      ancestors,
      descendants,
      senses: (organism?.lastFeatures ?? record.lastFeatures) ? this.decodeSenses((organism?.lastFeatures ?? record.lastFeatures)!) : [],
      outputs: (organism?.lastOutputs ?? record.lastOutputs) ? [...(organism?.lastOutputs ?? record.lastOutputs)!] : undefined,
      action: (organism?.lastAction ?? record.lastAction) === undefined ? undefined :
        ["Up", "Down", "Left", "Right", "Wait"][(organism?.lastAction ?? record.lastAction)!],
      season: localClimate.season, seasonPhase: localClimate.phase, temperature: localClimate.temperature,
      fertility: localClimate.fertility, thermalStress: organism?.thermalStress ?? record.thermalStress,
      totalDescendants: record.totalDescendants,
    };
  }

  select(id?: number): void { this.selectedId = id; }
  climateAt(y: number): ClimateSample { return climateAt(y, this.height, this.ticks); }
  populateBenchmark(count = 10_000): void {
    this.cells.fill(CellType.Empty); this.owners.fill(-1); this.organisms.clear(); this.activeIds=[]; this.lineage.clear(); this.deadOrder=[]; this.dirty.length=0; this.dirtyFlags.fill(0);
    const columns = Math.floor(Math.sqrt(count * this.width / this.height));
    for (let index=0; index<count; index++) {
      const x = 1 + Math.floor(index % columns) * Math.max(1, Math.floor((this.width-2)/columns));
      const y = 1 + Math.floor(index / columns) * Math.max(1, Math.floor((this.height-2)/Math.ceil(count/columns)));
      const organism=this.createOrganism(Math.min(this.width-2,x),Math.min(this.height-2,y)); this.addCell(organism,CellType.Mover,0,0); this.addOrganism(organism);
    }
  }
  consumeDelta(): CellDelta {
    const indices=Uint32Array.from(this.dirty), cells=Uint8Array.from(this.dirty,(index)=>this.cells[index]!), owners=Int32Array.from(this.dirty,(index)=>this.owners[index]!);
    for(let cursor=0;cursor<this.dirty.length;cursor++) this.dirtyFlags[this.dirty[cursor]!] = 0;
    this.dirty.length=0; return { indices, cells, owners };
  }

  private createOrganism(x: number, y: number, parent?: Organism): Organism {
    if (parent) {
      return {
        id: this.nextId++, x, y,
        parentId: parent.id,
        generation: parent.generation + 1,
        birthTick: this.ticks,
        cells: parent.cells.map((cell) => ({ ...cell })),
        brain: parent.brain?.clone(),
        food: 0, lifetime: 0, damage: 0,
        mutability: parent.mutability,
        neuralMutability: parent.neuralMutability,
        birthDistance: parent.birthDistance,
        moveRange: parent.moveRange,
        moveCount: 0,
        direction: Direction.Up,
        rotation: Direction.Up,
        living: true,
        isProducer: parent.isProducer,
        isMover: parent.isMover,
        mutations: [], thermalStress: parent.thermalStress, featureBuffer: [],
      };
    }
    return {
      id: this.nextId++, x, y, cells: [],
      generation: 0, birthTick: this.ticks,
      food: 0, lifetime: 0, damage: 0,
      mutability: 5, neuralMutability: 8, birthDistance: 4, moveRange: 4, moveCount: 0,
      direction: Direction.Up, rotation: Direction.Up,
      living: true, isProducer: false, isMover: false, mutations: [], thermalStress: 0, featureBuffer: [],
    };
  }

  private updateOrganism(organism: Organism): void {
    if (!organism.living) return;
    organism.lifetime++;
    const localClimate = climateAt(organism.y, this.height, this.ticks);
    organism.thermalStress = Math.max(0, organism.thermalStress + thermalStressDelta(localClimate.temperature));
    if (organism.thermalStress >= 1) { organism.thermalStress -= 1; this.harm(organism); if (!organism.living) return; }
    if (organism.lifetime > organism.cells.length * this.lifespan) {
      this.die(organism);
      return;
    }
    if (organism.food >= organism.cells.length) this.reproduce(organism);
    for (const local of organism.cells) {
      const [x, y] = this.realLocation(organism, local);
      if (local.type === CellType.Mouth) this.eat(organism, x, y);
      else if (local.type === CellType.Producer) this.produce(organism, x, y);
      else if (local.type === CellType.Killer) this.attack(organism, x, y);
    }
    if (!organism.living || !organism.isMover || !organism.brain) return;
    const features = this.features(organism);
    const outputs = organism.brain.evaluate(features);
    const action = this.bestAction(outputs);
    if (organism.id === this.selectedId) { organism.lastFeatures = features; organism.lastOutputs = Array.from(outputs); organism.lastAction = action; }
    organism.direction = action as Direction;
    if (organism.direction >= 4) return;
    organism.moveCount++;
    this.attemptMove(organism);
    if (organism.moveCount > organism.moveRange) this.attemptRotate(organism);
  }

  private reproduce(parent: Organism): void {
    const child = this.createOrganism(0, 0, parent);
    if (this.offspringRotate) child.rotation = this.randomDirection();
    child.mutability += Math.random() <= 0.5 ? 1 : -1;
    child.mutability = Math.max(1, child.mutability);
    child.mutations.push(`Body mutation rate changed to ${child.mutability.toFixed(1)}%`);
    if (Math.random() * 100 <= parent.mutability) this.mutate(child);
    if (child.isMover && child.brain && Math.random() * 100 <= child.neuralMutability) {
      child.brain.mutate();
      child.mutations.push("NNUE weights mutated");
    }
    if (Math.random() < 0.1) {
      child.neuralMutability = Math.min(50, Math.max(0.5,
        child.neuralMutability + (Math.random() < 0.5 ? -0.5 : 0.5)));
      child.mutations.push(`Neural mutation rate changed to ${child.neuralMutability.toFixed(1)}%`);
    }
    const direction = DIRECTIONS[Math.floor(Math.random() * 4)]!;
    const offset = Math.floor(Math.random() * 3);
    child.x = parent.x + direction[0] * (parent.birthDistance + offset);
    child.y = parent.y + direction[1] * (parent.birthDistance + offset);
    if (this.isClear(child, child.x, child.y) && this.isStraightPath(child.x, child.y, parent.x, parent.y, parent)) {
      this.addOrganism(child);
    }
    parent.food -= parent.cells.length;
  }

  private mutate(organism: Organism): void {
    const choice = Math.floor(Math.random() * 100);
    if (choice <= 33) {
      const base = organism.cells[Math.floor(Math.random() * organism.cells.length)]!;
      const [dx, dy] = [...DIRECTIONS, [-1, -1], [1, 1], [-1, 1], [1, -1]][Math.floor(Math.random() * 8)]!;
      const type = this.randomLivingType();
      if (this.addCell(organism, type, base.x + dx, base.y + dy)) {
        organism.birthDistance++;
        organism.mutations.push(`Added ${this.cellName(type)} cell`);
      }
    } else if (choice <= 66) {
      const cell = organism.cells[Math.floor(Math.random() * organism.cells.length)]!;
      const before = cell.type;
      cell.type = this.randomLivingType();
      organism.mutations.push(`Changed ${this.cellName(before)} to ${this.cellName(cell.type)}`);
      this.refreshCapabilities(organism);
    } else if (organism.cells.length > 1) {
      const cell = organism.cells[Math.floor(Math.random() * organism.cells.length)]!;
      if (cell.x !== 0 || cell.y !== 0) {
        organism.cells.splice(organism.cells.indexOf(cell), 1);
        organism.mutations.push(`Removed ${this.cellName(cell.type)} cell`);
      }
      this.refreshCapabilities(organism);
    }
    if (organism.isMover && Math.random() * 100 <= 10) {
      organism.moveRange = Math.max(1, organism.moveRange + Math.floor(Math.random() * 4) - 2);
      organism.mutations.push(`Movement range changed to ${organism.moveRange}`);
    }
    if (Math.random() * 100 <= 10) {
      organism.birthDistance = Math.max(1, organism.birthDistance + Math.floor(Math.random() * 5) - 2);
      organism.mutations.push(`Birth distance changed to ${organism.birthDistance}`);
    }
    if (organism.isMover) {
      organism.brain ??= new Nnue(this.seed);
    } else {
      organism.brain = undefined;
    }
  }

  private addCell(organism: Organism, type: CellType, x: number, y: number): boolean {
    if (organism.cells.some((cell) => cell.x === x && cell.y === y)) return false;
    organism.cells.push({ type, x, y });
    this.refreshCapabilities(organism);
    return true;
  }

  private refreshCapabilities(organism: Organism): void {
    organism.isProducer = organism.cells.some((cell) => cell.type === CellType.Producer);
    organism.isMover = organism.cells.some((cell) => cell.type === CellType.Mover);
    if (organism.isMover) organism.brain ??= new Nnue(this.seed);
    else organism.brain = undefined;
  }

  private eat(organism: Organism, x: number, y: number): void {
    for (const [dx, dy] of DIRECTIONS) {
      const index = this.safeIndex(x + dx, y + dy);
      if (index >= 0 && this.cells[index] === CellType.Food) {
        this.write(index, CellType.Empty, -1);
        organism.food++;
      }
    }
  }

  private produce(organism: Organism, x: number, y: number): void {
    if ((organism.isMover && !this.moversCanProduce) || Math.random() >= this.foodChance * climateAt(y, this.height, this.ticks).fertility) return;
    const [dx, dy] = DIRECTIONS[Math.floor(Math.random() * 4)]!;
    const index = this.safeIndex(x + dx, y + dy);
    if (index >= 0 && this.cells[index] === CellType.Empty) this.write(index, CellType.Food, -1);
  }

  private attack(attacker: Organism, x: number, y: number): void {
    for (const [dx, dy] of DIRECTIONS) {
      const index = this.safeIndex(x + dx, y + dy);
      if (index < 0 || this.cells[index] === CellType.Armor) continue;
      const victim = this.ownerAt(index);
      if (!victim || victim === attacker || !victim.living) continue;
      const mutualHit = this.cells[index] === CellType.Killer;
      this.harm(victim);
      if (mutualHit) this.harm(attacker);
    }
  }

  private harm(organism: Organism): void {
    organism.damage++;
    if (this.instaKill || organism.damage >= organism.cells.length) this.die(organism);
  }

  private attemptMove(organism: Organism): boolean {
    const [dx, dy] = DIRECTIONS[organism.direction]!;
    if (!this.isClear(organism, organism.x + dx, organism.y + dy)) return false;
    this.clearBody(organism);
    organism.x += dx; organism.y += dy;
    this.placeBody(organism);
    return true;
  }

  private attemptRotate(organism: Organism): boolean {
    if (!this.moversCanRotate) {
      organism.direction = this.randomDirection();
      organism.moveCount = 0;
      return true;
    }
    const rotation = this.randomDirection();
    if (!this.isClear(organism, organism.x, organism.y, rotation)) return false;
    this.clearBody(organism);
    organism.rotation = rotation;
    organism.direction = this.randomDirection();
    organism.moveCount = 0;
    this.placeBody(organism);
    return true;
  }

  private isClear(organism: Organism, x: number, y: number, rotation = organism.rotation): boolean {
    return organism.cells.every((cell) => {
      const [rx, ry] = this.rotated(cell.x, cell.y, rotation);
      const index = this.safeIndex(x + rx, y + ry);
      return index >= 0 && (this.owners[index] === organism.id || this.cells[index] === CellType.Empty ||
        (!this.foodBlocksReproduction && this.cells[index] === CellType.Food));
    });
  }

  private isStraightPath(x1: number, y1: number, x2: number, y2: number, parent: Organism): boolean {
    if (x1 === x2) {
      for (let y = Math.min(y1, y2); y < Math.max(y1, y2); y++) if (!this.passablePath(x1, y, parent)) return false;
      return true;
    }
    for (let x = Math.min(x1, x2); x < Math.max(x1, x2); x++) if (!this.passablePath(x, y1, parent)) return false;
    return true;
  }

  private passablePath(x: number, y: number, parent: Organism): boolean {
    const index = this.safeIndex(x, y);
    return index >= 0 && (this.cells[index] === CellType.Empty || this.cells[index] === CellType.Food || this.owners[index] === parent.id);
  }

  private features(organism: Organism): number[] {
    const features = organism.featureBuffer; features.length = 0;
    let square = 0;
    for (let dy = -SENSOR_RADIUS; dy <= SENSOR_RADIUS; dy++) for (let dx = -SENSOR_RADIUS; dx <= SENSOR_RADIUS; dx++) {
      if (dx === 0 && dy === 0) continue;
      const index = this.safeIndex(organism.x + dx, organism.y + dy);
      let category = 6;
      if (index >= 0) {
        const type = this.cells[index] as CellType;
        const owner = this.owners[index] ?? -1;
        const neighbor = owner >= 0 && owner !== organism.id
          ? this.organisms.get(owner)
          : undefined;
        category = owner === organism.id ? 3 : type === CellType.Killer ? 5 :
          neighbor?.isMover ? 7 + neighbor.direction : owner >= 0 ? 4 :
          type === CellType.Food ? 1 : type === CellType.Wall ? 2 : 0;
      }
      features.push(square++ * FEATURE_CATEGORIES + category);
    }
    const state = POSITION_COUNT * FEATURE_CATEGORIES;
    features.push(state + (organism.food >= organism.cells.length ? 1 : 0));
    features.push(state + 2 + (organism.damage > 0 ? 1 : 0));
    features.push(state + 4 + organism.direction);
    const climate = climateAt(organism.y, this.height, this.ticks);
    features.push(CLIMATE_FEATURE_OFFSET + climate.temperatureBand);
    features.push(CLIMATE_FEATURE_OFFSET + 5 + climate.gradient + 1);
    features.push(CLIMATE_FEATURE_OFFSET + 8 + Math.floor(climate.phase * 4) % 4);
    return features;
  }

  private addOrganism(organism: Organism): void {
    this.organisms.set(organism.id, organism); this.activeIds.push(organism.id);
    this.lineage.set(organism.id, {
      id: organism.id,
      parentId: organism.parentId,
      generation: organism.generation,
      birthTick: organism.birthTick,
      mutations: organism.mutations,
      children: [],
      cells: organism.cells.length,
      food: organism.food,
      damage: organism.damage,
      mutability: organism.mutability,
      neuralMutability: organism.neuralMutability,
      isMover: organism.isMover,
      thermalStress: organism.thermalStress,
      totalDescendants: 0,
    });
    if (organism.parentId !== undefined) {
      this.lineage.get(organism.parentId)?.children.push(organism.id);
      let ancestorId: number | undefined = organism.parentId;
      while (ancestorId !== undefined) { const ancestor = this.lineage.get(ancestorId); if (!ancestor) break; ancestor.totalDescendants++; ancestorId = ancestor.parentId; }
    }
    this.largest = Math.max(this.largest, organism.cells.length);
    this.placeBody(organism);
  }

  private die(organism: Organism): void {
    for (const cell of organism.cells) {
      const [x, y] = this.realLocation(organism, cell);
      const index = this.safeIndex(x, y);
      if (index >= 0 && this.owners[index] === organism.id) {
        this.write(index, CellType.Food, -1);
      }
    }
    organism.living = false;
    const record = this.lineage.get(organism.id);
    if (record && record.deathTick === undefined) {
      record.deathTick = this.ticks;
      record.cells = organism.cells.length;
      record.food = organism.food;
      record.damage = organism.damage;
      record.mutability = organism.mutability;
      record.neuralMutability = organism.neuralMutability;
      record.isMover = organism.isMover;
      record.lastFeatures = organism.lastFeatures ? [...organism.lastFeatures] : undefined;
      record.lastOutputs = organism.lastOutputs ? [...organism.lastOutputs] : undefined;
      record.lastAction = organism.lastAction;
      record.thermalStress = organism.thermalStress;
      this.deadOrder.push(organism.id);
      this.pruneLineage();
    }
  }

  private placeBody(organism: Organism): void {
    for (const cell of organism.cells) {
      const [x, y] = this.realLocation(organism, cell);
      const index = this.safeIndex(x, y);
      if (index >= 0) this.write(index, cell.type, organism.id);
    }
  }

  private clearBody(organism: Organism): void {
    for (const cell of organism.cells) {
      const [x, y] = this.realLocation(organism, cell);
      const index = this.safeIndex(x, y);
      if (index >= 0 && this.owners[index] === organism.id) {
        this.write(index, CellType.Empty, -1);
      }
    }
  }

  private realLocation(organism: Organism, cell: LocalCell): [number, number] {
    const [x, y] = this.rotated(cell.x, cell.y, organism.rotation);
    return [organism.x + x, organism.y + y];
  }

  private rotated(x: number, y: number, direction: Direction): [number, number] {
    if (direction === Direction.Down) return [-x, -y];
    if (direction === Direction.Left) return [y, -x];
    if (direction === Direction.Right) return [-y, x];
    return [x, y];
  }

  private ownerAt(index: number): Organism | undefined {
    const id = this.owners[index] ?? -1;
    return id < 0 ? undefined : this.organisms.get(id);
  }

  private randomDirection(): Direction { return Math.floor(Math.random() * 4) as Direction; }
  private randomLivingType(): CellType { return CellType.Mouth + Math.floor(Math.random() * 5); }
  private bestAction(outputs: ArrayLike<number>): number {
    let best = 0;
    for (let index = 1; index < outputs.length; index++) if (outputs[index]! > outputs[best]!) best = index;
    return best;
  }
  private lineageSummary(record: LineageRecord): LineageSummary {
    return { id: record.id, generation: record.generation, alive: record.deathTick === undefined };
  }
  private decodeSenses(features: readonly number[]): SensoryInput[] {
    const labels = ["Empty", "Food", "Wall", "Self", "Organism", "Killer", "Boundary", "Mover ↑", "Mover ↓", "Mover ←", "Mover →"];
    return features.slice(0, 24).map((feature, square) => {
      let cursor = 0;
      for (let y = -2; y <= 2; y++) for (let x = -2; x <= 2; x++) {
        if (x === 0 && y === 0) continue;
        if (cursor++ === square) {
          const category = feature % FEATURE_CATEGORIES;
          return { x, y, category, label: labels[category] ?? "Unknown" };
        }
      }
      return { x: 0, y: 0, category: 0, label: "Unknown" };
    });
  }
  private cellName(type: CellType): string {
    return ["empty", "food", "wall", "mouth", "producer", "mover", "killer", "armor"][type] ?? "unknown";
  }
  private safeIndex(x: number, y: number): number { return x >= 0 && y >= 0 && x < this.width && y < this.height ? y * this.width + x : -1; }
  private write(index: number, type: CellType, owner: number): void { this.cells[index]=type; this.owners[index]=owner; if(this.dirtyFlags[index]===0){this.dirtyFlags[index]=1;this.dirty.push(index);} }
  private pruneLineage(): void {
    if (this.deadOrder.length <= this.lineageLimit) return;
    const protectedIds = new Set<number>();
    const protect = (id: number | undefined): void => { while (id !== undefined && !protectedIds.has(id)) { protectedIds.add(id); id = this.lineage.get(id)?.parentId; } };
    for (const id of this.activeIds) protect(id); protect(this.selectedId);
    let deadCount = this.deadOrder.length;
    this.deadOrder = this.deadOrder.filter((id) => {
      if (deadCount <= this.lineageLimit) return true;
      const record = this.lineage.get(id);
      if (!record || protectedIds.has(id) || record.children.some((child) => this.lineage.has(child))) return true;
      if (record.parentId !== undefined) { const parent = this.lineage.get(record.parentId); if (parent) parent.children = parent.children.filter((child) => child !== id); }
      this.lineage.delete(id); deadCount--; return false;
    });
  }
}
