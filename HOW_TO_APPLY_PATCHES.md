# How to Apply the Review Fix Patches

This guide explains how to apply the patches that fix all 9 review comments from PR #6.

## Prerequisites

- You need to be on the PR #6 branch: `feature/implement-nude-usb-console-for-reduced-memory`
- All patch files should be in the repository root directory

## Quick Method: Apply All Patches at Once

From the repository root directory, run:

```bash
# Switch to the PR #6 branch
git checkout feature/implement-nude-usb-console-for-reduced-memory

# Apply all patches
git apply main.c.patch
git apply boards_overlay.patch
git apply pulse_monitor.patch
git apply create_icon.patch
git apply flash_storage_md.patch

# Check what changed
git status
git diff

# If everything looks good, commit the changes
git add -A
git commit -m "Fix all review comments from PR #6

- Fix flash partition boundary overflow (0xBC00 instead of 0xC000)
- Add similarity threshold constants
- Document hardcoded NVS sector sizes
- Update error message to mention RESET command
- Reduce polling interval from 250ms to 1000ms
- Fix documentation to match implementation
- Remove unused imports
"

# Push the changes
git push origin feature/implement-nude-usb-console-for-reduced-memory
```

## Manual Method: Apply Each Patch Individually

If you prefer to apply patches one at a time to review each change:

### 1. Fix src/main.c

```bash
git apply main.c.patch
git diff src/main.c
# Review the changes, then:
git add src/main.c
```

### 2. Fix boards/nucleo_l412rb_p.overlay

```bash
git apply boards_overlay.patch
git diff boards/nucleo_l412rb_p.overlay
# Review the changes, then:
git add boards/nucleo_l412rb_p.overlay
```

### 3. Fix scripts/pulse_monitor.py

```bash
git apply pulse_monitor.patch
git diff scripts/pulse_monitor.py
# Review the changes, then:
git add scripts/pulse_monitor.py
```

### 4. Fix scripts/create_icon.py

```bash
git apply create_icon.patch
git diff scripts/create_icon.py
# Review the changes, then:
git add scripts/create_icon.py
```

### 5. Fix docs/flash_storage.md

```bash
git apply flash_storage_md.patch
git diff docs/flash_storage.md
# Review the changes, then:
git add docs/flash_storage.md
```

### 6. Commit all changes

```bash
git commit -m "Fix all review comments from PR #6"
git push origin feature/implement-nude-usb-console-for-reduced-memory
```

## Alternative: Manual Application

If the patches don't apply cleanly (e.g., due to conflicts), you can apply the changes manually by following the detailed instructions in `IMPLEMENTATION_GUIDE.md`.

The guide shows exactly what to change in each file, line by line.

## Troubleshooting

### Error: "patch does not apply"

If you get this error, it means the files have changed since the patches were created. Options:

1. **Check your branch**: Make sure you're on the correct branch
   ```bash
   git branch --show-current
   # Should show: feature/implement-nude-usb-console-for-reduced-memory
   ```

2. **Check for uncommitted changes**: Stash them first
   ```bash
   git stash
   git apply <patch_file>
   git stash pop
   ```

3. **Apply manually**: Use `IMPLEMENTATION_GUIDE.md` to make changes by hand

### Error: "file not found"

If a file doesn't exist, the patch is for a different branch. Make sure you're on the PR #6 branch.

### Verify the Patches Before Applying

You can preview what each patch will do:

```bash
# Preview main.c changes
git apply --stat main.c.patch

# Check if patch can be applied cleanly
git apply --check main.c.patch
```

## What Gets Changed

Here's a summary of what each patch does:

- **main.c.patch**: Adds constants, improves comments, updates error message
- **boards_overlay.patch**: Fixes flash partition size (0xBC00 instead of 0xC000)
- **pulse_monitor.patch**: Removes unused import, adds constant, reduces polling
- **create_icon.patch**: Removes unused ImageFont import
- **flash_storage_md.patch**: Corrects documentation to match actual partition sizes

## After Applying

Once all patches are applied and committed:

1. Build the firmware to verify no errors:
   ```bash
   west build -b nucleo_l412rb_p --pristine
   ```

2. Test the GUI:
   ```bash
   cd scripts
   python pulse_monitor.py
   ```

3. Verify the changes resolved the review comments

## Need More Help?

- See `IMPLEMENTATION_GUIDE.md` for detailed, line-by-line instructions
- See `REVIEW_FIXES.md` for a summary of what was fixed
- See `SUMMARY.md` for the complete overview
