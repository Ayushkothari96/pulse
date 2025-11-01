# Implementation Guide for PR #6 Review Fixes

This guide provides the exact changes needed to fix all 9 review comments on PR #6.

## Summary of Changes

All review comments have been addressed with minimal, surgical changes:

1. ✅ **src/main.c line 179** - Added comment explaining hardcoded sector sizes
2. ✅ **boards/nucleo_l412rb_p.overlay line 58** - Fixed partition size to avoid boundary overflow  
3. ✅ **src/main.c line 490** - Updated error message to mention RESET command
4. ✅ **scripts/pulse_monitor.py line 295** - Changed polling from 250ms to 1000ms
5. ✅ **src/main.c lines 814-819** - Added threshold constants
6. ✅ **scripts/pulse_monitor.py line 250** - Added threshold constant  
7. ✅ **docs/flash_storage.md lines 48-49** - Fixed documentation
8. ✅ **scripts/create_icon.py line 4** - Removed unused ImageFont import
9. ✅ **scripts/pulse_monitor.py line 7** - Removed unused time import

## Detailed Changes

### 1. src/main.c - Add Threshold Constants

**Location:** After line 33 (after LED_PIN definition)

**Add:**
```c
/* Similarity thresholds for anomaly detection */
#define SIMILARITY_NORMAL_THRESHOLD 90    /* >= 90% is normal operation */
#define SIMILARITY_WARNING_THRESHOLD 70   /* 70-89% is warning range */
```

### 2. src/main.c - Document Hardcoded Sector Sizes

**Location:** Lines 199-200 (in nvs_storage_init function)

**Replace:**
```c
    /* STM32L412 flash: Use 2KB sectors for NVS */
    nvs.sector_size = 2048;  /* 2KB sectors (STM32L4 flash page size) */
```

**With:**
```c
    /* STM32L412 flash: Use 2KB sectors for NVS
     * NOTE: Hardcoded values used instead of info.size because NVS requires specific
     * sector alignment. The retrieved page info may not match NVS requirements.
     */
    nvs.sector_size = 2048;  /* 2KB sectors (must match STM32L4 flash page size) */
```

### 3. src/main.c - Update Error Message

**Location:** Line 490

**Replace:**
```c
                        LOG_WRN("Model already trained! Power cycle to retrain from scratch.");
```

**With:**
```c
                        LOG_WRN("Model already trained! Use RESET command or power cycle to retrain from scratch.");
```

### 4. src/main.c - Use Threshold Constants

**Location:** Lines 806-810 (in cmd_status function)

**Replace:**
```c
        if (last_similarity >= 90) {
            printk(" (NORMAL)\n");
        } else if (last_similarity >= 70) {
            printk(" (WARNING)\n");
```

**With:**
```c
        if (last_similarity >= SIMILARITY_NORMAL_THRESHOLD) {
            printk(" (NORMAL)\n");
        } else if (last_similarity >= SIMILARITY_WARNING_THRESHOLD) {
            printk(" (WARNING)\n");
```

### 5. boards/nucleo_l412rb_p.overlay - Fix Partition Size

**Location:** Line 58

**Replace:**
```c
            reg = <0x00014000 0x0000c000>;  /* 48KB = 24 sectors × 2KB */
```

**With:**
```c
            reg = <0x00014000 0x0000bc00>;  /* 0x1FFFF - 0x14000 + 1 = 0xBC00 (47,104 bytes) */
```

**Explanation:** The original 0xC000 (48KB) would make the partition end at 0x20000, which is one byte past the 128KB flash boundary (0x1FFFF). The corrected size ensures the partition stays within the flash boundary.

### 6. scripts/pulse_monitor.py - Remove Unused Import and Add Constant

**Location:** Lines 1-7 (imports section)

**Replace:**
```python
import tkinter as tk
from tkinter import ttk, scrolledtext
import serial
import serial.tools.list_ports
import threading
import queue
import time
import re
```

**With:**
```python
import tkinter as tk
from tkinter import ttk, scrolledtext
import serial
import serial.tools.list_ports
import threading
import queue
import re

# Similarity threshold for anomaly detection (must match firmware)
SIMILARITY_NORMAL_THRESHOLD = 90  # >= 90% is normal operation
```

### 7. scripts/pulse_monitor.py - Use Threshold Constant

**Location:** Line 250 (in update_similarity_display method)

**Replace:**
```python
                if self.last_similarity >= 90:
```

**With:**
```python
                if self.last_similarity >= SIMILARITY_NORMAL_THRESHOLD:
```

### 8. scripts/pulse_monitor.py - Change Polling Interval

**Location:** Line 298 (in poll_status method)

**Replace:**
```python
    def poll_status(self):
        """Automatically poll device status every 250ms"""
        if self.status_polling_active and self.is_connected:
            self.send_command("STATUS")
            # Schedule next poll
            self.root.after(250, self.poll_status)
```

**With:**
```python
    def poll_status(self):
        """Automatically poll device status every 1000ms"""
        if self.status_polling_active and self.is_connected:
            self.send_command("STATUS")
            # Schedule next poll
            self.root.after(1000, self.poll_status)
```

### 9. scripts/create_icon.py - Remove Unused Import

**Location:** Line 4

**Replace:**
```python
from PIL import Image, ImageDraw, ImageFont
```

**With:**
```python
from PIL import Image, ImageDraw
```

### 10. docs/flash_storage.md - Fix Documentation

**Location:** Lines 48-50 (Flash Partition Layout section)

**Replace:**
```
Flash Memory (128KB total)
├─ Application Code (0x00000 - 0x1BFFF): ~112KB
└─ Storage Partition (0x1C000 - 0x1FFFF): 16KB
```

**With:**
```
Flash Memory (128KB total)
├─ Application Code (0x00000 - 0x13FFF): 80KB
└─ Storage Partition (0x14000 - 0x1FBFF): 47,104 bytes (slightly less than 48KB to stay within boundary)
```

**Location:** Line 106

**Replace:**
```
- **Total NVS**: 16KB (8 sectors × 2KB each)
```

**With:**
```
- **Total NVS**: ~46KB (23 sectors × 2KB each, actual partition is 47,104 bytes)
```

**Location:** Lines 202-203 (Memory Usage section)

**Replace:**
```
- **Flash Code**: ~112KB (application + libraries)
- **Flash Storage**: 16KB (NVS partition for 10 samples)
```

**With:**
```
- **Flash Code**: 80KB (application + libraries)
- **Flash Storage**: ~46KB (NVS partition for 10 samples)
```

## Validation

After applying these changes:

1. ✅ Flash partition math is correct: 0x14000 + 0xBC00 = 0x1FC00 (which is < 0x20000)
2. ✅ All magic numbers replaced with named constants
3. ✅ Documentation matches implementation
4. ✅ Unused imports removed
5. ✅ Polling interval reduced to avoid excessive traffic
6. ✅ Error messages provide helpful guidance
7. ✅ Hardcoded values properly documented

## Testing Recommendations

1. Build the firmware to ensure no compilation errors
2. Verify flash partition fits within 128KB boundary
3. Test USB console commands (STATUS, RESET, LOGS)
4. Verify GUI monitor polling works at 1 second interval
5. Check that similarity thresholds are consistent between firmware and GUI
