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
  const cells=simulation.cells.slice(),owners=simulation.owners.slice(),terrain=simulation.terrain.slice(),resources=simulation.resources.slice(),resourceAmount=simulation.resourceAmount.slice();
  send({type:"full-snapshot",width:simulation.width,height:simulation.height,cells,owners,terrain,resources,resourceAmount},[cells.buffer,owners.buffer,terrain.buffer,resources.buffer,resourceAmount.buffer]);
  simulation.consumeDelta();
}
function flush(): void {
  if (!simulation) return;
  const activeSimulation=simulation;
  const delta = activeSimulation.consumeDelta();
  if(delta.indices.length){const terrain=Uint8Array.from(delta.indices,index=>activeSimulation.terrain[index]!),resources=Uint8Array.from(delta.indices,index=>activeSimulation.resources[index]!),resourceAmount=Uint16Array.from(delta.indices,index=>activeSimulation.resourceAmount[index]!);awaitingRender=true;send({type:"cell-delta",...delta,terrain,resources,resourceAmount},[delta.indices.buffer,delta.cells.buffer,delta.owners.buffer,terrain.buffer,resources.buffer,resourceAmount.buffer]);}
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
    if(data.type==="regenerate"){simulation.regenerateWorld(data.seed);selectedId=undefined;fullSnapshot();return;}
    if(data.type==="paint-terrain")simulation.paintTerrain(data.x,data.y,data.terrainType);
    else if(data.type==="paint-resource")simulation.paintResource(data.x,data.y,data.resourceType,data.amount);
    else if (data.type === "step-control") { running = data.running; ticksPerSecond = Math.max(1, Math.min(240, data.ticksPerSecond ?? ticksPerSecond)); }
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
