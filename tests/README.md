# Timer Manager host-side tests

Run on Linux, WSL or Git Bash with GCC:

```bash
bash tests/run_host_tests.sh
```

The mock `device_registers.h` is *test-only*. Do not add `tests/` to the S32 Design Studio MCU build include paths.

Coverage: LPIT source/configuration, CH2 1 ms tick, 100/200 ms coalesced display flags, CH0 periodic callbacks, CH1 TSOI (one-shot), rearm, channel limits, NVIC gating, reinitialization.

**Limits:** these are register-level mocks, not actual hardware timing or real S32DS compilation. Validate with the NXP device header, startup IRQ names, scope/logic analyzer, and actual EVB before accepting timing claims. LPIT functional clock is hardcoded to 40 MHz (SPLLDIV2); clock configuration changes must update this assumption.
