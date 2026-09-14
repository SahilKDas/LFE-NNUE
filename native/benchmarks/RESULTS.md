# Native performance results

Development-machine stress sample, 2026-09-13:

```text
life_engine_native_benchmark.exe 60
organisms=10000 elapsed_s=60.00 ticks=28216 measured_tps=470.26 nnue_eval_s=592029.96
```

This isolated release-gate sample uses deterministic multi-cell organisms with mixed mouth types, killers, armor, producers, perception radii, and channel masks on the 1024x640 layered world. It shares an immutable NNUE genome and disables reproduction and mortality so population size remains controlled while climate, physiology, resources, combat interactions, perception, inference, collision, and movement execute normally. The result exceeds the 60-TPS target by 7.8x.
