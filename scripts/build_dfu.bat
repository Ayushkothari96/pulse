@echo off
cd C:\zephyrproject

REM Build MCUboot
echo Building MCUboot...
west build -p -b nucleo_l412rb_p bootloader/mcuboot/boot/zephyr -d build-mcuboot
west flash -d build-mcuboot

REM Build application
echo Building Application...
west build -p -b nucleo_l412rb_p engine-analyser -d build-app

REM Sign application
echo Signing Application...
python bootloader/mcuboot/scripts/imgtool.py sign ^
    --key bootloader/mcuboot/scripts/root-rsa-2048.pem ^
    --align 8 ^
    --version 1.0.0 ^
    --header-size 0x200 ^
    --slot-size 0x32000 ^
    --pad ^
    build-app/zephyr/zephyr.bin ^
    build-app/zephyr/signed-zephyr.dfu