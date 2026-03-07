# Build Pipeline and CI/CD

## Overview

The ESP32RC project uses:
- **PlatformIO** for embedded build management
- **GitHub Actions** for Continuous Integration
- **Automated Testing** for build verification

## Local Build

### Prerequisites

```bash
# Install Python 3.8+
python --version

# Install PlatformIO
pip install platformio

# Verify installation
pio --version
```

### Building

```bash
# Build default environment (esp32dev)
pio run

# Build specific environment
pio run -e esp32dev_selftest

# Build and upload (requires connected ESP32)
pio run --target upload
pio run -e esp32dev_selftest --target upload

# Monitor serial output
pio device monitor --baud 115200
```

### Environments

The project defines two PlatformIO environments in `platformio.ini`:

| Environment | Purpose | Build Flags |
|-------------|---------|-------------|
| `esp32dev` | Default production firmware | `-DCORE_DEBUG_LEVEL=3` |
| `esp32dev_selftest` | Self-test mode for bring-up | `-DCORE_DEBUG_LEVEL=3 -DENABLE_SERIAL_SELF_TEST=1` |

Both environments:
- Use same board: **ESP32 DEV Module**
- Same framework: **Arduino**
- Same libraries: Adafruit INA219, BusIO
- Same upload speed: **921600 baud**

## GitHub Actions CI/CD

### Workflow

The `.github/workflows/build.yml` runs automatically on:
- Every push to `main` branch
- Every pull request against `main`
- Manual dispatch via GitHub UI

### Pipeline Stages

#### 1. PlatformIO Build (Matrix)

Builds both environments in parallel:
- `esp32dev`
- `esp32dev_selftest`

**Output:** Firmware binaries uploaded as artifacts

```yaml
Matrix: 
  - esp32dev → .pio/build/esp32dev/firmware.bin
  - esp32dev_selftest → .pio/build/esp32dev_selftest/firmware.bin
```

#### 2. Sanity Tests

Runs `scripts/build_sanity_tests.py`:
- 12 automated verification tests
- Checks for required symbols and functions
- Verifies configuration integrity

**Pass Criteria:** All 12 tests must pass

#### 3. Code Quality

Static analysis:
- Python syntax check (`py_compile`)
- Trailing whitespace detection
- Format consistency

**Pass Criteria:** No syntax errors, no trailing whitespace

#### 4. Test Summary

Aggregates results from all stages. Fails if any stage failed.

### Continuous Integration Matrix

```
Push → Branch Validator
         ↓
       Build esp32dev [parallel]
       Build esp32dev_selftest [parallel]
         ↓
       Run build_sanity_tests.py
         ↓
       Code quality checks
         ↓
       Final Status Report
```

### Viewing Results

1. **GitHub UI:** Actions tab → Click workflow run
2. **CLI:** (requires `gh` command-line tool)
   ```bash
   gh run list
   gh run view <run-id>
   ```

### Downloading Artifacts

**From GitHub UI:**
1. Go to Actions → latest successful run
2. Click "Artifacts" section
3. Download `firmware-esp32dev` or `firmware-esp32dev_selftest`

**From CLI:**
```bash
gh run download <run-id> -p firmware-esp32dev
```

### Artifacts Retention

- Artifacts retained for **30 days** by default
- Store in `.github/workflows/build.yml` if longer retention needed:
  ```yaml
  retention-days: 90  # Change to desired days
  ```

## Local Testing Script

Run the same sanity tests locally:

```bash
python scripts/build_sanity_tests.py
```

**Output Example:**
```
============================================================
ESP32RC Build Sanity Tests
============================================================

--- Build Tests ---
[PASS] Build esp32dev environment
[PASS] Build esp32dev_selftest environment

--- Configuration Tests ---
[PASS] Config header exists
[PASS] Config header constants defined
[PASS] PlatformIO selftest environment exists
[PASS] PlatformIO selftest flag set
[PASS] PSRAM flag removed from default
[PASS] CHANGELOG overvoltage correct

--- Source Code Tests ---
[PASS] Main.cpp exists
[PASS] Main.cpp functions present
[PASS] Atomic read helpers present
[PASS] Recovery mechanism present

============================================================
Results: 12 passed, 0 failed, 0 warnings
============================================================
```

## Hardware Verification

After successful build, verify hardware:

```bash
# Interactive hardware test (requires USB connection)
python scripts/hardware_verification.py COM3 115200

# Expected output:
============================================================
ESP32RC Hardware Verification
============================================================

--- Firmware Boot Test ---
[PASS] Firmware booted successfully
[PASS] Initialization completed

--- System Status ---
[PASS] System status

--- Sensor Availability ---
[PASS] INA219 sensor detected and responding

--- Self-Test Mode ---
[PASS] Self-test mode

============================================================
Verification Summary
============================================================
[PASS] Firmware running
[PASS] System status
[PASS] Sensor check
[PASS] Self-test mode

Total: 4/4 tests passed
============================================================
```

## Release Pipeline

Releases are automated via GitHub Actions when a tag is pushed:

```bash
git tag -a v1.1.1 -m "ESP32RC v1.1.1"
git push origin v1.1.1
```

**Auto-actions:**
1. Build both environments
2. Run all tests
3. Create GitHub Release with:
   - Firmware binaries
   - Release notes (from `release-notes/vX.Y.Z.md` or `CHANGELOG.md`)
   - Auto-generated changelog

See `.github/workflows/release.yml` for details.

## Build Optimization

### Reduce Build Time

```bash
# Parallel builds (faster)
pio run -j 4

# Incremental builds
pio run  # Only rebuilds changed files

# Clean build (slower)
pio run --target clean && pio run
```

### Reduce Binary Size

In `platformio.ini`:
```ini
build_flags =
    -DCORE_DEBUG_LEVEL=0    # No debug symbols
    -Os                      # Optimize for size
```

Current sizes:
- `esp32dev`: ~341 KB (26% of flash)
- `esp32dev_selftest`: ~329 KB (25% of flash)

Both fit comfortably in 1.3 MB flash.

## Troubleshooting CI/CD

### Build fails in CI but passes locally

**Cause:** Different environment (Ubuntu vs Windows)

**Solution:**
1. Check Python version: `python --version` (CI uses 3.11)
2. Clear cache: `git clean -fxd` then rebuild
3. Check for hardcoded paths (use relative paths instead)
4. Look at full CI logs for specific error

### Uploaded firmware is corrupt

**Cause:** Upload speed too high for USB connection

**Solution:**
Reduce in `platformio.ini`:
```ini
upload_speed = 115200  # Down from 921600
```

### Tests pass locally but fail in CI

**Cause:** CI environment differences (path separators, encoding)

**Solution:**
- Use `pathlib.Path` for cross-platform paths
- Specify encoding when reading files: `.read_text(encoding='utf-8')`
- Test on Linux: `docker run -it ubuntu:latest bash`

## Next Steps

### Monitoring

Watch CI runs automatically:
```bash
gh run watch  # Requires gh CLI
```

### Notifications

Configure GitHub to notify on:
- Build failures (default)
- Workflow runs (optional)
- Issue comments (optional)

In GitHub: Settings → Notifications

### Integration with IDE

Most IDEs support PlatformIO:
- **VS Code:** Install "PlatformIO IDE" extension
- **PyCharm:** Settings → Project → Project Interpreter → Add PlatformIO
- **Sublime:** Package Control → PlatformIO

Run tasks directly from IDE:  
- Build: Ctrl+Alt+B
- Upload: Ctrl+Alt+U  
- Monitor: Ctrl+Alt+M

## Resources

- PlatformIO Docs: https://docs.platformio.org/
- ESP32 Arduino Framework: https://github.com/espressif/arduino-esp32
- GitHub Actions Docs: https://docs.github.com/en/actions
- Adafruit INA219 Library: https://github.com/adafruit/Adafruit_INA219
