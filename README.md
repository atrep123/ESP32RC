# ESP32RC

## Project Overview

ESP32RC is a wireless communication and power control system designed for high-current applications (up to approximately 50A). The system is built around ESP microcontrollers (ESP32/ESP8266) and provides a comprehensive solution for combining RC (Radio Control) systems with modern IoT/ESP logic.

The project is ideal for:
- RC models (cars, boats, or special sci-fi props)
- High-current power modules with effect lighting
- Prototypes requiring integration of "hobby-grade" RC control with modern IoT/ESP logic

## Key Features

### Wireless Communication & Signal Processing
- **Wireless communication** via ESP (Wi-Fi, or custom protocol)
- **RC PWM signal reception and processing** from RC receivers
- **PWM decoding** (typically 1-2 ms pulses at 50 Hz)
- **Channel position and switch state evaluation**

### Control Capabilities
- **Lighting effects control:**
  - Headlights, turn signals, brake lights
  - Mode-based lighting
  - Various lighting effects and animations
  
- **Power management:**
  - Motor control
  - Load management
  - MOSFET/relay outputs

### Monitoring & Safety
- **Voltage monitoring** of the power supply branch
- **Current measurement** (via shunt or Hall sensor)
- **Protection features:**
  - Overvoltage protection
  - Undervoltage protection
  - Overcurrent protection
  - Automatic load disconnection when limits are exceeded

### Advanced Features
- **Telemetry support** (e.g., via Wi-Fi)
- **Parameter logging** (voltage, current, load status)
- **Web interface** connectivity
- **Serial diagnostics**
- **Diagnostic data display** for debugging and visual effects

## Technical Specifications

- **Microcontroller:** ESP32 or ESP8266
- **Maximum Current Handling:** Approximately 50A (depending on power stage implementation)
- **Input Signal:** RC PWM (1-2 ms pulse width, typically 50 Hz)
- **Communication:** Wi-Fi, serial interface
- **Measurement Capabilities:** Voltage and current monitoring

## How It Works

The ESP acts as a control unit that:

1. **Reads PWM signals** from the RC receiver (1-2 ms pulses at 50 Hz)
2. **Evaluates** channel positions and switch states
3. **Makes logic-based decisions** according to defined rules
4. **Controls outputs:**
   - MOSFET/relay outputs for power management
   - Light channels with various modes and effects
   - Safety systems (disconnect on overvoltage/undervoltage/overcurrent)

## Use Cases

- **RC Vehicle Models:** Cars, boats, and other vehicles requiring high-current control
- **Special Effects Props:** Sci-fi props with integrated lighting and power control
- **High-Power LED Lighting:** Effect lighting systems with RC control
- **Automated Systems:** Projects combining traditional RC control with IoT capabilities

## Expandability

The system is designed for expansion and customization:
- Web-based configuration interface
- Serial diagnostic interface
- Parameter logging and telemetry
- Custom lighting effects and sequences
- Integration with other IoT systems

## Future Development

- Enhanced telemetry features
- Advanced diagnostic displays
- Extended safety features
- Additional communication protocols
- Custom effect libraries