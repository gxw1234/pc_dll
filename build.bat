@echo off
set GCC=D:\tool\cmake\cmake_zip\mingw64\bin\gcc.exe

"%GCC%" -c usb_api.c -o usb_api.o
"%GCC%" -shared usb_api.o -o usb_api.dll libusb-1.0.dll
"%GCC%" usb_test.c -o usb_test.exe usb_api.dll libusb-1.0.dll
