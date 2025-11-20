# ESP32RC Hardware Guide

## Complete Wiring Schematic

### System Overview

```
┌─────────────────┐
│   RC Receiver   │
│   (PWM Output)  │
└────────┬────────┘
         │ 4x PWM Signals (1000-2000μs)
         │
    ┌────▼──────────────────────┐
    │      ESP32 Module         │
    │  ┌──────────────────┐    │
    │  │  RC Input (ISR)  │    │
    │  │  GPIO 34,35,32,33│    │
    │  └────────┬─────────┘    │
    │           │               │
    │  ┌────────▼─────────┐    │
    │  │  Control Logic   │    │
    │  │  Safety Checks   │    │
    │  └────────┬─────────┘    │
    │           │               │
    │  ┌────────▼─────────┐    │
    │  │  Outputs:        │    │
    │  │  - PWM (GPIO 25) │    │
    │  │  - Lights (26,27)│    │
    │  │  - Aux (GPIO 14) │    │
    │  └──────────────────┘    │
    │                           │
    │  I2C (GPIO 21,22)        │
    └───────────┬───────────────┘
                │
         ┌──────▼──────┐
         │   INA219    │
         │Current Sense│
         └──────┬──────┘
                │
         ┌──────▼──────────────────┐
         │    Current Shunt         │
         │    (0.01Ω, 50A)         │
         └──────┬──────────────────┘
                │
         ┌──────▼──────────────────┐
         │   Power Stage            │
         │   (MOSFET + Driver)     │
         │                          │
         │   ┌────────┐            │
         │   │ Driver │ ← GPIO 25  │
         │   └───┬────┘            │
         │       │                 │
         │   ┌───▼────┐            │
         │   │ MOSFET │            │
         │   │(50A+)  │            │
         │   └───┬────┘            │
         └───────┬─────────────────┘
                 │
         ┌───────▼─────────────┐
         │   Load (0-50A)      │
         │   (Motor/Heater)    │
         └─────────────────────┘
```

## Detailed Component Specifications

### 1. Power MOSFET Selection

For 50A operation, use N-channel MOSFETs with these minimum specs:
- **Voltage Rating (Vds)**: At least 1.5x your max voltage (e.g., 80V for 48V system)
- **Current Rating (Id)**: At least 1.5x your max current (e.g., 75A for 50A continuous)
- **On-Resistance (Rds_on)**: Low as possible (< 10mΩ recommended)
- **Gate Charge (Qg)**: Lower is better for efficiency

Recommended MOSFETs:
- **IRFP260N**: 200V, 50A, 40mΩ (good for 24-48V systems)
- **IRFP3077**: 75V, 120A, 3.3mΩ (excellent for high current)
- **IRFB4115**: 150V, 104A, 8.8mΩ (good balance)
- **IPP067N06**: 60V, 160A, 6.7mΩ (low voltage, very high current)

### 2. Gate Driver

ESP32 GPIO cannot directly drive power MOSFETs effectively. Use a gate driver:

**Half-Bridge Drivers (for high-side or low-side):**
- **UCC27211**: 4A peak, fast switching
- **MCP1416**: 1.5A peak, simple
- **TC4427**: 1.5A peak, dual output

**Bootstrap Drivers (if using high-side switching):**
- **IR2110**: Classic half-bridge driver
- **IRS2186**: Improved version
- **FAN7384**: Integrated solution

**Basic Driver Circuit:**
```
ESP32 GPIO 25 ──┬─── 1kΩ ───┐
                │            │
                └─── 10kΩ ──┴─ GND
                             │
                         ┌───▼───┐
                         │ Driver│ (e.g., UCC27211)
                         │  VDD  │ ← 12V
                         │  IN   │
                         │  OUT  │ ──┬─── Gate (MOSFET)
                         │  GND  │   │
                         └───────┘   │
                                     └─── 10Ω ─── Gate
                                     │
                                     └─── 10kΩ ─ Source/GND
```

### 3. Current Sensing with INA219

**INA219 Setup for High Current:**

The INA219 can directly measure up to 3.2A. For 50A:

**Option A: Current Divider**
- Use a precision current divider to scale down current
- Main path: Low-resistance shunt (0.001Ω)
- Sense path: Current divider (1:25 ratio)
- INA219 sees 2A when main path has 50A

**Option B: Low-Value Shunt with Scaling**
- Use 0.01Ω, 50A shunt resistor
- Configure INA219 for maximum sensitivity
- Apply software scaling factor
- Trade-off: Lower precision at low currents

**Recommended Shunt Resistors:**
- **Vishay WSLP3921**: 0.01Ω, 5W, ±1%
- **Ohmite LVK25**: 0.01Ω, 25W, ±1%
- **TE Connectivity 4-1879200-0**: 0.005Ω, 3W

**INA219 Wiring:**
```
    VIN+ ──────────────┐
                       │
                    [Shunt]  0.01Ω
                       │
    VIN- ──────────────┴──── To Load

    I2C:
    INA219 SDA ──── ESP32 GPIO 21
    INA219 SCL ──── ESP32 GPIO 22
    INA219 VCC ──── 3.3V or 5V
    INA219 GND ──── GND
```

### 4. Power Supply Design

**ESP32 Power:**
- **Input**: 5V regulated
- **Current**: 500mA minimum (1A recommended for safety margin)
- **Regulator**: LM2596 buck converter or AMS1117-5.0
- **Filtering**: 100μF + 0.1μF ceramic capacitors

**Logic Power:**
- ESP32: 3.3V (built-in regulator from 5V USB)
- INA219: Can run on 3.3V or 5V
- Gate Drivers: Typically 12V (check datasheet)

**Power Distribution:**
```
Main Power (12-48V) ──┬──────────── To Load (via MOSFET)
                      │
                      ├─── Buck (12V) ─── Gate Driver
                      │
                      └─── Buck (5V) ──┬─── ESP32 VIN
                                       └─── INA219 (if 5V)
```

### 5. RC Receiver Interface

**Voltage Level Considerations:**
- RC receivers typically output 3.3V or 5V PWM
- ESP32 GPIO is 3.3V tolerant
- **If 5V RC receiver**: Use voltage divider or level shifter

**Voltage Divider (if needed):**
```
RC Output (5V) ───┬─── 2kΩ ───┬─── ESP32 GPIO
                  │            │
                  └─── 1kΩ ───┴─── GND
                  
(Divides 5V to 3.3V)
```

**Better Option: Level Shifter**
- Use bidirectional level shifter (e.g., TXS0108E)
- Cleaner signals, no current draw

### 6. Protection Circuits

**MOSFET Protection:**
```
Gate ──── 10Ω ──── MOSFET Gate Pin
          │
          └──── 10kΩ ───┬─── Source
                        │
                    ┌───┴───┐
                    │ 15V   │  Zener diode (gate protection)
                    │ Zener │
                    └───┬───┘
                        │
                      Source
```

**Reverse Polarity Protection:**
- Use a Schottky diode (e.g., MBRF20200) in series with main power
- Or use P-MOSFET reverse polarity protection circuit

**Overcurrent Hardware Protection:**
- Add fast-blow fuse rated for 60A (1.2x max current)
- Consider resettable PTC for protection

### 7. PCB Layout Recommendations

**High-Current Traces:**
- **Width**: Minimum 10mm (0.4") for 50A on 2oz copper
- Use copper pours for high-current paths
- Keep traces short
- Use thick PCB (2oz or 4oz copper)

**Grounding:**
- Star ground configuration
- Separate analog ground (INA219) from power ground
- Connect at single point near ESP32

**Component Placement:**
- Keep high-current switching away from sensitive circuits
- Shield ESP32 from EMI with ground plane
- Place bypass capacitors close to ICs

## Bill of Materials (BOM)

### Essential Components

| Component | Specification | Quantity | Example Part Number |
|-----------|--------------|----------|---------------------|
| ESP32 Dev Board | ESP32-DevKitC | 1 | ESP32-DevKitC-32D |
| Power MOSFET | 75V, 100A+, <10mΩ | 1-2 | IRFP3077PBF |
| Gate Driver IC | 4A peak output | 1 | UCC27211DR |
| INA219 Module | Current/Voltage sensor | 1 | Adafruit INA219 |
| Current Shunt | 0.01Ω, 50A, ≥5W | 1 | WSLP3921L000FEA |
| Buck Converter (12V) | 2A output | 1 | LM2596 module |
| Buck Converter (5V) | 1A output | 1 | AMS1117-5.0 |
| Schottky Diode | 200V, 20A | 1 | MBRF20200CTG |
| Fast-Blow Fuse | 60A | 1 | - |
| Fuse Holder | Panel mount | 1 | - |

### Passive Components

| Component | Value | Quantity | Notes |
|-----------|-------|----------|-------|
| Resistor | 10Ω, 1/4W | 2 | Gate series |
| Resistor | 10kΩ, 1/4W | 4 | Pull-down |
| Resistor | 1kΩ, 1/4W | 4 | Signal |
| Capacitor | 100μF, 50V Electrolytic | 4 | Power filtering |
| Capacitor | 0.1μF, 50V Ceramic | 10 | Bypass |
| Capacitor | 1μF, 25V Ceramic | 2 | Driver |
| Zener Diode | 15V, 1W | 1 | Gate protection |

### Connectors and Hardware

| Component | Specification | Quantity |
|-----------|--------------|----------|
| Terminal Blocks | 10A screw terminal | 4 |
| Power Terminal | 50A+ Anderson/XT90 | 2 |
| Heat Sink | TO-247 compatible | 1-2 |
| Thermal Paste | High conductivity | 1 tube |
| Insulating Washers | TO-247 | 2 |
| Wire | 8 AWG silicone, red | 1m |
| Wire | 8 AWG silicone, black | 1m |

## Assembly Instructions

### Step 1: Prepare the Enclosure
1. Select a non-conductive enclosure with adequate space
2. Drill mounting holes for ESP32, power stage, and heat sink
3. Ensure proper ventilation for heat dissipation

### Step 2: Mount Heat Sink
1. Attach MOSFET(s) to heat sink with thermal paste
2. Use insulating washers if multiple MOSFETs share heat sink
3. Torque to manufacturer specifications

### Step 3: Build Power Stage
1. Solder gate driver circuit on perfboard or custom PCB
2. Add gate resistors and protection components
3. Connect MOSFET gate, source, and drain

### Step 4: Wire ESP32
1. Connect RC input pins with proper voltage levels
2. Wire I2C to INA219
3. Connect output pins to gate driver and lights
4. Add 5V power supply connection

### Step 5: Install Current Sensing
1. Mount current shunt in series with load
2. Wire INA219 to shunt terminals
3. Ensure proper polarity (VIN+ to battery, VIN- to load)

### Step 6: Power Supply
1. Install buck converters for 12V and 5V
2. Wire main power input with fuse
3. Add reverse polarity protection
4. Install filtering capacitors

### Step 7: Testing
1. **Visual inspection**: Check all connections
2. **Continuity test**: Verify no short circuits
3. **Low voltage test**: Power with 12V, check voltages
4. **No-load test**: Verify RC inputs and outputs work
5. **Load test**: Start with small load (1A), increase gradually
6. **Full current test**: Test at rated current with monitoring

## Safety Checklist

Before powering on:
- [ ] All connections tight and secure
- [ ] No exposed high-current conductors
- [ ] Proper wire gauge for current rating
- [ ] Fuse installed and rated correctly
- [ ] Heat sink adequate for power dissipation
- [ ] Reverse polarity protection in place
- [ ] Emergency disconnect accessible
- [ ] Testing area clear of flammable materials
- [ ] Fire extinguisher nearby
- [ ] Monitoring equipment ready (multimeter, current clamp)

## Calibration Procedure

### Current Sensor Calibration

1. **Connect Known Load**:
   - Use a calibrated ammeter in series
   - Start with 5A load

2. **Compare Readings**:
   - Read current from ESP32 serial output
   - Compare with ammeter reading

3. **Adjust Scaling Factor**:
   - In `src/main.cpp`, modify:
   ```cpp
   float current_scale = XX.X; // Adjust this value
   ```
   - Calculate: `new_scale = old_scale × (ammeter_reading / esp32_reading)`

4. **Verify**:
   - Test at multiple current levels (10A, 25A, 50A)
   - Ensure accuracy within ±5% across range

### RC Input Calibration

1. **Check PWM Values**:
   - Monitor serial output with RC transmitter
   - Verify min (1000μs), mid (1500μs), max (2000μs)

2. **Adjust if Needed**:
   - Edit `include/config.h`:
   ```cpp
   #define RC_MIN_PULSE 1000  // Adjust based on your RC system
   #define RC_MAX_PULSE 2000
   ```

3. **Set Thresholds**:
   - Adjust light control thresholds based on preferred switch points
   - Test hysteresis to avoid flickering

## Troubleshooting Guide

| Problem | Possible Cause | Solution |
|---------|---------------|----------|
| No power to ESP32 | Power supply issue | Check 5V regulator, connections |
| RC values not changing | Bad connection/voltage | Check wiring, voltage levels |
| MOSFET stays on | Gate driver issue | Check driver power, connections |
| High temperature | Insufficient heat sink | Upgrade heat sink, add fan |
| Inaccurate current | Wrong calibration | Recalibrate scaling factor |
| System shutdown | Safety triggered | Check current/voltage limits |
| Erratic behavior | EMI/noise | Add filtering, improve grounding |

## Performance Specifications

| Parameter | Value | Notes |
|-----------|-------|-------|
| Max Continuous Current | 50A | With adequate cooling |
| Max Voltage | 48V | Based on component ratings |
| PWM Frequency | 1kHz | Configurable |
| RC Input Channels | 4 | Expandable |
| Response Time | <10ms | From RC input to output |
| Current Accuracy | ±5% | After calibration |
| Voltage Accuracy | ±1% | INA219 specification |
| Operating Temp | 0-50°C | With active cooling |

---

**Always prioritize safety when working with high-current systems!**