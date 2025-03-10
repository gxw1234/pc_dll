# -*- coding: utf-8 -*-
import ctypes
from ctypes import *
import time
import sys

# 设置控制台输出编码
if sys.platform == 'win32':
    import codecs
    sys.stdout = codecs.getwriter('utf-8')(sys.stdout.buffer, 'strict')
    sys.stderr = codecs.getwriter('utf-8')(sys.stderr.buffer, 'strict')

# 定义设备信息结构体
class DeviceInfo(Structure):
    _fields_ = [
        ("vid", c_ushort),
        ("pid", c_ushort),
        ("serial_number", c_char * 64),
        ("bus_number", c_ubyte),
        ("device_address", c_ubyte)
    ]

# 加载DLL
usb_dll = CDLL("./usb_api.dll")

# 定义函数原型
usb_dll.USB_ScanDevice.argtypes = [POINTER(DeviceInfo), c_int]
usb_dll.USB_ScanDevice.restype = c_int

usb_dll.USB_OpenDevice.argtypes = [c_char_p]
usb_dll.USB_OpenDevice.restype = c_int

usb_dll.USB_CloseDevice.argtypes = [c_char_p]
usb_dll.USB_CloseDevice.restype = c_int

usb_dll.USB_ReadData.argtypes = [c_char_p, POINTER(c_ubyte), c_int]
usb_dll.USB_ReadData.restype = c_int

# 错误码定义
USB_SUCCESS = 0
USB_ERROR_NOT_FOUND = -1
USB_ERROR_ACCESS = -2
USB_ERROR_IO = -3
USB_ERROR_INVALID = -4
USB_ERROR_BUSY = -5
USB_ERROR_TIMEOUT = -6
USB_ERROR_OVERFLOW = -7
USB_ERROR_PIPE = -8
USB_ERROR_INTERRUPTED = -9
USB_ERROR_NO_MEM = -10
USB_ERROR_NOT_SUPPORTED = -11

def get_error_string(error_code):
    error_dict = {
        USB_SUCCESS: "成功",
        USB_ERROR_NOT_FOUND: "设备未找到",
        USB_ERROR_ACCESS: "访问权限错误",
        USB_ERROR_IO: "IO错误",
        USB_ERROR_INVALID: "无效参数",
        USB_ERROR_BUSY: "设备忙",
        USB_ERROR_TIMEOUT: "超时",
        USB_ERROR_OVERFLOW: "溢出",
        USB_ERROR_PIPE: "管道错误",
        USB_ERROR_INTERRUPTED: "被中断",
        USB_ERROR_NO_MEM: "内存不足",
        USB_ERROR_NOT_SUPPORTED: "不支持的操作"
    }
    return error_dict.get(error_code, f"未知错误: {error_code}")

def main():
    # 扫描设备
    print("正在扫描USB设备...")
    devices = (DeviceInfo * 16)()
    count = usb_dll.USB_ScanDevice(devices, 16)
    
    if count < 0:
        print(f"扫描设备失败: {get_error_string(count)}")
        return
    
    print(f"找到 {count} 个设备:")
    target_serial = None
    
    for i in range(count):
        device = devices[i]
        print(f"\n设备 {i + 1}:")
        print(f"  VID: 0x{device.vid:04X}")
        print(f"  PID: 0x{device.pid:04X}")
        print(f"  序列号: {device.serial_number.decode('utf-8')}")
        print(f"  总线号: {device.bus_number}")
        print(f"  设备地址: {device.device_address}")
        
        # 保存第一个设备的序列号用于测试
        if i == 0:
            target_serial = device.serial_number
    
    if not target_serial:
        print("未找到任何设备")
        return
    
    # 打开设备
    print(f"\n尝试打开设备 {target_serial.decode('utf-8')}...")
    result = usb_dll.USB_OpenDevice(target_serial)
    if result != USB_SUCCESS:
        print(f"打开设备失败: {get_error_string(result)}")
        return
    print("设备打开成功")
    
    try:
        # 读取数据
        print("\n开始读取数据...")
        buffer = (c_ubyte * 64)()
        for _ in range(5):  # 读取5次数据
            result = usb_dll.USB_ReadData(target_serial, buffer, 64)
            if result < 0:
                print(f"读取数据失败: {get_error_string(result)}")
                break
            
            # 打印接收到的数据
            data = bytes(buffer[:result])
            print(f"收到 {result} 字节数据:", end=" ")
            print(" ".join([f"{b:02X}" for b in data]))
            
            time.sleep(1)  # 等待1秒再次读取
    
    finally:
        # 关闭设备
        print("\n关闭设备...")
        result = usb_dll.USB_CloseDevice(target_serial)
        if result != USB_SUCCESS:
            print(f"关闭设备失败: {get_error_string(result)}")
        else:
            print("设备已关闭")

if __name__ == "__main__":
    main()
