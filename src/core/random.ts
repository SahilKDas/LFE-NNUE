export function mulberry32(seed: number): () => number {
  let state=seed>>>0;
  return () => { state=(state+0x6d2b79f5)>>>0; let value=Math.imul(state^(state>>>15),1|state); value^=value+Math.imul(value^(value>>>7),61|value); return ((value^(value>>>14))>>>0)/4294967296; };
}

export function hash2d(x:number,y:number,seed:number):number {
  let value=Math.imul(x^seed,0x45d9f3b)^Math.imul(y+seed,0x119de1f3); value=Math.imul(value^(value>>>16),0x45d9f3b); return ((value^(value>>>16))>>>0)/4294967295;
}
