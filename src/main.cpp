/**
 * ESP32RC - High-Current Load Controller with RC Input
 * 
 * This module provides wireless control of high-current loads (up to ~50A)
 * using ESP32. It receives PWM signals from RC equipment, converts them to
 * logic for controlling lights and power, and monitors voltage and current
 * in real-time.
 * 
 * Features:
 * - 4-channel RC PWM input
 * - Power control with PWM output (0-100%)
 * - Digital light control (ON/OFF)
 * - Real-time current and voltage monitoring using INA219
 * - Overcurrent and undervoltage protection
 * - Serial monitoring and debugging
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Preferences.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"

// Global objects
Adafruit_INA219 ina219;
Preferences preferences;
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature tempSensor(&oneWire);
bool current_sensor_available = false;
bool temp_sensor_available = false;
bool preferences_ready = false;
bool current_scale_has_saved_value = false;
float persisted_current_scale = CURRENT_SCALE;

constexpr char PREFERENCES_NAMESPACE[] = "esp32rc";
constexpr char PREF_KEY_CURRENT_SCALE[] = "curr_scale";

// RC channel values (pulse width in microseconds)
volatile unsigned long rc_ch1_value = RC_MIN_PULSE;
volatile unsigned long rc_ch2_value = RC_MIN_PULSE;
volatile unsigned long rc_ch3_value = RC_MIN_PULSE;
volatile unsigned long rc_ch4_value = RC_MIN_PULSE;

// RC channel timing
volatile unsigned long rc_ch1_start = 0;
volatile unsigned long rc_ch2_start = 0;
volatile unsigned long rc_ch3_start = 0;
volatile unsigned long rc_ch4_start = 0;

// Last valid signal times
volatile unsigned long last_ch1_update = 0;
volatile unsigned long last_ch2_update = 0;
volatile unsigned long last_ch3_update = 0;
volatile unsigned long last_ch4_update = 0;

// Track whether a valid pulse has ever been received on each channel.
volatile bool rc_ch1_seen = false;
volatile bool rc_ch2_seen = false;
volatile bool rc_ch3_seen = false;
volatile bool rc_ch4_seen = false;

// System state
bool system_enabled = true;
bool light1_state = false;
bool light2_state = false;
int power_output = 0;

// Monitoring variables
float current_scale = CURRENT_SCALE;
float raw_current_mA = 0;
float raw_power_mW = 0;
float current_mA = 0;
float voltage_V = 0;
float power_mW = 0;
unsigned long last_monitor_time = 0;
unsigned long last_serial_update = 0;

// Recovery tracking for safety shutdown
unsigned long system_fault_time = 0;
constexpr unsigned long RECOVERY_TIMEOUT_MS = 5000;  // 5 second timeout before recovery attempt
bool recovery_attempted = false;

// Fan control state (v1.2 feature)
int fan_speed = 0;                    // Current fan PWM speed (0-255)
float last_temp_reading = 25.0;       // Last temperature reading
unsigned long last_fan_update = 0;    // Last fan speed update time
bool fan_auto_mode = true;            // Auto temperature control enabled
bool use_external_temp = false;       // Using external sensor when available

// SD Card Telemetry Logging (v1.3 Phase 2 feature)
bool sd_card_ready = false;           // SD card initialization status
unsigned long last_telemetry_log_time = 0;  // Last telemetry log timestamp
bool telemetry_header_written = false; // Track if CSV header written

void printStatus();
void readCurrentVoltage();
void updateFanControl();
void setFanSpeed(int speed);
void initSDCard();
void logTelemetry();
bool writeTelemetryHeader();

/**
 * Atomic helpers for reading volatile RC state shared with ISR
 */
unsigned long readRCValue(volatile unsigned long &value) {
    noInterrupts();
    unsigned long result = value;
    interrupts();
    return result;
}

unsigned long readRCUpdateTime(volatile unsigned long &update_time) {
    noInterrupts();
    unsigned long result = update_time;
    interrupts();
    return result;
}

bool readRCChannelSeen(volatile bool &channel_seen) {
    noInterrupts();
    bool result = channel_seen;
    interrupts();
    return result;
}

#if ENABLE_SERIAL_SELF_TEST
bool serial_self_test_active = false;
int self_test_power_percent = 0;
bool self_test_light1 = false;
bool self_test_light2 = false;
bool self_test_aux = false;
char serial_command_buffer[64];
size_t serial_command_length = 0;
#endif

/**
 * Interrupt Service Routines for RC PWM input
 */
void IRAM_ATTR rc_ch1_isr() {
    if (digitalRead(RC_CH1_PIN) == HIGH) {
        rc_ch1_start = micros();
    } else {
        unsigned long pulse = micros() - rc_ch1_start;
        if (pulse >= RC_MIN_PULSE && pulse <= RC_MAX_PULSE) {
            rc_ch1_value = pulse;
            last_ch1_update = millis();
            rc_ch1_seen = true;
        }
    }
}

void IRAM_ATTR rc_ch2_isr() {
    if (digitalRead(RC_CH2_PIN) == HIGH) {
        rc_ch2_start = micros();
    } else {
        unsigned long pulse = micros() - rc_ch2_start;
        if (pulse >= RC_MIN_PULSE && pulse <= RC_MAX_PULSE) {
            rc_ch2_value = pulse;
            last_ch2_update = millis();
            rc_ch2_seen = true;
        }
    }
}

void IRAM_ATTR rc_ch3_isr() {
    if (digitalRead(RC_CH3_PIN) == HIGH) {
        rc_ch3_start = micros();
    } else {
        unsigned long pulse = micros() - rc_ch3_start;
        if (pulse >= RC_MIN_PULSE && pulse <= RC_MAX_PULSE) {
            rc_ch3_value = pulse;
            last_ch3_update = millis();
            rc_ch3_seen = true;
        }
    }
}

void IRAM_ATTR rc_ch4_isr() {
    if (digitalRead(RC_CH4_PIN) == HIGH) {
        rc_ch4_start = micros();
    } else {
        unsigned long pulse = micros() - rc_ch4_start;
        if (pulse >= RC_MIN_PULSE && pulse <= RC_MAX_PULSE) {
            rc_ch4_value = pulse;
            last_ch4_update = millis();
            rc_ch4_seen = true;
        }
    }
}

/**
 * Map RC PWM value to percentage (0-100)
 */
int mapRCToPower(unsigned long pulse) {
    if (pulse < RC_MIN_PULSE) pulse = RC_MIN_PULSE;
    if (pulse > RC_MAX_PULSE) pulse = RC_MAX_PULSE;
    
    return map(pulse, RC_MIN_PULSE, RC_MAX_PULSE, 0, 100);
}

/**
 * Check if a specific RC channel has seen a recent valid pulse
 */
bool isChannelSignalValid(unsigned long last_update, bool signal_seen) {
    if (!signal_seen) {
        return false;
    }

    unsigned long now = millis();
    return now - last_update < RC_TIMEOUT;
}

bool isPowerSignalValid() {
    unsigned long ch1_update = readRCUpdateTime(last_ch1_update);
    bool ch1_seen = readRCChannelSeen(rc_ch1_seen);
    return isChannelSignalValid(ch1_update, ch1_seen);
}

bool isAnyRCSignalValid() {
    unsigned long ch1_update = readRCUpdateTime(last_ch1_update);
    unsigned long ch2_update = readRCUpdateTime(last_ch2_update);
    unsigned long ch3_update = readRCUpdateTime(last_ch3_update);
    unsigned long ch4_update = readRCUpdateTime(last_ch4_update);
    
    bool ch1_seen = readRCChannelSeen(rc_ch1_seen);
    bool ch2_seen = readRCChannelSeen(rc_ch2_seen);
    bool ch3_seen = readRCChannelSeen(rc_ch3_seen);
    bool ch4_seen = readRCChannelSeen(rc_ch4_seen);
    
    return isChannelSignalValid(ch1_update, ch1_seen) ||
           isChannelSignalValid(ch2_update, ch2_seen) ||
           isChannelSignalValid(ch3_update, ch3_seen) ||
           isChannelSignalValid(ch4_update, ch4_seen);
}

bool initPreferencesStorage() {
    preferences_ready = preferences.begin(PREFERENCES_NAMESPACE, false);
    if (!preferences_ready) {
        Serial.println("WARNING: Preferences unavailable - using compile-time defaults");
    }

    return preferences_ready;
}

void loadPersistedCurrentScale() {
    current_scale = CURRENT_SCALE;
    current_scale_has_saved_value = false;
    persisted_current_scale = CURRENT_SCALE;

    if (!preferences_ready || !preferences.isKey(PREF_KEY_CURRENT_SCALE)) {
        return;
    }

    float stored_scale = preferences.getFloat(PREF_KEY_CURRENT_SCALE, CURRENT_SCALE);
    if (stored_scale > 0.0f) {
        current_scale = stored_scale;
        persisted_current_scale = stored_scale;
        current_scale_has_saved_value = true;
        return;
    }

    preferences.remove(PREF_KEY_CURRENT_SCALE);
    Serial.println("WARNING: Ignored invalid saved current scale");
}

void printCurrentScaleState() {
    Serial.print("Runtime current scale: ");
    Serial.println(current_scale, 4);
    Serial.print("Saved current scale: ");

    if (current_scale_has_saved_value) {
        Serial.println(persisted_current_scale, 4);
    } else {
        Serial.println("none");
    }
}

bool saveCurrentScalePreference() {
    if (!preferences_ready) {
        return false;
    }

    if (current_scale <= 0.0f) {
        return false;
    }

    size_t written = preferences.putFloat(PREF_KEY_CURRENT_SCALE, current_scale);
    if (written == 0) {
        return false;
    }

    persisted_current_scale = current_scale;
    current_scale_has_saved_value = true;
    return true;
}

bool clearCurrentScalePreference() {
    if (!preferences_ready) {
        return false;
    }

    if (preferences.isKey(PREF_KEY_CURRENT_SCALE) && !preferences.remove(PREF_KEY_CURRENT_SCALE)) {
        return false;
    }

    current_scale_has_saved_value = false;
    persisted_current_scale = CURRENT_SCALE;
    return true;
}

#if ENABLE_SERIAL_SELF_TEST
void resetOutputsToSafeState() {
    power_output = 0;
    light1_state = false;
    light2_state = false;

    ledcWrite(PWM_CHANNEL, 0);
    digitalWrite(LIGHT1_PIN, LOW);
    digitalWrite(LIGHT2_PIN, LOW);
    digitalWrite(AUX_OUTPUT_PIN, LOW);
}

void stopSerialSelfTest() {
    serial_self_test_active = false;
    self_test_power_percent = 0;
    self_test_light1 = false;
    self_test_light2 = false;
    self_test_aux = false;
    resetOutputsToSafeState();
}

void printSerialSelfTestHelp() {
    Serial.println("Self-test commands:");
    Serial.println("  help");
    Serial.println("  status");
    Serial.println("  sensor");
    Serial.println("  scale show");
    Serial.println("  scale reset");
    Serial.println("  scale save");
    Serial.println("  scale clear");
    Serial.println("  scale <value>");
    Serial.println("  selftest on");
    Serial.println("  selftest off");
    Serial.print("  pwm 0-");
    Serial.println(SELF_TEST_PWM_MAX_PERCENT);
    Serial.println("  light1 on|off");
    Serial.println("  light2 on|off");
    Serial.println("  aux on|off");
    Serial.println("Fan commands (v1.2):");
    Serial.println("  fan status");
    Serial.println("  fan auto       - auto mode (temp control)");
    Serial.println("  fan off        - turn off");
    Serial.println("  fan speed 0-255 - manual speed");
}

void printSerialSelfTestStatus() {
    Serial.print("Self-test: ");
    Serial.println(serial_self_test_active ? "ACTIVE" : "INACTIVE");
    printCurrentScaleState();
    Serial.print("Self-test PWM: ");
    Serial.print(self_test_power_percent);
    Serial.println("%");
    Serial.print("Self-test Light1: ");
    Serial.println(self_test_light1 ? "ON" : "OFF");
    Serial.print("Self-test Light2: ");
    Serial.println(self_test_light2 ? "ON" : "OFF");
    Serial.print("Self-test Aux: ");
    Serial.println(self_test_aux ? "ON" : "OFF");
}

void printSensorReadings() {
    if (!current_sensor_available) {
        Serial.println("INA219 not available");
        return;
    }

    readCurrentVoltage();

    Serial.println("--- INA219 Sensor ---");
    Serial.print("Bus voltage: ");
    Serial.print(voltage_V, 3);
    Serial.println(" V");
    Serial.print("Raw current: ");
    Serial.print(raw_current_mA, 3);
    Serial.println(" mA");
    Serial.print("Scaled current: ");
    Serial.print(current_mA, 3);
    Serial.println(" mA");
    Serial.print("Raw power: ");
    Serial.print(raw_power_mW, 3);
    Serial.println(" mW");
    Serial.print("Scaled power: ");
    Serial.print(power_mW, 3);
    Serial.println(" mW");
    Serial.print("Current scale: ");
    Serial.println(current_scale, 4);
}

void applySerialSelfTestOutputs() {
    if (!system_enabled) {
        stopSerialSelfTest();
        Serial.println("Self-test stopped due to system fault");
        return;
    }

    power_output = map(self_test_power_percent, 0, 100, 0, 255);
    light1_state = self_test_light1;
    light2_state = self_test_light2;

    ledcWrite(PWM_CHANNEL, power_output);
    digitalWrite(LIGHT1_PIN, self_test_light1 ? HIGH : LOW);
    digitalWrite(LIGHT2_PIN, self_test_light2 ? HIGH : LOW);
    digitalWrite(AUX_OUTPUT_PIN, self_test_aux ? HIGH : LOW);
}

void setSelfTestBinaryOutput(const char *command, const char *prefix, bool *target, const char *label) {
    const size_t prefix_len = strlen(prefix);
    if (strncmp(command, prefix, prefix_len) != 0) {
        return;
    }

    if (!serial_self_test_active) {
        Serial.println("Enable self-test first with 'selftest on'");
        return;
    }

    const char *state = command + prefix_len;
    if (strcmp(state, "on") == 0) {
        *target = true;
    } else if (strcmp(state, "off") == 0) {
        *target = false;
    } else {
        Serial.print("Invalid state for ");
        Serial.println(label);
        return;
    }

    Serial.print(label);
    Serial.print(" set to ");
    Serial.println(*target ? "ON" : "OFF");
}

bool tryHandleSelfTestBinaryOutput(const char *command) {
    if (strncmp(command, "light1 ", 7) == 0) {
        setSelfTestBinaryOutput(command, "light1 ", &self_test_light1, "Light1");
        return true;
    }

    if (strncmp(command, "light2 ", 7) == 0) {
        setSelfTestBinaryOutput(command, "light2 ", &self_test_light2, "Light2");
        return true;
    }

    if (strncmp(command, "aux ", 4) == 0) {
        setSelfTestBinaryOutput(command, "aux ", &self_test_aux, "Aux");
        return true;
    }

    return false;
}

void processSerialCommand(const char *command) {
    if (strcmp(command, "help") == 0 || strcmp(command, "?") == 0) {
        printSerialSelfTestHelp();
        return;
    }

    if (strcmp(command, "status") == 0) {
        printStatus();
        printSerialSelfTestStatus();
        return;
    }

    if (strcmp(command, "sensor") == 0) {
        printSensorReadings();
        return;
    }

    if (strcmp(command, "scale show") == 0) {
        printCurrentScaleState();
        return;
    }

    if (strcmp(command, "scale reset") == 0) {
        current_scale = CURRENT_SCALE;
        Serial.print("Runtime current scale reset to ");
        Serial.println(current_scale, 4);
        if (current_scale_has_saved_value) {
            Serial.println("Saved current scale is unchanged; use 'scale clear' or 'scale save' if needed");
        }
        return;
    }

    if (strcmp(command, "scale save") == 0) {
        if (!saveCurrentScalePreference()) {
            Serial.println("Failed to save current scale");
            return;
        }

        Serial.println("Current scale saved to Preferences");
        printCurrentScaleState();
        return;
    }

    if (strcmp(command, "scale clear") == 0) {
        if (!clearCurrentScalePreference()) {
            Serial.println("Failed to clear saved current scale");
            return;
        }

        Serial.println("Saved current scale cleared");
        printCurrentScaleState();
        return;
    }

    if (strncmp(command, "scale ", 6) == 0) {
        float parsed_scale = static_cast<float>(atof(command + 6));
        if (parsed_scale <= 0.0f) {
            Serial.println("Scale must be greater than 0");
            return;
        }

        current_scale = parsed_scale;
        Serial.print("Current scale set to ");
        Serial.println(current_scale, 4);
        return;
    }

    if (strcmp(command, "selftest on") == 0) {
        if (isAnyRCSignalValid()) {
            Serial.println("Self-test refused while live RC signal is present");
            return;
        }

        serial_self_test_active = true;
        self_test_power_percent = 0;
        self_test_light1 = false;
        self_test_light2 = false;
        self_test_aux = false;
        resetOutputsToSafeState();
        Serial.println("Self-test enabled");
        return;
    }

    if (strcmp(command, "selftest off") == 0) {
        stopSerialSelfTest();
        Serial.println("Self-test disabled");
        return;
    }

    if (strncmp(command, "pwm ", 4) == 0) {
        if (!serial_self_test_active) {
            Serial.println("Enable self-test first with 'selftest on'");
            return;
        }

        int percent = atoi(command + 4);
        if (percent < 0 || percent > SELF_TEST_PWM_MAX_PERCENT) {
            Serial.print("PWM out of range. Allowed: 0-");
            Serial.println(SELF_TEST_PWM_MAX_PERCENT);
            return;
        }

        self_test_power_percent = percent;
        Serial.print("Self-test PWM set to ");
        Serial.print(self_test_power_percent);
        Serial.println("%");
        return;
    }

    if (tryHandleSelfTestBinaryOutput(command)) {
        return;
    }

    // Fan control commands (v1.2 feature)
    if (strcmp(command, "fan status") == 0) {
        Serial.println("--- Fan Status ---");
        Serial.print("Fan Enabled: ");
        Serial.println(FAN_ENABLED ? "YES" : "NO");
        Serial.print("Auto Mode: ");
        Serial.println(fan_auto_mode ? "ON" : "OFF");
        Serial.print("Current Speed: ");
        Serial.print(fan_speed);
        Serial.println(" (0-255)");
        Serial.print("Temperature: ");
        Serial.print(last_temp_reading);
        Serial.println("°C");
        if (FAN_ENABLED) {
            Serial.print("Temp Range: ");
            Serial.print(FAN_TEMP_LOW);
            Serial.print("°C - ");
            Serial.print(FAN_TEMP_HIGH);
            Serial.println("°C");
        }
        return;
    }

    if (strcmp(command, "fan auto") == 0) {
        if (!FAN_ENABLED) {
            Serial.println("Fan is disabled - configure FAN_ENABLED in config.h");
            return;
        }
        setFanSpeed(-1);  // -1 means return to auto
        return;
    }

    if (strcmp(command, "fan off") == 0) {
        if (!FAN_ENABLED) {
            Serial.println("Fan is disabled - configure FAN_ENABLED in config.h");
            return;
        }
        fan_auto_mode = false;
        fan_speed = 0;
        ledcWrite(PWM_FAN_CHANNEL, 0);
        Serial.println("Fan: turned OFF");
        return;
    }

    if (strncmp(command, "fan speed ", 10) == 0) {
        if (!FAN_ENABLED) {
            Serial.println("Fan is disabled - configure FAN_ENABLED in config.h");
            return;
        }
        int speed = atoi(command + 10);
        setFanSpeed(speed);
        return;
    }

    Serial.println("Unknown command. Type 'help' for self-test commands.");
}

void handleSerialCommands() {
    while (Serial.available() > 0) {
        char incoming = static_cast<char>(Serial.read());

        if (incoming == '\r') {
            continue;
        }

        if (incoming == '\n') {
            if (serial_command_length == 0) {
                continue;
            }

            serial_command_buffer[serial_command_length] = '\0';
            processSerialCommand(serial_command_buffer);
            serial_command_length = 0;
            continue;
        }

        if (serial_command_length < sizeof(serial_command_buffer) - 1) {
            serial_command_buffer[serial_command_length++] = incoming;
        } else {
            serial_command_length = 0;
            Serial.println("Command too long");
        }
    }
}
#endif

/**
 * Initialize INA219 current/voltage sensor
 */
bool initCurrentSensor() {
    if (!ina219.begin()) {
        Serial.println("Failed to find INA219 chip");
        return false;
    }
    
    // Configure for high current measurement (up to 50A)
    // Using 32V, 2A range with external shunt for scaling
    ina219.setCalibration_32V_2A();
    
    Serial.println("INA219 initialized successfully");
    return true;
}

/**
 * Read current and voltage from INA219
 */
void readCurrentVoltage() {
    raw_current_mA = ina219.getCurrent_mA();
    voltage_V = ina219.getBusVoltage_V();
    raw_power_mW = ina219.getPower_mW();
    
    // Scale current reading based on external shunt (if using current divider)
    // For 50A max, you would typically use a 0.01 ohm shunt with current divider
    // Adjust this scaling factor based on your actual hardware setup
    current_mA = raw_current_mA * current_scale;
    power_mW = raw_power_mW * current_scale;
}

/**
 * Check safety limits and disable system if exceeded
 */
void checkSafetyLimits() {
    float current_A = current_mA / 1000.0;
    bool fault_detected = false;
    
    // Check overcurrent
    if (current_A > OVERCURRENT_THRESHOLD) {
        if (system_enabled) {
            Serial.print("OVERCURRENT DETECTED: ");
            Serial.print(current_A);
            Serial.println("A - Shutting down!");
        }
        system_enabled = false;
        fault_detected = true;
    }
    
    // Check undervoltage
    if (voltage_V < MIN_VOLTAGE_V && voltage_V > 0.1) {
        if (system_enabled) {
            Serial.print("UNDERVOLTAGE DETECTED: ");
            Serial.print(voltage_V);
            Serial.println("V - Shutting down!");
        }
        system_enabled = false;
        fault_detected = true;
    }
    
    // Check overvoltage
    if (voltage_V > MAX_VOLTAGE_V) {
        if (system_enabled) {
            Serial.print("OVERVOLTAGE DETECTED: ");
            Serial.print(voltage_V);
            Serial.println("V - Shutting down!");
        }
        system_enabled = false;
        fault_detected = true;
    }
    
    // Track fault time for recovery attempt
    if (fault_detected && system_fault_time == 0) {
        system_fault_time = millis();
        recovery_attempted = false;
    }
    
    // Attempt recovery if enough time has passed since fault
    if (!system_enabled && !recovery_attempted && system_fault_time > 0) {
        unsigned long fault_duration = millis() - system_fault_time;
        if (fault_duration >= RECOVERY_TIMEOUT_MS) {
            // Check if conditions are safe again
            if (current_A <= OVERCURRENT_THRESHOLD - 2.0 &&
                (voltage_V >= MIN_VOLTAGE_V || voltage_V < 0.1) &&
                voltage_V <= MAX_VOLTAGE_V) {
                Serial.println("Safety limits stable - attempting recovery");
                system_enabled = true;
                system_fault_time = 0;
                recovery_attempted = false;
            } else {
                recovery_attempted = true;
                Serial.print("Recovery conditions not met (I=");
                Serial.print(current_A, 1);
                Serial.print("A, V=");
                Serial.print(voltage_V, 1);
                Serial.println("V)");
            }
        }
    }
}

/**
 * Reset system to safe state (for recovery)
 */
void resetSystemState() {
    power_output = 0;
    light1_state = false;
    light2_state = false;
    system_fault_time = 0;
    recovery_attempted = false;
    
    ledcWrite(PWM_CHANNEL, 0);
    digitalWrite(LIGHT1_PIN, LOW);
    digitalWrite(LIGHT2_PIN, LOW);
    digitalWrite(AUX_OUTPUT_PIN, LOW);
}

/**
 * Update power output based on RC channel 1 (throttle)
 */
void updatePowerControl() {
    if (!system_enabled) {
        power_output = 0;
        ledcWrite(PWM_CHANNEL, 0);
        return;
    }
    
    if (!isPowerSignalValid()) {
        // No RC signal - set to safe state (zero power)
        power_output = 0;
        ledcWrite(PWM_CHANNEL, 0);
        return;
    }
    
    // Map RC input to power percentage (read atomically)
    unsigned long ch1_pulse = readRCValue(rc_ch1_value);
    int power_percent = mapRCToPower(ch1_pulse);
    
    // Convert percentage to PWM value (0-255 for 8-bit resolution)
    power_output = map(power_percent, 0, 100, 0, 255);
    
    // Write PWM output
    ledcWrite(PWM_CHANNEL, power_output);
}

/**
 * Update light control based on RC channel 2 and 3
 */
void updateLightControl() {
    if (!system_enabled) {
        digitalWrite(LIGHT1_PIN, LOW);
        digitalWrite(LIGHT2_PIN, LOW);
        light1_state = false;
        light2_state = false;
        return;
    }
    
    // Read channel values atomically
    unsigned long ch2_pulse = readRCValue(rc_ch2_value);
    unsigned long ch3_pulse = readRCValue(rc_ch3_value);
    unsigned long ch2_update = readRCUpdateTime(last_ch2_update);
    unsigned long ch3_update = readRCUpdateTime(last_ch3_update);
    bool ch2_seen = readRCChannelSeen(rc_ch2_seen);
    bool ch3_seen = readRCChannelSeen(rc_ch3_seen);
    
    if (!isChannelSignalValid(ch2_update, ch2_seen)) {
        digitalWrite(LIGHT1_PIN, LOW);
        light1_state = false;
    }

    if (isChannelSignalValid(ch2_update, ch2_seen) &&
        ch2_pulse > LIGHT_ON_THRESHOLD && !light1_state) {
        digitalWrite(LIGHT1_PIN, HIGH);
        light1_state = true;
    } else if (isChannelSignalValid(ch2_update, ch2_seen) &&
               ch2_pulse < LIGHT_OFF_THRESHOLD && light1_state) {
        digitalWrite(LIGHT1_PIN, LOW);
        light1_state = false;
    }

    if (!isChannelSignalValid(ch3_update, ch3_seen)) {
        digitalWrite(LIGHT2_PIN, LOW);
        light2_state = false;
    }

    // Light 2: ON when RC_CH3 > threshold (with hysteresis)
    if (isChannelSignalValid(ch3_update, ch3_seen) &&
        ch3_pulse > LIGHT_ON_THRESHOLD && !light2_state) {
        digitalWrite(LIGHT2_PIN, HIGH);
        light2_state = true;
    } else if (isChannelSignalValid(ch3_update, ch3_seen) &&
               ch3_pulse < LIGHT_OFF_THRESHOLD && light2_state) {
        digitalWrite(LIGHT2_PIN, LOW);
        light2_state = false;
    }
}

/**
 * Update auxiliary output based on RC channel 4
 */
void updateAuxOutput() {
    if (!system_enabled) {
        digitalWrite(AUX_OUTPUT_PIN, LOW);
        return;
    }
    
    unsigned long ch4_update = readRCUpdateTime(last_ch4_update);
    unsigned long ch4_pulse = readRCValue(rc_ch4_value);
    bool ch4_seen = readRCChannelSeen(rc_ch4_seen);
    
    if (!isChannelSignalValid(ch4_update, ch4_seen)) {
        digitalWrite(AUX_OUTPUT_PIN, LOW);
        return;
    }
    
    // Auxiliary output: ON when RC_CH4 > threshold
    digitalWrite(AUX_OUTPUT_PIN, ch4_pulse > LIGHT_ON_THRESHOLD ? HIGH : LOW);
}

/**
 * Print system status to serial
 */
void printStatus() {
    Serial.println("=== ESP32RC Status ===");
    Serial.print("System: ");
    Serial.println(system_enabled ? "ENABLED" : "DISABLED");
    Serial.print("RC Link: ");
    Serial.println(isAnyRCSignalValid() ? "OK" : "LOST");
    
    Serial.println("\n--- RC Channels ---");
    
    unsigned long ch1 = readRCValue(rc_ch1_value);
    unsigned long ch2 = readRCValue(rc_ch2_value);
    unsigned long ch3 = readRCValue(rc_ch3_value);
    unsigned long ch4 = readRCValue(rc_ch4_value);
    
    unsigned long ch1_upd = readRCUpdateTime(last_ch1_update);
    unsigned long ch2_upd = readRCUpdateTime(last_ch2_update);
    unsigned long ch3_upd = readRCUpdateTime(last_ch3_update);
    unsigned long ch4_upd = readRCUpdateTime(last_ch4_update);
    
    bool ch1_seen = readRCChannelSeen(rc_ch1_seen);
    bool ch2_seen = readRCChannelSeen(rc_ch2_seen);
    bool ch3_seen = readRCChannelSeen(rc_ch3_seen);
    bool ch4_seen = readRCChannelSeen(rc_ch4_seen);
    
    Serial.print("CH1 (Power): ");
    Serial.print(ch1);
    Serial.print(" us (");
    Serial.print(mapRCToPower(ch1));
    Serial.print("%) ");
    Serial.println(isChannelSignalValid(ch1_upd, ch1_seen) ? "OK" : "LOST");
    
    Serial.print("CH2 (Lights): ");
    Serial.print(ch2);
    Serial.print(" us ");
    Serial.println(isChannelSignalValid(ch2_upd, ch2_seen) ? "OK" : "LOST");
    
    Serial.print("CH3 (Aux1): ");
    Serial.print(ch3);
    Serial.print(" us ");
    Serial.println(isChannelSignalValid(ch3_upd, ch3_seen) ? "OK" : "LOST");
    
    Serial.print("CH4 (Aux2): ");
    Serial.print(ch4);
    Serial.print(" us ");
    Serial.println(isChannelSignalValid(ch4_upd, ch4_seen) ? "OK" : "LOST");
    
    Serial.println("\n--- Outputs ---");
    Serial.print("Power Output: ");
    Serial.print((power_output * 100) / 255);
    Serial.println("%");
    
    Serial.print("Light 1: ");
    Serial.println(light1_state ? "ON" : "OFF");
    
    Serial.print("Light 2: ");
    Serial.println(light2_state ? "ON" : "OFF");
    
    Serial.println("\n--- Power Monitoring ---");
    Serial.print("Voltage: ");
    Serial.print(voltage_V);
    Serial.println(" V");
    
    Serial.print("Current: ");
    Serial.print(current_mA / 1000.0);
    Serial.println(" A");
    
    Serial.print("Power: ");
    Serial.print(power_mW / 1000.0);
    Serial.println(" W");
    
    Serial.println("====================\n");
}

/**
 * Read temperature from external DS18B20 sensor or fallback to internal ESP32 sensor
 * Returns temperature in Celsius
 * External sensor accuracy: ±0.5°C (v1.3 feature)
 * Internal sensor accuracy: ±5°C
 */
float readSystemTemperature() {
    // Try external DS18B20 sensor first (v1.3)
    if (TEMP_SENSOR_ENABLED && use_external_temp && temp_sensor_available) {
        tempSensor.requestTemperatures();
        
        // Wait for conversion (depends on resolution)
        // 12-bit = 750ms, 11-bit = 375ms, 10-bit = 187.5ms, 9-bit = 93.75ms
        delay(800);  // Conservative wait time
        
        float temp = tempSensor.getTempCByIndex(0);
        
        // Validate reading (DS18B20 returns -127.00 if reading failed)
        if (temp != -127.00 && temp > -50.0 && temp < 125.0) {
            return temp;
        }
        
        // If reading failed, fall through to internal sensor
        Serial.println("WARNING: External temperature sensor read failed, using internal sensor");
        use_external_temp = false;
    }
    
    // Fallback to internal ESP32 temperature sensor
    // ESP32 has internal temperature sensor accessible via temperatureRead()
    // temperatureRead() returns Fahrenheit, we need Celsius
    return temperatureRead() - 32.0;
}

/**
 * Calculate fan speed based on temperature (linear interpolation)
 * Returns PWM value 0-255 or 0 if fan disabled
 */
int calculateFanSpeed(float temperature) {
    if (!FAN_ENABLED || !fan_auto_mode) {
        return 0;
    }
    
    // If below minimum temperature, fan off
    if (temperature <= FAN_TEMP_LOW) {
        return 0;
    }
    
    // If above maximum temperature, fan at max
    if (temperature >= FAN_TEMP_HIGH) {
        return FAN_SPEED_MAX;
    }
    
    // Linear interpolation between min and max
    float temp_range = FAN_TEMP_HIGH - FAN_TEMP_LOW;
    float speed_range = FAN_SPEED_MAX - FAN_SPEED_MIN;
    float normalized = (temperature - FAN_TEMP_LOW) / temp_range;
    
    return (int)(FAN_SPEED_MIN + (normalized * speed_range));
}

/**
 * Update fan PWM speed based on current temperature
 * Called periodically from main loop
 */
void updateFanControl() {
    unsigned long current_time = millis();
    
    // Update fan speed at monitoring interval
    if (current_time - last_fan_update < MONITOR_INTERVAL) {
        return;
    }
    last_fan_update = current_time;
    
    if (!FAN_ENABLED) {
        return;
    }
    
    // Read current system temperature
    last_temp_reading = readSystemTemperature();
    
    // Calculate new fan speed
    int new_fan_speed = calculateFanSpeed(last_temp_reading);
    
    // Apply hysteresis to prevent rapid on/off cycling
    // Only update if change is > 10 PWM units
    if (abs(new_fan_speed - fan_speed) > 10 || new_fan_speed == 0 || new_fan_speed == FAN_SPEED_MAX) {
        fan_speed = new_fan_speed;
        ledcWrite(PWM_FAN_CHANNEL, fan_speed);
    }
}

/**
 * Set fan speed manually (bypasses auto mode)
 * speed: 0-255 PWM value, or -1 to return to auto mode
 */
void setFanSpeed(int speed) {
    if (speed == -1) {
        fan_auto_mode = true;
        Serial.println("Fan: switched to AUTO mode");
        return;
    }
    
    if (speed < 0 || speed > 255) {
        Serial.println("ERROR: Fan speed must be 0-255");
        return;
    }
    
    fan_auto_mode = false;
    fan_speed = speed;
    ledcWrite(PWM_FAN_CHANNEL, fan_speed);
    Serial.print("Fan: speed set to ");
    Serial.println(speed);
}

/**
 * Initialize SD card for telemetry logging
 * Sets up SPI pins and initializes SD filesystem
 */
void initSDCard() {
    #if SD_CARD_ENABLED
    
    // Initialize SD card with SPI
    SPI.begin(SD_CLK_PIN, SD_MISO_PIN, SD_MOSI_PIN);
    
    if (!SD.begin(SD_CS_PIN)) {
        Serial.println("ERROR: SD card initialization failed");
        sd_card_ready = false;
        return;
    }
    
    sd_card_ready = true;
    uint8_t cardType = SD.cardType();
    
    if (cardType == CARD_NONE) {
        Serial.println("ERROR: No SD card found");
        sd_card_ready = false;
        return;
    }
    
    Serial.print("SD Card Type: ");
    if (cardType == CARD_MMC) {
        Serial.println("MMC");
    } else if (cardType == CARD_SD) {
        Serial.println("SD");
    } else if (cardType == CARD_SDHC) {
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
    }
    
    // Get card size
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.print("SD Card Size: ");
    Serial.print(cardSize);
    Serial.println(" MB");
    
    // Write telemetry header if needed
    writeTelemetryHeader();
    
    #else
    sd_card_ready = false;
    #endif
}

/**
 * Write CSV header to telemetry log file
 * Returns true if successful
 */
bool writeTelemetryHeader() {
    #if SD_CARD_ENABLED
    
    if (!sd_card_ready) {
        return false;
    }
    
    // Check if file exists
    if (!SD.exists(TELEMETRY_FILENAME)) {
        // Create new file and write header
        File file = SD.open(TELEMETRY_FILENAME, FILE_WRITE);
        if (!file) {
            Serial.print("ERROR: Cannot create ");
            Serial.println(TELEMETRY_FILENAME);
            return false;
        }
        
        // Write CSV header
        file.println("Time_ms,Power_%,Current_A,Voltage_V,Temp_C,Fan_0-255");
        file.close();
        telemetry_header_written = true;
        Serial.print("Created telemetry log: ");
        Serial.println(TELEMETRY_FILENAME);
        return true;
    }
    
    telemetry_header_written = true;
    return true;
    
    #else
    return false;
    #endif
}

/**
 * Log telemetry data to SD card
 * CSV format: timestamp, power%, current_A, voltage_V, temperature_C, fan_speed
 * Called periodically from main loop
 */
void logTelemetry() {
    #if SD_CARD_ENABLED
    
    if (!sd_card_ready || !telemetry_header_written) {
        return;
    }
    
    unsigned long current_time = millis();
    
    // Log at configured interval
    if (current_time - last_telemetry_log_time < TELEMETRY_LOG_INTERVAL) {
        return;
    }
    last_telemetry_log_time = current_time;
    
    // Open file in append mode
    File file = SD.open(TELEMETRY_FILENAME, FILE_APPEND);
    if (!file) {
        Serial.println("ERROR: Cannot open telemetry log for writing");
        return;
    }
    
    // Build CSV line with all logged fields
    char buffer[256];
    int written = 0;
    
    // Timestamp
    written += sprintf(buffer + written, "%lu,", current_time);
    
    // Power percentage (0-100)
    int power_percent = (power_output * 100) / 255;
    written += sprintf(buffer + written, "%d,", power_percent);
    
    // Current (A)
    written += sprintf(buffer + written, "%.2f,", current_mA / 1000.0);
    
    // Voltage (V)
    written += sprintf(buffer + written, "%.2f,", voltage_V);
    
    // Temperature (C)
    written += sprintf(buffer + written, "%.2f,", last_temp_reading);
    
    // Fan speed (0-255)
    written += sprintf(buffer + written, "%d", fan_speed);
    
    file.println(buffer);
    file.close();
    
    #endif
}

/**
 * Setup function - runs once at startup
 */
void setup() {
    // Initialize serial communication
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\nESP32RC - High-Current Load Controller");
    Serial.println("Starting initialization...");
    
    // Configure RC input pins
    pinMode(RC_CH1_PIN, INPUT);
    pinMode(RC_CH2_PIN, INPUT);
    pinMode(RC_CH3_PIN, INPUT);
    pinMode(RC_CH4_PIN, INPUT);
    
    // Attach interrupts for RC inputs
    attachInterrupt(digitalPinToInterrupt(RC_CH1_PIN), rc_ch1_isr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(RC_CH2_PIN), rc_ch2_isr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(RC_CH3_PIN), rc_ch3_isr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(RC_CH4_PIN), rc_ch4_isr, CHANGE);
    
    Serial.println("RC inputs configured");
    
    // Configure output pins
    pinMode(LIGHT1_PIN, OUTPUT);
    pinMode(LIGHT2_PIN, OUTPUT);
    pinMode(AUX_OUTPUT_PIN, OUTPUT);
    
    digitalWrite(LIGHT1_PIN, LOW);
    digitalWrite(LIGHT2_PIN, LOW);
    digitalWrite(AUX_OUTPUT_PIN, LOW);
    
    Serial.println("Output pins configured");
    
    // Configure PWM for power control
    ledcSetup(PWM_CHANNEL, PWM_FREQUENCY, PWM_RESOLUTION);
    ledcAttachPin(POWER_OUTPUT_PIN, PWM_CHANNEL);
    ledcWrite(PWM_CHANNEL, 0);
    
    Serial.println("PWM output configured");
    
    // Configure PWM for fan control (v1.2)
    if (FAN_ENABLED) {
        pinMode(FAN_PIN, OUTPUT);
        ledcSetup(PWM_FAN_CHANNEL, FAN_PWM_FREQUENCY, PWM_RESOLUTION);
        ledcAttachPin(FAN_PIN, PWM_FAN_CHANNEL);
        ledcWrite(PWM_FAN_CHANNEL, 0);
        Serial.println("Fan control configured");
    }
    
    // Initialize external temperature sensor (v1.3 feature)
    if (TEMP_SENSOR_ENABLED) {
        pinMode(TEMP_SENSOR_PIN, INPUT_PULLUP);
        tempSensor.begin();
        tempSensor.setResolution(TEMP_SENSOR_PRECISION);
        
        // Check if sensor is present
        if (tempSensor.getDeviceCount() > 0) {
            temp_sensor_available = true;
            use_external_temp = TEMP_SENSOR_USE_EXTERNAL;
            Serial.print("External temperature sensor found (");
            Serial.print(tempSensor.getDeviceCount());
            Serial.println(" device(s))");
        } else {
            Serial.println("WARNING: Temperature sensor not found - using internal ESP32 sensor");
            temp_sensor_available = false;
            use_external_temp = false;
        }
    }
    
    // Initialize I2C for current sensor
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    
    // Initialize current/voltage sensor
    initPreferencesStorage();
    loadPersistedCurrentScale();
    printCurrentScaleState();

    current_sensor_available = initCurrentSensor();
    if (current_sensor_available) {
        Serial.println("Current sensor ready");
    } else {
        Serial.println("WARNING: Current sensor not found - continuing without monitoring");
    }

    // Initialize SD card for telemetry logging (v1.3 Phase 2)
    #if SD_CARD_ENABLED
    initSDCard();
    if (sd_card_ready) {
        Serial.println("SD card telemetry logging enabled");
    } else {
        Serial.println("WARNING: SD card not available - telemetry logging disabled");
    }
    #endif

#if ENABLE_SERIAL_SELF_TEST
    Serial.println("Serial self-test support enabled");
    Serial.println("Type 'help' in the serial monitor for commands");
#endif

    last_monitor_time = millis();
    last_serial_update = millis();
    
    Serial.println("Initialization complete!");
    Serial.println("Ready to receive RC signals\n");
}

/**
 * Main loop - runs continuously
 */
void loop() {
    unsigned long current_time = millis();

#if ENABLE_SERIAL_SELF_TEST
    handleSerialCommands();

    if (serial_self_test_active && isAnyRCSignalValid()) {
        stopSerialSelfTest();
        Serial.println("RC signal detected - self-test disabled");
    }
#endif
    
    // Monitor current and voltage
    if (current_time - last_monitor_time >= MONITOR_INTERVAL) {
        if (current_sensor_available) {
            readCurrentVoltage();
            checkSafetyLimits();
        }
        last_monitor_time = current_time;
    }
    
    // Update fan control based on temperature
    updateFanControl();
    
    // Log telemetry data to SD card (v1.3 Phase 2)
    logTelemetry();
    
    // Update outputs
#if ENABLE_SERIAL_SELF_TEST
    if (serial_self_test_active) {
        applySerialSelfTestOutputs();
    } else {
        updatePowerControl();
        updateLightControl();
        updateAuxOutput();
    }
#else
    updatePowerControl();
    updateLightControl();
    updateAuxOutput();
#endif
    
    // Print status to serial
    if (current_time - last_serial_update >= SERIAL_UPDATE_INTERVAL) {
        printStatus();
        last_serial_update = current_time;
    }
    
    // Small delay to prevent tight loop
    delay(10);
}
