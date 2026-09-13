import { performance } from 'node:perf_hooks';
import { createEcologyKernel,stepEcologyKernel,kernelChecksum } from '../public/nim-ecology-kernel.js';
const kernel=createEcologyKernel(10_000,1024,640);stepEcologyKernel(kernel,10);const start=performance.now();stepEcologyKernel(kernel,60);const elapsed=performance.now()-start,tps=60_000/elapsed;
console.log(`Nim JS 10,000-organism kernel: ${tps.toFixed(1)} TPS (${elapsed.toFixed(1)} ms, checksum ${kernelChecksum(kernel)})`);if(tps<60)throw new Error(`Nim JS kernel below 60 TPS: ${tps.toFixed(1)}`);
