# Hardware Verification Scenarios

This document defines comprehensive hardware testing scenarios for ESP32RC bring-up and validation.

## Prerequisites

- Assembled ESP32RC hardware with all connections soldered
- ESP32-DEV Module board
- USB cable connected to computer at 115200 baud
- Optional: Multimeter, oscilloscope, RC receiver/transmitter

## Scenario 1: Firmware Verification (No Hardware)

**Purpose:** Verify firmware boots and initializes without connected hardware  
**Time:** ~2 minutes  
**Required:** USB connection only

### Steps

1. **Flash firmware:**
   ```bash
   pio run --target upload
   ```

2. **Start serial monitor:**
   ```bash
   pio device monitor --baud 115200
   ```

3. **Expected output (first 3 seconds):**
   ```
   ESP32RC - High-Current Load Controller
   Starting initialization...
   RC inputs configured
   Output pins configured
   PWM output configured
   Current sensor ready  [or: Current sensor not found - continuing without monitoring]
   Serial self-test support enabled  [if esp32dev_selftest]
   Initialization complete!
   Ready to receive RC signals
   ```

4. **Verify status updates:**
   - Every 500ms, should see `=== ESP32RC Status ===`
   - System should show `ENABLED`
   - RC Link should show `LOST` (no receiver connected)
   - Power output should be `0%`

### Pass Criteria

- ✅ Firmware boots successfully
- ✅ Serial output at 115200 baud is readable
- ✅ Status updates appear every 500ms
- ✅ All outputs start at OFF/0%
- ✅ No repeated error messages

---

## Scenario 2: INA219 Sensor Detection

**Purpose:** Verify current/voltage monitoring hardware  
**Time:** ~5 minutes  
**Required:** Assembled INA219 on I2C (GPIO 21/22)

### Steps

1. **Boot and verify sensor:**
   ```
   [Watch serial output first 3 seconds]
   Look for: "INA219 initialized successfully"
   ```

2. **Enter self-test mode:**
   ```
   [Send via serial monitor]
   selftest on
   sensor
   ```

3. **Expected sensor output:**
   ```
   --- INA219 Sensor ---
   Bus voltage: X.XXX V
   Raw current: X.XXX mA
   Scaled current: X.XXX mA
   Raw power: X.XXX mW
   Scaled power: X.XXX W
   Current scale: XXX.XXXX
   ```

4. **Verify at zero load:**
   ```
   Bus voltage: Should be close to your power supply (e.g., 12V)
   Raw current: Should be ~0-100 mA (self-test circuit current)
   Scaled current: Should match raw (if scale = 25.0)
   ```

5. **Check calibration:**
   ```
   pwm 0
   sensor
   # Current should be minimal (~supply quiescent)
   
   pwm 50
   sensor
   # Current should increase proportionally
   ```

### Pass Criteria

- ✅ INA219 detected by firmware
- ✅ Bus voltage reading matches power supply
- ✅ Current starts at ~0 mA with no load
- ✅ Current increases with PWM level
- ✅ Voltage remains stable

### Failure Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| "INA219 not available" | I2C connection problem | Check GPIO 21/22 wiring |
| Completely wrong voltage (e.g., 0V) | Power supply issue | Verify 3.3V/5V to sensor VCC |
| Constant high current (~1000+ mA) | Sensor miscalibration | Run `scale reset` |
| Noise in readings | I2C pull-up too strong | Reduce from 4.7kΩ to 10kΩ |

---

## Scenario 3: RC Signal Reception (With Receiver)

**Purpose:** Verify RC input capture hardware  
**Time:** ~10 minutes  
**Required:** RC receiver connected (CH1-4 on GPIO 34/35/32/33)

### Steps

1. **Boot firmware:**
   ```bash
   pio device monitor
   ```

2. **Turn on RC transmitter and move controls:**
   - Move CH1 (throttle) stick slowly from neutral to min/max
   - Move CH2 (light) stick to extremes
   - Move CH3 and CH4 likewise

3. **Watch serial for RC signal detection:**
   ```
   First valid pulse → system shows:
   RC Link: OK
   CH1 (Power): XXXX us (XX%) OK
   CH2 (Lights): XXXX us OK
   CH3 (Aux1): XXXX us OK
   CH4 (Aux2): XXXX us OK
   ```

4. **Verify pulse width range (typical RC receiver):**
   - **Min stick:** ~1000 µs → 0%
   - **Neutral:** ~1500 µs → 50%
   - **Max stick:** ~2000 µs → 100%

5. **Test failsafe (RC signal loss):**
   - Turn off transmitter
   - Wait ~100 ms
   - Observe: CH1/2/3/4 all show `LOST`
   - Power output returns to 0%

### Pass Criteria

- ✅ RC Link changes from `LOST` → `OK` when transmitter on
- ✅ All 4 channels show pulse widths in 1000-2000 µs range
- ✅ Throttle percentage matches stick position
- ✅ Failsafe activates quickly (< 150 ms)
- ✅ RC Link returns `LOST` within 100 ms of tx off

### Failure Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| "RC Link: LOST" always | No signal | Check receiver wiring, verify pairing |
| Pulses outside 1000-2000 µs | Receiver miscalibration | Calibrate receiver in basic mode first |
| Only CH1 works | Other channels not wired | Check GPIO 35/32/33 connections |
| Noisy pulse readings | EMI from motor | Add 100nF capacitor across signal pin |
| 5V logic damage risk | Receiver outputting 5V | Add voltage divider: 1kΩ--[GPIO]--2kΩ--GND |

---

## Scenario 4: Power Stage Testing (Gate Driver)

**Purpose:** Verify PWM output and gate driver  
**Time:** ~10 minutes  
**Required:** Gate driver on GPIO 25, oscilloscope recommended

### Steps

1. **Use self-test mode (no RC needed):**
   ```
   [Serial]
   selftest on
   pwm 10
   ```

2. **Measure GPIO 25 with oscilloscope:**
   - Frequency should be 1 kHz (1 ms period)
   - At PWM 10%: ~100 µs high pulse per period
   - At PWM 50%: ~500 µs high pulse per period
   - At PWM 100%: ~1000 µs (continuous)

3. **Incrementally test PWM levels:**
   ```
   pwm 25  # Max allowed in self-test mode
   # Check voltage/frequency stable
   ```

4. **Verify no interference with other outputs:**
   ```
   light1 on
   light2 off
   aux off
   pwm 50
   # Lights should toggle independently
   ```

### Pass Criteria

- ✅ PWM frequency is 1 kHz
- ✅ Duty cycle matches percentage (+-5%)
- ✅ Output goes from 0V-3.3V (ESP32 logic levels)
- ✅ Gate driver receives signal without distortion
- ✅ No interference between PWM and digital outputs

### Oscilloscope Readings

**PWM 10%:**
```
Frequency: 1.000 kHz
High Time: 100 µs
Period: 1000 µs
Duty: 10%
```

**PWM 50%:**
```
Frequency: 1.000 kHz
High Time: 500 µs
Period: 1000 µs
Duty: 50%
```

---

## Scenario 5: Light & Auxiliary Output Control

**Purpose:** Verify digital output pins  
**Time:** ~5 minutes  
**Required:** LEDs on GPIO 26, 27, 14 (or multimeter to measure 3.3V)

### Steps

1. **Test with self-test mode:**
   ```
   selftest on
   light1 on
   light2 on
   aux on
   ```

2. **Verify lights turn on:**
   - GPIO 26 (Light 1) → 3.3V
   - GPIO 27 (Light 2) → 3.3V
   - GPIO 14 (Aux) → 3.3V

3. **Test individual control:**
   ```
   light1 off
   # Light 1 should turn off (GPIO 26 → 0V)
   # Light 2 and Aux still on
   
   light2 off
   aux off
   # All outputs now off
   ```

4. **Test with RC receiver (if available):**
   - Move CH2 stick → Light 1 toggles on/off
   - Move CH3 stick → Light 2 toggles on/off
   - Move CH4 stick → Aux toggles on/off

### Pass Criteria

- ✅ Each output can control independently
- ✅ Output is 3.3V when on, 0V when off
- ✅ No cross-talk between outputs
- ✅ RC receiver control works as expected
- ✅ Failsafe disables lights/aux when RC lost

---

## Scenario 6: Safety Shutdown & Recovery

**Purpose:** Verify overcurrent, over/under-voltage protection  
**Time:** ~15 minutes  
**Required:** Adjustable power supply, current limiting capability

### Steps

1. **Trigger overcurrent shutdown:**
   ```
   [With load connected]
   selftest on
   pwm 100  # Max power
   
   [Gradually increase load until...]
   OVERCURRENT DETECTED: XX.X A - Shutting down!
   [Power output goes to 0%]
   ```

2. **Wait for automatic recovery:**
   ```
   [Wait 5 seconds with load removed]
   [Observe serial output]
   Safety limits stable - attempting recovery
   [Power output resumes if RC input present]
   ```

3. **Trigger voltage violations:**
   ```
   [Lower supply voltage]
   UNDERVOLTAGE DETECTED: 8.5V - Shutting down!
   
   [Raise supply voltage]
   Safety limits stable - attempting recovery
   ```

4. **Test overvoltage:**
   ```
   [Boost supply above 26V]
   OVERVOLTAGE DETECTED: 27.5V - Shutting down!
   
   [Wait 5 seconds for recovery attempt]
   Recovery conditions not met...
   [No recovery until voltage in range]
   ```

### Pass Criteria

- ✅ System detects overcurrent correctly
- ✅ System detects under/over-voltage
- ✅ All outputs shut off immediately
- ✅ System recovers after 5 seconds if conditions safe
- ✅ Recovery requires fresh RC signal to re-enable power
- ✅ No permanent lockup

### Recovery Test Details

| Scenario | Trigger | Shutdown Time | Recovery Time | Recovery Condition |
|----------|---------|---|---|---|
| Overcurrent | Load shock | Immediate | 5 sec | Load < threshold - 2A |
| Undervoltage | Supply drop | Immediate | 5 sec | Voltage > MIN_VOLTAGE_V |
| Overvoltage | Supply spike | Immediate | 5 sec | Voltage <= MAX_VOLTAGE_V |

---

## Scenario 7: Full System Integration Test

**Purpose:** End-to-end test with RC receiver + load  
**Time:** ~20 minutes  
**Required:** Complete assembled system with receiver and small motor/light load

### Steps

1. **Power up system:**
   ```bash
   pio device monitor
   ```

2. **Transmitter on, RC link established:**
   ```
   Observe: "RC Link: OK"
   All 4 channels show valid pulse widths
   ```

3. **Throttle test (CH1):**
   - Min → Power output 0%
   - Neutral → Power output ~50%
   - Max → Power output 100%
   - **Result:** Load (motor/light) responds proportionally

4. **Light control test (CH2/CH3):**
   - Move CH2 up → Light 1 on
   - Move CH2 down → Light 1 off
   - Similar for CH3 → Light 2

5. **Auxiliary test (CH4):**
   - Move CH4 → Auxiliary output toggles
   - Verify independent of other channels

6. **Failsafe test:**
   - Transmitter on, load spinning
   - Turn transmitter off
   - Observe:
     - Power drops to 0% immediately
     - RC Link changes to LOST
     - All lights off

7. **Recovery test:**
   - While RC is lost, increase load current draw
   - After 5 sec with load removed, turn transmitter back on
   - Verify system resumes power control

### Pass Criteria

- ✅ All 4 functions work independently
- ✅ Throttle matches RC position (±5%)
- ✅ Lights toggle cleanly without glitch
- ✅ Failsafe effective (< 100 ms)
- ✅ Automatic recovery works
- ✅ Serial status updates every 500 ms
- ✅ Sensor readings consistent

---

## Scenario 8: Stress Test (Optional)

**Purpose:** Verify stability under prolonged operation  
**Time:** 1 hour continuous

### Steps

1. **Ramp power:**
   ```
   Cycle throttle slowly: 0% → 100% → 0% (repeat 60 times)
   Observe: No crashes, stable current/voltage
   ```

2. **Temperature monitoring:**
   - Check MOSFET temperature (should not exceed 60°C continuous)
   - Check ESP32 board temperature (should stay cool)
   - Current consumption should be stable

3. **Memory stability:**
   ```
   [After 1 hour]
   Observe free RAM in serial output
   Should not decrease significantly (no memory leak)
   ```

4. **Serial reliability:**
   - Count serial updates: Should be ~7200 (every 500ms × 3600s)
   - No corrupted output or garbled text

### Pass Criteria

- ✅ No crashes or resets over 1 hour
- ✅ MOSFET temperature acceptable
- ✅ RAM stable (no leaks apparent)
- ✅ Serial updates reliable
- ✅ Current/voltage readings consistent

---

## Appendix: Serial Command Reference

### Status Commands
```
status              # Full system status
sensor              # INA219 readings (if available)
help                # Command help (self-test mode)
```

### Self-Test Mode
```
selftest on         # Enable self-test (requires no RC signal)
selftest off        # Disable self-test
pwm 0-25            # Set PWM percentage (limited to 25% max)
light1 on|off       # Control light 1
light2 on|off       # Control light 2
aux on|off          # Control auxiliary output
```

### Calibration
```
scale show          # Show current calibration scale
scale reset         # Reset to compile-time default
scale <value>       # Set new scale value (e.g., scale 15.0)
scale save          # Save scale to NVS (persists across reboot)
scale clear         # Clear saved scale from NVS
```

---

**Last Updated:** 2026-03-07  
**Firmware Version:** v1.1.1  
**Created for:** ESP32-DEV with INA219 sensor
