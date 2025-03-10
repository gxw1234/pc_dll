import ctypes
from ctypes import Structure, c_ushort, c_ubyte, c_char, POINTER, c_int
import time

# 定义设备信息结构体
class DeviceInfo(Structure):
    _fields_ = [
        ("vid", c_ushort),
        ("pid", c_ushort),
        ("serial", c_char * 64),
        ("bus", c_ubyte),
        ("address", c_ubyte),
        ("manufacturer", c_char * 64),
        ("product", c_char * 64)
    ]

def main():
    try:
        # 加载DLL
        dll = ctypes.CDLL("./usb_api.dll")

        # 定义函数原型
        dll.USB_Init.restype = c_int
        dll.USB_ScanDevice.argtypes = [POINTER(DeviceInfo), c_int]
        dll.USB_ScanDevice.restype = c_int
        dll.USB_OpenDevice.argtypes = [ctypes.c_char_p]
        dll.USB_OpenDevice.restype = c_int
        dll.USB_ReadData.argtypes = [ctypes.c_char_p, POINTER(c_ubyte), c_int]
        dll.USB_ReadData.restype = c_int
        dll.USB_CloseDevice.argtypes = [ctypes.c_char_p]
        dll.USB_CloseDevice.restype = c_int
        dll.USB_Cleanup.restype = None

        print("初始化USB系统...")
        if dll.USB_Init() < 0:
            print("初始化USB系统失败")
            return

        print("\n扫描设备...")
        devices = (DeviceInfo * 10)()
        count = dll.USB_ScanDevice(devices, 10)
        if count <= 0:
            print("未找到设备")
            dll.USB_Cleanup()
            return

        print(f"找到 {count} 个设备:")
        for i in range(count):
            serial_str = bytes(devices[i].serial).decode('ascii').strip('\0')
            manufacturer_str = bytes(devices[i].manufacturer).decode('ascii').strip('\0')
            product_str = bytes(devices[i].product).decode('ascii').strip('\0')
            print(f"设备 {i + 1}:")
            print(f"  VID: 0x{devices[i].vid:04X}")
            print(f"  PID: 0x{devices[i].pid:04X}")
            print(f"  序列号: {serial_str}")
            print(f"  制造商: {manufacturer_str}")
            print(f"  产品名: {product_str}")
            print(f"  总线: {devices[i].bus}")
            print(f"  地址: {devices[i].address}")
            print("-------------------")

        # 打开第一个设备
        print("\n打开第一个设备...")
        device_serial = ctypes.create_string_buffer(bytes(devices[0].serial))
        if dll.USB_OpenDevice(device_serial) < 0:
            print("打开设备失败")
            dll.USB_Cleanup()
            return

        print("成功打开设备")
        print("\n开始读取数据...")
        
        # 读取数据10秒
        buffer = (c_ubyte * 1024)()
        total_bytes = 0
        
        time.sleep(2)
        read_bytes = dll.USB_ReadData(device_serial, buffer, len(buffer))
        if read_bytes > 0:
            print(f"\n收到数据 ({read_bytes} 字节):")
            print(" ".join(f"{b:02X}" for b in buffer[:read_bytes]))
            total_bytes += read_bytes
        time.sleep(0.001)  # 1ms延时

        print(f"\n总共接收: {total_bytes} 字节")

        # 关闭设备
        print("\n关闭设备...")
        dll.USB_CloseDevice(device_serial)
        print("已关闭设备")

        # 清理USB系统
        print("\n清理USB系统...")
        dll.USB_Cleanup()
        print("清理完成")

    except Exception as e:
        print(f"错误: {e}")
        try:
            dll.USB_Cleanup()
        except:
            pass

if __name__ == "__main__":
    main()
