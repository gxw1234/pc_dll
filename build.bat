@echo off
chcp 65001
set INCLUDE_DIR=-I.
set DEFINES=-DUSB_EXPORTS
gcc %INCLUDE_DIR% %DEFINES% -c usb_api.c -o usb_api.o
gcc -shared -o usb_api.dll usb_api.o -Wl,--out-implib,libusb_api.a
