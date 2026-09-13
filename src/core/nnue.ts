export const SENSOR_RADIUS = 2;
export const FEATURE_CATEGORIES = 11;
export const HIDDEN_SIZE = 16;
export const OUTPUT_SIZE = 5;
export const POSITION_COUNT = 24;
export const LEGACY_INPUT_SIZE = POSITION_COUNT * FEATURE_CATEGORIES + 8;
export const CLIMATE_FEATURE_OFFSET = LEGACY_INPUT_SIZE;
export const CLIMATE_FEATURE_COUNT = 12;
export const INPUT_SIZE = LEGACY_INPUT_SIZE + CLIMATE_FEATURE_COUNT;
export const BRAIN_SCHEMA_VERSION = 2;

export interface BrainSeed {
  schemaVersion?: number;
  inputWeights: number[][];
  hiddenBias: number[];
  outputWeights: number[][];
  outputBias: number[];
}

const randomWeight = (scale: number, random = Math.random): number => (random() * 2 - 1) * scale;

export class Nnue {
  readonly inputWeights: Float32Array[];
  readonly hiddenBias: Float32Array;
  readonly outputWeights: Float32Array[];
  readonly outputBias: Float32Array;
  private accumulator: Float32Array;
  private output = new Float32Array(OUTPUT_SIZE);
  private active: number[] = [];
  private featureMarks = new Uint32Array(INPUT_SIZE);
  private activeFlags = new Uint8Array(INPUT_SIZE);
  private markGeneration = 0;

  constructor(seed?: BrainSeed, random = Math.random) {
    if (seed) validateSeed(seed);
    this.inputWeights = seed
      ? Array.from({ length: INPUT_SIZE }, (_, index) => Float32Array.from(seed.inputWeights[index] ?? new Float32Array(HIDDEN_SIZE)))
      : Array.from({ length: INPUT_SIZE }, (_, index) => Float32Array.from({ length: HIDDEN_SIZE }, () => index < CLIMATE_FEATURE_OFFSET ? randomWeight(0.12, random) : 0));
    this.hiddenBias = seed ? Float32Array.from(seed.hiddenBias) : Float32Array.from({ length: HIDDEN_SIZE }, () => randomWeight(0.05, random));
    this.outputWeights = seed
      ? seed.outputWeights.map((row) => Float32Array.from(row))
      : Array.from({ length: OUTPUT_SIZE }, () => Float32Array.from({ length: HIDDEN_SIZE }, () => randomWeight(0.18, random)));
    this.outputBias = seed ? Float32Array.from(seed.outputBias) : Float32Array.from({ length: OUTPUT_SIZE }, () => randomWeight(0.05, random));
    this.accumulator = new Float32Array(this.hiddenBias);
  }

  clone(): Nnue {
    return new Nnue(this.toSeed());
  }

  toSeed(): BrainSeed {
    return {
      schemaVersion: BRAIN_SCHEMA_VERSION,
      inputWeights: this.inputWeights.map((row) => Array.from(row)),
      hiddenBias: Array.from(this.hiddenBias),
      outputWeights: this.outputWeights.map((row) => Array.from(row)),
      outputBias: Array.from(this.outputBias),
    };
  }

  evaluate(features: readonly number[]): Float32Array {
    this.markGeneration++;
    if (this.markGeneration === 0xffffffff) { this.featureMarks.fill(0); this.markGeneration = 1; }
    const mark = this.markGeneration;
    for (let index=0; index<features.length; index++) { const feature=features[index]!; if (feature>=0&&feature<INPUT_SIZE) this.featureMarks[feature]=mark; }
    for (let index=0; index<this.active.length; index++) { const feature=this.active[index]!; if (this.featureMarks[feature]!==mark) { this.apply(feature,-1); this.activeFlags[feature]=0; } }
    for (let index=0; index<features.length; index++) { const feature=features[index]!; if (feature>=0&&feature<INPUT_SIZE && this.activeFlags[feature]===0) { this.apply(feature,1); this.activeFlags[feature]=1; } }
    this.active = Array.from(features);

    for (let output=0; output<OUTPUT_SIZE; output++) {
      let value = this.outputBias[output] ?? 0; const row = this.outputWeights[output]!;
      for (let hidden = 0; hidden < HIDDEN_SIZE; hidden++) {
        value += Math.max(0, this.accumulator[hidden] ?? 0) * (row[hidden] ?? 0);
      }
      this.output[output]=value;
    }
    return this.output;
  }

  action(features: readonly number[]): number {
    const output = this.evaluate(features);
    let best = 0;
    for (let i = 1; i < output.length; i++) if (output[i]! > output[best]!) best = i;
    return best;
  }

  train(features: readonly number[], target: number, rate: number): void {
    const scores = this.evaluate(features);
    const maximum = Math.max(...scores);
    const probabilities = Array.from(scores, (score) => Math.exp(score - maximum));
    const total = probabilities.reduce((sum, value) => sum + value, 0);
    const hiddenGradient = new Float32Array(HIDDEN_SIZE);

    for (let output = 0; output < OUTPUT_SIZE; output++) {
      const gradient = (probabilities[output] ?? 0) / total - (output === target ? 1 : 0);
      const row = this.outputWeights[output]!;
      for (let hidden = 0; hidden < HIDDEN_SIZE; hidden++) {
        hiddenGradient[hidden] = (hiddenGradient[hidden] ?? 0) + gradient * (row[hidden] ?? 0);
        row[hidden] = (row[hidden] ?? 0) - rate * gradient * Math.max(0, this.accumulator[hidden] ?? 0);
      }
      this.outputBias[output] = (this.outputBias[output] ?? 0) - rate * gradient;
    }

    for (let hidden = 0; hidden < HIDDEN_SIZE; hidden++) {
      if ((this.accumulator[hidden] ?? 0) <= 0) continue;
      const gradient = hiddenGradient[hidden] ?? 0;
      this.hiddenBias[hidden] = (this.hiddenBias[hidden] ?? 0) - rate * gradient;
      for (const feature of features) {
        const row = this.inputWeights[feature];
        if (row) row[hidden] = (row[hidden] ?? 0) - rate * gradient;
      }
    }
    this.reset();
  }

  mutate(probability = 0.03, magnitude = 0.2): void {
    let changed = false;
    const perturb = (values: Float32Array): void => {
      for (let i = 0; i < values.length; i++) {
        if (Math.random() < probability) {
          values[i] = (values[i] ?? 0) + randomWeight(magnitude);
          changed = true;
        }
      }
    };
    this.inputWeights.forEach(perturb);
    perturb(this.hiddenBias);
    this.outputWeights.forEach(perturb);
    perturb(this.outputBias);
    if (!changed) this.outputBias[Math.floor(Math.random() * OUTPUT_SIZE)]! += randomWeight(magnitude);
    this.reset();
  }

  private apply(feature: number, sign: number): void {
    const weights = this.inputWeights[feature];
    if (!weights) return;
    for (let hidden = 0; hidden < HIDDEN_SIZE; hidden++) {
      this.accumulator[hidden] = (this.accumulator[hidden] ?? 0) + sign * (weights[hidden] ?? 0);
    }
  }

  private reset(): void {
    this.active = [];
    this.activeFlags.fill(0);
    this.accumulator = new Float32Array(this.hiddenBias);
  }
}

function validateSeed(seed: BrainSeed): void {
  const rows = seed.inputWeights.length;
  if (rows !== LEGACY_INPUT_SIZE && rows !== INPUT_SIZE) throw new Error("Invalid brain input row count: " + rows);
  if (seed.hiddenBias.length !== HIDDEN_SIZE || seed.outputBias.length !== OUTPUT_SIZE || seed.outputWeights.length !== OUTPUT_SIZE) throw new Error("Invalid brain hidden/output dimensions");
  if (seed.inputWeights.some((row) => row.length !== HIDDEN_SIZE) || seed.outputWeights.some((row) => row.length !== HIDDEN_SIZE)) throw new Error("Invalid brain row width");
}
