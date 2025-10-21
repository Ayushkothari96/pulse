# Quick Start - USB Shell Interface

## What You Get

A professional command-line interface over USB using Zephyr's built-in Shell:
- ✅ Tab completion
- ✅ Command history
- ✅ Color terminal
- ✅ Built-in help
- ✅ Easy to extend

## Build & Flash (3 commands)

```powershell
cd c:\development\zephyr\zephyrproject\pulse
west build -b nucleo_l412rb_p --pristine
west flash
```

## Connect & Test

**Option 1: PuTTY (Windows)**
1. Download PuTTY: https://www.putty.org/
2. Connection type: Serial
3. Serial line: COM3 (check Device Manager)
4. Speed: 115200
5. Click "Open"
6. Press Enter to see: `pulse:~$`

**Option 2: Python Script**
```powershell
python scripts\test_shell.py COM3
```

**Option 3: Linux/macOS**
```bash
screen /dev/ttyACM0 115200
```

## Try These Commands

```bash
pulse:~$ help              # Show all commands
pulse:~$ ml state          # Check ML state
pulse:~$ ml info           # Detailed ML info
pulse:~$ device            # Device information
pulse:~$ accel             # Accelerometer info

# Change ML mode
pulse:~$ ml set 0          # Set to idle
pulse:~$ ml set 1          # Set to training
pulse:~$ ml set 2          # Set to inferencing

# Use tab completion
pulse:~$ ml <Tab>          # Shows: state, set, info
```

## Monitor Training

```bash
pulse:~$ ml set 1
ML state set to: training

pulse:~$ ml state
ML State: training (1)
Training iteration: 15/40

# Wait and check again...
pulse:~$ ml state
ML State: inferencing (2)
Last similarity: 98%
```

## Monitor Anomalies

```bash
pulse:~$ ml set 2
ML state set to: inferencing

pulse:~$ ml info
=== ML Information ===
Current state:      inferencing
Training iteration: 40/40
Last similarity:    95%
Status:             NORMAL

# If anomaly detected:
Last similarity:    65%
Status:             ANOMALY!
```

## Features

- **Tab Completion**: Press Tab to autocomplete
- **History**: Use ↑↓ arrows for command history
- **Help**: Type `help` or `<command> --help`
- **Colors**: Green for success, red for errors
- **Shortcuts**: Ctrl+L (clear), Ctrl+A/E (line start/end)

## Implementation

This uses **Zephyr's built-in Shell** - no custom parsing code needed!

**Total code added**: ~150 lines of command handlers in `main.c`

**Shell handles**:
- Command parsing
- Tab completion
- History management
- Help system
- Terminal control
- Error handling

## Add Your Own Commands

Super simple:

```c
static int cmd_mycommand(const struct shell *sh, size_t argc, char **argv)
{
    shell_print(sh, "Hello from my command!");
    return 0;
}

SHELL_CMD_REGISTER(mycommand, NULL, "My command help", cmd_mycommand);
```

Rebuild, flash, and done!

## Documentation

- **Complete Guide**: `USB_SHELL_GUIDE.md`
- **Implementation Details**: `IMPLEMENTATION_SUMMARY.md`
- **Zephyr Shell Docs**: https://docs.zephyrproject.org/latest/services/shell/index.html

## Troubleshooting

**No prompt?**
- Press Enter
- Check baud rate: 115200
- Check COM port is correct

**Commands not working?**
- Enable line ending: LF or CR+LF
- Try pressing Ctrl+C to reset

**Garbled text?**
- Enable VT100/ANSI mode in terminal
- Set UTF-8 encoding

## Success!

You should see:
```
pulse:~$ ml state
ML State: training (1)
Training iteration: 15/40
```

Enjoy your professional USB interface! 🎉
