import { Simulation } from "./core/simulation";
import type { WorkerCommand, WorkerResponse } from "./core/protocol";

interface WorkerScope { onmessage: ((event: MessageEvent<WorkerCommand>) => void) | null; postMessage(message: WorkerResponse, transfer?: Transferable[]): void; }
const scope = self as unknown as WorkerScope;
let simulation: Simulation | undefined;
let running = true;
let ticksPerSecond = 30;
let selectedId: number | undefined;
let lastTime = performance.now();
let accumulator = 0;
let lastFlush = 0;
let lastTelemetry = 0;
let awaitingRender = false;

function send(message: WorkerResponse, transfer: Transferable[] = []): void { scope.postMessage(message, transfer); }
function fullSnapshot(): void {
  if (!simulation) return;
  const cells = simulation.cells.slice(); const owners = simulation.owners.slice();
  send({ type: "full-snapshot", width: simulation.width, height: simulation.height, cells, owners }, [cells.buffer, owners.buffer]);
  simulation.consumeDelta();
}
function flush(): void {
  if (!simulation) return;
  const delta = simulation.consumeDelta();
  if (delta.indices.length) { awaitingRender=true; send({ type: "cell-delta", ...delta }, [delta.indices.buffer, delta.cells.buffer, delta.owners.buffer]); }
  const now=performance.now();
  if(now-lastTelemetry>=100){lastTelemetry=now;send({type:"metrics",metrics:simulation.metrics()});if(selectedId!==undefined)send({type:"selection",id:selectedId,inspection:simulation.inspect(selectedId)});}
}

scope.onmessage = ({ data }: MessageEvent<WorkerCommand>) => {
  try {
    if (data.type === "initialize") {
      simulation = new Simulation({ width: data.width ?? 1024, height: data.height ?? 640, foodChance: 0.04, lifespan: 120 }, data.seed);
      send({ type: "ready", width: simulation.width, height: simulation.height }); fullSnapshot(); return;
    }
    if (!simulation) throw new Error("Simulation worker is not initialized");
    if(data.type==="render-ack"){awaitingRender=false;return;}
    if (data.type === "step-control") { running = data.running; ticksPerSecond = Math.max(1, Math.min(240, data.ticksPerSecond ?? ticksPerSecond)); }
    else if (data.type === "paint") simulation.paint(data.x, data.y, data.cellType);
    else if (data.type === "reset") { simulation.reset(); selectedId = undefined; fullSnapshot(); }
    else if (data.type === "settings") { if (data.foodChance !== undefined) simulation.foodChance = data.foodChance; if (data.lifespan !== undefined) simulation.lifespan = data.lifespan; }
    else if (data.type === "select") { selectedId = simulation.organismIdAt(data.x, data.y); simulation.select(selectedId); send({ type: "selection", id: selectedId, inspection: selectedId === undefined ? undefined : simulation.inspect(selectedId) }); }
    else if (data.type === "inspect") { selectedId = data.id; simulation.select(data.id); send({ type: "selection", id: data.id, inspection: simulation.inspect(data.id) }); }
    flush();
  } catch (error) { send({ type: "error", message: error instanceof Error ? error.message : String(error) }); }
};

setInterval(() => {
  const now = performance.now(); const elapsed = Math.min(250, now - lastTime); lastTime = now;
  if (simulation && running) { accumulator += elapsed*ticksPerSecond/1000; const steps=Math.floor(accumulator); if(steps>0){accumulator-=steps;simulation.step(steps);} if(!awaitingRender&&now-lastFlush>=1000/30){lastFlush=now;flush();} }
}, 16);
