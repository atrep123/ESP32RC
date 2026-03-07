# ESP32RC Project Notes and Known Issues

## Recently Fixed (v2026-03-07)

### Build Configuration
- [x] Added `esp32dev_selftest` PlatformIO environment with `-DENABLE_SERIAL_SELF_TEST=1` flag
- [x] Removed unnecessary `-DBOARD_HAS_PSRAM` flag from default environment
- [x] Fixed CHANGELOG overvoltage threshold documentation (60V → 26V)

### Code Improvements
- [x] Added atomic read helpers (`readRCValue`, `readRCUpdateTime`, `readRCChannelSeen`) for ISR-safe RC channel access
- [x] Replaced all direct volatile reads with atomic wrappers in:`updatePowerControl()`, `updateLightControl()`, `updateAuxOutput()`, `isPowerSignalValid()`, `isAnyRCSignalValid()`, `printStatus()`
- [x] Implemented recovery mechanism for  safety shutdown state:
  - Tracks fault time with `system_fault_time`
  - Automatic recovery after `RECOVERY_TIMEOUT_MS` (5 seconds) if conditions stabilize
  - Added `resetSystemState()` helper function
  - Added `recovery_attempted` flag to prevent repeated recovery attempts

### Testing Infrastructure
- [x] Created `build_sanity_tests.py` for automated verification:
  - Builds both PlatformIO environments
  - Verifies config constants
  - Checks for critical functions in main.cpp
  - Validates atomic read helpers presence
  - Verifies recovery mechanism implementation
- [x] Created `hardware_verification.py` for interactive hardware testing via serial connection

## Known Issues (non-critical, documented for future work)

### 1. Volatile Variable Race Conditions (Minor)
**Status:** MITIGATED
- **Issue:** `unsigned long` volatile variables without atomic guards could theoretically have undefined behavior in edge cases
- **Mitigation:** Implemented atomic read wrappers for all ISR-shared state
- **Note:** On 32-bit ESP32, reads are atomic by default, but code is now formally correct
- **Reference:** [src/main.cpp](src/main.cpp) lines 90-106

### 2. Safety Shutdown is Permanent Until Recovery Timeout
**Status:** FIXED
- **Issue:** After safety violations (overcurrent/overvoltage/undervoltage), system disabled were permanent
- **Fix** Added automatic recovery after 5-second stabilization period
- **Behavior:** 
  - System waits `RECOVERY_TIMEOUT_MS` (5000ms) before attempting recovery
  - Recovery only happens if all safety conditions are met (current < threshold-2A, voltage in range)
  - If recovery fails, no further attempts until another safety event
- **Reference:** [src/main.cpp](src/main.cpp) lines 654-705

### 3. No Manual Recovery Command in Self-Test Mode
**Status:** DESIGN CHOICE
- **Issue:** Serial self-test doesn't have a "reset fault" or "recover" command
- **Rationale:** Automatic recovery after stabilization is safer than manual intervention
- **Mitigation:** Users can power-cycle the ESP32 to force a hard reset if needed
- **Future Enhancement:** Could add `system reset` or `fault reset` serial command if needed

### 4. ISR Timing Precision
**Status:** ACCEPTABLE
- **Note:** RC signal detection uses `micros()` for pulse width measurement
- **Potential Issue:** If ISR latency exceeds ~100µs, pulse edge timing could be off
- **Mitigation:** ESP32 interrupt latency is typically <1µs, far below RC signal resolution
- **No Action Required:** This is a fundamental hardware characteristic

### 5. Current Scaling is Global
**Status:** DOCUMENTED
- **Note:** `CURRENT_SCALE` applies uniformly to all current readings
- **Issue:** If hardware has non-linear current measurement (rare), this won't handle it
- **Mitigation:** Persistence layer allows runtime tuning via `scale <value>` command
- **Future Work:** Could implement per-range scaling or lookup tables if needed
- **Reference:** [include/config.h](include/config.h) line 37

### 6. I2C Bus Shares GPIO 21/22
**Status:** FIXED (No conflict)
- **Note:** Stock I2C pins (SDA 21, SCL 22) are free on ESP32
- **If Needed:** Can be remapped to different pins and recompile via [include/config.h](include/config.h) lines 10-11

### 7. No Watchdog Timer
**Status:** INTENTIONAL
- **Note:** ESP32 has internal watchdog but it's not enabled in this firmware
- **Reason:** Allows graceful failsafe on RC signal loss; watchdog would force restart
- **If Needed:** Can enable ESP32 watchdog in `setup()` with `esp_task_wdt_init()` if hard restart is preferred

### 8. Serial Monitor Output Rate is Fixed
**Status:** DOCUMENTED
- **Issue:** Status printed every 500ms (SERIAL_UPDATE_INTERVAL); cannot be changed at runtime
- **Workaround:** Change `#define SERIAL_UPDATE_INTERVAL 500` in [include/config.h](include/config.h) and recompile
- **Future Enhancement:** Could expose interval as settable parameter via NVS Preferences

### 9. Light Control Hysteresis is Fixed
**Status:** DOCUMENTED
- **Note:** Light ON threshold is 1600µs, OFF is 1400µs (40µs hysteresis)
- **If Needed:** Can be adjusted via [include/config.h](include/config.h) lines 40-41
- **No Runtime Tuning:** Would require persistence layer similar to current scaling

### 10. No Telemetry or Metrics Persistence  
**Status:** PARTIALLY MITIGATED
- **Note:** Only current scaling is persisted via ESP32 NVS Preferences
- **Data Lost on Restart:** Overcurrent count, operational time, etc. are not saved
- **Future Enhancement:** Could add optional logging to SD card or cloud if needed

## Test Coverage Summary

### Automated Tests (build_sanity_tests.py)
- ✓ Build both PlatformIO environments
- ✓ Config constants defined
- ✓ Self-test environment configured
- ✓ PSRAM flag removed
- ✓ Critical functions present
- ✓ Atomic read helpers exist
- ✓ Recovery mechanism integrated

### Manual Tests (hardware_verification.py)
- Can verify firmware boots
- Can check system status
- Can test INA219 sensor
- Can test self-test mode
- Not yet implemented: Load testing, failsafe verification, RC signal loss tests

### Missing Automated Tests (Future Work)
- [ } Failsafe behavior on RC signal loss
- [ ] Overcurrent shutdown and recovery
- [ ] Undervoltage/Overvoltage handling
- [ ] Light hysteresis switching
- [ ] PWM output accuracy
- [ ] I2C sensor communication
- [ ] Preferences persistence/recovery

## Performance Notes

### Memory Usage (ESP32 DEV)
- RAM: ~7% used (22.7 KB / 320 KB)
- Flash: ~26% used (341 KB / 1.3 MB)
- Headroom: Comfortable for future features

### CPU Usage
- Main loop: ~10ms delay, so max ~100 Hz cycle rate
- Monitoring: 100ms interval (10 Hz)
- Serial output: 500ms interval (2 Hz)
- No detected bottlenecks

## Future Enhancement Ideas

1. **Fan/Heatsink Control** - Add PWM fan driver based on temperature/load
2. **Telemetry Logging** - SD card logging of current/voltage/events
3. **OTA Firmware Updates** - Over-the-air update capability
4. **Wireless Control** - WiFi or Bluetooth control in addition to RC
5. **Behavioral Modes** - Presets (racing, gentle, sport) that adjust thresholds and response curves
6. **Battery Management** - LiPo cell balancing and capacity monitoring
7. **Load Profiling** - Real-time power curve analysis
8. **CAN Bus Support** - Integration with vehicle CAN networks

## References

- Main firmware: [src/main.cpp](src/main.cpp)
- Configuration: [include/config.h](include/config.h)
- Testing scripts: [scripts/build_sanity_tests.py](scripts/build_sanity_tests.py), [scripts/hardware_verification.py](scripts/hardware_verification.py)
- Build system: [platformio.ini](platformio.ini)
- Documentation: `docs/`

---

**Last Updated:** 2026-03-07  
**Firmware Version:** ~1.1.0+dev  
**Build Status:** ✓ Both environments compile successfully
