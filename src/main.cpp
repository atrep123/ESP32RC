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
#include "config.h"

// Global objects
Adafruit_INA219 ina219;

// RC channel values (pulse width in microseconds)
volatile unsigned long rc_ch1_value = RC_MID_PULSE;
volatile unsigned long rc_ch2_value = RC_MID_PULSE;
volatile unsigned long rc_ch3_value = RC_MID_PULSE;
volatile unsigned long rc_ch4_value = RC_MID_PULSE;

// RC channel timing
volatile unsigned long rc_ch1_start = 0;
volatile unsigned long rc_ch2_start = 0;
volatile unsigned long rc_ch3_start = 0;
volatile unsigned long rc_ch4_start = 0;

// Last valid signal times
unsigned long last_ch1_update = 0;
unsigned long last_ch2_update = 0;
unsigned long last_ch3_update = 0;
unsigned long last_ch4_update = 0;

// System state
bool system_enabled = true;
bool light1_state = false;
bool light2_state = false;
int power_output = 0;

// Monitoring variables
float current_mA = 0;
float voltage_V = 0;
float power_mW = 0;
unsigned long last_monitor_time = 0;
unsigned long last_serial_update = 0;

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
 * Check if RC signal is present (not timed out)
 */
bool isRCSignalValid() {
    unsigned long now = millis();
    return (now - last_ch1_update < RC_TIMEOUT) ||
           (now - last_ch2_update < RC_TIMEOUT);
}

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
    current_mA = ina219.getCurrent_mA();
    voltage_V = ina219.getBusVoltage_V();
    power_mW = ina219.getPower_mW();
    
    // Scale current reading based on external shunt (if using current divider)
    // For 50A max, you would typically use a 0.01 ohm shunt with current divider
    // Adjust this scaling factor based on your actual hardware setup
    float current_scale = 25.0; // Example: 25x scaling for 50A range
    current_mA *= current_scale;
    power_mW *= current_scale;
}

/**
 * Check safety limits and disable system if exceeded
 */
void checkSafetyLimits() {
    float current_A = current_mA / 1000.0;
    
    // Check overcurrent
    if (current_A > OVERCURRENT_THRESHOLD) {
        Serial.print("OVERCURRENT DETECTED: ");
        Serial.print(current_A);
        Serial.println("A - Shutting down!");
        system_enabled = false;
    }
    
    // Check undervoltage
    if (voltage_V < MIN_VOLTAGE_V && voltage_V > 0.1) {
        Serial.print("UNDERVOLTAGE DETECTED: ");
        Serial.print(voltage_V);
        Serial.println("V - Shutting down!");
        system_enabled = false;
    }
    
    // Check overvoltage
    if (voltage_V > MAX_VOLTAGE_V) {
        Serial.print("OVERVOLTAGE DETECTED: ");
        Serial.print(voltage_V);
        Serial.println("V - Shutting down!");
        system_enabled = false;
    }
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
    
    if (!isRCSignalValid()) {
        // No RC signal - set to safe state (zero power)
        power_output = 0;
        ledcWrite(PWM_CHANNEL, 0);
        return;
    }
    
    // Map RC input to power percentage
    int power_percent = mapRCToPower(rc_ch1_value);
    
    // Convert percentage to PWM value (0-255 for 8-bit resolution)
    power_output = map(power_percent, 0, 100, 0, 255);
    
    // Write PWM output
    ledcWrite(PWM_CHANNEL, power_output);
}

/**
 * Update light control based on RC channel 2
 */
void updateLightControl() {
    if (!system_enabled) {
        digitalWrite(LIGHT1_PIN, LOW);
        digitalWrite(LIGHT2_PIN, LOW);
        light1_state = false;
        light2_state = false;
        return;
    }
    
    if (!isRCSignalValid()) {
        // No RC signal - turn off lights
        digitalWrite(LIGHT1_PIN, LOW);
        digitalWrite(LIGHT2_PIN, LOW);
        light1_state = false;
        light2_state = false;
        return;
    }
    
    // Light 1: ON when RC_CH2 > threshold (with hysteresis)
    if (rc_ch2_value > LIGHT_ON_THRESHOLD && !light1_state) {
        digitalWrite(LIGHT1_PIN, HIGH);
        light1_state = true;
    } else if (rc_ch2_value < LIGHT_OFF_THRESHOLD && light1_state) {
        digitalWrite(LIGHT1_PIN, LOW);
        light1_state = false;
    }
    
    // Light 2: ON when RC_CH3 > threshold (with hysteresis)
    if (rc_ch3_value > LIGHT_ON_THRESHOLD && !light2_state) {
        digitalWrite(LIGHT2_PIN, HIGH);
        light2_state = true;
    } else if (rc_ch3_value < LIGHT_OFF_THRESHOLD && light2_state) {
        digitalWrite(LIGHT2_PIN, LOW);
        light2_state = false;
    }
}

/**
 * Update auxiliary output based on RC channel 4
 */
void updateAuxOutput() {
    if (!system_enabled || !isRCSignalValid()) {
        digitalWrite(AUX_OUTPUT_PIN, LOW);
        return;
    }
    
    // Auxiliary output: ON when RC_CH4 > threshold
    digitalWrite(AUX_OUTPUT_PIN, rc_ch4_value > LIGHT_ON_THRESHOLD ? HIGH : LOW);
}

/**
 * Print system status to serial
 */
void printStatus() {
    Serial.println("=== ESP32RC Status ===");
    Serial.print("System: ");
    Serial.println(system_enabled ? "ENABLED" : "DISABLED");
    Serial.print("RC Signal: ");
    Serial.println(isRCSignalValid() ? "OK" : "LOST");
    
    Serial.println("\n--- RC Channels ---");
    Serial.print("CH1 (Power): ");
    Serial.print(rc_ch1_value);
    Serial.print(" us (");
    Serial.print(mapRCToPower(rc_ch1_value));
    Serial.println("%)");
    
    Serial.print("CH2 (Lights): ");
    Serial.print(rc_ch2_value);
    Serial.println(" us");
    
    Serial.print("CH3 (Aux1): ");
    Serial.print(rc_ch3_value);
    Serial.println(" us");
    
    Serial.print("CH4 (Aux2): ");
    Serial.print(rc_ch4_value);
    Serial.println(" us");
    
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
    
    // Initialize I2C for current sensor
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    
    // Initialize current/voltage sensor
    if (initCurrentSensor()) {
        Serial.println("Current sensor ready");
    } else {
        Serial.println("WARNING: Current sensor not found - continuing without monitoring");
    }
    
    // Initialize timing
    last_ch1_update = millis();
    last_ch2_update = millis();
    last_ch3_update = millis();
    last_ch4_update = millis();
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
    
    // Monitor current and voltage
    if (current_time - last_monitor_time >= MONITOR_INTERVAL) {
        readCurrentVoltage();
        checkSafetyLimits();
        last_monitor_time = current_time;
    }
    
    // Update outputs
    updatePowerControl();
    updateLightControl();
    updateAuxOutput();
    
    // Print status to serial
    if (current_time - last_serial_update >= SERIAL_UPDATE_INTERVAL) {
        printStatus();
        last_serial_update = current_time;
    }
    
    // Small delay to prevent tight loop
    delay(10);
}
