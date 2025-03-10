@echo off
gcc -c usb_api.c -o usb_api.o -DUSB_API_EXPORTS
gcc -shared -o usb_api.dll usb_api.o -L. -llibusb-1.0
gcc usb_test.c -o usb_test.exe -L. -llibusb-1.0
