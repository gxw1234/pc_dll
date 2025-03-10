#include "usb_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <windows.h>
#include <time.h>

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

enum libusb_transfer_type {
    LIBUSB_TRANSFER_TYPE_CONTROL = 0,
    LIBUSB_TRANSFER_TYPE_ISOCHRONOUS = 1,
    LIBUSB_TRANSFER_TYPE_BULK = 2,
    LIBUSB_TRANSFER_TYPE_INTERRUPT = 3,
    LIBUSB_TRANSFER_TYPE_BULK_STREAM = 4,
};

// libusb 回调函数类型定义
struct libusb_transfer;
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

#define VENDOR_ID  0x1733
#define PRODUCT_ID 0xAABB
#define INTERRUPT_EP_IN 0x81
#define INTERRUPT_TIMEOUT 1000
#define BUFFER_SIZE 64
#define RING_BUFFER_SIZE 4096

// 环形缓冲区
static struct {
    uint8_t buffer[RING_BUFFER_SIZE];
    int write_pos;
    int read_pos;
    int data_size;
    CRITICAL_SECTION cs;
} ring_buffer = {0};

// 全局变量
static HMODULE hLib = NULL;
static libusb_context *ctx = NULL;
static libusb_device_handle *current_handle = NULL;
static struct libusb_transfer *transfer = NULL;
static unsigned char *transfer_buffer = NULL;
static volatile int transfer_completed = 0;
static char current_serial[64] = {0};

// 添加线程相关变量
static HANDLE event_thread = NULL;
static volatile BOOL thread_running = FALSE;

// 函数指针定义
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

// libusb 辅助函数声明
static void LIBUSB_CALL libusb_fill_interrupt_transfer(
    struct libusb_transfer *transfer,
    libusb_device_handle *dev_handle,
    unsigned char endpoint,
    unsigned char *buffer,
    int length,
    libusb_transfer_cb_fn callback,
    void *user_data,
    unsigned int timeout
) {
    transfer->dev_handle = dev_handle;
    transfer->endpoint = endpoint;
    transfer->type = LIBUSB_TRANSFER_TYPE_INTERRUPT;
    transfer->timeout = timeout;
    transfer->buffer = buffer;
    transfer->length = length;
    transfer->user_data = user_data;
    transfer->callback = callback;
}

// 中断传输回调函数
static void LIBUSB_CALL interrupt_transfer_callback(struct libusb_transfer *transfer) {
    if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
        EnterCriticalSection(&ring_buffer.cs);
        
        // 将数据存入环形缓冲区
        for (int i = 0; i < transfer->actual_length; i++) {
            if (ring_buffer.data_size < RING_BUFFER_SIZE) {
                ring_buffer.buffer[ring_buffer.write_pos] = transfer->buffer[i];
                ring_buffer.write_pos = (ring_buffer.write_pos + 1) % RING_BUFFER_SIZE;
                ring_buffer.data_size++;
            }
        }
        LeaveCriticalSection(&ring_buffer.cs);
    }
    
    // 无论传输是否成功，只要设备还在，就继续提交新的传输请求
    if (current_handle && !transfer_completed) {
        // 重置传输结构
        transfer->actual_length = 0;
        transfer->status = LIBUSB_TRANSFER_COMPLETED;
        
        // 重新提交传输请求
        if (fn_submit_transfer(transfer) < 0) {
            transfer_completed = 1;
        }
    } else {
        transfer_completed = 1;
    }
}

// 事件处理线程函数
static DWORD WINAPI event_thread_proc(LPVOID param) {
    struct timeval tv = {0, 1000};  // 1ms timeout
    
    while (thread_running) {
        if (ctx && current_handle) {
            fn_handle_events_timeout(ctx, &tv);
        }
        Sleep(1);  // 休眠1ms，避免过度占用CPU
    }
    return 0;
}

// 加载USB函数
static int load_usb_functions(void) {
    if (!hLib) {
        hLib = LoadLibrary("libusb-1.0.dll");
        if (!hLib) return -1;
    }

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

    return (fn_init && fn_exit && fn_get_device_list && fn_free_device_list && 
            fn_get_bus_number && fn_get_device_address && fn_get_device_descriptor && 
            fn_open && fn_close && fn_claim_interface && fn_release_interface && 
            fn_alloc_transfer && fn_free_transfer && fn_submit_transfer && 
            fn_handle_events_timeout && fn_get_string_descriptor_ascii) ? 0 : -1;
}

// 初始化函数，在DLL加载时调用
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            InitializeCriticalSection(&ring_buffer.cs);
            break;
        case DLL_PROCESS_DETACH:
            if (current_handle) {
                USB_CloseDevice(current_serial);
            }
            if (hLib) {
                FreeLibrary(hLib);
                hLib = NULL;
            }
            DeleteCriticalSection(&ring_buffer.cs);
            break;
    }
    return TRUE;
}

// 实现导出函数
USB_API int USB_ScanDevice(device_info_t* devices, int max_devices) {
    if (!devices || max_devices <= 0) return -1;

    // 初始化libusb
    if (!hLib && load_usb_functions() < 0) return -1;
    if (!ctx && fn_init(&ctx) < 0) return -1;

    libusb_device **list;
    ssize_t count = fn_get_device_list(ctx, &list);
    if (count < 0) return -1;

    int found = 0;
    for (ssize_t i = 0; i < count && found < max_devices; i++) {
        struct libusb_device_descriptor desc;
        if (fn_get_device_descriptor(list[i], &desc) == 0) {
            // 只返回目标设备
            if (desc.idVendor == VENDOR_ID && desc.idProduct == PRODUCT_ID) {
                devices[found].vid = desc.idVendor;
                devices[found].pid = desc.idProduct;
                devices[found].bus = fn_get_bus_number(list[i]);
                devices[found].address = fn_get_device_address(list[i]);

                // 获取序列号
                libusb_device_handle *handle;
                if (fn_open(list[i], &handle) == 0) {
                    fn_get_string_descriptor_ascii(handle, desc.iSerialNumber,
                        (unsigned char*)devices[found].serial, sizeof(devices[0].serial));
                    fn_close(handle);
                }
                found++;
            }
        }
    }

    fn_free_device_list(list, 1);
    return found;
}

USB_API int USB_OpenDevice(const char* target_serial) {
    if (!target_serial || current_handle) return -1;

    // 初始化环形缓冲区
    ring_buffer.read_pos = 0;
    ring_buffer.write_pos = 0;
    ring_buffer.data_size = 0;

    // 扫描设备
    device_info_t devices[10];
    int count = USB_ScanDevice(devices, 10);
    if (count <= 0) return -1;

    // 查找目标设备
    int found = -1;
    for (int i = 0; i < count; i++) {
        if (strcmp(devices[i].serial, target_serial) == 0) {
            found = i;
            break;
        }
    }
    if (found < 0) return -1;

    // 重新获取设备列表并打开设备
    libusb_device **list;
    ssize_t list_count = fn_get_device_list(ctx, &list);
    if (list_count < 0) return -1;

    int result = -1;
    for (ssize_t i = 0; i < list_count; i++) {
        if (fn_get_bus_number(list[i]) == devices[found].bus &&
            fn_get_device_address(list[i]) == devices[found].address) {
            
            if (fn_open(list[i], &current_handle) == 0 &&
                fn_claim_interface(current_handle, 0) == 0) {
                
                // 分配传输缓冲区
                transfer_buffer = (unsigned char*)malloc(BUFFER_SIZE);
                transfer = fn_alloc_transfer(0);
                if (transfer_buffer && transfer) {
                    // 设置传输参数
                    libusb_fill_interrupt_transfer(transfer, current_handle,
                        INTERRUPT_EP_IN, transfer_buffer, BUFFER_SIZE,
                        interrupt_transfer_callback, NULL, INTERRUPT_TIMEOUT);
                    
                    // 提交传输请求
                    transfer_completed = 0;
                    if (fn_submit_transfer(transfer) == 0) {
                        strncpy(current_serial, target_serial, sizeof(current_serial)-1);
                        
                        // 启动事件处理线程
                        thread_running = TRUE;
                        event_thread = CreateThread(NULL, 0, event_thread_proc, NULL, 0, NULL);
                        if (event_thread) {
                            result = 0;
                        } else {
                            transfer_completed = 1;
                        }
                    }
                }
                
                if (result < 0) {
                    free(transfer_buffer);
                    transfer_buffer = NULL;
                    fn_free_transfer(transfer);
                    transfer = NULL;
                    fn_release_interface(current_handle, 0);
                    fn_close(current_handle);
                    current_handle = NULL;
                }
            }
            break;
        }
    }

    fn_free_device_list(list, 1);
    return result;
}

USB_API int USB_CloseDevice(const char* target_serial) {
    if (!target_serial || !current_handle || strcmp(target_serial, current_serial) != 0)
        return -1;

    // 停止事件处理线程
    if (event_thread) {
        thread_running = FALSE;
        WaitForSingleObject(event_thread, 1000);  // 等待线程结束
        CloseHandle(event_thread);
        event_thread = NULL;
    }

    // 停止传输
    transfer_completed = 1;
    
    // 释放资源
    if (transfer) {
        fn_free_transfer(transfer);
        transfer = NULL;
    }
    
    if (transfer_buffer) {
        free(transfer_buffer);
        transfer_buffer = NULL;
    }

    fn_release_interface(current_handle, 0);
    fn_close(current_handle);
    current_handle = NULL;
    current_serial[0] = '\0';

    return 0;
}

USB_API int USB_ReadData(const char* target_serial, unsigned char* data, int length) {
    if (!target_serial || !data || length <= 0 || 
        !current_handle || strcmp(target_serial, current_serial) != 0)
        return -1;

    int read_count = 0;
    struct timeval tv = {0, 1}; // 1微秒超时，快速处理事件

    // 处理所有待处理的USB事件
    for(int i = 0; i < 10; i++) {
        fn_handle_events_timeout(ctx, &tv);
    }

    EnterCriticalSection(&ring_buffer.cs);
    
    // 从环形缓冲区读取数据，但最多只读取用户请求的长度
    read_count = (length < ring_buffer.data_size) ? length : ring_buffer.data_size;
    
    // 复制数据到用户缓冲区
    for (int i = 0; i < read_count; i++) {
        data[i] = ring_buffer.buffer[ring_buffer.read_pos];
        ring_buffer.read_pos = (ring_buffer.read_pos + 1) % RING_BUFFER_SIZE;
    }
    
    // 更新缓冲区大小
    ring_buffer.data_size -= read_count;
    
    LeaveCriticalSection(&ring_buffer.cs);

    return read_count;
}
