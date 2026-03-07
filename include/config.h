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

// Fan control settings (v1.2 feature)
#define FAN_PIN 12                // GPIO 12 for fan PWM control
#define PWM_FAN_CHANNEL 1         // ESP32 PWM channel 1 (power uses channel 0)
#define FAN_PWM_FREQUENCY 1000    // 1kHz PWM frequency for fan
#define FAN_ENABLED 1             // Enable fan control by default

// Fan temperature control thresholds
#define FAN_TEMP_LOW 35.0         // Start fan at 35°C
#define FAN_TEMP_HIGH 50.0        // Max fan speed at 50°C
#define FAN_SPEED_MIN 30          // Minimum PWM (30/255 = 12% minimum speed)
#define FAN_SPEED_MAX 255         // Maximum PWM (100% speed)

// External Temperature Sensor (DS18B20) - v1.3 feature
#define TEMP_SENSOR_PIN 19        // GPIO 19 for OneWire (DS18B20)
#define TEMP_SENSOR_ENABLED 1     // Enable external temperature sensor
#define TEMP_SENSOR_PRECISION 12  // 12-bit precision (0.0625°C resolution)
#define TEMP_SENSOR_REQUEST_TIMEOUT 1000  // 1 second timeout for sensor read
#define TEMP_SENSOR_USE_EXTERNAL 1        // Use external sensor when available (fallback to internal)

// SD Card Telemetry Logging (v1.3 Phase 2 feature)
#define SD_CARD_ENABLED 1         // Enable SD card telemetry logging
#define SD_CS_PIN 5               // SD card chip select (GPIO 5)
#define SD_MOSI_PIN 23            // SD card MOSI (GPIO 23)
#define SD_MISO_PIN 24            // SD card MISO (GPIO 24)
#define SD_CLK_PIN 18             // SD card CLK (GPIO 18)
#define TELEMETRY_LOG_INTERVAL 1000  // Log telemetry every 1000ms (1 second)
#define TELEMETRY_FILENAME "/telemetry.csv"  // Default telemetry log file

// Telemetry data fields to log (bitmask)
#define LOG_TIMESTAMP 1           // Log timestamp (ms)
#define LOG_POWER_PERCENT 2       // Log power percentage (0-100)
#define LOG_CURRENT_A 4           // Log current (Amperes)
#define LOG_VOLTAGE_V 8           // Log voltage (Volts)
#define LOG_TEMPERATURE_C 16      // Log temperature (Celsius)
#define LOG_FAN_SPEED 32          // Log fan speed (0-255)
#define LOG_ALL (LOG_TIMESTAMP | LOG_POWER_PERCENT | LOG_CURRENT_A | LOG_VOLTAGE_V | LOG_TEMPERATURE_C | LOG_FAN_SPEED)

#endif // CONFIG_H
