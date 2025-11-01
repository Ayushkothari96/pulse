@echo off
REM Pulse Device Monitor Launcher
REM This script automatically runs the Python GUI application

echo ========================================
echo   Pulse Device Monitor - Launcher
echo ========================================
echo.

REM Check if Python is installed
python --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python is not installed or not in PATH
    echo Please install Python from https://www.python.org/
    pause
    exit /b 1
)

REM Check if pyserial is installed
python -c "import serial" >nul 2>&1
if errorlevel 1 (
    echo Installing required package: pyserial
    python -m pip install pyserial
    if errorlevel 1 (
        echo ERROR: Failed to install pyserial
        pause
        exit /b 1
    )
)

REM Run the application
echo Starting Pulse Device Monitor...
echo.
python "%~dp0pulse_monitor.py"

REM If the script exits with an error, keep window open
if errorlevel 1 (
    echo.
    echo ERROR: Application exited with an error
    pause
)
