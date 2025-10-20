# Engine Analyser

An embedded machine learning system for real-time engine vibration analysis and anomaly detection using STMicroelectronics NanoEdge AI on STM32L412RB.

[![Zephyr RTOS](https://img.shields.io/badge/Zephyr-3.x-blue.svg)](https://www.zephyrproject.org/)
[![Platform](https://img.shields.io/badge/Platform-STM32L412RB-green.svg)](https://www.st.com/en/microcontrollers-microprocessors/stm32l4-series.html)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

## 🎯 Overview

This project implements an intelligent engine monitoring system that:
- ✅ Collects 3-axis accelerometer data at 500 Hz
- ✅ Learns normal vibration patterns during training phase
- ✅ Detects anomalies in real-time during inference
- ✅ Uses STMicroelectronics NanoEdge AI for embedded ML
- ✅ Runs on Zephyr RTOS for reliability and performance

### Key Features

- **Real-time Anomaly Detection**: Uses machine learning to identify abnormal engine behavior
- **On-device Learning**: Train the ML model directly on the embedded device
- **Low Resource Footprint**: Optimized for resource-constrained MCUs (40KB RAM)
- **USB Data Logging**: Stream results over USB CDC for monitoring and debugging
- **Thread-safe Design**: Double buffering and mutex-protected critical sections
- **Production Ready**: Comprehensive error handling and fault diagnostics

---

## 📋 Table of Contents

- [Hardware Requirements](#-hardware-requirements)
- [Software Requirements](#-software-requirements)
- [Project Structure](#-project-structure)
- [Getting Started](#-getting-started)
- [Configuration](#-configuration)
- [Building and Flashing](#-building-and-flashing)
- [Usage](#-usage)
- [Architecture](#-architecture)
- [Troubleshooting](#-troubleshooting)
- [Contributing](#-contributing)
- [License](#-license)

---

## 🔧 Hardware Requirements

### Required Components

- **Microcontroller**: STM32L412RB Nucleo board (NUCLEO-L412RB-P)
  - ARM Cortex-M4F @ 80 MHz
  - 128 KB Flash
  - 40 KB SRAM
  - FPU support (hardware floating-point)
  
- **Accelerometer**: LIS2DH12 3-axis MEMS accelerometer
  - I2C interface
  - ±2g/±4g/±8g/±16g selectable scale
  - Connected to I2C3 (PC0=SCL, PC1=SDA)
  - I2C address: 0x18

### Wiring Diagram

```
STM32L412RB          LIS2DH12
-----------          --------
PC0 (I2C3_SCL) ----> SCL
PC1 (I2C3_SDA) ----> SDA
3.3V           ----> VDD
GND            ----> GND
```

### Optional Components

- USB cable (for power, programming, and data logging)
- ST-Link debugger (built-in on Nucleo board)

---

## 💻 Software Requirements

### Development Environment

1. **Zephyr RTOS**: v3.x or later
   - [Installation Guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html)

2. **Toolchain**: 
   - ARM GNU Toolchain (GCC)
   - Recommended: Zephyr SDK

3. **Build Tools**:
   - CMake (>= 3.20.0)
   - Ninja or Make
   - West (Zephyr's meta-tool)

4. **IDE** (Optional):
   - VS Code with Cortex-Debug extension
   - STM32CubeIDE

### NanoEdge AI Library

This project requires the NanoEdge AI library from STMicroelectronics:

1. Visit [NanoEdge AI Studio](https://www.st.com/en/development-tools/nanoedgeaistudio.html)
2. Create a project with these parameters:
   - **Type**: Anomaly Detection
   - **Sensor**: 3-axis accelerometer
   - **Sampling Rate**: 500 Hz
   - **Signal Length**: 256 samples per axis (768 total values)
   - **Target**: STM32 (ARM Cortex-M4F)

3. Export the library and place files in `lib/nanoedge_ai/`:
   - `libneai.a` (precompiled library)
   - `NanoEdgeAI.h` (header file)
   - Update `KNOWLEDGE_BUFFER_SIZE` in `src/main.c` based on your model

---

## 📁 Project Structure

```
engine-analyser/
├── boards/
│   └── nucleo_l412rb_p.overlay      # Device tree overlay for board configuration
├── hardware/
│   └── list_of_hardware_issues.txt  # Known hardware issues and fixes
├── inc/
│   └── accelerometer.h              # Accelerometer driver API
├── lib/
│   └── nanoedge_ai/
│       ├── libneai.a                # NanoEdge AI precompiled library
│       └── NanoEdgeAI.h             # NanoEdge AI API header
├── scripts/
│   ├── build_all.bat                # Build script
│   └── build_dfu.bat                # DFU build script
├── src/
│   ├── main.c                       # Main application with ML state machine
│   └── accelerometer.c              # LIS2DH12 driver implementation
├── .vscode/
│   ├── launch.json                  # Cortex-Debug configuration
│   └── settings.json                # VS Code workspace settings
├── CMakeLists.txt                   # Build configuration
├── prj.conf                         # Zephyr project configuration
├── neai_overlay.ld                  # Linker script for NanoEdge AI memory
├── west.yml                         # West manifest
├── start_zephyr_env.bat             # Environment setup script
└── README.md                        # This file
```

---

## 🚀 Getting Started

### 1. Clone the Repository

```bash
cd c:/development/zephyr/zephyrproject/
git clone https://github.com/Ayushkothari96/engine-analyser.git
cd engine-analyser
```

### 2. Install Dependencies

Ensure Zephyr SDK and tools are installed:

```bash
# Install Zephyr dependencies (Ubuntu/Debian)
sudo apt install --no-install-recommends git cmake ninja-build gperf \
  ccache dfu-util device-tree-compiler wget \
  python3-dev python3-pip python3-setuptools python3-tk python3-wheel \
  xz-utils file make gcc gcc-multilib g++-multilib libsdl2-dev

# Install West
pip3 install --user west

# Initialize Zephyr workspace (if not already done)
west init ~/zephyrproject
cd ~/zephyrproject
west update
west zephyr-export
pip3 install --user -r ~/zephyrproject/zephyr/scripts/requirements.txt
```

### 3. Setup NanoEdge AI Library

1. Download and install [NanoEdge AI Studio](https://www.st.com/en/development-tools/nanoedgeaistudio.html)
2. Create a new Anomaly Detection project
3. Import your training data (see `engine-analyser-logger` for data collection)
4. Benchmark and select the best model
5. Export the library for STM32 ARM Cortex-M4F
6. Copy `libneai.a` and `NanoEdgeAI.h` to `lib/nanoedge_ai/`
7. Update `KNOWLEDGE_BUFFER_SIZE` in `src/main.c` (check your exported README)

### 4. Build the Project

```bash
# On Windows
start_zephyr_env.bat
west build -b nucleo_l412rb_p

# On Linux/Mac
cd ~/zephyrproject/engine-analyser
west build -b nucleo_l412rb_p
```

### 5. Flash to Board

```bash
west flash
```

### 6. Monitor Output

Connect to the USB CDC serial port:

```bash
# Linux
sudo screen /dev/ttyACM0 115200

# Windows (use PuTTY, Tera Term, or similar)
# Connect to COMx port at any baud rate (CDC ignores baud rate)
```

---

## ⚙️ Configuration

### Key Configuration Options

Edit `prj.conf` to customize:

```properties
# Logging Level (0=OFF, 1=ERR, 2=WRN, 3=INF, 4=DBG)
CONFIG_LOG_DEFAULT_LEVEL=3

# Accelerometer Sample Rate (modify in src/main.c)
# Default: 500 Hz

# Main Stack Size
CONFIG_MAIN_STACK_SIZE=2048

# Heap Size
CONFIG_HEAP_MEM_POOL_SIZE=2048

# Enable/Disable MPU
CONFIG_ARM_MPU=n  # Disabled for debugging
```

### NanoEdge AI Configuration

In `src/main.c`, configure:

```c
// Knowledge buffer size (from your NanoEdge AI export)
#define KNOWLEDGE_BUFFER_SIZE 4096  // Update this!

// Accelerometer sample rate
struct accel_config config = {
    .sample_rate_hz = 500,  // Adjust as needed
    .data_ready_cb = data_ready_handler
};
```

---

## 🔨 Building and Flashing

### Build Commands

```bash
# Clean build
west build -b nucleo_l412rb_p --pristine

# Incremental build
west build

# Build with menuconfig
west build -t menuconfig

# Build for debugging
west build -- -DCONFIG_DEBUG=y
```

### Flashing Commands

```bash
# Flash via ST-Link
west flash

# Flash specific file
west flash --hex-file build/zephyr/zephyr.hex

# Erase flash before programming
west flash --erase
```

### Debugging

Using VS Code with Cortex-Debug:

1. Open project in VS Code
2. Press `F5` to start debugging
3. Set breakpoints as needed
4. Use Debug Console for GDB commands

---

## 📖 Usage

### System Operation Modes

The system operates in three states:

#### 1. **IDLE State**
- Waiting for initialization
- No ML processing
- Occurs on startup errors

#### 2. **TRAINING State**
- Learning normal vibration patterns
- Collects 20+ samples (configurable)
- Automatically transitions to INFERENCING when complete
- **Important**: Only feed "normal" engine data during training!

#### 3. **INFERENCING State**
- Real-time anomaly detection
- Outputs similarity percentage (0-100%)
  - **100%** = Completely normal (identical to training data)
  - **0%** = Completely abnormal (anomaly detected)
- Typical threshold: < 80% indicates potential issue

### Workflow

```
Power On
   ↓
Initialize NanoEdge AI
   ↓
Start Accelerometer (500 Hz)
   ↓
TRAINING: Feed 20 "normal" samples
   ↓
Automatic transition
   ↓
INFERENCING: Monitor similarity %
   ↓
Alert if similarity < threshold
```

### Example Output

```
[00:00:01.234] <inf> main: NanoEdge AI initialized successfully
[00:00:01.456] <inf> main: Current state: 1 (TRAINING)
[00:00:02.789] <inf> accel: LIS2DH12 accelerometer initialized
[00:00:03.012] <inf> main: Training iteration 1
[00:00:05.234] <inf> main: Training iteration 2
...
[00:01:23.456] <inf> main: Training complete (max iterations), entering inferencing
[00:01:24.567] <inf> main: Similarity: 95
[00:01:25.678] <inf> main: Similarity: 92
[00:01:26.789] <inf> main: Similarity: 45  <- Potential anomaly!
```

---

## 🏗️ Architecture

### System Architecture

```
┌─────────────────────────────────────────────────────┐
│                   Main Thread                        │
│  ┌──────────────┐      ┌──────────────────────┐    │
│  │ State Machine│◄────►│ NanoEdge AI Library  │    │
│  └──────────────┘      └──────────────────────┘    │
│         ▲                                            │
│         │ Events (EVT_DATA_READY)                    │
│         │                                            │
└─────────┼────────────────────────────────────────────┘
          │
          │ Callback
          │
┌─────────┼────────────────────────────────────────────┐
│         ▼                                            │
│  ┌──────────────┐      ┌──────────────────────┐    │
│  │ Accel Thread │◄────►│ LIS2DH12 Sensor      │    │
│  │ (500 Hz)     │      │ (I2C3)               │    │
│  └──────────────┘      └──────────────────────┘    │
│                                                      │
│       Accelerometer Driver                           │
└──────────────────────────────────────────────────────┘
```

### Data Flow

1. **Accelerometer Thread** (500 Hz):
   - Reads sensor via I2C3
   - Fills double buffer (256 samples)
   - Calls callback when buffer full
   - **Thread-safe**: Uses mutex for buffer access

2. **Main Thread**:
   - Receives event from callback
   - Copies data to ML buffer (interleaved format)
   - Processes with NanoEdge AI
   - Logs results

3. **Double Buffering**:
   - Buffer A: Being filled by sensor
   - Buffer B: Being processed by ML
   - Prevents data corruption during processing

### Memory Layout

```
Flash (128 KB):
├── Application Code        (~40 KB)
├── NanoEdge AI Library     (~30 KB)
├── Zephyr RTOS             (~50 KB)
└── Free Space              (~8 KB)

RAM (40 KB):
├── Main Stack              (2 KB)
├── Accel Thread Stack      (1 KB)
├── ML Buffer (768 floats)  (3 KB)
├── Accel Buffers (2×768)   (6 KB)
├── Knowledge Buffer        (4-8 KB, model-specific)
├── Zephyr Kernel           (~10 KB)
└── Heap                    (2 KB)
```

---

## 🐛 Troubleshooting

### Common Issues

#### 1. **MemManage Fault at 0x80111A4**

**Symptom**: Crash during `neai_anomalydetection_init()`

**Cause**: Missing or incorrectly sized knowledge buffer

**Solution**:
```c
// In src/main.c, ensure you have:
#define KNOWLEDGE_BUFFER_SIZE 4096  // Check your NanoEdge AI README!
static uint8_t knowledge_buffer[KNOWLEDGE_BUFFER_SIZE] __attribute__((aligned(4)));
```

#### 2. **Accelerometer Not Found**

**Symptom**: `Accelerometer device not ready`

**Cause**: I2C connection or device tree issue

**Solution**:
- Verify wiring (PC0=SCL, PC1=SDA)
- Check I2C address (0x18)
- Verify device tree overlay is loaded

#### 3. **USB Serial Not Appearing**

**Symptom**: No COM port visible

**Cause**: USB CDC not initialized

**Solution**:
- Check `CONFIG_USB_DEVICE_STACK=y` in prj.conf
- Verify USB D+/D- connections (PA11/PA12)
- Try different USB cable
- Wait 2-3 seconds after power-on

#### 4. **Stack Overflow**

**Symptom**: `STACK CHECK FAIL`

**Cause**: Insufficient stack size

**Solution**:
```properties
# In prj.conf
CONFIG_MAIN_STACK_SIZE=2048  # Increase if needed
```

#### 5. **Build Errors with NanoEdge AI**

**Symptom**: Linker errors about `knowledge_buffer`

**Cause**: Custom linker script not loaded

**Solution**:
- Ensure `neai_overlay.ld` exists
- Check `zephyr_linker_sources()` in CMakeLists.txt

---

## 📊 Performance

### Resource Usage

| Resource | Usage | Total | Percentage |
|----------|-------|-------|------------|
| Flash    | ~70 KB | 128 KB | ~55% |
| SRAM     | ~25 KB | 40 KB | ~62% |
| CPU (avg)| ~30% | 80 MHz | - |

### Timing Characteristics

- **Sample Collection**: 256 samples @ 500 Hz = 512 ms
- **ML Inference Time**: ~10-50 ms (model-dependent)
- **Training Time**: ~20-30 seconds (20 iterations)
- **End-to-end Latency**: < 600 ms per decision

---

## 🤝 Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

### Development Guidelines

- Follow Zephyr coding style
- Add comments for complex logic
- Test on hardware before submitting
- Update documentation for API changes

---

## 📝 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

## 🙏 Acknowledgments

- **STMicroelectronics** for NanoEdge AI Studio and libraries
- **Zephyr Project** for the excellent RTOS
- **ARM** for Cortex-M4F architecture

---

## 📧 Contact

**Ayush Kothari** - [@Ayushkothari96](https://github.com/Ayushkothari96)

Project Link: [https://github.com/Ayushkothari96/engine-analyser](https://github.com/Ayushkothari96/engine-analyser)

---

## 📚 Additional Resources

- [NanoEdge AI Studio Documentation](https://wiki.st.com/stm32mcu/wiki/AI:NanoEdge_AI_Studio)
- [Zephyr Documentation](https://docs.zephyrproject.org/)
- [STM32L4 Reference Manual](https://www.st.com/resource/en/reference_manual/rm0394-stm32l41xxx42xxx43xxx44xxx45xxx46xxx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)
- [LIS2DH12 Datasheet](https://www.st.com/resource/en/datasheet/lis2dh12.pdf)

---

**⭐ If you find this project helpful, please consider giving it a star!**
