# ESP32RC Examples and Use Cases

This document provides practical examples and use cases for the ESP32RC high-current load controller.

## Table of Contents
1. [Basic RC Boat Motor Controller](#basic-rc-boat-motor-controller)
2. [RC Truck with Lights](#rc-truck-with-lights)
3. [High-Power LED Controller](#high-power-led-controller)
4. [Electric Skateboard ESC](#electric-skateboard-esc)
5. [Battery-Powered Winch Control](#battery-powered-winch-control)

---

## Basic RC Boat Motor Controller

### Application
Control a high-power brushed DC motor (24V, 30A) in an RC boat with current monitoring.

### Hardware Setup
- **Motor**: 24V, 500W brushed DC motor
- **Battery**: 6S LiPo (22.2V nominal, 25.9V max)
- **MOSFET**: IRFP3077 (75V, 120A)
- **Gate Driver**: UCC27211
- **Current Shunt**: 0.01Ω, 50W

### Configuration Changes

Edit `include/config.h`:
```cpp
#define MAX_VOLTAGE_V 28.0    // Slightly above 6S max voltage
#define MIN_VOLTAGE_V 18.0    // Safety cutoff for depleted battery
#define OVERCURRENT_THRESHOLD 35.0  // Safety margin above normal operation
```

Edit `src/main.cpp` - adjust current scaling:
```cpp
float current_scale = 15.0; // For ~30A max current
```

### Wiring
```
RC Channel 1 (Throttle) → ESP32 GPIO 34
RC Channel 2 (Unused)   → Not connected
Battery (+) → Fuse (40A) → MOSFET Drain
MOSFET Source → Motor (+)
Motor (-) → Current Shunt → Battery (-)
```

### Expected Behavior
- Throttle stick controls motor speed (0-100%)
- Current monitoring shows real-time motor current
- System shuts down if current exceeds 35A
- Battery voltage monitoring for low-voltage cutoff

### Serial Output Example
```
=== ESP32RC Status ===
System: ENABLED
RC Signal: OK

--- RC Channels ---
CH1 (Power): 1750 us (75%)

--- Outputs ---
Power Output: 75%

--- Power Monitoring ---
Voltage: 23.8 V
Current: 22.5 A
Power: 535.5 W
```

---

## RC Truck with Lights

### Application
RC crawler with variable speed motor control and headlights/taillights controlled by RC switches.

### Hardware Setup
- **Motor**: 12V, 20A brushed motor
- **Battery**: 3S LiPo (11.1V nominal)
- **Lights**: 12V LED strips (2A each)
- **MOSFETs**: 
  - 1x for motor (IRFP3077)
  - 2x for lights (can use smaller, e.g., IRF540N)

### Configuration Changes

`include/config.h`:
```cpp
#define MAX_VOLTAGE_V 13.5    // 3S max
#define MIN_VOLTAGE_V 9.0     // 3S depleted
#define OVERCURRENT_THRESHOLD 25.0
#define LIGHT_ON_THRESHOLD 1550   // Switch point for 3-position switch
#define LIGHT_OFF_THRESHOLD 1450
```

### Wiring
```
RC CH1 (Throttle)  → GPIO 34
RC CH2 (Headlights)→ GPIO 35
RC CH3 (Taillights)→ GPIO 32

GPIO 25 → Motor PWM Control
GPIO 26 → Headlight MOSFET
GPIO 27 → Taillight MOSFET
```

### Example Usage
- **Channel 1**: Forward/reverse speed control
- **Channel 2**: Headlights ON/OFF (flip switch up to turn on)
- **Channel 3**: Taillights ON/OFF (independent control)

### Enhancements
Add this to `setup()` in `main.cpp` for brake lights when stopped:
```cpp
// Add after other setup code
Serial.println("RC Truck mode with automatic brake lights");
```

Add to `loop()`:
```cpp
// Automatic brake lights when throttle near center
if (abs(rc_ch1_value - RC_MID_PULSE) < RC_DEADBAND && light2_state == false) {
    digitalWrite(LIGHT2_PIN, HIGH);  // Turn on brake lights
    light2_state = true;
} else if (abs(rc_ch1_value - RC_MID_PULSE) > RC_DEADBAND + 100) {
    digitalWrite(LIGHT2_PIN, LOW);   // Turn off when moving
    light2_state = false;
}
```

---

## High-Power LED Controller

### Application
Control high-power LED arrays for RC scale lighting, film lighting, or stage effects.

### Hardware Setup
- **LEDs**: 48V LED strips, 10A total
- **Battery**: 12S LiPo (50.4V max) or 48V power supply
- **Dimming**: PWM control from RC transmitter
- **MOSFET**: IRFP260N (200V rated for high voltage)

### Configuration Changes

`include/config.h`:
```cpp
#define MAX_VOLTAGE_V 55.0    // Above 12S max
#define MIN_VOLTAGE_V 42.0    // 12S depleted
#define OVERCURRENT_THRESHOLD 12.0
#define PWM_FREQUENCY 5000    // Higher frequency for flicker-free dimming
```

### Special Considerations
- Use PWM dimming for smooth brightness control
- Higher PWM frequency (5kHz+) prevents visible flicker
- Current limit set lower as LEDs are sensitive to overcurrent

### Wiring
```
RC CH1 (Brightness) → GPIO 34
RC CH2 (Color 1)    → GPIO 35  (if using multiple LED colors)
RC CH3 (Color 2)    → GPIO 32

GPIO 25 (PWM) → LED Driver (Color 1)
GPIO 26       → LED Driver (Color 2)
GPIO 27       → LED Driver (Color 3)
```

### Brightness Control
The standard code already provides 0-100% brightness control via Channel 1.

---

## Electric Skateboard ESC

### Application
Custom electric skateboard controller with regenerative braking simulation and battery monitoring.

### Hardware Setup
- **Motor**: 36V, 1500W brushless motor (via VESC or brushed with this controller)
- **Battery**: 10S LiPo (37V nominal, 42V max)
- **Current**: Up to 40A continuous
- **Safety**: Critical - requires reliable brake function

### Configuration Changes

`include/config.h`:
```cpp
#define MAX_VOLTAGE_V 45.0
#define MIN_VOLTAGE_V 30.0    // Conservative for safety
#define OVERCURRENT_THRESHOLD 45.0
#define RC_TIMEOUT 50         // Shorter timeout for safety
```

### Safety Enhancements

Add to `src/main.cpp` before `updatePowerControl()`:
```cpp
// Add deadman switch - must hold button to enable
bool deadman_active = (rc_ch4_value > 1700);  // CH4 must be high

if (!deadman_active) {
    system_enabled = false;
}
```

### Additional Features

**Battery State-of-Charge Estimation:**
```cpp
// Add to loop() for battery percentage estimation
float battery_percent = map(voltage_V, MIN_VOLTAGE_V, MAX_VOLTAGE_V, 0, 100);
battery_percent = constrain(battery_percent, 0, 100);

if (current_time - last_serial_update >= SERIAL_UPDATE_INTERVAL) {
    Serial.print("Battery: ");
    Serial.print(battery_percent);
    Serial.println("%");
}
```

**Speed Limiting:**
```cpp
// Add to updatePowerControl() to limit maximum speed
int max_speed_percent = 80;  // Limit to 80% for beginners
power_percent = constrain(power_percent, 0, max_speed_percent);
```

---

## Battery-Powered Winch Control

### Application
RC-controlled electric winch for RC crawlers, boats, or recovery operations.

### Hardware Setup
- **Winch Motor**: 12V, 15000lb, 400A stall (need multiple MOSFETs in parallel)
- **Battery**: 12V lead-acid or 3S LiPo
- **MOSFETs**: 4x IRFP3077 in parallel (for 400A+ capability)
- **Current**: Monitor to prevent stall damage

### Configuration Changes

`include/config.h`:
```cpp
#define MAX_VOLTAGE_V 15.0
#define MIN_VOLTAGE_V 10.5
#define OVERCURRENT_THRESHOLD 100.0  // Much higher for winch
#define PWM_FREQUENCY 100    // Lower frequency for high current
```

### Stall Detection

Add to `src/main.cpp`:
```cpp
// Add global variables
unsigned long high_current_start = 0;
bool stall_detected = false;

// Add to checkSafetyLimits()
float current_A = current_mA / 1000.0;

// Detect stall: high current + low speed for >2 seconds
if (current_A > 80.0 && power_output < 50) {
    if (high_current_start == 0) {
        high_current_start = millis();
    } else if (millis() - high_current_start > 2000) {
        Serial.println("STALL DETECTED - Shutting down!");
        system_enabled = false;
        stall_detected = true;
    }
} else {
    high_current_start = 0;
}
```

### Direction Control

For bidirectional winch (in/out):
```cpp
// Use CH1 for speed, center = stop, up = in, down = out
void updateWinchControl() {
    if (!system_enabled || !isRCSignalValid()) {
        digitalWrite(DIR_PIN_1, LOW);
        digitalWrite(DIR_PIN_2, LOW);
        ledcWrite(PWM_CHANNEL, 0);
        return;
    }
    
    if (rc_ch1_value > RC_MID_PULSE + RC_DEADBAND) {
        // Pull in
        digitalWrite(DIR_PIN_1, HIGH);
        digitalWrite(DIR_PIN_2, LOW);
        int speed = map(rc_ch1_value, RC_MID_PULSE, RC_MAX_PULSE, 0, 255);
        ledcWrite(PWM_CHANNEL, speed);
    } else if (rc_ch1_value < RC_MID_PULSE - RC_DEADBAND) {
        // Let out
        digitalWrite(DIR_PIN_1, LOW);
        digitalWrite(DIR_PIN_2, HIGH);
        int speed = map(rc_ch1_value, RC_MIN_PULSE, RC_MID_PULSE, 255, 0);
        ledcWrite(PWM_CHANNEL, speed);
    } else {
        // Stop
        digitalWrite(DIR_PIN_1, LOW);
        digitalWrite(DIR_PIN_2, LOW);
        ledcWrite(PWM_CHANNEL, 0);
    }
}
```

---

## Common Modifications

### Data Logging

Add SD card logging for performance analysis:
```cpp
#include <SD.h>
#include <SPI.h>

File logFile;

void setup() {
    // ... existing setup ...
    
    if (SD.begin()) {
        logFile = SD.open("/log.csv", FILE_WRITE);
        if (logFile) {
            logFile.println("Time,Voltage,Current,Power,RC_CH1,Power_Output");
            logFile.close();
        }
    }
}

void loop() {
    // ... existing code ...
    
    // Log every second
    static unsigned long last_log = 0;
    if (millis() - last_log > 1000) {
        logFile = SD.open("/log.csv", FILE_APPEND);
        if (logFile) {
            logFile.print(millis());
            logFile.print(",");
            logFile.print(voltage_V);
            logFile.print(",");
            logFile.print(current_mA/1000.0);
            logFile.print(",");
            logFile.print(power_mW/1000.0);
            logFile.print(",");
            logFile.print(rc_ch1_value);
            logFile.print(",");
            logFile.println(power_output);
            logFile.close();
        }
        last_log = millis();
    }
}
```

### WiFi Monitoring

Add WiFi for remote monitoring:
```cpp
#include <WiFi.h>
#include <WebServer.h>

WebServer server(80);

void setup() {
    // ... existing setup ...
    
    WiFi.softAP("ESP32RC", "password123");
    
    server.on("/", []() {
        String html = "<html><body>";
        html += "<h1>ESP32RC Status</h1>";
        html += "<p>Voltage: " + String(voltage_V) + " V</p>";
        html += "<p>Current: " + String(current_mA/1000.0) + " A</p>";
        html += "<p>Power: " + String(power_mW/1000.0) + " W</p>";
        html += "<p>Output: " + String((power_output*100)/255) + " %</p>";
        html += "</body></html>";
        server.send(200, "text/html", html);
    });
    
    server.begin();
}

void loop() {
    server.handleClient();
    // ... existing code ...
}
```

### Telemetry to RC Transmitter

Some RC systems support telemetry. Add S.Port or FrSky telemetry:
```cpp
// Example for FrSky S.Port telemetry
#include <SoftwareSerial.h>

SoftwareSerial sport(16, 17);  // RX, TX

void sendTelemetry() {
    // Send voltage
    sport.write(0x7E);  // Header
    sport.write(0x1A);  // Sensor ID (voltage)
    // ... format and send data per protocol
    
    // Send current
    sport.write(0x7E);
    sport.write(0x1B);  // Sensor ID (current)
    // ... format and send data
}

void loop() {
    // ... existing code ...
    
    static unsigned long last_telem = 0;
    if (millis() - last_telem > 1000) {
        sendTelemetry();
        last_telem = millis();
    }
}
```

---

## Testing Procedures

### Bench Testing
1. **No-Load Test**: Verify all functions without connected load
2. **Resistive Load**: Test with resistor bank (known current)
3. **Actual Load**: Test with real motor/device
4. **Failsafe Test**: Verify behavior when RC signal lost
5. **Overload Test**: Intentionally trigger safety limits

### Field Testing
1. Start with low-power loads
2. Gradually increase to rated current
3. Monitor temperatures continuously
4. Test in various environmental conditions
5. Verify wireless range and interference handling

---

## Performance Tuning

### Optimizing Response Time
- Reduce `MONITOR_INTERVAL` for faster safety response
- Increase PWM frequency for smoother control
- Adjust RC timeout based on your system

### Reducing Noise
- Add input filtering capacitors
- Use shielded cables for RC inputs
- Implement software filtering on RC inputs
- Separate power and signal grounds

### Improving Efficiency
- Use lower Rds(on) MOSFETs
- Minimize wire lengths in high-current paths
- Optimize PWM frequency (trade-off: switching losses vs. conductor losses)
- Ensure adequate cooling for continuous operation

---

Each example can be customized further based on specific requirements. Always test thoroughly before deploying in final application!