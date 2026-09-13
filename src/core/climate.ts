export const CLIMATE_CYCLE_TICKS = 24_000;
export const COMFORT_MIN = 0.35;
export const COMFORT_MAX = 0.65;
export const SEASONS = ["Spring", "Summer", "Autumn", "Winter"] as const;
export type Season = (typeof SEASONS)[number];
export interface ClimateSample { season: Season; phase: number; temperature: number; fertility: number; gradient: -1 | 0 | 1; temperatureBand: number; }
const clamp = (value: number, low: number, high: number): number => Math.max(low, Math.min(high, value));
export function cyclePhase(tick: number): number { return ((tick % CLIMATE_CYCLE_TICKS) + CLIMATE_CYCLE_TICKS) % CLIMATE_CYCLE_TICKS / CLIMATE_CYCLE_TICKS; }
export function climateAt(y: number, height: number, tick: number): ClimateSample {
  const phase = cyclePhase(tick); const latitude = height <= 1 ? 0 : 1 - 2 * clamp(y, 0, height - 1) / (height - 1); const wave = Math.sin(phase * Math.PI * 2);
  const temperature = clamp(0.5 + 0.5 * latitude * wave, 0, 1); const north = clamp(0.5 + 0.5 * clamp(latitude + 2 / Math.max(2, height - 1), -1, 1) * wave, 0, 1); const south = clamp(0.5 + 0.5 * clamp(latitude - 2 / Math.max(2, height - 1), -1, 1) * wave, 0, 1); const difference = north - south;
  return { season: SEASONS[Math.floor(phase * 4) % 4]!, phase, temperature, fertility: clamp(0.35 + temperature, 0.35, 1.35), gradient: Math.abs(difference) < 0.002 ? 0 : difference > 0 ? -1 : 1, temperatureBand: Math.min(4, Math.floor(temperature * 5)) };
}
export function thermalStressDelta(temperature: number): number { if (temperature >= COMFORT_MIN && temperature <= COMFORT_MAX) return -0.004; const excess = temperature < COMFORT_MIN ? COMFORT_MIN - temperature : temperature - COMFORT_MAX; return 0.001 + excess * 0.012; }
