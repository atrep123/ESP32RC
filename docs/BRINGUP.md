# ESP32RC Bring-Up Checklist

## Scope

This checklist is for the first flash and first powered hardware tests of the stock PlatformIO firmware in `src/main.cpp`.

## Safety first

1. Keep the monitored power bus at or below `26V` with the stock INA219 setup.
2. Start without the final load connected.
3. If possible, use a current-limited bench supply for first power tests.
4. Verify common ground between ESP32, RC receiver, gate driver, and sensor.
5. Verify the RC receiver output voltage is safe for ESP32 GPIO.

## Stage 1: firmware only

1. Build the project:

```bash
platformio run
```

2. Flash the board:

```bash
platformio run --target upload
```

3. Open the serial monitor:

```bash
platformio device monitor
```

Expected result:

- Boot messages are readable at `115200`.
- No output pin should turn on by itself.
- Power PWM should remain off before the first valid `CH1` pulse.

Optional self-test firmware:

```bash
platformio run -e esp32dev_selftest --target upload
platformio device monitor
```

When `esp32dev_selftest` is flashed, these serial commands are available:

- `help`
- `status`
- `sensor`
- `scale show`
- `scale reset`
- `scale save`
- `scale clear`
- `scale <value>`
- `selftest on`
- `selftest off`
- `pwm 0-25`
- `light1 on|off`
- `light2 on|off`
- `aux on|off`

Notes:

- Self-test refuses to start while a live RC signal is present.
- Self-test automatically stops if a live RC signal appears.
- PWM is intentionally capped by `SELF_TEST_PWM_MAX_PERCENT`.

## Stage 2: receiver validation

1. Connect only the RC receiver signal and ground wiring.
2. Power the receiver.
3. Move each configured channel one at a time.

Expected result:

- `CH1` changes when throttle moves.
- `CH2`, `CH3`, and `CH4` report activity only when their own channels move.
- `RC Link: OK` appears after valid pulses are received.

## Stage 3: failsafe validation

1. With the receiver active, move `CH1` so PWM is clearly non-zero.
2. Turn the transmitter off or remove the `CH1` signal.

Expected result:

- Power output goes to zero.
- Light and auxiliary outputs also turn off when their own channels go stale.
- The serial log shows the affected channel as `LOST`.

Important check:

- Loss of `CH1` must disable throttle even if other channels are still alive.

## Stage 4: power stage without load

1. Connect the gate driver and power stage.
2. Leave the main load disconnected.
3. Measure the PWM output path with a meter or scope if available.

Expected result:

- No PWM before a valid `CH1` pulse.
- PWM duty rises only with live `CH1`.
- Switching `CH2`, `CH3`, and `CH4` affects only their assigned outputs.

## Stage 5: low-risk load test

1. Connect a small test load or dummy load.
2. Increase throttle slowly.
3. Watch the serial status output continuously.

Expected result:

- Current and voltage readings are stable if INA219 is installed.
- No unexpected shutdown occurs under normal load.
- Overcurrent and voltage limits remain within configured thresholds.

## Stage 6: INA219 validation

1. Compare reported bus voltage to a multimeter.
2. Compare reported current to an external reference if available.
3. In `esp32dev_selftest`, use `sensor` to view raw and scaled INA219 values.
4. Use `scale <value>` to tune the runtime multiplier until scaled current matches the reference.
5. Use `scale save` to persist the tuned value in ESP32 `Preferences`.
6. Optionally copy the final value into `CURRENT_SCALE` in `include/config.h` so the repository matches the calibrated hardware.

Expected result:

- Voltage is close to the measured bus voltage.
- Current scaling is consistent with the installed shunt or divider arrangement.

## Stop conditions

Stop testing immediately if any of the following happens:

- Output turns on before a valid `CH1` pulse.
- Throttle stays active after `CH1` is removed.
- Measured bus voltage exceeds `26V` with the stock INA219 configuration.
- MOSFET, shunt, or wiring heats rapidly during light-load tests.
- Serial output reports repeated overcurrent, undervoltage, or overvoltage faults.

## Recommended follow-up

After basic bring-up succeeds:

1. Save your real hardware limits in `include/config.h`.
2. Rebuild and reflash.
3. Repeat the failsafe test.
4. Only then connect the final load.
