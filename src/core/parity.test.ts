import { readFileSync } from "node:fs";
import { resolve } from "node:path";
import { describe, expect, it } from "vitest";
import { recordParity } from "./parity";

describe("native rewrite parity fixture", () => {
  it("matches the frozen deterministic TypeScript trace", () => {
    const expected = JSON.parse(readFileSync(resolve("reference/parity/simulation-v1.json"), "utf8"));
    expect(recordParity()).toEqual(expected);
  }, 15_000);
});
