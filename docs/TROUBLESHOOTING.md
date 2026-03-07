# Troubleshooting Guide

## Build Issues

### "PlatformIO not found"

**Problem:** `pio: command not found` or `pip: command not found`

**Solution:**
```bash
# Install Python 3.8+ first
pip install --upgrade pip
pip install platformio
```

### Build fails with "library not found"

**Problem:** Error like `Adafruit INA219 library not found`

**Solution:**
```bash
# PlatformIO should auto-install from platformio.ini
# If not, manually specify:
pio lib install "adafruit/Adafruit INA219@^1.2.1"
pio lib install "adafruit/Adafruit BusIO@^1.14.1"

# Clean and rebuild
pio run --target clean
pio run -e esp32dev
```

### "Invalid project configuration" error

**Problem:** `platformio.ini` contains invalid syntax (common on Windows with BOM)

**Solution:**
```bash
# Ensure file has no UTF-8 BOM and uses Unix line endings (LF)
# On Windows PowerShell:
(Get-Content platformio.ini -Raw) | Set-Content platformio.ini -Encoding ASCII

# On Linux/Mac:
dos2unix platformio.ini  # if available
# or manually edit to remove BOM
```

### Compilation fails with "undefined reference"

**Problem:** Linker error: `undefined reference to 'someFunction'`

**Solution:**
1. Check that all `.cpp` files are in `src/` directory
2. Verify `include/` files are properly guarded with `#ifndef`
3. Check `platformio.ini` has correct `build_flags` and `lib_deps`
4. Clean build directory: `pio run --target clean && pio run -e esp32dev`

### Memory overflow ("program too large" or RAM issues)

**Problem:** `section .text will not fit...` or similar

**Solution:**
- Remove debug flags in `platformio.ini`: `-DCORE_DEBUG_LEVEL=0`
- Optimize for size: Add `-O2` to build_flags
- Review `src/main.cpp` for unnecessary features
- Use `pio run -e esp32dev_selftest --target upload` (smaller build)

---

## Hardware Issues

### Not receiving RC signal

**Problem:** Serial output shows `RC Link: LOST` continuously

**Checklist:**
1. [ ] Verify RC receiver is powered and paired with transmitter
2. [ ] Check wiring: CH1 → GPIO 34, CH2 → GPIO 35, CH3 → GPIO 32, CH4 → GPIO 33
3. [ ] Verify 3.3V logic compatibility (not 5V!)
4. [ ] Test directly: `pio device monitor # Check serial at 115200 baud`
5. [ ] Use oscilloscope to verify PWM signal at GPIO pins
6. [ ] Check for ground loops: Verify common ground between ESP32 and RC receiver

**Solutions:**
- Add level shifter if receiver outputs 5V logic (ESP32 GPIO needs 3.3V)
- Use 330Ω resistor divider if no level shifter: `5V --[1kΩ]-- GPIO --[2kΩ]-- GND`
- Increase RC_TIMEOUT in `config.h` if signals are intermittent: default 100ms

### INA219 sensor not detected

**Problem:** Serial output shows `Failed to find INA219 chip` or `INA219 not available`

**Checklist:**
1. [ ] Verify I2C wiring: SDA → GPIO 21, SCL → GPIO 22
2. [ ] Check 3.3V or 5V power to sensor (see sensor datasheet)
3. [ ] Check address: Default is 0x40, verify with `i2cdetect` if available
4. [ ] Verify pull-up resistors (typical: 4.7kΩ each on SDA/SCL)

**Solutions:**
```bash
# Scan I2C bus for devices (if using external tools)
# esp32 doesn't have built-in i2cdetect, but you can:
# 1. Use Arduino IDE's Wire library to scan
# 2. Or use self-test mode to verify sensor presence

# Test with self-test firmware:
pio run -e esp32dev_selftest --target upload
pio device monitor
# Then send: sensor
```

### High current reading doesn't match multimeter

**Problem:** INA219 reads 25A but external ammeter shows 10A (or vice versa)

**Solution:**
1. Use self-test mode to check raw vs scaled readings:
   ```
   selftest on
   pwm 50
   sensor
   ```
2. Note the "Raw current" and "Scaled current"
3. Adjust `CURRENT_SCALE` in `config.h` or runtime via serial:
   ```
   scale show           # See current scale
   scale <new_value>    # e.g., scale 15.0
   scale save           # Save to NVS
   ```
4. Verify shunt resistor value matches circuit design

### Power output not responding to RC

**Problem:** PWM doesn't increase when throttle stick moves

**Checklist:**
1. [ ] Verify CH1 (throttle) wiring to GPIO 34
2. [ ] Move throttle stick fully left and right - should see `CH1: 1000 us` to `2000 us` in status
3. [ ] Check gate driver is powered (separate 12V supply often needed)
4. [ ] Verify PWM output pin 25 is connected to gate driver input

**Solutions:**
- Confirm RC receiver is sending valid PWM (oscilloscope on GPIO 34)
- Check MOSFET gate driver has adequate input current (~1mA @ 3.3V)
- Test PWM output directly with oscilloscope on GPIO 25
- Use self-test to verify PWM can be generated without RC:
  ```
  selftest on
  pwm 50
  # Should see power output increase
  ```

### System shuts down immediately

**Problem:** Serial shows `OVERCURRENT DETECTED` immediately after startup

**Causes & Solutions:**
1. **Sensor miscalibration:** Current reading offset is wrong
   - Run `scale reset` in self-test mode
   - Use `sensor` command to check readings at zero load
   
2. **Load is too high:** OVERCURRENT_THRESHOLD exceeded (default 48A)
   - Check `OVERCURRENT_THRESHOLD` in `config.h`
   - Verify load is within spec
   
3. **Shunt resistor is incorrect:** If using external shunt, verify value and adjust `CURRENT_SCALE`

---

## Serial Monitor Issues

### Serial data is garbled or unreadable

**Problem:** Serial output contains: `▊▊▊▊ggΩ…` or similar garbage

**Solution:**
```bash
# Ensure baud rate is 115200
pio device monitor --baud 115200

# Or manually specify:
screen /dev/ttyUSB0 115200  # Linux
putty -serial COM3 -sercfg 115200  # Windows
```

### No serial output at all

**Problem:** No text appears after upload completes

**Checklist:**
1. [ ] Verify USB cable is working (reliable charge cable)
2. [ ] Check COM port: `pio device list`
3. [ ] Try different USB port on computer
4. [ ] Verify upload completed: "Hard resetting via RTS pin" message
5. [ ] Check that ESP32 actually rebooted (might need manual reset button)

**Solutions:**
- Press ESP32 reset button (RST pin) after flashing
- Check `platformio.ini` has correct upload_speed: `upload_speed = 921600`
- Try slower upload speed: `upload_speed = 115200` (common cause of upload failures)

---

## Performance Issues

### Firmware is slow / laggy responses

**Problem:** Power output reacts slowly to throttle changes

**Causes:**
1. **10ms delay in main loop** - Intentional, reduces CPU usage
2. **Monitor interval 100ms** - Safety checks run every 100ms
3. **Serial printing blocks** - Status printed every 500ms

**Solutions:**
- Reduce `delay(10)` in loop if responsiveness critical
- Lower `SERIAL_UPDATE_INTERVAL` if don't need frequent status (but increases serial traffic)
- Disable safety limit checks if confident (not recommended for production)

### High CPU / Battery drain

**Problem:** ESP32 running hot or battery dies quickly

**Solutions:**
```cpp
// In include/config.h:

// Reduce monitoring frequency (less power draw)
#define MONITOR_INTERVAL 200    // Was 100ms

// Reduce serial output
#define SERIAL_UPDATE_INTERVAL 2000  // Was 500ms

// Lower debug level (saves RAM/CPU)
// In platformio.ini: -DCORE_DEBUG_LEVEL=0 (was 3)
```

---

## Self-Test Mode Issues

### Self-test won't enable

**Problem:** `Self-test refused while live RC signal is present`

**Solution:**
- Turn off RC transmitter before starting self-test
- Or wait for RC timeout: 100ms with no RC signal

### Self-test commands not recognized

**Problem:** "Unknown command. Type 'help' for self-test commands."

**Solutions:**
1. Ensure proper line ending: Send `\r\n` (CRLF), not just `\n`
2. Verify self-test firmware was uploaded: `pio run -e esp32dev_selftest --target upload`
3. No spaces allowed at end of line

### Self-test PWM limited to 25%

**Problem:** Can't set PWM above 25% (e.g., `pwm 50` doesn't work)

**Reason:** Safety feature - `SELF_TEST_PWM_MAX_PERCENT = 25`

**Solutions:**
- Increase in `include/config.h`: `#define SELF_TEST_PWM_MAX_PERCENT 100`
- Rebuild and upload: `pio run -e esp32dev_selftest --target upload`

---

## Recovery and Reset

### Factory Reset

To reset all settings to defaults:

```cpp
// Uncomment in setup() temporarily, then comment out:
preferences.begin(PREFERENCES_NAMESPACE, false);
preferences.clear();  // Wipe all saved values
preferences.end();

// Then recompile and upload
pio run -e esp32dev --target upload
```

### Force Hard Reset

```bash
# If ESP32 is stuck:
1. Disconnect USB
2. Wait 5 seconds
3. Reconnect USB
# Or press RST button on board if available
```

### Recovery from Corrupted NVS

If persistent storage is corrupted:

```bash
# Erase all flash memory
pio run -e esp32dev --target erase

# Then upload fresh firmware
pio run -e esp32dev --target upload
```

---

## Reporting Issues

If you encounter a problem not listed here:

1. **Collect information:**
   ```bash
   platformio --version
   python --version
   
   pio device list
   
   # Capture serial output:
   pio device monitor > output.log 2>&1
   # Let it run for 30 seconds, then stop (Ctrl+C)
   ```

2. **Check GitHub Issues:** https://github.com/atrep123/ESP32RC/issues

3. **Create new issue with:**
   - Hardware setup (board type, power supply, load type)
   - Library versions (from `pio lib list`)
   - Serial output (from step 1)
   - Steps to reproduce
   - Expected vs actual behavior

---

## Quick Reference: Common Commands

```bash
# Building
pio run                              # Build default environment
pio run -e esp32dev_selftest         # Build self-test
pio run --target clean               # Clean build artifacts

# Uploading
pio run --target upload              # Build and upload default
pio run -e esp32dev_selftest --target upload

# Monitoring
pio device monitor --baud 115200
pio device monitor --filter colorize

# Testing
python scripts/build_sanity_tests.py # Run all checks
python scripts/hardware_verification.py COM3 115200

# Debugging
pio run --target erase               # Full flash erase
pio device list                      # Find connected ESP32
```
