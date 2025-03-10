#ifndef USB_API_H
#define USB_API_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef USB_API_EXPORTS
#define USB_API __declspec(dllexport)
#else
#define USB_API __declspec(dllimport)
#endif

// 设备信息结构体
typedef struct {
    unsigned short vid;          // 厂商ID
    unsigned short pid;          // 产品ID
    char serial[64];            // 序列号
    unsigned char bus;          // 总线号
    unsigned char address;      // 设备地址
    char manufacturer[64];      // 制造商字符串
    char product[64];          // 产品字符串
} device_info_t;

// 初始化USB系统
USB_API int USB_Init(void);

// 扫描设备
USB_API int USB_ScanDevice(device_info_t* devices, int max_devices);

// 打开设备
USB_API int USB_OpenDevice(const char* target_serial);

// 读取数据
USB_API int USB_ReadData(const char* target_serial, unsigned char* data, int length);

// 关闭设备
USB_API int USB_CloseDevice(const char* target_serial);

// 清理USB系统
USB_API void USB_Cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // USB_API_H
