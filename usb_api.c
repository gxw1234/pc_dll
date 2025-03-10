#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <windows.h>
#include "usb_api.h"

// libusb 基本类型定义
typedef struct libusb_context libusb_context;
typedef struct libusb_device libusb_device;
typedef struct libusb_device_handle libusb_device_handle;

#define LIBUSB_CALL __stdcall

// libusb 枚举定义
enum libusb_transfer_status {
    LIBUSB_TRANSFER_COMPLETED,
    LIBUSB_TRANSFER_ERROR,
    LIBUSB_TRANSFER_TIMED_OUT,
    LIBUSB_TRANSFER_CANCELLED,
    LIBUSB_TRANSFER_STALL,
    LIBUSB_TRANSFER_NO_DEVICE,
    LIBUSB_TRANSFER_OVERFLOW
};

enum libusb_option {
    LIBUSB_OPTION_LOG_LEVEL
};

// libusb 回调函数类型定义
struct libusb_transfer;  // 前向声明
typedef void (LIBUSB_CALL *libusb_transfer_cb_fn)(struct libusb_transfer *transfer);

// libusb 传输结构
struct libusb_transfer {
    libusb_device_handle *dev_handle;
    uint8_t flags;
    unsigned char endpoint;
    unsigned char type;
    unsigned int timeout;
    enum libusb_transfer_status status;
    int length;
    int actual_length;
    libusb_transfer_cb_fn callback;
    void *user_data;
    unsigned char *buffer;
};

// libusb 设备描述符
struct libusb_device_descriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
};

// 定义函数指针类型
typedef int (LIBUSB_CALL *libusb_init_t)(libusb_context **ctx);
typedef void (LIBUSB_CALL *libusb_exit_t)(libusb_context *ctx);
typedef ssize_t (LIBUSB_CALL *libusb_get_device_list_t)(libusb_context *ctx, libusb_device ***list);
typedef void (LIBUSB_CALL *libusb_free_device_list_t)(libusb_device **list, int unref_devices);
typedef uint8_t (LIBUSB_CALL *libusb_get_bus_number_t)(libusb_device *dev);
typedef uint8_t (LIBUSB_CALL *libusb_get_device_address_t)(libusb_device *dev);
typedef int (LIBUSB_CALL *libusb_get_device_descriptor_t)(libusb_device *dev, struct libusb_device_descriptor *desc);
typedef int (LIBUSB_CALL *libusb_open_t)(libusb_device *dev, libusb_device_handle **dev_handle);
typedef void (LIBUSB_CALL *libusb_close_t)(libusb_device_handle *dev_handle);
typedef int (LIBUSB_CALL *libusb_claim_interface_t)(libusb_device_handle *dev_handle, int interface_number);
typedef int (LIBUSB_CALL *libusb_release_interface_t)(libusb_device_handle *dev_handle, int interface_number);
typedef int (LIBUSB_CALL *libusb_get_string_descriptor_ascii_t)(libusb_device_handle *dev_handle, uint8_t desc_index, unsigned char *data, int length);
typedef int (LIBUSB_CALL *libusb_interrupt_transfer_t)(libusb_device_handle *dev_handle, unsigned char endpoint, unsigned char *data, int length, int *actual_length, unsigned int timeout);

// 全局变量
static HMODULE g_hLib = NULL;
static libusb_context *g_ctx = NULL;
static libusb_init_t fn_init;
static libusb_exit_t fn_exit;
static libusb_get_device_list_t fn_get_device_list;
static libusb_free_device_list_t fn_free_device_list;
static libusb_get_bus_number_t fn_get_bus_number;
static libusb_get_device_address_t fn_get_device_address;
static libusb_get_device_descriptor_t fn_get_device_descriptor;
static libusb_open_t fn_open;
static libusb_close_t fn_close;
static libusb_claim_interface_t fn_claim_interface;
static libusb_release_interface_t fn_release_interface;
static libusb_get_string_descriptor_ascii_t fn_get_string_descriptor_ascii;
static libusb_interrupt_transfer_t fn_interrupt_transfer;

// 设备句柄映射表
#define MAX_DEVICES 16
static struct {
    char serial[64];
    libusb_device_handle *handle;
    int in_use;
} g_device_map[MAX_DEVICES];

// 初始化标志
static int g_initialized = 0;

// 内部函数：加载USB函数
static int load_usb_functions(void) {
    if (!g_hLib) {
        g_hLib = LoadLibrary("libusb-1.0.dll");
        if (!g_hLib) {
            return USB_ERROR_NOT_FOUND;
        }
    }

    #define LOAD_FUNC(func, name) \
        func = (typeof(func))GetProcAddress(g_hLib, name); \
        if (!func) return USB_ERROR_NOT_SUPPORTED;

    LOAD_FUNC(fn_init, "libusb_init");
    LOAD_FUNC(fn_exit, "libusb_exit");
    LOAD_FUNC(fn_get_device_list, "libusb_get_device_list");
    LOAD_FUNC(fn_free_device_list, "libusb_free_device_list");
    LOAD_FUNC(fn_get_bus_number, "libusb_get_bus_number");
    LOAD_FUNC(fn_get_device_address, "libusb_get_device_address");
    LOAD_FUNC(fn_get_device_descriptor, "libusb_get_device_descriptor");
    LOAD_FUNC(fn_open, "libusb_open");
    LOAD_FUNC(fn_close, "libusb_close");
    LOAD_FUNC(fn_claim_interface, "libusb_claim_interface");
    LOAD_FUNC(fn_release_interface, "libusb_release_interface");
    LOAD_FUNC(fn_get_string_descriptor_ascii, "libusb_get_string_descriptor_ascii");
    LOAD_FUNC(fn_interrupt_transfer, "libusb_interrupt_transfer");

    return USB_SUCCESS;
}

// 内部函数：初始化USB系统
static int initialize_usb(void) {
    if (g_initialized) return USB_SUCCESS;

    int result = load_usb_functions();
    if (result != USB_SUCCESS) return result;

    result = fn_init(&g_ctx);
    if (result < 0) return USB_ERROR_IO;

    memset(g_device_map, 0, sizeof(g_device_map));
    g_initialized = 1;

    return USB_SUCCESS;
}

// 内部函数：查找设备映射
static libusb_device_handle* find_device_handle(const char* serial) {
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (g_device_map[i].in_use && strcmp(g_device_map[i].serial, serial) == 0) {
            return g_device_map[i].handle;
        }
    }
    return NULL;
}

// 内部函数：添加设备映射
static int add_device_mapping(const char* serial, libusb_device_handle* handle) {
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (!g_device_map[i].in_use) {
            strncpy(g_device_map[i].serial, serial, sizeof(g_device_map[i].serial) - 1);
            g_device_map[i].handle = handle;
            g_device_map[i].in_use = 1;
            return USB_SUCCESS;
        }
    }
    return USB_ERROR_NO_MEM;
}

// 内部函数：移除设备映射
static void remove_device_mapping(const char* serial) {
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (g_device_map[i].in_use && strcmp(g_device_map[i].serial, serial) == 0) {
            g_device_map[i].in_use = 0;
            break;
        }
    }
}

// 导出函数实现
USB_API int USB_ScanDevice(device_info_t* devices, int max_devices) {
    if (!devices || max_devices <= 0) return USB_ERROR_INVALID;

    int result = initialize_usb();
    if (result != USB_SUCCESS) return result;

    libusb_device **list;
    ssize_t count = fn_get_device_list(g_ctx, &list);
    if (count < 0) return USB_ERROR_IO;

    int found = 0;
    for (ssize_t i = 0; i < count && found < max_devices; i++) {
        libusb_device *device = list[i];
        struct libusb_device_descriptor desc;
        
        if (fn_get_device_descriptor(device, &desc) < 0) continue;

        // 只处理指定 VID/PID 的设备
        if (desc.idVendor != VENDOR_ID || desc.idProduct != PRODUCT_ID) continue;

        // 获取设备基本信息
        devices[found].vid = desc.idVendor;
        devices[found].pid = desc.idProduct;
        devices[found].bus_number = fn_get_bus_number(device);
        devices[found].device_address = fn_get_device_address(device);

        // 获取序列号
        libusb_device_handle *handle;
        if (fn_open(device, &handle) == 0) {
            unsigned char serial[64];
            if (fn_get_string_descriptor_ascii(handle, desc.iSerialNumber, serial, sizeof(serial)) > 0) {
                strncpy(devices[found].serial_number, (char*)serial, sizeof(devices[found].serial_number) - 1);
            }
            fn_close(handle);
        }

        found++;
    }

    fn_free_device_list(list, 1);
    return found;
}

USB_API int USB_OpenDevice(const char* target_serial) {
    if (!target_serial) return USB_ERROR_INVALID;
    if (find_device_handle(target_serial)) return USB_ERROR_BUSY;

    int result = initialize_usb();
    if (result != USB_SUCCESS) return result;

    libusb_device **list;
    ssize_t count = fn_get_device_list(g_ctx, &list);
    if (count < 0) return USB_ERROR_IO;

    int found = 0;
    for (ssize_t i = 0; i < count; i++) {
        libusb_device *device = list[i];
        struct libusb_device_descriptor desc;
        
        if (fn_get_device_descriptor(device, &desc) < 0) continue;

        libusb_device_handle *handle;
        if (fn_open(device, &handle) == 0) {
            unsigned char serial[64];
            if (fn_get_string_descriptor_ascii(handle, desc.iSerialNumber, serial, sizeof(serial)) > 0) {
                if (strcmp(target_serial, (char*)serial) == 0) {
                    // 找到目标设备
                    if (fn_claim_interface(handle, 0) == 0) {
                        result = add_device_mapping(target_serial, handle);
                        if (result == USB_SUCCESS) {
                            found = 1;
                            break;
                        }
                        fn_release_interface(handle, 0);
                    }
                }
            }
            if (!found) fn_close(handle);
        }
    }

    fn_free_device_list(list, 1);
    return found ? USB_SUCCESS : USB_ERROR_NOT_FOUND;
}

USB_API int USB_CloseDevice(const char* target_serial) {
    if (!target_serial) return USB_ERROR_INVALID;

    libusb_device_handle* handle = find_device_handle(target_serial);
    if (!handle) return USB_ERROR_NOT_FOUND;

    fn_release_interface(handle, 0);
    fn_close(handle);
    remove_device_mapping(target_serial);

    return USB_SUCCESS;
}

USB_API int USB_ReadData(const char* target_serial, unsigned char* data, int length) {
    if (!target_serial || !data || length <= 0) return USB_ERROR_INVALID;

    libusb_device_handle* handle = find_device_handle(target_serial);
    if (!handle) return USB_ERROR_NOT_FOUND;

    int actual_length;
    int result = fn_interrupt_transfer(handle, 0x81, data, length, &actual_length, 1000);
    
    if (result == 0) return actual_length;
    return USB_ERROR_IO;
}

// DLL入口点
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            // 初始化
            break;
        case DLL_PROCESS_DETACH:
            // 清理
            if (g_initialized) {
                // 关闭所有打开的设备
                for (int i = 0; i < MAX_DEVICES; i++) {
                    if (g_device_map[i].in_use) {
                        fn_release_interface(g_device_map[i].handle, 0);
                        fn_close(g_device_map[i].handle);
                        g_device_map[i].in_use = 0;
                    }
                }
                fn_exit(g_ctx);
                g_ctx = NULL;
                g_initialized = 0;
            }
            if (g_hLib) {
                FreeLibrary(g_hLib);
                g_hLib = NULL;
            }
            break;
    }
    return TRUE;
}
