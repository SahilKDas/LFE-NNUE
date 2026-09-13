import "./style.css";
import { Simulation } from "./core/simulation";
import { CELL_COLORS, CellType } from "./core/types";
import type { BrainSeed } from "./core/nnue";

const app = document.querySelector<HTMLDivElement>("#app");
if (!app) throw new Error("Missing app root");

app.innerHTML = `
  <main class="shell">
    <header>
      <div><span class="eyebrow">EVOLUTION, ACCELERATED</span><h1>Life Engine <em>NNUE</em></h1></div>
      <div class="status"><i></i><span id="run-state">Running</span></div>
    </header>
    <section class="stage">
      <canvas id="world" aria-label="Life Engine simulation"></canvas>
      <div class="legend">
        <span><b class="food"></b>Food</span><span><b class="mouth"></b>Mouth</span>
        <span><b class="producer"></b>Producer</span><span><b class="mover"></b>Mover</span>
        <span><b class="killer"></b>Killer</span><span><b class="armor"></b>Armor</span>
      </div>
    </section>
    <aside>
      <section class="panel hero-panel">
        <p class="label">CORE</p><h2>Sparse speed.<br><span>Neural instinct.</span></h2>
        <p>Sparse NNUE brains inherit, mutate, and learn movement strategies under natural selection.</p>
      </section>
      <section class="metrics">
        <article><span>ORGANISMS</span><strong id="organisms">0</strong></article>
        <article><span>RECORD</span><strong id="record">0</strong></article>
        <article><span>GENERATION</span><strong id="generation">0</strong></article>
        <article><span>LARGEST</span><strong id="largest">0</strong></article>
      </section>
      <section class="panel controls">
        <div class="control-title"><h3>Simulation</h3><button id="toggle">Pause</button></div>
        <label>Ticks per frame <output id="speed-value">2</output><input id="speed" type="range" min="1" max="40" value="2"></label>
        <label>Food production <output id="food-value">4%</output><input id="food-rate" type="range" min="0" max="20" value="4"></label>
        <div class="button-grid"><button id="reset">Reset world</button><button id="seed">Seed food</button></div>
      </section>
      <section class="panel tools">
        <h3>World tools</h3>
        <div class="tool-row">
          <button class="tool active" data-tool="inspect">Inspect</button>
          <button class="tool" data-tool="1">Food</button>
          <button class="tool" data-tool="2">Wall</button>
          <button class="tool" data-tool="0">Erase</button>
        </div>
        <p>Paint directly on the ecosystem. Painting over an organism removes it.</p>
      </section>
      <section class="panel observatory">
        <div class="control-title"><h3>Evolution Observatory</h3><button id="clear-selection">Clear</button></div>
        <div id="observation" class="observation-empty">Choose Inspect, then select a creature.</div>
      </section>
      <footer><span id="core-state">TypeScript NNUE</span><span id="ticks">0 ticks</span></footer>
    </aside>
  </main>`;

const canvas = document.querySelector<HTMLCanvasElement>("#world")!;
const context = canvas.getContext("2d", { alpha: false })!;
const cellSize = 5;
let seed: BrainSeed | undefined;
try {
  const response = await fetch("/trained-brain.json");
  if (response.ok) seed = await response.json() as BrainSeed;
} catch { /* random initialization remains available */ }

let simulation: Simulation;
let running = true;
let ticksPerFrame = 2;
let tool: CellType | "inspect" = "inspect";
let selectedId: number | undefined;

function resize(): void {
  const bounds = canvas.getBoundingClientRect();
  const scale = window.devicePixelRatio || 1;
  canvas.width = Math.floor(bounds.width * scale);
  canvas.height = Math.floor(bounds.height * scale);
  context.setTransform(scale, 0, 0, scale, 0, 0);
  simulation = new Simulation({
    width: Math.max(40, Math.floor(bounds.width / cellSize)),
    height: Math.max(40, Math.floor(bounds.height / cellSize)),
    foodChance: 0.04,
    lifespan: 120,
  }, seed);
}

function render(): void {
  const width = simulation.width;
  context.fillStyle = CELL_COLORS[CellType.Empty];
  context.fillRect(0, 0, canvas.clientWidth, canvas.clientHeight);
  for (let index = 0; index < simulation.cells.length; index++) {
    const type = simulation.cells[index] as CellType;
    if (type === CellType.Empty) continue;
    context.fillStyle = CELL_COLORS[type];
    context.fillRect((index % width) * cellSize, Math.floor(index / width) * cellSize, cellSize, cellSize);
    if (simulation.owners[index] === selectedId) {
      context.strokeStyle = "#ffe45e";
      context.lineWidth = 1;
      context.strokeRect((index % width) * cellSize + 0.5, Math.floor(index / width) * cellSize + 0.5, cellSize - 1, cellSize - 1);
    }
  }
}

function softmax(values: readonly number[]): number[] {
  const maximum = Math.max(...values);
  const exponential = values.map((value) => Math.exp(value - maximum));
  const total = exponential.reduce((sum, value) => sum + value, 0);
  return exponential.map((value) => value / total);
}

function lineageButtons(items: { id: number; generation: number; alive: boolean }[]): string {
  if (!items.length) return '<span class="muted">None</span>';
  const visible = items.slice(-16);
  const hidden = items.length - visible.length;
  return `${hidden > 0 ? `<span class="muted">+${hidden} earlier</span>` : ""}${visible.map((item) =>
    `<button class="lineage-node ${item.alive ? "alive" : "dead"}" data-organism-id="${item.id}">#${item.id}<small>G${item.generation}</small></button>`
  ).join("")}`;
}

function sensoryGrid(senses: { x: number; y: number; category: number; label: string }[]): string {
  const byPosition = new Map(senses.map((sense) => [`${sense.x},${sense.y}`, sense]));
  let html = "";
  for (let y = -2; y <= 2; y++) for (let x = -2; x <= 2; x++) {
    if (x === 0 && y === 0) {
      html += '<span class="sense center" title="Selected organism">◎</span>';
    } else {
      const sense = byPosition.get(`${x},${y}`);
      html += `<span class="sense sense-${sense?.category ?? 0}" title="${sense?.label ?? "Unknown"}"></span>`;
    }
  }
  return html;
}

function renderObservatory(): void {
  const root = document.querySelector<HTMLElement>("#observation")!;
  if (selectedId === undefined) {
    root.className = "observation-empty";
    root.textContent = "Choose Inspect, then select a creature.";
    return;
  }
  const organism = simulation.inspect(selectedId);
  if (!organism) {
    selectedId = undefined;
    root.className = "observation-empty";
    root.textContent = "That lineage is no longer part of this world.";
    return;
  }
  root.className = "";
  const probabilities = organism.outputs ? softmax(organism.outputs) : [];
  const actions = ["Up", "Down", "Left", "Right", "Wait"];
  root.innerHTML = `
    <div class="organism-heading">
      <div><span class="label">CREATURE</span><strong>#${organism.id}</strong></div>
      <span class="life-badge ${organism.alive ? "alive" : "dead"}">${organism.alive ? "Alive" : "Dead"}</span>
    </div>
    <div class="inspection-stats">
      <span>Generation<strong>${organism.generation}</strong></span>
      <span>Age<strong>${organism.age}</strong></span>
      <span>Cells<strong>${organism.cells}</strong></span>
      <span>Food<strong>${organism.food}</strong></span>
      <span>Damage<strong>${organism.damage}</strong></span>
      <span>Brain<strong>${organism.isMover ? "NNUE" : "Static"}</strong></span>
    </div>
    <h4>Ancestry</h4><div class="lineage-list">${lineageButtons(organism.ancestors)}</div>
    <h4>Descendants <small>${organism.descendants.length}</small></h4><div class="lineage-list">${lineageButtons(organism.descendants)}</div>
    <h4>Mutations</h4>
    <ul class="mutation-list">${organism.mutations.length
      ? organism.mutations.map((mutation) => `<li>${mutation}</li>`).join("")
      : "<li class=\"muted\">No mutations from parent</li>"}</ul>
    <div class="rates"><span>Body mutation <b>${organism.mutability.toFixed(1)}%</b></span><span>Neural mutation <b>${organism.neuralMutability.toFixed(1)}%</b></span></div>
    <h4>Current sensory inputs</h4>
    ${organism.senses.length ? `<div class="sensory-row"><div class="sense-grid">${sensoryGrid(organism.senses)}</div><p>Hover cells to inspect the encoded feature.</p></div>` : '<p class="muted">Static organisms have no neural inputs.</p>'}
    <h4>Neural outputs ${organism.action ? `<small>→ ${organism.action}</small>` : ""}</h4>
    <div class="output-list">${probabilities.length ? probabilities.map((value, index) =>
      `<div class="${actions[index] === organism.action ? "chosen" : ""}"><span>${actions[index]}</span><i><b style="width:${(value * 100).toFixed(1)}%"></b></i><output>${(value * 100).toFixed(1)}%</output></div>`
    ).join("") : '<p class="muted">No NNUE outputs for this organism.</p>'}</div>`;
}

let lastObservationRender = 0;
function frame(time = 0): void {
  if (running) simulation.step(ticksPerFrame);
  render();
  const metrics = simulation.metrics();
  for (const key of ["organisms", "record", "generation", "largest"] as const) {
    document.querySelector<HTMLElement>(`#${key}`)!.textContent = String(metrics[key]);
  }
  document.querySelector("#ticks")!.textContent = `${metrics.ticks.toLocaleString()} ticks`;
  if (time - lastObservationRender > 150) {
    renderObservatory();
    lastObservationRender = time;
  }
  requestAnimationFrame(frame);
}

document.querySelector("#toggle")!.addEventListener("click", (event) => {
  running = !running;
  (event.currentTarget as HTMLButtonElement).textContent = running ? "Pause" : "Resume";
  document.querySelector("#run-state")!.textContent = running ? "Running" : "Paused";
  document.body.classList.toggle("paused", !running);
});
document.querySelector("#reset")!.addEventListener("click", () => {
  simulation.reset();
  selectedId = undefined;
});
document.querySelector("#seed")!.addEventListener("click", () => {
  for (let i = 0; i < 500; i++) simulation.paint(
    Math.floor(Math.random() * simulation.width),
    Math.floor(Math.random() * simulation.height),
    CellType.Food,
  );
});
document.querySelector<HTMLInputElement>("#speed")!.addEventListener("input", (event) => {
  ticksPerFrame = Number((event.target as HTMLInputElement).value);
  document.querySelector("#speed-value")!.textContent = String(ticksPerFrame);
});
document.querySelector<HTMLInputElement>("#food-rate")!.addEventListener("input", (event) => {
  const value = Number((event.target as HTMLInputElement).value);
  simulation.foodChance = value / 100;
  document.querySelector("#food-value")!.textContent = `${value}%`;
});
document.querySelectorAll<HTMLButtonElement>(".tool").forEach((button) => button.addEventListener("click", () => {
  document.querySelector(".tool.active")?.classList.remove("active");
  button.classList.add("active");
  tool = button.dataset.tool === "inspect" ? "inspect" : Number(button.dataset.tool) as CellType;
}));
canvas.addEventListener("pointerdown", (event) => {
  const bounds = canvas.getBoundingClientRect();
  const x = Math.floor((event.clientX - bounds.left) / cellSize);
  const y = Math.floor((event.clientY - bounds.top) / cellSize);
  if (tool === "inspect") selectedId = simulation.organismIdAt(x, y);
  else simulation.paint(x, y, tool);
});
document.querySelector("#clear-selection")!.addEventListener("click", () => { selectedId = undefined; });
document.querySelector("#observation")!.addEventListener("pointerdown", (event) => {
  const button = (event.target as HTMLElement).closest<HTMLButtonElement>("[data-organism-id]");
  if (button) selectedId = Number(button.dataset.organismId);
});

window.addEventListener("resize", resize);
resize();
frame();
