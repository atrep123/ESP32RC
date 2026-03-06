#ifndef CONFIG_H
#define CONFIG_H

// Pin definitions for PWM inputs from RC receiver
#define RC_CH1_PIN 34    // Channel 1 - Throttle/Power control
#define RC_CH2_PIN 35    // Channel 2 - Lights control
#define RC_CH3_PIN 32    // Channel 3 - Auxiliary 1
#define RC_CH4_PIN 33    // Channel 4 - Auxiliary 2

// Pin definitions for outputs
#define POWER_OUTPUT_PIN 25   // PWM output for power control (MOSFET gate)
#define LIGHT1_PIN 26         // Digital output for light 1
#define LIGHT2_PIN 27         // Digital output for light 2
#define AUX_OUTPUT_PIN 14     // Auxiliary output

// I2C pins for current/voltage sensor (INA219)
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

// PWM settings
#define PWM_FREQUENCY 1000    // 1kHz PWM frequency for power control
#define PWM_RESOLUTION 8      // 8-bit resolution (0-255)
#define PWM_CHANNEL 0         // ESP32 PWM channel

// RC PWM signal parameters (typical RC values)
#define RC_MIN_PULSE 1000     // Minimum pulse width in microseconds
#define RC_MAX_PULSE 2000     // Maximum pulse width in microseconds
#define RC_MID_PULSE 1500     // Middle position pulse width
#define RC_DEADBAND 50        // Deadband around middle position
#define RC_TIMEOUT 100        // Timeout in ms for signal loss

// Safety limits
#define MAX_CURRENT_A 50.0    // Maximum current in Amperes
#define MIN_VOLTAGE_V 10.0    // Minimum voltage threshold
#define MAX_VOLTAGE_V 26.0    // INA219 bus measurement limit
#define OVERCURRENT_THRESHOLD 48.0  // Trigger shutdown at 48A

// Current measurement scaling for an external shunt/divider arrangement
#define CURRENT_SCALE 25.0

// Optional serial self-test mode for bring-up without a live RC signal
#ifndef ENABLE_SERIAL_SELF_TEST
#define ENABLE_SERIAL_SELF_TEST 0
#endif

#ifndef SELF_TEST_PWM_MAX_PERCENT
#define SELF_TEST_PWM_MAX_PERCENT 25
#endif

// Monitoring intervals
#define MONITOR_INTERVAL 100  // Monitor current/voltage every 100ms
#define SERIAL_UPDATE_INTERVAL 500  // Update serial output every 500ms

// Light control thresholds
#define LIGHT_ON_THRESHOLD 1600   // PWM value above which light turns on
#define LIGHT_OFF_THRESHOLD 1400  // PWM value below which light turns off

#endif // CONFIG_H
