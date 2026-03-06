# ESP32RC Quick Start Guide

## 5-minute setup

### What you need

1. ESP32 development board
2. USB cable
3. RC receiver with PWM outputs
4. Power stage components for the load
5. Optional INA219 current sensor
6. A monitored power bus at or below 26V when using the stock INA219 design

## Software installation

### PlatformIO

```bash
pip install platformio
git clone https://github.com/atrep123/ESP32RC.git
cd ESP32RC
pio run --target upload
pio device monitor
```

Optional bring-up/self-test build:

```bash
pio run -e esp32dev_selftest --target upload
pio device monitor
```

Useful self-test serial commands:

- `help`
- `sensor`
- `scale <value>`
- `scale save`
- `selftest on`

### Arduino IDE

1. Install ESP32 board support.
2. Install `Adafruit INA219` and `Adafruit BusIO`.
3. Open `src/main.cpp` and convert it into a sketch if needed.
4. Copy `include/config.h` next to the sketch.
5. Select `ESP32 Dev Module`.
6. Upload.

## Basic wiring

### Minimum setup without INA219

```text
RC receiver CH1 -> ESP32 GPIO 34
ESP32 GPIO 25  -> Gate driver input
Gate driver    -> MOSFET gate
ESP32 GND      -> Common ground
ESP32 5V       -> USB or regulated 5V input
```

### INA219 wiring

```text
INA219 SDA -> ESP32 GPIO 21
INA219 SCL -> ESP32 GPIO 22
INA219 VCC -> 3.3V or 5V
INA219 GND -> GND
```

## First power-on test

1. Power the ESP32 from USB.
2. Open the serial monitor at `115200`.
3. Confirm that the firmware boots and reports status.
4. Verify that power output stays off until the first valid `CH1` pulse is received.
5. Move the throttle stick and confirm `CH1` changes between about `1000-2000 us`.
6. Turn the transmitter off and confirm the outputs return to a safe state.

## Default pins

| Function | Pin | Notes |
|----------|-----|-------|
| RC Channel 1 | GPIO 34 | Power / throttle |
| RC Channel 2 | GPIO 35 | Light 1 |
| RC Channel 3 | GPIO 32 | Light 2 |
| RC Channel 4 | GPIO 33 | Auxiliary |
| Power PWM out | GPIO 25 | Gate driver input |
| Light 1 out | GPIO 26 | Digital |
| Light 2 out | GPIO 27 | Digital |
| Aux out | GPIO 14 | Digital |
| I2C SDA | GPIO 21 | INA219 |
| I2C SCL | GPIO 22 | INA219 |

## Common issues

### `RC Link: LOST`

- Check that the receiver is powered and bound.
- Verify signal wiring to the configured GPIO pins.
- If the receiver outputs 5V logic, use level shifting or a divider.

### `Failed to find INA219`

- Check `SDA` and `SCL` wiring.
- Verify the sensor has power and common ground.
- The system will continue without monitoring if the sensor is missing.

### No PWM output

- Confirm `system_enabled` was not latched off by a safety fault.
- Confirm `CH1` is receiving fresh valid pulses.
- Power output remains disabled until a valid `CH1` pulse has been captured.

## Customization

Edit `include/config.h`:

```cpp
#define MAX_CURRENT_A 50.0
#define OVERCURRENT_THRESHOLD 48.0
#define MAX_VOLTAGE_V 26.0
#define CURRENT_SCALE 25.0
```

If you need a bus above 26V, replace the voltage-sensing hardware and update the firmware safety logic before increasing `MAX_VOLTAGE_V`.
