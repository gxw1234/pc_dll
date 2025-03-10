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
        printf("收到数据 (%d 字节): ", transfer->actual_length);
        for (int i = 0; i < transfer->actual_length; i++) {
            printf("%02X ", transfer->buffer[i]);
        }
        printf("\n");
        
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

int main(void) {
    HMODULE hLib;
    libusb_context *ctx = NULL;
    libusb_device **list;
    libusb_device_handle *handle = NULL;
    int result;
    ssize_t count;
    unsigned char *buffer = NULL;
    struct libusb_transfer *transfer = NULL;

    // 加载 DLL
    hLib = LoadLibrary("libusb-1.0.dll");
    if (!hLib) {
        printf("无法加载 libusb-1.0.dll, 错误码: %lu\n", GetLastError());
        return 1;
    }
    printf("1. 成功加载 libusb-1.0.dll\n");

    // 加载函数
    if (!load_usb_functions(hLib)) {
        printf("加载函数失败\n");
        FreeLibrary(hLib);
        return 1;
    }
    printf("2. 成功加载所有函数\n");

    // 初始化 libusb
    result = fn_init(&ctx);
    if (result < 0) {
        printf("初始化失败: %s\n", fn_error_name(result));
        FreeLibrary(hLib);
        return 1;
    }
    printf("3. libusb 初始化成功\n");

    // 设置调试级别
    fn_set_option(ctx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_INFO);

    // 获取设备列表
    count = fn_get_device_list(ctx, &list);
    if (count < 0) {
        printf("获取设备列表失败: %s\n", fn_error_name((int)count));
        fn_exit(ctx);
        FreeLibrary(hLib);
        return 1;
    }
    printf("4. 发现 %zd 个 USB 设备\n", count);

    // 遍历并显示所有设备信息
    printf("\n扫描 USB 设备:\n");
    for (ssize_t i = 0; i < count; i++) {
        libusb_device *device = list[i];
        struct libusb_device_descriptor desc;
        uint8_t bus = fn_get_bus_number(device);
        uint8_t address = fn_get_device_address(device);

        result = fn_get_device_descriptor(device, &desc);
        if (result < 0) {
            printf("获取设备描述符失败: %s\n", fn_error_name(result));
            continue;
        }

        printf("-------------------\n");
        printf("设备 %zd:\n", i + 1);
        printf("  总线: %d, 地址: %d\n", bus, address);
        printf("  厂商ID: 0x%04x\n", desc.idVendor);
        printf("  产品ID: 0x%04x\n", desc.idProduct);

        // 检查是否是目标设备
        if (desc.idVendor == VENDOR_ID && desc.idProduct == PRODUCT_ID) {
            printf("\n找到目标设备! (VID:0x%04x, PID:0x%04x)\n", VENDOR_ID, PRODUCT_ID);
            
            // 尝试打开设备
            result = fn_open(device, &handle);
            if (result < 0) {
                printf("打开设备失败: %s\n", fn_error_name(result));
                continue;
            }
            printf("成功打开设备\n");

            // 尝试声明接口
            result = fn_claim_interface(handle, 0);
            if (result < 0) {
                printf("声明接口失败: %s\n", fn_error_name(result));
                fn_close(handle);
                continue;
            }
            printf("成功声明接口\n");

            // 分配传输缓冲区
            buffer = (unsigned char *)malloc(BUFFER_SIZE);
            if (!buffer) {
                printf("内存分配失败\n");
                fn_release_interface(handle, 0);
                fn_close(handle);
                continue;
            }

            // 分配传输结构
            transfer = fn_alloc_transfer(0);
            if (!transfer) {
                printf("传输结构分配失败\n");
                free(buffer);
                fn_release_interface(handle, 0);
                fn_close(handle);
                continue;
            }

            // 手动设置传输参数
            transfer->dev_handle = handle;
            transfer->flags = 0;
            transfer->endpoint = INTERRUPT_EP_IN;
            transfer->type = 3;  // LIBUSB_TRANSFER_TYPE_INTERRUPT
            transfer->timeout = INTERRUPT_TIMEOUT;
            transfer->buffer = buffer;
            transfer->length = BUFFER_SIZE;
            transfer->callback = interrupt_transfer_callback;
            transfer->user_data = NULL;

            // 提交传输请求
            result = fn_submit_transfer(transfer);
            if (result < 0) {
                printf("提交传输请求失败: %s\n", fn_error_name(result));
                fn_free_transfer(transfer);
                free(buffer);
                fn_release_interface(handle, 0);
                fn_close(handle);
                continue;
            }
            printf("开始接收数据...\n");
            printf("将在5秒后自动停止...\n");

            // 处理事件循环
            time_t start_time = time(NULL);
            struct timeval tv = {1, 0};  // 1秒超时
            while (!transfer_completed && (time(NULL) - start_time) < 5) {
                fn_handle_events_timeout(ctx, &tv);
            }

            // 取消传输
            if (!transfer_completed) {
                fn_cancel_transfer(transfer);
                struct timeval tv = {1, 0};
                while (!transfer_completed) {
                    fn_handle_events_timeout(ctx, &tv);
                }
            }

            // 清理传输资源
            fn_free_transfer(transfer);
            free(buffer);

            // 释放接口
            fn_release_interface(handle, 0);
            printf("已释放接口\n");

            // 关闭设备
            fn_close(handle);
            printf("已关闭设备\n");
            break;
        }
    }

    // 清理
    fn_free_device_list(list, 1);
    fn_exit(ctx);
    FreeLibrary(hLib);
    printf("\n测试完成\n");

    return 0;
}
