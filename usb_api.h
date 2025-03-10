#ifndef USB_API_H
#define USB_API_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
    #ifdef USB_EXPORTS
        #define USB_API __declspec(dllexport)
    #else
        #define USB_API __declspec(dllimport)
    #endif
#else
    #define USB_API
#endif

// 目标设备的 VID 和 PID
#define VENDOR_ID   0x1733
#define PRODUCT_ID  0xAABB

// 设备信息结构体
typedef struct {
    unsigned short vid;          // 厂商ID
    unsigned short pid;          // 产品ID
    char serial_number[64];      // 序列号
    unsigned char bus_number;    // 总线号
    unsigned char device_address;// 设备地址
} device_info_t;

// 函数返回值定义
#define USB_SUCCESS             0    // 成功
#define USB_ERROR_NOT_FOUND    -1    // 设备未找到
#define USB_ERROR_ACCESS       -2    // 访问权限错误
#define USB_ERROR_IO          -3    // IO错误
#define USB_ERROR_INVALID     -4    // 无效参数
#define USB_ERROR_BUSY        -5    // 设备忙
#define USB_ERROR_TIMEOUT     -6    // 超时
#define USB_ERROR_OVERFLOW    -7    // 溢出
#define USB_ERROR_PIPE        -8    // 管道错误
#define USB_ERROR_INTERRUPTED -9    // 被中断
#define USB_ERROR_NO_MEM      -10   // 内存不足
#define USB_ERROR_NOT_SUPPORTED -11 // 不支持的操作

/**
 * @brief 扫描USB设备
 * @param devices 设备信息数组，用于存储扫描到的设备信息
 * @param max_devices 最大设备数量
 * @return 成功返回扫描到的设备数量，失败返回错误码
 */
USB_API int USB_ScanDevice(device_info_t* devices, int max_devices);

/**
 * @brief 打开USB设备
 * @param target_serial 目标设备序列号
 * @return 成功返回USB_SUCCESS，失败返回错误码
 */
USB_API int USB_OpenDevice(const char* target_serial);

/**
 * @brief 关闭USB设备
 * @param target_serial 目标设备序列号
 * @return 成功返回USB_SUCCESS，失败返回错误码
 */
USB_API int USB_CloseDevice(const char* target_serial);

/**
 * @brief 读取数据
 * @param target_serial 目标设备序列号
 * @param data 数据缓冲区
 * @param length 要读取的数据长度
 * @return 成功返回实际读取的数据长度，失败返回错误码
 */
USB_API int USB_ReadData(const char* target_serial, unsigned char* data, int length);

#ifdef __cplusplus
}
#endif

#endif // USB_API_H
