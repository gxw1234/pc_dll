@echo off
chcp 65001 >nul
gcc -finput-charset=UTF-8 -fexec-charset=UTF-8 usb_test.c -o usb_test.exe libusb-1.0.dll
