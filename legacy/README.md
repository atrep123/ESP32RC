# Legacy Sketches - Reference Only

This folder contains older standalone Arduino sketches kept only for reference and historical purposes.

**Status:** ARCHIVED - Do not use for new projects  
**Active Firmware:** `src/main.cpp` (PlatformIO-based)

## Legacy Sketches

| File | Description | Features | Status |
|------|-------------|----------|--------|
| `ESP32RC_Advanced.ino` | Advanced vehicle controller with odometry | Encoder feedback, MPU6050 IMU, multiple drive modes, Czech comments | Archived - complex vehicle control |
| `ESP32RC_Ultimate_Edition.ino` | Feature-rich RC controller | Unknown - see comments in file | Archived - superseded |
| `ESP32RC_WebConfig.ino` | Web-based configuration UI | WiFi interface, configuration saving | Archived - potential future reference |

## Why These Are Deprecated

1. **Standalone sketches** - Require manual library management via Arduino IDE
2. **No version control** - Config values hardcoded, no way to track changes
3. **Limited reusability** - Not modular; difficult to extract components
4. **No automated testing** - No build verification or CI/CD
5. **Czech comments** - Documentation in multiple languages makes maintenance harder

## Active Firmware Advantages

The current `src/main.cpp` + PlatformIO approach provides:
- ✓ Dependency management via `platformio.ini`
- ✓ Persistent configuration via ESP32 Preferences (NVS)
- ✓ Automated build verification and testing
- ✓ Multi-environment support (default + self-test)
- ✓ Consistent documentation in English
- ✓ GitHub Actions CI/CD integration

## If You Need These Features

If you need features from legacy sketches (e.g., IMU/encoder support), consider:

1. **Port to new architecture** - Extract the useful functions and integrate into `src/main.cpp`
2. **Create separate firmware variant** - Use PlatformIO environment to build alternative configurations
3. **Reference implementation** - Study the code for algorithms, but rewrite using modern practices

## Migration Path

To migrate from legacy to current firmware:

1. Review the feature you need in the legacy sketch
2. Check if it's already in `src/main.cpp` (see `include/config.h` for customization options)
3. If not, create a GitHub Issue requesting the feature
4. Propose implementation sticking to:
   - ISR-safe volatile access (use atomic helpers)
   - Automated recovery where possible
   - NVS Preferences for runtime tuning
   - Comprehensive inline documentation

## Archive Considerations

These sketches are preserved as:
- Learning examples of older approaches
- Historical record of project evolution
- Reference for complex algorithms (IMU, odometry, etc.)

They are **not** maintained and may use deprecated APIs or outdated libraries.
