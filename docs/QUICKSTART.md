# ESP32RC Quick Start Guide

## 5-Minute Setup

### What You Need
1. ESP32 development board
2. USB cable
3. RC receiver (PWM output)
4. INA219 current sensor module (optional but recommended)
5. Power stage components (MOSFET + driver)

### Software Installation

#### Option 1: PlatformIO (Recommended)
```bash
# Install PlatformIO
pip install platformio

# Clone and build
git clone https://github.com/atrep123/ESP32RC.git
cd ESP32RC
pio run --target upload
pio device monitor
```

#### Option 2: Arduino IDE
1. Install ESP32 board support
2. Install libraries: Adafruit INA219, Adafruit BusIO
3. Open `src/main.cpp` (rename to `.ino`)
4. Select "ESP32 Dev Module"
5. Upload

### Basic Wiring

**Minimum Setup (without current monitoring):**
```
RC Receiver CH1 → ESP32 GPIO 34
ESP32 GPIO 25   → Gate Driver Input
Gate Driver Out → MOSFET Gate
ESP32 GND       → Common GND
ESP32 5V        → From USB or external 5V
```

**Complete Setup (with monitoring):**
```
Add:
INA219 SDA → ESP32 GPIO 21
INA219 SCL → ESP32 GPIO 22
INA219 VCC → 3.3V or 5V
INA219 GND → GND
```

### First Test

1. **Power On**: Connect ESP32 via USB
2. **Open Serial Monitor**: 115200 baud
3. **Check Output**: You should see status messages
4. **Move RC Stick**: Channel 1 values should change (1000-2000 μs)
5. **Verify Safety**: Turn off transmitter, confirm outputs go to zero

### Default Pin Configuration

| Function | Pin | Notes |
|----------|-----|-------|
| RC Channel 1 (Power) | GPIO 34 | Main throttle |
| RC Channel 2 (Light 1) | GPIO 35 | Optional |
| RC Channel 3 (Light 2) | GPIO 32 | Optional |
| RC Channel 4 (Aux) | GPIO 33 | Optional |
| Power PWM Out | GPIO 25 | To gate driver |
| Light 1 Out | GPIO 26 | Digital ON/OFF |
| Light 2 Out | GPIO 27 | Digital ON/OFF |
| Aux Out | GPIO 14 | Digital ON/OFF |
| I2C SDA | GPIO 21 | INA219 |
| I2C SCL | GPIO 22 | INA219 |

### Common Issues & Quick Fixes

**Issue: "RC Signal: LOST"**
- Check RC receiver is on and bound
- Verify wiring connections
- Check voltage levels (use 3.3V or voltage divider for 5V signals)

**Issue: "Failed to find INA219"**
- Check I2C wiring (SDA, SCL)
- Verify INA219 has power
- System continues without monitoring (warning only)

**Issue: "OVERCURRENT DETECTED"**
- Reduce load or increase threshold in `config.h`
- Check for short circuits
- Verify current sensor calibration

**Issue: No PWM output**
- Check `system_enabled` flag (should be true)
- Verify RC signal is valid
- Check safety limits not triggered

### Customization

**Change Power Limits:**
Edit `include/config.h`:
```cpp
#define MAX_CURRENT_A 50.0              // Your max current
#define OVERCURRENT_THRESHOLD 48.0      // Safety threshold
#define MAX_VOLTAGE_V 60.0              // Your max voltage
```

**Change Pins:**
Edit `include/config.h` and modify pin definitions.

**Adjust Light Thresholds:**
```cpp
#define LIGHT_ON_THRESHOLD 1600   // RC value to turn ON
#define LIGHT_OFF_THRESHOLD 1400  // RC value to turn OFF
```

### Serial Monitor Output Explained

```
=== ESP32RC Status ===
System: ENABLED              ← System state (ENABLED/DISABLED)
RC Signal: OK                ← RC connection (OK/LOST)

--- RC Channels ---
CH1 (Power): 1750 us (75%)  ← Throttle position
CH2 (Lights): 1500 us       ← Light switch position
...

--- Outputs ---
Power Output: 75%            ← Actual power output
Light 1: ON                  ← Light states
...

--- Power Monitoring ---
Voltage: 24.5 V             ← Battery voltage
Current: 12.3 A             ← Load current
Power: 301.4 W              ← Power consumption
```

### Safety Features

✓ **Automatic Shutdown** on:
- Overcurrent (>threshold)
- Overvoltage (>max)
- Undervoltage (<min)

✓ **Failsafe** on:
- RC signal loss (>100ms)
- Outputs go to zero/off

✓ **Protected Inputs**:
- PWM validation (1000-2000μs range)
- Time-based signal verification

### Next Steps

1. **Test Basic Functionality**: Verify RC input and serial output
2. **Add Current Sensor**: For monitoring and protection
3. **Build Power Stage**: MOSFET + driver circuit (see HARDWARE.md)
4. **Calibrate**: Adjust current sensor scaling
5. **Test with Load**: Start small, work up to rated current
6. **Add Enclosure**: Protect electronics

### Resources

- **Full Documentation**: [README.md](../README.md)
- **Hardware Guide**: [docs/HARDWARE.md](HARDWARE.md)
- **Examples**: [docs/EXAMPLES.md](EXAMPLES.md)
- **Support**: Open an issue on GitHub

### Warning Signs

🚨 **STOP and check if you see:**
- Repeated overcurrent shutdowns
- Voltage readings outside expected range
- Hot components (MOSFETs, wires)
- Smoke or burning smell
- Erratic behavior or crashes

### Safety First!

⚠️ **Always:**
- Use proper wire gauge (8 AWG for 50A)
- Add fuses for protection
- Ensure adequate heat sinking
- Test incrementally (start low current)
- Have fire safety equipment ready
- Never bypass safety features

⚠️ **Never:**
- Exceed component ratings
- Work on live high-voltage circuits
- Skip testing phases
- Ignore warning signs
- Use in wet conditions without proper protection

---

**Ready to build?** Start with the hardware guide for complete schematics and BOM!

**Need help?** Check the examples for common use cases!

**Found a bug?** Open an issue on GitHub!