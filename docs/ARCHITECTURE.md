# ESP32RC Architecture

## Runtime blocks

```text
RC receiver PWM inputs
        |
        v
GPIO interrupts measure pulse widths
        |
        v
Channel validity checks
        |
        +--> CH1 -> PWM power control
        +--> CH2 -> Light 1 output
        +--> CH3 -> Light 2 output
        +--> CH4 -> Aux output
        |
        v
Safety manager
        |
        +--> INA219 monitoring
        +--> Overcurrent shutdown
        +--> Undervoltage shutdown
        +--> Overvoltage shutdown
```

## Main loop

1. Periodically read INA219 if present.
2. Apply safety limits.
3. Update PWM output from `CH1` only if a fresh `CH1` pulse exists.
4. Update lights and auxiliary output from their own fresh channels.
5. Print serial status at the configured interval.

## RC validity model

- A channel is considered valid only after at least one valid pulse has been captured.
- A channel becomes stale if it is not refreshed within `RC_TIMEOUT`.
- Power output is gated by `CH1` only.
- Lights and auxiliary outputs are gated by their own channels.

This prevents stale data on unrelated channels from keeping throttle active.

## Startup behavior

- All outputs start OFF.
- No output is enabled until the firmware receives a valid pulse for the relevant channel.
- This removes the earlier startup window where default pulse values could look valid before any RC input had arrived.

## Monitoring behavior

- INA219 setup is optional.
- If INA219 initialization fails, the firmware continues without voltage or current monitoring.
- With the stock monitoring path, the monitored bus ceiling is 26V.

## Key files

- `src/main.cpp`
- `include/config.h`
