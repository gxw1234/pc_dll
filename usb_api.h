#ifndef USB_API_H
#define USB_API_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
    #define USB_API __declspec(dllexport)
#else
    #define USB_API
#endif

// 设备信息结构体
typedef struct {
    unsigned short vid;        // 厂商ID
    unsigned short pid;        // 产品ID
    char serial[64];          // 序列号
    unsigned char bus;        // USB总线号
    unsigned char address;    // 设备地址
} device_info_t;

// 扫描设备，返回找到的设备数量
USB_API int USB_ScanDevice(device_info_t* devices, int max_devices);

// 打开指定序列号的设备，返回0表示成功
USB_API int USB_OpenDevice(const char* target_serial);

// 关闭指定序列号的设备，返回0表示成功
USB_API int USB_CloseDevice(const char* target_serial);

// 读取数据，返回实际读取的字节数，小于0表示错误
USB_API int USB_ReadData(const char* target_serial, unsigned char* data, int length);

// 清空缓冲区，返回0表示成功
USB_API int USB_ClearBuffer(const char* target_serial);

#ifdef __cplusplus
}
#endif

#endif // USB_API_H
