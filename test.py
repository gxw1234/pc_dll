import ctypes
import time
import atexit
from ctypes import Structure, c_ushort, c_ubyte, c_char, POINTER, c_int, create_string_buffer

# 定义设备信息结构体
class DeviceInfo(Structure):
    _fields_ = [
        ("vid", c_ushort),
        ("pid", c_ushort),
        ("serial", c_char * 64),
        ("bus", c_ubyte),
        ("address", c_ubyte)
    ]

# 全局变量
g_device_serial = None
g_dll = None

def cleanup():
    """确保在程序退出时关闭设备"""
    global g_device_serial, g_dll
    if g_device_serial and g_dll:
        try:
            g_dll.USB_CloseDevice(g_device_serial)
        except:
            pass

# 注册清理函数
atexit.register(cleanup)

def main():
    global g_device_serial, g_dll
    try:
        # 加载DLL
        g_dll = ctypes.CDLL("./usb_api.dll")

        # 定义函数原型
        g_dll.USB_ScanDevice.argtypes = [POINTER(DeviceInfo), c_int]
        g_dll.USB_ScanDevice.restype = c_int

        g_dll.USB_OpenDevice.argtypes = [ctypes.c_char_p]
        g_dll.USB_OpenDevice.restype = c_int

        g_dll.USB_CloseDevice.argtypes = [ctypes.c_char_p]
        g_dll.USB_CloseDevice.restype = c_int

        g_dll.USB_ReadData.argtypes = [ctypes.c_char_p, POINTER(c_ubyte), c_int]
        g_dll.USB_ReadData.restype = c_int

        # 扫描设备
        devices = (DeviceInfo * 10)()
        count = g_dll.USB_ScanDevice(devices, 10)
        
        if count <= 0:
            print("No devices found")
            return
        
        print(f"Found {count} devices:")
        for i in range(count):
            serial_str = bytes(devices[i].serial).decode('ascii').strip('\0')
            print(f"Device {i + 1}:")
            print(f"  VID: 0x{devices[i].vid:04X}")
            print(f"  PID: 0x{devices[i].pid:04X}")
            print(f"  Serial: {serial_str}")
            print(f"  Bus: {devices[i].bus}")
            print(f"  Address: {devices[i].address}")
            print("-------------------")
        
        # 保存第一个设备的序列号
        g_device_serial = create_string_buffer(bytes(devices[0].serial))
        
        # 打开设备
        if g_dll.USB_OpenDevice(g_device_serial) < 0:
            print("Failed to open device")
            return
        
        print("Successfully opened device")
        print("Waiting 5 seconds to accumulate data...")
        time.sleep(5)
        
        # 创建接收缓冲区
        buffer = (c_ubyte * 200)()
        
        # 第一次读取10个字节
        print("\nFirst read (10 bytes):")
        read_count = g_dll.USB_ReadData(g_device_serial, buffer, 200)
        print(f'read_count:{read_count}')
        if read_count > 0:
            print(f"Received ({read_count} bytes):", end=" ")
            for i in range(read_count):
                print(f"{buffer[i]:02X}", end=" ")
            print()
        else:
            print("No data received")
            
        print("\nWaiting 2 seconds...")
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    main()
