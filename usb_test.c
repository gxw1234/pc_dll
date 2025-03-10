#include <stdio.h>
#include <stdlib.h>
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

#define LIBUSB_LOG_LEVEL_INFO 3

#define VENDOR_ID  0x1733
#define PRODUCT_ID 0xAABB
#define INTERRUPT_EP_IN 0x81  // 中断输入端点
#define INTERRUPT_TIMEOUT 1000 // 超时时间(ms)
#define BUFFER_SIZE 64        // 接收缓冲区大小

// 环形缓冲区定义
#define RING_BUFFER_SIZE 1024
static uint8_t ring_buffer[RING_BUFFER_SIZE];
static int write_pos = 0;
static int total_bytes = 0;

// 定义函数指针类型
typedef int (LIBUSB_CALL *libusb_init_t)(libusb_context **ctx);
typedef void (LIBUSB_CALL *libusb_exit_t)(libusb_context *ctx);
typedef void (LIBUSB_CALL *libusb_set_option_t)(libusb_context *ctx, enum libusb_option option, ...);
typedef ssize_t (LIBUSB_CALL *libusb_get_device_list_t)(libusb_context *ctx, libusb_device ***list);
typedef void (LIBUSB_CALL *libusb_free_device_list_t)(libusb_device **list, int unref_devices);
typedef uint8_t (LIBUSB_CALL *libusb_get_bus_number_t)(libusb_device *dev);
typedef uint8_t (LIBUSB_CALL *libusb_get_device_address_t)(libusb_device *dev);
typedef int (LIBUSB_CALL *libusb_get_device_descriptor_t)(libusb_device *dev, struct libusb_device_descriptor *desc);
typedef int (LIBUSB_CALL *libusb_open_t)(libusb_device *dev, libusb_device_handle **dev_handle);
typedef void (LIBUSB_CALL *libusb_close_t)(libusb_device_handle *dev_handle);
typedef int (LIBUSB_CALL *libusb_claim_interface_t)(libusb_device_handle *dev_handle, int interface_number);
typedef int (LIBUSB_CALL *libusb_release_interface_t)(libusb_device_handle *dev_handle, int interface_number);
typedef const char* (LIBUSB_CALL *libusb_error_name_t)(int error_code);
typedef struct libusb_transfer* (LIBUSB_CALL *libusb_alloc_transfer_t)(int iso_packets);
typedef void (LIBUSB_CALL *libusb_free_transfer_t)(struct libusb_transfer *transfer);
typedef int (LIBUSB_CALL *libusb_submit_transfer_t)(struct libusb_transfer *transfer);
typedef int (LIBUSB_CALL *libusb_handle_events_timeout_t)(libusb_context *ctx, struct timeval *tv);
typedef int (LIBUSB_CALL *libusb_cancel_transfer_t)(struct libusb_transfer *transfer);

// 函数指针
libusb_init_t fn_init;
libusb_exit_t fn_exit;
libusb_set_option_t fn_set_option;
libusb_get_device_list_t fn_get_device_list;
libusb_free_device_list_t fn_free_device_list;
libusb_get_bus_number_t fn_get_bus_number;
libusb_get_device_address_t fn_get_device_address;
libusb_get_device_descriptor_t fn_get_device_descriptor;
libusb_open_t fn_open;
libusb_close_t fn_close;
libusb_claim_interface_t fn_claim_interface;
libusb_release_interface_t fn_release_interface;
libusb_error_name_t fn_error_name;
libusb_alloc_transfer_t fn_alloc_transfer;
libusb_free_transfer_t fn_free_transfer;
libusb_submit_transfer_t fn_submit_transfer;
libusb_handle_events_timeout_t fn_handle_events_timeout;
libusb_cancel_transfer_t fn_cancel_transfer;

// 全局变量
volatile int transfer_completed = 0;

// 中断传输回调函数
void LIBUSB_CALL interrupt_transfer_callback(struct libusb_transfer *transfer) {
    if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
        // 将数据存入环形缓冲区
        for (int i = 0; i < transfer->actual_length; i++) {
            if (total_bytes < RING_BUFFER_SIZE) {
                ring_buffer[write_pos] = transfer->buffer[i];
                write_pos = (write_pos + 1) % RING_BUFFER_SIZE;
                total_bytes++;
            }
        }
        
        // 重新提交传输请求
        fn_submit_transfer(transfer);
    } else {
        printf("传输状态: %d\n", transfer->status);
        transfer_completed = 1;
    }
}

#define CHECK_FUNC_LOAD(func, name) \
    func = (typeof(func))GetProcAddress(hLib, name); \
    if (!func) { \
        printf("加载函数 %s 失败\n", name); \
        return 0; \
    } \
    printf("成功加载函数: %s\n", name);

// 加载所有函数
int load_usb_functions(HMODULE hLib) {
    CHECK_FUNC_LOAD(fn_init, "libusb_init");
    CHECK_FUNC_LOAD(fn_exit, "libusb_exit");
    CHECK_FUNC_LOAD(fn_set_option, "libusb_set_option");
    CHECK_FUNC_LOAD(fn_get_device_list, "libusb_get_device_list");
    CHECK_FUNC_LOAD(fn_free_device_list, "libusb_free_device_list");
    CHECK_FUNC_LOAD(fn_get_bus_number, "libusb_get_bus_number");
    CHECK_FUNC_LOAD(fn_get_device_address, "libusb_get_device_address");
    CHECK_FUNC_LOAD(fn_get_device_descriptor, "libusb_get_device_descriptor");
    CHECK_FUNC_LOAD(fn_open, "libusb_open");
    CHECK_FUNC_LOAD(fn_close, "libusb_close");
    CHECK_FUNC_LOAD(fn_claim_interface, "libusb_claim_interface");
    CHECK_FUNC_LOAD(fn_release_interface, "libusb_release_interface");
    CHECK_FUNC_LOAD(fn_error_name, "libusb_error_name");
    CHECK_FUNC_LOAD(fn_alloc_transfer, "libusb_alloc_transfer");
    CHECK_FUNC_LOAD(fn_free_transfer, "libusb_free_transfer");
    CHECK_FUNC_LOAD(fn_submit_transfer, "libusb_submit_transfer");
    CHECK_FUNC_LOAD(fn_handle_events_timeout, "libusb_handle_events_timeout");
    CHECK_FUNC_LOAD(fn_cancel_transfer, "libusb_cancel_transfer");

    return 1;
}

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include "usb_api.h"

int main(void) {
    device_info_t devices[10];
    unsigned char buffer[64];
    int count, read_bytes;

    // 扫描设备
    count = USB_ScanDevice(devices, 10);
    if (count <= 0) {
        printf("未找到设备\n");
        return 1;
    }

    printf("找到 %d 个设备:\n", count);
    for (int i = 0; i < count; i++) {
        printf("设备 %d:\n", i + 1);
        printf("  VID: 0x%04X\n", devices[i].vid);
        printf("  PID: 0x%04X\n", devices[i].pid);
        printf("  序列号: %s\n", devices[i].serial);
        printf("  总线: %d\n", devices[i].bus);
        printf("  地址: %d\n", devices[i].address);
        printf("-------------------\n");
    }

    // 打开第一个设备
    if (USB_OpenDevice(devices[0].serial) < 0) {
        printf("打开设备失败\n");
        return 1;
    }
    printf("成功打开设备\n");

    // 持续接收数据5秒
    printf("开始接收数据...\n");
    time_t start_time = time(NULL);
    while (time(NULL) - start_time < 5) {
        read_bytes = USB_ReadData(devices[0].serial, buffer, sizeof(buffer));
        if (read_bytes > 0) {
            printf("收到数据 (%d 字节): ", read_bytes);
            for (int i = 0; i < read_bytes; i++) {
                printf("%02X ", buffer[i]);
            }
            printf("\n");
        }
        Sleep(10); // 短暂延时，避免CPU占用过高
    }

    // 关闭设备
    USB_CloseDevice(devices[0].serial);
    printf("\n测试完成\n");

    return 0;
}
