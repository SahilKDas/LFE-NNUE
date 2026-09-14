# Native performance results

Development-machine stress sample, 2026-09-13:

```text
life_engine_native_benchmark.exe 60
organisms=10000 elapsed_s=60.00 ticks=26012 measured_tps=433.53 nnue_eval_s=395028.14
```

The benchmark holds exactly 10,000 one-cell movers on the 1024x640 layered world, shares an immutable NNUE genome, runs climate, physiology, perception, inference, collision, and movement, and disables reproduction and mortality so population size remains controlled. The native GUI was active during this sample; repeat in isolation for the formal release gate.
