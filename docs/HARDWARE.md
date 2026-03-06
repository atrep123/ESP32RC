# ESP32RC Hardware Guide

## Overview

The stock ESP32RC firmware is designed around:

- An ESP32 development board
- One RC receiver with up to four PWM channels
- One PWM-controlled power stage
- Two digital light outputs
- One digital auxiliary output
- Optional INA219 current and voltage monitoring

## Stock electrical limits

- Current target: up to 50A with appropriate MOSFETs, shunt, cooling, and PCB or busbar design
- Stock monitored voltage ceiling: 26V
- Default undervoltage limit: 10V
- Default overvoltage limit: 26V

The 26V ceiling comes from the bundled INA219-based monitoring path.
If you need higher system voltage, use different voltage-sensing hardware and update the firmware safety logic first.

## Default pinout

| Function | ESP32 pin |
|----------|-----------|
| RC CH1 power input | GPIO 34 |
| RC CH2 light 1 input | GPIO 35 |
| RC CH3 light 2 input | GPIO 32 |
| RC CH4 auxiliary input | GPIO 33 |
| Power PWM output | GPIO 25 |
| Light 1 output | GPIO 26 |
| Light 2 output | GPIO 27 |
| Auxiliary output | GPIO 14 |
| I2C SDA | GPIO 21 |
| I2C SCL | GPIO 22 |

## RC input notes

- ESP32 GPIO inputs are 3.3V logic.
- Many RC receivers output 3.3V-compatible PWM directly.
- If your receiver outputs 5V logic, add level shifting or a divider before the ESP32 pins.
- The firmware now requires a valid `CH1` pulse before enabling power output.

## Power stage

Use a proper gate driver between the ESP32 PWM output and the MOSFET gate.
Do not drive a large MOSFET gate directly from the ESP32 pin.

Recommended design elements:

- Logic-level power MOSFET with enough current margin
- Gate driver with adequate peak current
- Flyback protection for inductive loads
- Fuse sized for the expected load
- Heat sink and airflow for continuous current
- Low-resistance high-current wiring

Example MOSFET choices:

- `IRFP3077`
- `IRFB4115`
- `IPP067N06`

Choose voltage rating based on your actual bus, not just the firmware defaults.

## Current sensing with INA219

The INA219 is optional, but if installed the stock firmware uses it for:

- Bus voltage measurement
- Current measurement
- Power reporting
- Overcurrent and voltage-based shutdown

For high current systems, the current path usually needs external scaling:

- Use a suitable shunt resistor
- Use a divider or calibrated scaling arrangement if needed
- Adjust `CURRENT_SCALE` in `include/config.h` to match the real hardware

## Power domains

Typical power arrangement:

```text
Main bus (up to 26V stock) -> load path -> shunt -> load
Main bus -> buck converter -> gate driver supply
Main bus or USB -> 5V rail -> ESP32
ESP32 3.3V / 5V rail -> INA219
```

Keep grounds common, but route high-current return paths so they do not inject noise into the ESP32 and sensor wiring.

## Wiring checklist

1. Verify common ground between ESP32, RC receiver, gate driver, and sensor.
2. Verify the RC signal voltage level is safe for ESP32 GPIO.
3. Verify the gate driver input is connected to `GPIO 25`.
4. Verify the monitored bus stays at or below 26V with the stock INA219 setup.
5. Verify the shunt and wiring are sized for worst-case current.
6. Verify the load has the correct flyback or transient suppression.

## Commissioning checklist

1. Power the ESP32 from USB first.
2. Confirm serial output is readable at `115200`.
3. Confirm outputs stay off before the first valid `CH1` pulse.
4. Confirm PWM reacts only to live `CH1`.
5. Confirm each light or aux output turns off if its own channel goes stale.
6. If INA219 is installed, confirm voltage and current readings before connecting the final load.
