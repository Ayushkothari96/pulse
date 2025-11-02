# Quick Start Guide - Pulse Predictive Maintenance Device

## What You Get

A complete predictive maintenance system with:
- ✅ Real-time anomaly detection using on-device ML
- ✅ USB console with simple text commands
- ✅ Professional GUI monitor application
- ✅ Persistent training data storage in flash
- ✅ Low power consumption for battery operation

## Build & Flash (3 commands)

```powershell
cd c:\development\zephyr\zephyrproject\pulse
west build -b nucleo_l412rb_p --pristine
west flash
```

## Connect & Monitor

**Option 1: GUI Monitor (Recommended)**

The easiest way to interact with your Pulse device:

```powershell
cd scripts
pip install -r requirements.txt
python pulse_monitor.py

# Or simply double-click: run_pulse_monitor.bat
```

Features:
- Real-time status display with color coding
- Automatic device polling every 250ms
- One-click commands (Reset, Status, Toggle Logs)
- Live log viewer with dark theme
- Similarity percentage monitoring

**Option 2: Terminal (Advanced Users)**

Connect directly to USB console:

```powershell
# Windows - Use PuTTY or Tera Term
# Serial Port: COM3 (check Device Manager)
# Baud Rate: 115200
```

```bash
# Linux/macOS
screen /dev/ttyACM0 115200
```

## Available Commands

The device accepts three simple commands via USB console:

```bash
STATUS    # Show device status, ML state, similarity %, flash storage
RESET     # Clear ML model and training data, restart learning
LOGS      # Toggle verbose logging on/off
```

### Using the GUI

1. **Connect**: Select COM port and click "▶ Connect"
2. **Status**: Displays automatically (updated every 250ms)
3. **Start Learning**: Click "🔄 Reset & Learn" to train on normal machine operation
4. **Monitor**: Watch the status indicator change colors:
   - 🟢 **Green (NORMAL)**: Similarity ≥ 90% - Machine operating normally
   - 🔴 **Red (ANOMALY)**: Similarity < 90% - Potential issue detected
   - 🟠 **Orange (TRAINING)**: Currently learning normal patterns
   - 🔵 **Blue (IDLE)**: Waiting to start
5. **Toggle Logs**: Click "📝 Toggle Logs" to reduce/enable verbose output

### Using Terminal Commands

Type commands directly (case-insensitive):

```
STATUS
```

Response:
```
=== PULSE DEVICE STATUS ===
Device:      Pulse v1.0.0
Board:       nucleo_l412rb_p
Uptime:      123456 ms

ML State:    INFERENCING
Training:    40/40 iterations
Similarity:  95% (NORMAL)

Accel Rate:  500 Hz
Accel Buf:   256 samples

Flash Store: 10/10 samples stored
Flash CRC:   0x12345678
===========================
```

## Testing Workflow

### Step 1: Prepare Your Machine

Choose a machine to monitor (motor, fan, pump, mixer, etc.) and mount the Pulse device securely on its body using double-sided tape or a bracket.

### Step 2: Learn Normal Operation

1. Start the machine in its normal, healthy operating state
2. In the GUI, click **"🔄 Reset & Learn"** (or type `RESET` in terminal)
3. Let it run for 20-30 seconds during training
4. The device will automatically transition to monitoring mode
5. Status will change from 🟠 TRAINING to 🟢 NORMAL

**Important**: Only feed "normal" operation data during training!

### Step 3: Monitor for Anomalies

The device now continuously monitors the machine:
- **Similarity ≥ 90%**: Machine is operating normally (green)
- **Similarity < 90%**: Potential issue detected (red)

### Step 4: Test Anomaly Detection

Simulate a fault to verify detection works:

**Example: Fan Test**
1. Train on a balanced, freely-spinning fan
2. Partially obstruct airflow with paper
3. Watch similarity drop and status turn red ⚠

**Example: Motor Test**
1. Train on smooth motor operation
2. Add small weight (tape) to create imbalance
3. Observe anomaly detection

**Example: Mixer/Grinder**
1. Train while running empty
2. Add load (ice, beans, etc.)
3. See immediate anomaly detection

## Advanced Features

### Persistent Training Data

Training data is automatically saved to flash memory:
- Stores up to 10 training samples
- Survives power cycles
- CRC-validated for integrity
- Can be cleared with RESET command

### Power Consumption Monitoring

To measure battery life:
1. Use a multimeter in series with power supply
2. Measure current in different states:
   - IDLE: ~5-10 mA
   - TRAINING: ~15-20 mA
   - INFERENCING: ~10-15 mA
3. Calculate battery life: `Battery_mAh / Average_Current_mA = Hours`

### Verbose Logging Control

Reduce serial traffic to save power:
- Click **"📝 Toggle Logs"** in GUI
- Or type `LOGS` in terminal
- When OFF: Only shows critical messages
- Device still responds to all commands
- Useful for production deployment

## Creating a Portable Executable

Build a standalone .exe for the GUI (no Python installation needed):

```powershell
cd scripts

# Optional: Create custom icon
python create_icon.py

# Build the executable
python build_exe.py

# Output: dist\PulseMonitor.exe
```

The .exe can be copied to any Windows PC and run directly. Perfect for:
- Field testing
- Sharing with colleagues
- Customer demonstrations
- Production use

## Documentation

- **Main README**: `README.md` - Complete project documentation
- **GUI Icon Guide**: `scripts/ICON_GUIDE.md` - Customize executable icon
- **Hardware Issues**: `hardware/list_of_hardware_issues.txt`
- **Product Requirements**: `prd/phm_device_prd.md`
- **Flash Storage**: `docs/flash_storage.md`

## Troubleshooting

### GUI Issues

**COM port not showing?**
- Click "🔄 Refresh" button
- Check Device Manager for COM port
- Wait 2-3 seconds after plugging in device

**"Cannot connect" error?**
- Close other serial terminal programs
- Try different USB port
- Replug the device
- Check COM port permissions (Linux: add user to dialout group)

**GUI won't start?**
- Install dependencies: `pip install -r requirements.txt`
- Check Python version: 3.7 or later required
- Try: `python --version`

### Device Issues

**Device not responding?**
- Press reset button on Nucleo board
- Power cycle the device
- Reflash firmware: `west flash`

**Training never completes?**
- Ensure machine is running during training
- Check accelerometer connection (I2C3: PC0/PC1)
- Verify device logs for errors

**High false positive rate?**
- Retrain with more consistent "normal" data
- Ensure mounting is secure (no vibration from mounting)
- Increase similarity threshold in firmware

**Similarity always 0% or 100%?**
- ML model may not be properly trained
- Run RESET command and retrain
- Ensure accelerometer is detecting vibrations

## Success Indicators

You should see in the GUI:
- ✅ Status: "🟠 TRAINING" during learning phase
- ✅ Training iterations counting up
- ✅ Automatic transition to "🟢 NORMAL"
- ✅ Similarity values changing with machine state
- ✅ Red "⚠ ANOMALY" when fault introduced

Example successful session:
```
[Connected to COM7]
ML State: TRAINING
Training iteration 1
Training iteration 2
...
Training iteration 40
Training complete - switching to inference
ML State: INFERENCING
Similarity: 97% (NORMAL)
Similarity: 95% (NORMAL)
Similarity: 42% (ANOMALY DETECTED!)
```

Congratulations! Your predictive maintenance system is working! 🎉

## Next Steps

1. **Test thoroughly** with various fault conditions
2. **Determine optimal threshold** for your specific machine (default: 90%)
3. **Measure power consumption** for battery life estimation
4. **Build portable .exe** for field deployment
5. **Read the full documentation** in README.md

Ready to deploy in production! 🚀
