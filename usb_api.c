#include "usb_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <windows.h>
#include <time.h>
#include "libusb.h"

#define VENDOR_ID  0x1733
#define PRODUCT_ID 0xAABB
#define INTERRUPT_EP_IN 0x81
#define INTERRUPT_TIMEOUT 1000
#define TRANSFER_BUFFER_SIZE 64
#define DATA_BUFFER_SIZE (1024*1024)  // 1MB 数据缓冲区

// 数据缓冲区
static struct {
    unsigned char buffer[DATA_BUFFER_SIZE];
    int write_pos;
    int read_pos;
    int data_size;
    CRITICAL_SECTION cs;
    HANDLE event_thread;
    volatile BOOL thread_running;
} data_buffer = {0};

// 传输缓冲区
static unsigned char transfer_buffer[TRANSFER_BUFFER_SIZE];

// 全局变量
static HMODULE hLib = NULL;
static libusb_context *ctx = NULL;
static libusb_device_handle *current_handle = NULL;
static struct libusb_transfer *transfer = NULL;
static char current_serial[64] = {0};
static volatile int transfer_completed = 0;

// 函数指针类型定义
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
typedef struct libusb_transfer* (LIBUSB_CALL *libusb_alloc_transfer_t)(int iso_packets);
typedef void (LIBUSB_CALL *libusb_free_transfer_t)(struct libusb_transfer *transfer);
typedef int (LIBUSB_CALL *libusb_submit_transfer_t)(struct libusb_transfer *transfer);
typedef int (LIBUSB_CALL *libusb_handle_events_timeout_t)(libusb_context *ctx, struct timeval *tv);
typedef int (LIBUSB_CALL *libusb_get_string_descriptor_ascii_t)(libusb_device_handle *dev_handle, uint8_t desc_index, unsigned char *data, int length);
typedef int (LIBUSB_CALL *libusb_cancel_transfer_t)(struct libusb_transfer *transfer);

// 函数指针
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
static libusb_alloc_transfer_t fn_alloc_transfer;
static libusb_free_transfer_t fn_free_transfer;
static libusb_submit_transfer_t fn_submit_transfer;
static libusb_handle_events_timeout_t fn_handle_events_timeout;
static libusb_get_string_descriptor_ascii_t fn_get_string_descriptor_ascii;
static libusb_cancel_transfer_t fn_cancel_transfer;

// 加载USB函数
static int load_usb_functions(void) {
    hLib = LoadLibrary("libusb-1.0.dll");
    if (!hLib) return 0;

    fn_init = (libusb_init_t)GetProcAddress(hLib, "libusb_init");
    fn_exit = (libusb_exit_t)GetProcAddress(hLib, "libusb_exit");
    fn_get_device_list = (libusb_get_device_list_t)GetProcAddress(hLib, "libusb_get_device_list");
    fn_free_device_list = (libusb_free_device_list_t)GetProcAddress(hLib, "libusb_free_device_list");
    fn_get_bus_number = (libusb_get_bus_number_t)GetProcAddress(hLib, "libusb_get_bus_number");
    fn_get_device_address = (libusb_get_device_address_t)GetProcAddress(hLib, "libusb_get_device_address");
    fn_get_device_descriptor = (libusb_get_device_descriptor_t)GetProcAddress(hLib, "libusb_get_device_descriptor");
    fn_open = (libusb_open_t)GetProcAddress(hLib, "libusb_open");
    fn_close = (libusb_close_t)GetProcAddress(hLib, "libusb_close");
    fn_claim_interface = (libusb_claim_interface_t)GetProcAddress(hLib, "libusb_claim_interface");
    fn_release_interface = (libusb_release_interface_t)GetProcAddress(hLib, "libusb_release_interface");
    fn_alloc_transfer = (libusb_alloc_transfer_t)GetProcAddress(hLib, "libusb_alloc_transfer");
    fn_free_transfer = (libusb_free_transfer_t)GetProcAddress(hLib, "libusb_free_transfer");
    fn_submit_transfer = (libusb_submit_transfer_t)GetProcAddress(hLib, "libusb_submit_transfer");
    fn_handle_events_timeout = (libusb_handle_events_timeout_t)GetProcAddress(hLib, "libusb_handle_events_timeout");
    fn_get_string_descriptor_ascii = (libusb_get_string_descriptor_ascii_t)GetProcAddress(hLib, "libusb_get_string_descriptor_ascii");
    fn_cancel_transfer = (libusb_cancel_transfer_t)GetProcAddress(hLib, "libusb_cancel_transfer");

    return (fn_init && fn_exit && fn_get_device_list && fn_free_device_list &&
            fn_get_bus_number && fn_get_device_address && fn_get_device_descriptor &&
            fn_open && fn_close && fn_claim_interface && fn_release_interface &&
            fn_alloc_transfer && fn_free_transfer && fn_submit_transfer &&
            fn_handle_events_timeout && fn_get_string_descriptor_ascii && fn_cancel_transfer);
}

// 中断传输回调函数
static void LIBUSB_CALL interrupt_transfer_callback(struct libusb_transfer *transfer) {
    if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
        EnterCriticalSection(&data_buffer.cs);
        
        // 确保有足够的空间
        if (data_buffer.data_size + transfer->actual_length <= DATA_BUFFER_SIZE) {
            // 复制数据到缓冲区
            for (int i = 0; i < transfer->actual_length; i++) {
                data_buffer.buffer[data_buffer.write_pos] = transfer->buffer[i];
                data_buffer.write_pos = (data_buffer.write_pos + 1) % DATA_BUFFER_SIZE;
                data_buffer.data_size++;
            }
        }
        
        LeaveCriticalSection(&data_buffer.cs);

        // 重新提交传输
        fn_submit_transfer(transfer);
    }
}

// 事件处理线程函数
static DWORD WINAPI event_thread_proc(LPVOID param) {
    struct timeval tv = {0, 100};  // 100微秒超时
    while (data_buffer.thread_running) {
        if (ctx) {
            fn_handle_events_timeout(ctx, &tv);
        }
        Sleep(1);  // 避免CPU占用过高
    }
    return 0;
}

// 初始化USB系统
USB_API int USB_Init(void) {
    // 加载libusb DLL
    hLib = LoadLibraryA("libusb-1.0.dll");
    if (!hLib) {
        return -1;
    }

    // 加载函数
    if (load_usb_functions() < 0) {
        FreeLibrary(hLib);
        return -1;
    }

    // 初始化libusb
    if (fn_init(&ctx) < 0) {
        FreeLibrary(hLib);
        return -1;
    }

    // 初始化数据缓冲区
    InitializeCriticalSection(&data_buffer.cs);
    data_buffer.write_pos = 0;
    data_buffer.read_pos = 0;
    data_buffer.data_size = 0;
    data_buffer.thread_running = TRUE;
    
    // 创建事件处理线程
    data_buffer.event_thread = CreateThread(NULL, 0, event_thread_proc, NULL, 0, NULL);
    if (!data_buffer.event_thread) {
        DeleteCriticalSection(&data_buffer.cs);
        fn_exit(ctx);
        FreeLibrary(hLib);
        return -1;
    }
    
    return 0;
}

// 扫描设备
USB_API int USB_ScanDevice(device_info_t* devices, int max_devices) {
    if (!devices || max_devices <= 0) {
        return -1;
    }

    libusb_device **list;
    ssize_t count = fn_get_device_list(ctx, &list);
    if (count < 0) {
        return -1;
    }

    int num_devices = 0;
    struct libusb_device_descriptor desc;
    libusb_device_handle *handle = NULL;
    unsigned char string_desc[256];

    for (ssize_t i = 0; i < count && num_devices < max_devices; i++) {
        if (fn_get_device_descriptor(list[i], &desc) == 0) {
            // 尝试打开设备以读取字符串描述符
            if (fn_open(list[i], &handle) == 0) {
                devices[num_devices].vid = desc.idVendor;
                devices[num_devices].pid = desc.idProduct;
                devices[num_devices].bus = fn_get_bus_number(list[i]);
                devices[num_devices].address = fn_get_device_address(list[i]);

                // 读取序列号
                if (desc.iSerialNumber > 0) {
                    if (fn_get_string_descriptor_ascii(handle, desc.iSerialNumber,
                        (unsigned char*)devices[num_devices].serial, 
                        sizeof(devices[num_devices].serial)) <= 0) {
                        devices[num_devices].serial[0] = '\0';
                    }
                } else {
                    devices[num_devices].serial[0] = '\0';
                }

                // 读取制造商字符串
                if (desc.iManufacturer > 0) {
                    if (fn_get_string_descriptor_ascii(handle, desc.iManufacturer,
                        (unsigned char*)devices[num_devices].manufacturer,
                        sizeof(devices[num_devices].manufacturer)) <= 0) {
                        devices[num_devices].manufacturer[0] = '\0';
                    }
                } else {
                    devices[num_devices].manufacturer[0] = '\0';
                }

                // 读取产品字符串
                if (desc.iProduct > 0) {
                    if (fn_get_string_descriptor_ascii(handle, desc.iProduct,
                        (unsigned char*)devices[num_devices].product,
                        sizeof(devices[num_devices].product)) <= 0) {
                        devices[num_devices].product[0] = '\0';
                    }
                } else {
                    devices[num_devices].product[0] = '\0';
                }

                fn_close(handle);
                num_devices++;
            }
        }
    }

    fn_free_device_list(list, 1);
    return num_devices;
}

// 打开设备
USB_API int USB_OpenDevice(const char* target_serial) {
    libusb_device **list;
    struct libusb_device_descriptor desc;
    ssize_t count = fn_get_device_list(ctx, &list);
    int found = 0;

    if (count < 0) return -1;

    // 查找并打开设备
    for (ssize_t i = 0; i < count && !found; i++) {
        if (fn_get_device_descriptor(list[i], &desc) == 0) {
            if (desc.idVendor == VENDOR_ID && desc.idProduct == PRODUCT_ID) {
                libusb_device_handle *handle;
                if (fn_open(list[i], &handle) == 0) {
                    char serial[64] = {0};
                    if (fn_get_string_descriptor_ascii(handle, desc.iSerialNumber,
                        (unsigned char*)serial, sizeof(serial)) > 0) {
                        if (strcmp(serial, target_serial) == 0) {
                            current_handle = handle;
                            strcpy(current_serial, target_serial);
                            found = 1;
                            continue;
                        }
                    }
                    if (!found) fn_close(handle);
                }
            }
        }
    }

    fn_free_device_list(list, 1);
    if (!found) return -1;

    // 声明接口
    if (fn_claim_interface(current_handle, 0) < 0) {
        fn_close(current_handle);
        current_handle = NULL;
        return -1;
    }

    // 分配传输结构
    transfer = fn_alloc_transfer(0);
    if (!transfer) {
        fn_release_interface(current_handle, 0);
        fn_close(current_handle);
        current_handle = NULL;
        return -1;
    }

    // 设置传输参数
    transfer->dev_handle = current_handle;
    transfer->flags = 0;
    transfer->endpoint = INTERRUPT_EP_IN;
    transfer->type = LIBUSB_TRANSFER_TYPE_INTERRUPT;
    transfer->timeout = INTERRUPT_TIMEOUT;
    transfer->buffer = transfer_buffer;
    transfer->length = TRANSFER_BUFFER_SIZE;
    transfer->callback = interrupt_transfer_callback;
    transfer->user_data = NULL;

    // 提交传输请求
    if (fn_submit_transfer(transfer) < 0) {
        fn_free_transfer(transfer);
        fn_release_interface(current_handle, 0);
        fn_close(current_handle);
        current_handle = NULL;
        return -1;
    }

    return 0;
}

// 读取数据
USB_API int USB_ReadData(const char* target_serial, unsigned char* data, int length) {
    if (!target_serial || !current_handle || strcmp(target_serial, current_serial) != 0) {
        return -1;
    }

    EnterCriticalSection(&data_buffer.cs);
    
    // 读取数据
    int bytes_to_read = (length < data_buffer.data_size) ? length : data_buffer.data_size;
    for (int i = 0; i < bytes_to_read; i++) {
        data[i] = data_buffer.buffer[data_buffer.read_pos];
        data_buffer.read_pos = (data_buffer.read_pos + 1) % DATA_BUFFER_SIZE;
    }
    data_buffer.data_size -= bytes_to_read;
    
    LeaveCriticalSection(&data_buffer.cs);
    
    return bytes_to_read;
}

// 关闭设备
USB_API int USB_CloseDevice(const char* target_serial) {
    if (!target_serial || !current_handle || strcmp(target_serial, current_serial) != 0) {
        return -1;
    }

    // 停止传输
    if (transfer) {
        fn_cancel_transfer(transfer);
        Sleep(100);  // 等待传输完全停止
        fn_free_transfer(transfer);
        transfer = NULL;
    }

    // 释放接口
    if (current_handle) {
        fn_release_interface(current_handle, 0);
        fn_close(current_handle);
        current_handle = NULL;
    }

    memset(current_serial, 0, sizeof(current_serial));
    return 0;
}

// 清理USB系统
USB_API void USB_Cleanup() {
    // 停止事件处理线程
    if (data_buffer.thread_running) {
        data_buffer.thread_running = FALSE;
        if (data_buffer.event_thread) {
            WaitForSingleObject(data_buffer.event_thread, 1000);
            CloseHandle(data_buffer.event_thread);
            data_buffer.event_thread = NULL;
        }
    }

    // 关闭当前设备
    if (current_handle) {
        if (transfer) {
            fn_cancel_transfer(transfer);
            Sleep(100);  // 等待传输完全停止
            fn_free_transfer(transfer);
            transfer = NULL;
        }
        fn_release_interface(current_handle, 0);
        fn_close(current_handle);
        current_handle = NULL;
    }

    // 清理libusb
    if (ctx) {
        fn_exit(ctx);
        ctx = NULL;
    }

    // 释放DLL
    if (hLib) {
        FreeLibrary(hLib);
        hLib = NULL;
    }

    // 删除临界区
    DeleteCriticalSection(&data_buffer.cs);

    // 清理缓冲区
    memset(&data_buffer, 0, sizeof(data_buffer));
}

// DLL入口点
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            break;
        case DLL_PROCESS_DETACH:
            USB_Cleanup();
            break;
        case DLL_THREAD_ATTACH:
            break;
        case DLL_THREAD_DETACH:
            break;
    }
    return TRUE;
}
