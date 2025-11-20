# ESP32RC System Architecture

## System Block Diagram

```
┌────────────────────────────────────────────────────────────────────────┐
│                         ESP32RC CONTROLLER                             │
│                                                                         │
│  ┌─────────────┐         ┌──────────────┐        ┌─────────────┐     │
│  │   RC INPUT  │────────▶│ MAIN CONTROL │───────▶│   OUTPUTS   │     │
│  │  (4 CH PWM) │         │    LOGIC     │        │  (PWM + DIG)│     │
│  └─────────────┘         └──────┬───────┘        └─────────────┘     │
│        ▲                         │                        │            │
│        │                         ▼                        ▼            │
│        │                 ┌──────────────┐                             │
│        │                 │SAFETY MANAGER│                             │
│        │                 └──────┬───────┘                             │
│        │                        │                                      │
│        │                        ▼                                      │
│        │                 ┌──────────────┐                             │
│        └─────────────────│ MONITORING   │                             │
│                          │  (I2C INA219)│                             │
│                          └──────────────┘                             │
└────────────────────────────────────────────────────────────────────────┘
                                  │
                                  ▼
                         ┌─────────────────┐
                         │  POWER STAGE    │
                         │ (MOSFET+DRIVER) │
                         └────────┬────────┘
                                  ▼
                         ┌─────────────────┐
                         │      LOAD       │
                         │   (0-50A)       │
                         └─────────────────┘
```

## Data Flow Diagram

```
RC Transmitter
      │
      │ PWM (1000-2000μs)
      ▼
┌─────────────┐
│RC Receiver  │
└──────┬──────┘
       │ CH1: Power Control
       │ CH2: Light 1
       │ CH3: Light 2  
       │ CH4: Auxiliary
       ▼
┌────────────────────────────────────────┐
│         ESP32 GPIO (ISR)               │
│  ┌──────────────────────────────────┐ │
│  │ Interrupt Service Routines       │ │
│  │ - Measure pulse width            │ │
│  │ - Validate signal                │ │
│  │ - Update timestamps              │ │
│  └──────────────────────────────────┘ │
└──────────────┬─────────────────────────┘
               │
               ▼
┌──────────────────────────────────────┐
│      Main Control Loop               │
│  ┌────────────────────────────────┐ │
│  │ 1. Read current/voltage (I2C)  │ │
│  │ 2. Check safety limits         │ │
│  │ 3. Validate RC signals         │ │
│  │ 4. Update power output         │ │
│  │ 5. Update light outputs        │ │
│  │ 6. Print status                │ │
│  └────────────────────────────────┘ │
└──────────────┬─────────────────────────┘
               │
               ├─────────────┬─────────────┬──────────────┐
               │             │             │              │
               ▼             ▼             ▼              ▼
         ┌─────────┐   ┌─────────┐  ┌─────────┐   ┌─────────┐
         │Power PWM│   │Light 1  │  │Light 2  │   │Aux Out  │
         │GPIO 25  │   │GPIO 26  │  │GPIO 27  │   │GPIO 14  │
         └─────────┘   └─────────┘  └─────────┘   └─────────┘
               │             │             │              │
               ▼             ▼             ▼              ▼
         Gate Driver   Relay/Driver  Relay/Driver   Relay/Driver
               │             │             │              │
               ▼             ▼             ▼              ▼
            MOSFET        Light 1       Light 2          Aux
               │
               ▼
            Load (Motor/etc)
               │
               ▼
         Current Shunt
               │
               ▼
         ┌──────────┐
         │  INA219  │
         │  Sensor  │
         └────┬─────┘
              │ I2C (SDA/SCL)
              ▼
         ESP32 GPIO 21/22
```

## State Machine Diagram

```
                    ┌─────────────┐
                    │   STARTUP   │
                    └──────┬──────┘
                           │
                           │ Initialize
                           ▼
                    ┌─────────────┐
         ┌─────────▶│   RUNNING   │◀──────────┐
         │          └──────┬──────┘           │
         │                 │                   │
         │    RC Signal OK │                   │
         │    Safety OK    │                   │
         │                 │                   │ Reset
         │                 │ Safety Fault      │ (Power Cycle)
         │                 │ OR                │
         │                 │ RC Lost           │
         │                 ▼                   │
         │          ┌─────────────┐            │
         │          │   STOPPED   │            │
         │          └──────┬──────┘            │
         │                 │                   │
         │      All Outputs│OFF                │
         │      Power = 0  │                   │
         │                 │                   │
         │                 │ Fault Cleared?    │
         │                 │ (Manual Reset)    │
         │                 │                   │
         │                 ├───────────────────┘
         │                 │ NO
         │                 │
         └─────────────────┘
           (Auto recover if
            signal returns
            and no safety
            fault)

States:
- STARTUP: Initialize peripherals, sensors
- RUNNING: Normal operation, outputs active
- STOPPED: Safety fault or signal loss, outputs disabled
```

## Safety System Flow

```
┌─────────────────────────────────────────────────┐
│         Monitoring Loop (Every 100ms)           │
└──────────────────┬──────────────────────────────┘
                   │
                   ▼
        ┌──────────────────────┐
        │ Read INA219 Sensor   │
        │ - Current (mA)       │
        │ - Voltage (V)        │
        │ - Power (mW)         │
        └──────────┬───────────┘
                   │
                   ▼
        ┌──────────────────────┐
        │  Check Overcurrent   │
        │  Current > 48A?      │
        └──────────┬───────────┘
                   │
        ┌──────────┴───────────┐
        │                      │
        YES                    NO
        │                      │
        ▼                      ▼
┌───────────────┐    ┌──────────────────────┐
│   SHUTDOWN    │    │  Check Undervoltage  │
│ System OFF    │    │  Voltage < 10V?      │
└───────────────┘    └──────────┬───────────┘
                                │
                     ┌──────────┴───────────┐
                     │                      │
                     YES                    NO
                     │                      │
                     ▼                      ▼
              ┌───────────────┐    ┌──────────────────────┐
              │   SHUTDOWN    │    │  Check Overvoltage   │
              │ System OFF    │    │  Voltage > 60V?      │
              └───────────────┘    └──────────┬───────────┘
                                              │
                                   ┌──────────┴───────────┐
                                   │                      │
                                   YES                    NO
                                   │                      │
                                   ▼                      ▼
                            ┌───────────────┐    ┌──────────────┐
                            │   SHUTDOWN    │    │   CONTINUE   │
                            │ System OFF    │    │   Operation  │
                            └───────────────┘    └──────────────┘
```

## RC Signal Processing

```
RC Receiver Output
       │
       │ Digital Pulse (1000-2000μs)
       ▼
┌──────────────┐
│ GPIO Change  │
│  Interrupt   │
└──────┬───────┘
       │
       ▼
┌──────────────────────┐
│ Rising Edge?         │
└──────┬───────────────┘
       │
   ┌───┴────┐
   │        │
  YES       NO
   │        │
   ▼        ▼
Store     Calculate
Start     Pulse Width
Time      (micros)
   │        │
   │        ▼
   │   ┌──────────────────┐
   │   │ Validate Pulse   │
   │   │ 1000-2000μs?     │
   │   └────────┬─────────┘
   │            │
   │        ┌───┴────┐
   │        │        │
   │       YES       NO
   │        │        │
   │        ▼        ▼
   │   Store      Discard
   │   Value      (Invalid)
   │        │
   │        ▼
   │   Update
   │   Timestamp
   │        │
   └────────┴──────────┐
                       │
                       ▼
                 ┌──────────────┐
                 │ Main Loop    │
                 │ Checks Time  │
                 │ Since Update │
                 └──────┬───────┘
                        │
                 ┌──────┴───────┐
                 │              │
              < 100ms        > 100ms
                 │              │
                 ▼              ▼
            ┌────────┐    ┌──────────┐
            │  Valid │    │   LOST   │
            │ Signal │    │  Signal  │
            └────────┘    └──────────┘
                              │
                              ▼
                        Set Outputs
                        to Safe State
```

## Power Control Mapping

```
RC Channel 1 Stick Position
       │
       │ 1000μs = 0%
       │ 1500μs = 50%
       │ 2000μs = 100%
       ▼
┌──────────────────┐
│ Map Function     │
│ 1000-2000 → 0-100│
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│ Power Percent    │
│ (0-100%)         │
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│ Convert to PWM   │
│ 0-100 → 0-255    │
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│ ledcWrite()      │
│ (8-bit PWM)      │
└────────┬─────────┘
         │
         │ PWM @ 1kHz
         ▼
    Gate Driver
         │
         ▼
    MOSFET Gate
         │
         ▼
    Load Power
    (0-100%)
```

## Timing Diagram

```
Time (ms):  0    100   200   300   400   500   600   ...

Monitor:    ▼     ▼     ▼     ▼     ▼     ▼     ▼
            │     │     │     │     │     │     │
            └─────┴─────┴─────┴─────┴─────┴─────┴─── Read INA219
            Every 100ms

Serial:                             ▼                 ▼
                                    │                 │
                                    └─────────────────┴─── Print Status
                                    Every 500ms

RC Update:  ▼ ▼  ▼▼ ▼   ▼ ▼▼    ▼   ▼▼  ▼    ▼▼
            Interrupt-driven, varies with RC frame rate
            Typical: Every 20ms (50Hz) per channel

Main Loop:  ▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼
            Continuous with 10ms delay
```

## Memory Organization

```
┌─────────────────────────────────┐
│         Flash Memory            │
├─────────────────────────────────┤
│  Program Code (~50KB)           │
│  - Main loop                    │
│  - ISR functions (IRAM)         │
│  - Library code                 │
│  - Strings/Constants            │
└─────────────────────────────────┘

┌─────────────────────────────────┐
│         SRAM (RAM)              │
├─────────────────────────────────┤
│  Global Variables               │
│  - RC channel values (volatile) │
│  - Timing data (volatile)       │
│  - State variables              │
│  - Sensor readings              │
│                                 │
│  Stack                          │
│  - Function calls               │
│  - Local variables              │
│                                 │
│  Heap                           │
│  - Dynamic allocations          │
│  - Objects (INA219, etc)        │
└─────────────────────────────────┘

┌─────────────────────────────────┐
│      IRAM (Fast RAM)            │
├─────────────────────────────────┤
│  ISR Functions                  │
│  - rc_ch1_isr()                 │
│  - rc_ch2_isr()                 │
│  - rc_ch3_isr()                 │
│  - rc_ch4_isr()                 │
└─────────────────────────────────┘
```

---

This architecture ensures:
- ✓ Fast, deterministic interrupt handling
- ✓ Continuous safety monitoring
- ✓ Reliable RC signal processing
- ✓ Failsafe behavior on errors
- ✓ Real-time status reporting