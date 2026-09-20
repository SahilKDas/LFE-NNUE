# Native performance results

Development-machine stress sample, 2026-09-13:

```text
life_engine_native_benchmark.exe 60
organisms=10000 elapsed_s=60.00 ticks=28216 measured_tps=470.26 nnue_eval_s=592029.96
```

This historical result used a shared NNUE and disabled reproduction and mortality. It is retained only for comparison.

Plant-inclusive diverse ecology sample, 2026-09-16:

```text
organisms=9997 elapsed_s=5.00 ticks=691 measured_tps=138.06 nnue_eval_s=161896.70 brain_families=732 plant_tiles=39339 births=2865 deaths=2857
```

The benchmark is now split. `life_engine_native_benchmark` holds the 10,000-organism population fixed so throughput cannot fail because ecology changed. `life_engine_ecology_benchmark` runs the full lifecycle for a fixed tick count twice and validates deterministic Atlas samples, species, resources, births, deaths, and bounded retention.

Evolution Atlas smoke sample, 2026-09-19:

```text
performance: organisms=10000 measured_tps=239.67 brain_families=256 species=48
ecology: ticks=300 organisms=9998 births=10354 deaths=10356 living_species=148 deterministic=yes
```

Integrated native UI acceptance sample, 2026-09-13:

```text
life_engine.exe --ui-benchmark=60
organisms=10000 elapsed_s=60.00 measured_tps=60.01 render_fps=63.66 render_ms=3.31 input_p95_ms=6.12 interactions=229 result=PASS
```

This second gate runs the actual 1500x900 Win32/Skia application, including simulation and presentation threads, density rendering, telemetry, and repeated resource-paint interactions. It verifies the user-facing performance targets of 60 simulation TPS, at least 45 render FPS, and under 100 ms p95 input-to-visible response.
