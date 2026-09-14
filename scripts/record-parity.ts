import { mkdir, writeFile } from "node:fs/promises";
import { resolve } from "node:path";
import { recordParity } from "../src/core/parity";

const output = resolve("reference/parity/simulation-v1.json");
await mkdir(resolve("reference/parity"), { recursive: true });
await writeFile(output, `${JSON.stringify(recordParity(), null, 2)}\n`, "utf8");
console.log(`Wrote ${output}`);
