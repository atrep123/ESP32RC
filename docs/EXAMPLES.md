# ESP32RC Example Use Cases

## 1. RC boat motor controller

### Typical hardware

- 24V brushed DC motor
- 6S LiPo pack
- External gate driver
- Power MOSFET
- INA219 plus calibrated current scaling

### Suggested config changes

```cpp
#define MIN_VOLTAGE_V 18.0
#define MAX_VOLTAGE_V 26.0
#define OVERCURRENT_THRESHOLD 35.0
#define CURRENT_SCALE 15.0
```

## 2. RC truck with lights

### Typical hardware

- 3S LiPo pack
- Brushed motor or other switched load
- Headlights on `GPIO 26`
- Taillights on `GPIO 27`

### Suggested config changes

```cpp
#define MIN_VOLTAGE_V 9.0
#define MAX_VOLTAGE_V 13.0
#define OVERCURRENT_THRESHOLD 25.0
#define LIGHT_ON_THRESHOLD 1550
#define LIGHT_OFF_THRESHOLD 1450
```

## 3. 24V LED controller

### Typical hardware

- 24V LED strips or lamp banks
- 24V power supply or 6S LiPo
- Higher PWM frequency for reduced visible flicker

### Suggested config changes

```cpp
#define MIN_VOLTAGE_V 20.0
#define MAX_VOLTAGE_V 26.0
#define OVERCURRENT_THRESHOLD 12.0
#define PWM_FREQUENCY 5000
```

## 4. Utility winch or actuator controller

### Typical hardware

- 12V or 24V motorized actuator
- RC deadman function on `CH4`
- Conservative timeout for fast failsafe

### Suggested config changes

```cpp
#define RC_TIMEOUT 50
#define OVERCURRENT_THRESHOLD 20.0
```

### Deadman example

```cpp
bool deadman_active = isChannelSignalValid(last_ch4_update, rc_ch4_seen) &&
                      rc_ch4_value > 1700;

if (!deadman_active) {
    power_output = 0;
    ledcWrite(PWM_CHANNEL, 0);
}
```

## Notes

- Keep stock examples at or below 26V if you use the bundled INA219 monitoring path.
- If you redesign voltage sensing, update both firmware limits and documentation for your hardware.
