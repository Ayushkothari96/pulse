# Pulse Device - USB Shell Interface Guide

## Overview

The Pulse device now uses **Zephyr's built-in Shell** over USB CDC-ACM for user interaction. This provides a professional command-line interface with:
- ✅ Tab completion
- ✅ Command history (arrow keys)
- ✅ Built-in help system
- ✅ Color support
- ✅ Wildcards and auto-completion

## Quick Start

### 1. Build and Flash

```powershell
cd c:\development\zephyr\zephyrproject\pulse
west build -b nucleo_l412rb_p --pristine
west flash
```

### 2. Connect via USB

1. Connect the device USB port to your PC
2. Find the COM port in Device Manager (Windows) or `/dev/ttyACM0` (Linux)
3. Open a serial terminal at **115200 baud**

### 3. Use Terminal

**Windows - PuTTY:**
- Connection type: Serial
- Serial line: COM3 (your port)
- Speed: 115200
- Terminal > Keyboard: Enable VT100 function keys

**Linux/macOS:**
```bash
# Using screen
screen /dev/ttyACM0 115200

# Using minicom
minicom -D /dev/ttyACM0 -b 115200

# Using picocom
picocom -b 115200 /dev/ttyACM0
```

## Available Commands

### Machine Learning Commands

#### `ml state`
Get the current ML state

```
pulse:~$ ml state
ML State: training (1)
Training iteration: 15/40
```

**States:**
- `idle (0)` - Not processing data
- `training (1)` - Learning normal behavior
- `inferencing (2)` - Detecting anomalies

---

#### `ml set <state>`
Set the ML state

```
pulse:~$ ml set 2
ML state set to: inferencing

pulse:~$ ml set 0
ML state set to: idle
```

**Parameters:**
- `0` - Set to idle
- `1` - Set to training (resets iteration counter)
- `2` - Set to inferencing

---

#### `ml info`
Get detailed ML information

```
pulse:~$ ml info
=== ML Information ===
Current state:      inferencing
Training iteration: 40/40
Last similarity:    95%
Status:             NORMAL
```

**Status interpretation:**
- `NORMAL` - Similarity ≥ 90%
- `WARNING` - Similarity 70-89%
- `ANOMALY!` - Similarity < 70%

---

### Device Commands

#### `device`
Get device information

```
pulse:~$ device
=== Device Information ===
Name:         Pulse
Manufacturer: Kothari
Version:      1.0.0
Board:        nucleo_l412rb_p
Uptime:       123456 ms
```

---

#### `accel`
Get accelerometer information

```
pulse:~$ accel
=== Accelerometer Information ===
Sample rate:  500 Hz
Buffer size:  256 samples
Status:       running
```

---

### Built-in Shell Commands

#### `help`
Show all available commands

```
pulse:~$ help
Please press the <Tab> button to see all available commands.
You can also use the <Tab> button to prompt or auto-complete all commands or its subcommands.
You can try to call commands with <-h> or <--help> parameter for more information.

Shell supports following meta-keys:
  Ctrl+a, Ctrl+b, Ctrl+c, Ctrl+d, Ctrl+e, Ctrl+f, Ctrl+k, Ctrl+l, Ctrl+n, Ctrl+p, Ctrl+u, Ctrl+w
  Alt+b, Alt+f.
```

#### `clear`
Clear the terminal screen

```
pulse:~$ clear
```

#### `history`
Show command history

```
pulse:~$ history
  0  ml state
  1  ml set 2
  2  ml info
  3  device
```

---

## Shell Features

### Tab Completion
Press `Tab` to auto-complete commands:
```
pulse:~$ ml <Tab>
state  set  info
```

### Command History
Use arrow keys to navigate command history:
- `↑` - Previous command
- `↓` - Next command

### Shortcuts
- `Ctrl+C` - Clear current line
- `Ctrl+L` - Clear screen (same as `clear`)
- `Ctrl+A` - Move to beginning of line
- `Ctrl+E` - Move to end of line
- `Ctrl+U` - Clear line before cursor
- `Ctrl+K` - Clear line after cursor

---

## Example Use Cases

### Monitor Training Progress

```powershell
pulse:~$ ml set 1
ML state set to: training

# Check progress periodically
pulse:~$ ml state
ML State: training (1)
Training iteration: 10/40

# ... wait ...

pulse:~$ ml state
ML State: training (1)
Training iteration: 25/40

# ... wait ...

pulse:~$ ml state
ML State: inferencing (2)
Last similarity: 98%
```

### Monitor Anomaly Detection

```powershell
pulse:~$ ml set 2
ML state set to: inferencing

# Check similarity periodically
pulse:~$ ml info
=== ML Information ===
Current state:      inferencing
Training iteration: 40/40
Last similarity:    95%
Status:             NORMAL

# ... wait and check again ...

pulse:~$ ml info
=== ML Information ===
Current state:      inferencing
Training iteration: 40/40
Last similarity:    65%
Status:             ANOMALY!
```

### Switch Between Modes

```powershell
# Stop processing
pulse:~$ ml set 0
ML state set to: idle

# Start training again
pulse:~$ ml set 1
ML state set to: training

# Check status
pulse:~$ ml state
ML State: training (1)
Training iteration: 0/40
```

---

## Python Script Integration

You can also control the device programmatically using PySerial:

```python
import serial
import time

# Connect to device
ser = serial.Serial('COM3', 115200, timeout=1)

def send_command(cmd):
    """Send command and read response"""
    ser.write((cmd + '\n').encode())
    time.sleep(0.2)
    
    # Read all available data
    response = []
    while ser.in_waiting:
        line = ser.readline().decode('utf-8', errors='ignore')
        response.append(line.strip())
    
    return '\n'.join(response)

# Example: Get ML state
print(send_command('ml state'))

# Example: Set to inferencing mode
print(send_command('ml set 2'))

# Example: Get ML info
print(send_command('ml info'))

# Close connection
ser.close()
```

---

## Adding Custom Commands

To add your own commands, add them to `src/main.c`:

```c
/* Your command handler */
static int cmd_my_feature(const struct shell *sh, size_t argc, char **argv)
{
    shell_print(sh, "My feature value: %d", my_value);
    return 0;
}

/* Register command */
SHELL_CMD_REGISTER(myfeature, NULL, "Description of my feature", cmd_my_feature);
```

For subcommands:

```c
/* Subcommand handlers */
static int cmd_my_get(const struct shell *sh, size_t argc, char **argv)
{
    shell_print(sh, "Value: %d", value);
    return 0;
}

static int cmd_my_set(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_error(sh, "Usage: my set <value>");
        return -EINVAL;
    }
    value = atoi(argv[1]);
    shell_print(sh, "Value set to: %d", value);
    return 0;
}

/* Create subcommand set */
SHELL_STATIC_SUBCMD_SET_CREATE(my_cmds,
    SHELL_CMD(get, NULL, "Get value", cmd_my_get),
    SHELL_CMD(set, NULL, "Set value", cmd_my_set),
    SHELL_SUBCMD_SET_END
);

/* Register parent command */
SHELL_CMD_REGISTER(my, &my_cmds, "My feature commands", NULL);
```

---

## Troubleshooting

### No Shell Prompt

**Issue:** Device connects but no prompt appears

**Solutions:**
1. Press `Enter` to get prompt
2. Type `clear` and press Enter
3. Check baud rate is 115200
4. Disable flow control in terminal
5. Try different terminal program

### Commands Not Working

**Issue:** Commands typed but no response

**Solutions:**
1. Ensure line ending is set to `LF` or `CR+LF`
2. Check keyboard input is being sent
3. Press `Ctrl+C` to clear any stuck state
4. Disconnect and reconnect

### Garbled Text

**Issue:** Strange characters or formatting issues

**Solutions:**
1. Enable VT100/ANSI terminal emulation
2. Set terminal to UTF-8 encoding
3. Ensure baud rate is 115200
4. Check USB cable quality

### Tab Completion Not Working

**Issue:** Tab key doesn't show completions

**Solutions:**
1. Ensure terminal sends `Tab` character (0x09)
2. Enable VT100 mode in terminal
3. Try different terminal program
4. Check `CONFIG_SHELL_TAB=y` in prj.conf

---

## Technical Details

### Configuration (prj.conf)

```properties
# Zephyr Shell over USB
CONFIG_SHELL=y
CONFIG_SHELL_BACKEND_SERIAL=y
CONFIG_SHELL_PROMPT_UART="pulse:~$ "

# Shell features
CONFIG_SHELL_TAB=y
CONFIG_SHELL_TAB_AUTOCOMPLETION=y
CONFIG_SHELL_HISTORY=y
CONFIG_SHELL_HELP=y
CONFIG_SHELL_VT100_COLORS=y
```

### Memory Usage

- Shell command buffers: ~256 bytes
- Shell history: ~512 bytes
- Command handlers: Minimal (stack-based)
- Total overhead: ~2-3 KB

### Performance

- No polling required - event-driven
- Negligible CPU usage when idle
- Fast command response (< 1ms)
- No impact on ML processing

---

## Advantages Over Custom Protocol

✅ **No custom parsing code** - Uses Zephyr's robust parser  
✅ **Built-in help system** - Users can discover commands  
✅ **Tab completion** - Easier to use  
✅ **Command history** - More efficient workflow  
✅ **Color support** - Better UX  
✅ **Well tested** - Zephyr shell is production-quality  
✅ **Extensible** - Easy to add new commands  
✅ **Standard** - Familiar to embedded developers  

---

## Resources

- [Zephyr Shell Documentation](https://docs.zephyrproject.org/latest/services/shell/index.html)
- [Shell API Reference](https://docs.zephyrproject.org/latest/doxygen/html/group__shell.html)
- PuTTY: https://www.putty.org/
- TeraTerm: https://ttssh2.osdn.jp/
