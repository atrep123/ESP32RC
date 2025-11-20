# ESP32RC - High-Current Load Controller

Modul pro bezdrátové řízení vysokoproudové zátěže (až ~50 A) pomocí ESP32, který přijímá PWM signály z RC soupravy, převádí je na logiku pro ovládání světel, řízení výkonu a hlídání napětí i proudu v reálném čase.

*Module for wireless control of high-current loads (up to ~50A) using ESP32, which receives PWM signals from RC equipment, converts them to logic for controlling lights, power management, and real-time voltage and current monitoring.*

## Features

- **RC PWM Input**: 4-channel RC receiver input (standard 1000-2000μs PWM signals)
- **Power Control**: Variable power output (0-100%) with PWM control for MOSFET gates
- **Light Control**: Digital ON/OFF control for multiple lights with hysteresis
- **Real-time Monitoring**: Continuous voltage and current monitoring using INA219 sensor
- **Safety Features**: 
  - Overcurrent protection (configurable threshold)
  - Undervoltage protection
  - Overvoltage protection
  - Automatic shutdown on safety violations
  - RC signal loss detection
- **High Current Capability**: Designed for loads up to 50A with proper external power stage

## Hardware Requirements

### ESP32 Board
- Any ESP32 development board (ESP32-DevKitC, NodeMCU-32S, etc.)
- Recommended: Board with at least 4MB flash

### Required Components
- **INA219 Current/Voltage Sensor**: For monitoring (I2C)
- **RC Receiver**: Standard PWM output (4+ channels)
- **Power MOSFETs**: For switching high-current loads (e.g., IRFP260N, IRFP3077)
- **Gate Drivers**: Recommended for driving power MOSFETs (e.g., IR2110, UCC27211)
- **Power Supply**: 
  - Main power: 12-48V for the load (up to 50A capacity)
  - ESP32 power: 5V regulated supply
- **Current Shunt**: Low-resistance shunt (e.g., 0.01Ω) for current measurement
- **Passive Components**: Pull-down resistors for MOSFET gates, bypass capacitors

### Wiring Diagram

```
RC Receiver PWM Outputs → ESP32 GPIO (34, 35, 32, 33)
                          ↓
                    ESP32 Processing
                          ↓
                    ┌─────┴─────┐
                    ↓           ↓
              PWM Output    Digital Outputs
              (GPIO 25)     (GPIO 26, 27, 14)
                    ↓           ↓
              Gate Driver   Relay/Driver
                    ↓           ↓
              Power MOSFET  Lights/Aux
                    ↓
              Load (0-50A)
                    ↓
              Current Shunt
                    ↓
              INA219 → I2C → ESP32 (GPIO 21, 22)
```

### Pin Connections

| Function | ESP32 Pin | Component |
|----------|-----------|-----------|
| RC Channel 1 (Power) | GPIO 34 | RC Receiver CH1 |
| RC Channel 2 (Lights) | GPIO 35 | RC Receiver CH2 |
| RC Channel 3 (Aux1) | GPIO 32 | RC Receiver CH3 |
| RC Channel 4 (Aux2) | GPIO 33 | RC Receiver CH4 |
| Power PWM Output | GPIO 25 | MOSFET Gate Driver |
| Light 1 Output | GPIO 26 | Relay/Driver |
| Light 2 Output | GPIO 27 | Relay/Driver |
| Auxiliary Output | GPIO 14 | Relay/Driver |
| I2C SDA | GPIO 21 | INA219 SDA |
| I2C SCL | GPIO 22 | INA219 SCL |

**Important Notes:**
- All RC inputs use ESP32's ADC1 pins (supports input during WiFi operation)
- Use 3.3V logic levels or voltage dividers for 5V RC signals
- Ensure proper grounding between RC receiver, ESP32, and power stage
- Use optical isolation between ESP32 and high-power stage for safety

## Software Setup

### Using PlatformIO (Recommended)

1. **Install PlatformIO**:
   - Install [Visual Studio Code](https://code.visualstudio.com/)
   - Install [PlatformIO extension](https://platformio.org/install/ide?install=vscode)

2. **Clone and Build**:
   ```bash
   git clone https://github.com/atrep123/ESP32RC.git
   cd ESP32RC
   pio run
   ```

3. **Upload to ESP32**:
   ```bash
   pio run --target upload
   ```

4. **Monitor Serial Output**:
   ```bash
   pio device monitor
   ```

### Using Arduino IDE

1. **Install ESP32 Board Support**:
   - Add to Board Manager URLs: `https://dl.espressif.com/dl/package_esp32_index.json`
   - Install "esp32" by Espressif Systems

2. **Install Required Libraries**:
   - Adafruit INA219
   - Adafruit BusIO

3. **Open Project**:
   - Copy `src/main.cpp` to Arduino sketch folder (rename to `.ino`)
   - Copy `include/config.h` to the same folder
   - Select board: "ESP32 Dev Module"
   - Upload

## Configuration

Edit `include/config.h` to customize:

### Pin Assignments
Modify pin definitions to match your hardware setup.

### RC Signal Parameters
```cpp
#define RC_MIN_PULSE 1000     // Minimum pulse width (μs)
#define RC_MAX_PULSE 2000     // Maximum pulse width (μs)
#define RC_MID_PULSE 1500     // Center position
#define RC_TIMEOUT 100        // Signal loss timeout (ms)
```

### Safety Limits
```cpp
#define MAX_CURRENT_A 50.0              // Maximum current
#define OVERCURRENT_THRESHOLD 48.0      // Shutdown threshold
#define MIN_VOLTAGE_V 10.0              // Minimum voltage
#define MAX_VOLTAGE_V 60.0              // Maximum voltage
```

### Light Control
```cpp
#define LIGHT_ON_THRESHOLD 1600   // PWM value to turn ON
#define LIGHT_OFF_THRESHOLD 1400  // PWM value to turn OFF
```

### Current Sensor Calibration
In `src/main.cpp`, adjust the current scaling factor based on your shunt resistor and INA219 configuration:

```cpp
float current_scale = 25.0; // Adjust based on hardware
```

For 50A with 0.01Ω shunt:
- INA219 max: 3.2A (with 0.1Ω shunt)
- With 0.01Ω external shunt: 32A max direct
- Use current divider or adjust scaling factor

## Operation

### Power-Up Sequence
1. ESP32 boots and initializes peripherals
2. RC inputs are configured with interrupt handlers
3. INA219 sensor is initialized
4. System enters monitoring mode

### Normal Operation
1. **RC Signal Input**: 
   - Channel 1 controls power output (0-100%)
   - Channel 2 controls Light 1 (ON/OFF with hysteresis)
   - Channel 3 controls Light 2 (ON/OFF with hysteresis)
   - Channel 4 controls auxiliary output

2. **Power Control**:
   - RC stick position maps to 0-100% power
   - PWM output drives MOSFET gate (via driver)
   - Smooth linear control

3. **Monitoring**:
   - Every 100ms: Read voltage and current
   - Check safety limits
   - Automatic shutdown if limits exceeded

4. **Serial Output**:
   - Every 500ms: Print status to serial monitor
   - Shows RC values, outputs, voltage, current, power

### Safety Features

**Signal Loss Protection**: 
- If RC signal lost for >100ms, outputs go to safe state (zero power, lights off)

**Overcurrent Protection**:
- Continuous monitoring
- Automatic shutdown if current exceeds threshold
- Manual reset required (power cycle)

**Voltage Protection**:
- Monitors for under/over voltage
- Protects against battery issues
- Automatic shutdown on violation

## Serial Monitor Output

```
=== ESP32RC Status ===
System: ENABLED
RC Signal: OK

--- RC Channels ---
CH1 (Power): 1750 us (75%)
CH2 (Lights): 1800 us
CH3 (Aux1): 1200 us
CH4 (Aux2): 1500 us

--- Outputs ---
Power Output: 75%
Light 1: ON
Light 2: OFF

--- Power Monitoring ---
Voltage: 24.5 V
Current: 12.3 A
Power: 301.4 W
====================
```

## Use Cases

1. **RC Electric Boats**: High-current motor control with lights
2. **RC Trucks**: Power and lighting control with current monitoring
3. **Robotics**: High-power motor control with safety features
4. **DIY Electric Vehicles**: ESC replacement with monitoring
5. **Industrial RC Control**: Remote control of high-power equipment

## Safety Warnings

⚠️ **HIGH CURRENT WARNING**: This system can control very high currents (50A+)
- Use appropriate wire gauge (minimum 8 AWG for 50A)
- Ensure proper heat sinking for MOSFETs
- Use fuses or circuit breakers
- Follow electrical safety guidelines
- Test with low currents first

⚠️ **ELECTRICAL SAFETY**:
- Never work on live circuits
- Use proper insulation
- Ensure adequate ventilation for power components
- Keep away from flammable materials

⚠️ **RC SAFETY**:
- Always test failsafe behavior
- Verify signal loss handling
- Use proper RC equipment
- Never bypass safety features

## Troubleshooting

**RC Signal Not Detected**:
- Check wiring and connections
- Verify RC receiver is powered and bound
- Check voltage levels (3.3V max for ESP32)
- Monitor serial output for RC values

**Current Sensor Not Found**:
- Check I2C connections (SDA, SCL)
- Verify INA219 address (default 0x40)
- Check power supply to INA219
- System continues without monitoring (warning printed)

**Overcurrent Shutdown**:
- Check load and wiring
- Verify current sensor calibration
- Adjust `OVERCURRENT_THRESHOLD` if needed
- Check for short circuits

**Erratic Behavior**:
- Check for loose connections
- Verify power supply quality
- Add decoupling capacitors
- Check for electrical interference

## Development

### Building from Source
```bash
# Clone repository
git clone https://github.com/atrep123/ESP32RC.git
cd ESP32RC

# Build
pio run

# Upload
pio run --target upload

# Monitor
pio device monitor
```

### Project Structure
```
ESP32RC/
├── platformio.ini      # PlatformIO configuration
├── include/
│   └── config.h       # Pin definitions and constants
├── src/
│   └── main.cpp       # Main application code
└── README.md          # This file
```

## Contributing

Contributions are welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## License

This project is open source. Please check the repository for license details.

## Authors

- GitHub: [@atrep123](https://github.com/atrep123)

## Acknowledgments

- Adafruit for the INA219 library
- ESP32 Arduino Core team
- PlatformIO development team

## Support

For issues, questions, or suggestions:
- Open an issue on GitHub
- Check existing issues for solutions

---

**Version**: 1.0.0  
**Last Updated**: November 2025