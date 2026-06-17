# Dual Syringe Pump ESP-IDF Firmware

This repository contains the ESP-IDF based firmware for a high-precision **Dual Syringe Pump** system. The system controls two independent syringe channels driven by stepper motors with closed-loop PID control via magnetic encoders, featuring an interactive menu interface displayed on an ILI9341 TFT screen and buttons.

---

## Table of Contents
1. [Key Features](#1-key-features)
2. [Hardware Architecture & Pinout](#2-hardware-architecture--pinout)
3. [Software Structure](#3-software-structure)
4. [Calibration & Conversions](#4-calibration--conversions)
5. [UART Communication Protocol](#5-uart-communication-protocol)
6. [Flashing & Monitoring Guide](#6-flashing--monitoring-guide)

---

## 1. Key Features

- **Dual Syringe Control**: Controls two independent or coordinated syringe channels (`Channel 0` & `Channel 1`).
- **Closed-Loop Control**: Combines an open-loop **Trapezoidal Motion Profiling** algorithm with closed-loop **PID Feedback** using **AS5600 Magnetic Encoders** to correct positioning errors in real time.
- **Multiple Operation Modes**:
  - `Independent`: Channels run completely independently with their own target volumes and flow rates.
  - `Simultaneous`: Both channels start and stop synchronously; a fault in one channel halts both for safety.
  - `Sequential`: Continuous infusion where Channel 1 starts automatically as soon as Channel 0 finishes.
  - `Homing`: Carriage homing routine to calibrate physical start limits.
- **Self-Healing I2C Watchdog**: Real-time communication watchdog for the PCF8574 button expander. If communication fails 3 consecutive times, it performs a dynamic I2C bus scan and re-registers the expander automatically to prevent system lockups.
- **LVGL Graphical User Interface**: Renders real-time telemetry (flow rates, infused/target volumes, time elapsed, progress bar, and channel state) on an ILI9341 SPI TFT LCD screen using physical buttons for navigation.

---

## 2. Hardware Architecture & Pinout

The hardware is based on the **ESP32** microcontroller. Peripherals are mapped as defined in [io_config.h](file:///d:/pc/Documents/Micropump/Newversion/idf/Dual_Syringe_pump_idf/components/drivers/io_config.h) and detailed in [Noi_day.txt](file:///d:/pc/Documents/Micropump/Newversion/idf/Dual_Syringe_pump_idf/Noi_day.txt):

### Stepper Motor Drivers (TMC2209)
Each channel is driven by a TMC2209 driver configured via step/dir pins and configured over a shared UART bus.
- **Channel 0 Stepper Pins**:
  - `STEP`: GPIO 25
  - `DIR`: GPIO 2
  - `EN`: GPIO 27
- **Channel 1 Stepper Pins**:
  - `STEP`: GPIO 14
  - `DIR`: GPIO 13
  - `EN`: GPIO 17
- **Shared UART (TMC2209 Communication)**:
  - `UART Port`: UART1
  - `TX Pin`: GPIO 15
  - `RX Pin`: GPIO 26
  - Node IDs: Channel 0 is Node `0`, Channel 1 is Node `1`

### Magnetic Position Encoders (AS5600)
AS5600 magnetic encoders read the motor/plunger carriage rotation to verify actual delivery rates.
- **Channel 0 AS5600**: I2C Bus 0 (`SDA` = GPIO 21, `SCL` = GPIO 22)
- **Channel 1 AS5600**: I2C Bus 1 (`SDA` = GPIO 32, `SCL` = GPIO 33)

### Button Input (PCF8574)
Five physical navigation buttons are interfaced through a PCF8574 I2C I/O expander sharing **I2C Bus 0** (address `0x20`).
- **Button Pin Mapping**:
  - `UP`: Pin 0
  - `SELECT`: Pin 1
  - `RIGHT`: Pin 2
  - `LEFT`: Pin 3
  - `DOWN`: Pin 4

### TFT Display (ILI9341)
An SPI TFT LCD is used to render the LVGL interface.
- **SPI Host**: SPI2 (VSPI)
- **SCLK**: GPIO 18
- **MOSI**: GPIO 23
- **MISO**: GPIO 19
- **DC (Data/Command)**: GPIO 16
- **CS (Chip Select)**: GPIO 5
- **RST (Reset)**: -1 (Unused/tied high)
- **BK_LIGHT (Backlight)**: GPIO 12

### Host UART Configuration
- **UART Port**: UART0 (Default Debug & Command Port)
- **Baudrate**: 115200
- **TX/RX**: ESP32 Default pins (GPIO 1 / GPIO 3)

---

## 3. Software Structure

The code is developed under the **ESP-IDF v5.x** framework:

- **`main/main.c`**: Entry point. Initializes state, starts the LVGL and button polling tasks.
- **`components/app`**:
  - `pump_channel`: Individual channel operations and status indicators.
  - `pump_manager`: Logic coordinator for coordinate modes (Sequential/Simultaneous) and task scheduling.
  - `unit_converter`: Standard equations for units translation.
- **`components/drivers`**:
  - `as5600`: Multi-turn tracking and angle reporting.
  - `button`: Expander polling interface with self-healing recovery.
  - `tft`: SPI LCD configuration and LVGL driver bindings.
  - `tmc2209`: Stepper settings.
- **`components/motion`**:
  - `trapezoidal_profile`: Profile generation for acceleration/deceleration.
- **`components/services`**:
  - `error_handle`: Error diagnosis and safe shutdowns.

---

## 4. Calibration & Conversions

All translation constants are defined in [unit_converter.h](file:///d:/pc/Documents/Micropump/Newversion/idf/Dual_Syringe_pump_idf/components/app/unit_converter/unit_converter.h).

### Volumetric to Stepper Translation
The calibration constants in the firmware are tuned for a **1 mL syringe** with an inner diameter of **4.6 mm** (Cross-sectional Area = `16.619025 mm²`).
- **Microstepping**: 256 microsteps/step (`51200` steps per revolution).
- **Steps to Volumetric Equations**:
  $$\text{Steps per mL} = 7701955.0$$
  $$\text{mL per Step} \approx 0.000000129836$$
  $$\text{Steps per Encoder Tick} = 12.5$$
- **Linear Speed to Pulse Frequency**:
  $$\text{Frequency (Hz)} = \text{Velocity (mm/s)} \times 128000.0$$

### Look-up Table Dynamic Calibration
Because friction and mechanical backlash affect volume accuracy differently at different flow speeds, the system uses dynamic calibration factor $K$ interpolation based on the target flow rate.

**Calibration Tables:**
```c
// Channel 0 Look-up Table
static const calib_point_t ch0_calib_table[] = {
    {0.03f, 1.000f}, 
    {0.06f, 0.987f}, 
    {0.60f, 1.000f}, 
    {1.50f, 0.955f}, 
    {3.00f, 0.926f}  
};

// Channel 1 Look-up Table
static const calib_point_t ch1_calib_table[] = {
    {0.03f, 1.027f},
    {0.06f, 0.955f},
    {0.60f, 1.034f},
    {1.50f, 0.968f},
    {3.00f, 0.926f} 
};
```
To recalibrate:
1. Dispense a set volume (e.g. $0.6$ mL) at a test flow rate.
2. Measure the actual weight/volume delivered.
3. Compute the correction factor: $K = \frac{\text{Measured Volume}}{\text{Expected Volume}}$.
4. Update the corresponding coefficient in `unit_converter.h`.

---

## 5. UART Communication Protocol

The ESP32 communicates with a host machine over UART0 using JSON format frames terminated with `\n`.

### 1. Serial Commands (Host to ESP32)

- **Start Independent Mode (`INDEP`):**
  ```json
  {"cmd": "START", "mode": "INDEP", "ch": 0, "flow": 0.5, "vol": 0.2}
  ```
- **Start Simultaneous Mode (`SIMUL`):**
  ```json
  {"cmd": "START", "mode": "SIMUL", "flow": 1.0, "vol": 0.5}
  ```
- **Start Sequential Mode (`SEQ`):**
  ```json
  {"cmd": "START", "mode": "SEQ", "ch0_flow": 0.5, "ch0_vol": 0.2, "ch1_flow": 1.0, "ch1_vol": 0.5}
  ```
- **Stop Infusion:**
  - Stop specific channel:
    ```json
    {"cmd": "STOP", "ch": 0}
    ```
  - Stop all channels immediately:
    ```json
    {"cmd": "STOP"}
    ```
- **Home Plunger Carriage:**
  - Home specific channel:
    ```json
    {"cmd": "HOME", "ch": 1}
    ```
  - Home all channels:
    ```json
    {"cmd": "HOME"}
    ```

### 2. Device Telemetry (ESP32 to Host)
Every 1 second, the ESP32 outputs telemetry details for each active channel:
```json
{"ch":0,"algo":"TRAP+PID","vol_infused":0.05214,"vol_target":0.20000,"flow_measure":0.49812,"flow_setpoint":0.50000,"time_run":12.0,"state":1,"steps":401524,"kp":1.20,"ki":0.05,"kd":0.10}
```

---

## 6. Flashing & Monitoring Guide

A Python monitor utility `plotter.py` is provided for terminal visualization and CSV recording.

### Prerequisites
Install Python 3 and the required dependencies:
```bash
pip install pyserial esptool rich
```

### Auto-Flashing
Compile the firmware using `idf.py build`, then run the following command to auto-detect the ESP32 and flash:
```bash
python plotter.py --flash ./build
```

### Terminal Telemetry
Run the plotter tool to open a live connection and render real-time progress tables:
```bash
python plotter.py
```
If you need to define the serial port manually:
```bash
python plotter.py --port COM3
```

All data packets are automatically saved to CSV files in the `./Log/` directory (e.g. `Log/pump_log_20260617_112450.csv`) for graphing and review.
