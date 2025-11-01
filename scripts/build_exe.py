# Pulse Device Monitor - Standalone Executable Builder
# This script creates a standalone .exe file using PyInstaller

import subprocess
import sys
import os

def check_pyinstaller():
    """Check if PyInstaller is installed"""
    try:
        import PyInstaller
        return True
    except ImportError:
        return False

def install_pyinstaller():
    """Install PyInstaller"""
    print("Installing PyInstaller...")
    subprocess.check_call([sys.executable, "-m", "pip", "install", "pyinstaller"])

def build_exe():
    """Build the standalone executable"""
    print("\n" + "="*50)
    print("  Building Pulse Monitor Executable")
    print("="*50 + "\n")
    
    # Check for icon file
    icon_path = "pulse_icon.ico"
    if os.path.exists(icon_path):
        print(f"✓ Using icon: {icon_path}\n")
        icon_arg = f"--icon={icon_path}"
    else:
        print("⚠ No icon file found. Run 'python create_icon.py' to create one.\n")
        icon_arg = "--icon=NONE"
    
    # PyInstaller command
    cmd = [
        "pyinstaller",
        "--onefile",                    # Single executable file
        "--windowed",                   # No console window (GUI only)
        "--name=PulseMonitor",          # Name of the executable
        icon_arg,                       # Icon file
        "--clean",                      # Clean PyInstaller cache
        "pulse_monitor.py"
    ]
    
    print(f"Running: {' '.join(cmd)}\n")
    subprocess.check_call(cmd)
    
    print("\n" + "="*50)
    print("  Build Complete!")
    print("="*50)
    print("\nYour executable is located at:")
    print("  dist\\PulseMonitor.exe")
    print("\nYou can copy this file to any Windows system and run it.")
    print("No Python installation required on the target system!")
    print("="*50 + "\n")

if __name__ == "__main__":
    script_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(script_dir)
    
    print("Pulse Device Monitor - Executable Builder")
    print("="*50 + "\n")
    
    # Check and install PyInstaller if needed
    if not check_pyinstaller():
        print("PyInstaller not found. Installing...")
        try:
            install_pyinstaller()
            print("PyInstaller installed successfully!\n")
        except subprocess.CalledProcessError:
            print("\nERROR: Failed to install PyInstaller")
            print("Please install manually: pip install pyinstaller")
            sys.exit(1)
    
    # Build the executable
    try:
        build_exe()
    except subprocess.CalledProcessError:
        print("\nERROR: Build failed")
        sys.exit(1)
    except Exception as e:
        print(f"\nERROR: {e}")
        sys.exit(1)
