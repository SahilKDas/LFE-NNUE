# Native performance results

Development-machine stress sample, 2026-09-13:

```text
life_engine_native_benchmark.exe 60
organisms=10000 elapsed_s=60.00 ticks=28216 measured_tps=470.26 nnue_eval_s=592029.96
```

This isolated release-gate sample uses deterministic multi-cell organisms with mixed mouth types, killers, armor, producers, perception radii, and channel masks on the 1024x640 layered world. It shares an immutable NNUE genome and disables reproduction and mortality so population size remains controlled while climate, physiology, resources, combat interactions, perception, inference, collision, and movement execute normally. The result exceeds the 60-TPS target by 7.8x.

Integrated native UI acceptance sample, 2026-09-13:

```text
life_engine.exe --ui-benchmark=60
organisms=10000 elapsed_s=60.00 measured_tps=60.01 render_fps=63.66 render_ms=3.31 input_p95_ms=6.12 interactions=229 result=PASS
```

This second gate runs the actual 1500x900 Win32/Skia application, including simulation and presentation threads, density rendering, telemetry, and repeated resource-paint interactions. It verifies the user-facing performance targets of 60 simulation TPS, at least 45 render FPS, and under 100 ms p95 input-to-visible response.
