# Review Fixes for PR #6

This document summarizes all the fixes made to address review comments on PR #6.

## Fixed Issues

### 1. src/main.c line 179 - Document hardcoded sector sizes
**Issue:** Hardcoded sector_size and sector_count override flash page info
**Fix:** Added comment explaining why hardcoded values are necessary despite retrieving page info

### 2. boards/nucleo_l412rb_p.overlay line 58 - Fix flash partition boundary
**Issue:** Storage partition ends at 0x20000 (one byte beyond 128KB boundary)
**Fix:** Changed size from 0xC000 (48KB) to 0xBC00 (47,104 bytes) to end at 0x1FFFF

### 3. src/main.c line 490 - Update error message
**Issue:** Message says "Power cycle" but RESET command exists
**Fix:** Changed to mention both RESET command and power cycle options

### 4. scripts/pulse_monitor.py line 295 - Reduce polling interval  
**Issue:** STATUS polling at 250ms is too aggressive
**Fix:** Changed from 250ms to 1000ms (1 second) to reduce serial traffic

### 5. src/main.c lines 814-819 - Define similarity threshold constants
**Issue:** Magic numbers 90 and 70 hardcoded
**Fix:** Added #define constants for thresholds

### 6. scripts/pulse_monitor.py line 250 - Define similarity threshold constant
**Issue:** Hardcoded threshold 90 duplicated from firmware
**Fix:** Added SIMILARITY_NORMAL_THRESHOLD constant at module level

### 7. docs/flash_storage.md lines 48-49 - Fix documentation
**Issue:** Documentation incorrectly states 16KB at 0x1C000-0x1FFFF
**Fix:** Updated to show correct 48KB partition at 0x14000-0x1FFFF (actual: 0x14000-0x1FBFF)

### 8. scripts/create_icon.py line 4 - Remove unused import
**Issue:** ImageFont is imported but not used
**Fix:** Removed ImageFont from import statement

### 9. scripts/pulse_monitor.py line 7 - Remove unused import  
**Issue:** time module imported but not used
**Fix:** Removed time from import statement
