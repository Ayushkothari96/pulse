#!/usr/bin/env python3

try:
    from intelhex import IntelHex
except ImportError:
    import subprocess
    print("Installing intelhex package...")
    subprocess.check_call(['pip', 'install', 'intelhex'])
    from intelhex import IntelHex

import os

# Create build-combined directory if it doesn't exist
os.makedirs("build-combined", exist_ok=True)

# Load hex files
print("Loading bootloader hex...")
bootloader = IntelHex("build-mcuboot/zephyr/zephyr.hex")
print("Loading application hex...")
app = IntelHex("build-app/zephyr/signed-zephyr.hex")

# Merge files
print("Merging hex files...")
bootloader.merge(app)

# Write combined hex
print("Writing combined hex file...")
bootloader.write_hex_file("build-combined/merged-image.hex")
print("Merge complete!")