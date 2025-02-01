REM Create output directory
mkdir build-combined 2>nul

REM Convert signed binary to hex with correct offset
arm-none-eabi-objcopy --change-addresses 0x0800C000 --input-target=binary --output-target=ihex build-app\zephyr\signed-zephyr.bin build-app\zephyr\signed-zephyr.hex

REM Merge hex files using Windows commands
echo Merging hex files...
copy /b build-mcuboot\zephyr\zephyr.hex + build-app\zephyr\signed-zephyr.hex build-combined\merged-image.hex

REM Flash combined image
echo Flashing combined image...
west flash --hex-file build-combined/merged-image.hex --build-dir build-app