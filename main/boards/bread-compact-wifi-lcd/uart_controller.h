#ifndef __UART_CONTROLLER_H__
#define __UART_CONTROLLER_H__

#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <cstdlib>
#include <cstring>
#include <string>
#include "config.h"
#include "esp_log.h"
#include "uart_service.h"
#include "fan_controller.h"
#include "lamp_controller.h"

class UartController {
private:
    static constexpr const char* TAG = "UartController";
    inline static UartController* instance_ = nullptr;

    uart_port_t uart_port_;
    gpio_num_t tx_gpio_;
    gpio_num_t rx_gpio_;
    uint32_t baud_rate_;

    QueueHandle_t rx_event_queue_;
    SemaphoreHandle_t tx_mutex_;

    TaskHandle_t rx_task_handle_;

    std::string last_message_;
    SemaphoreHandle_t rx_buffer_mutex_;

    // 初始化 UART 外设参数与驱动
    void ConfigureUart() {
        uart_config_t uart_config = {
            .baud_rate = static_cast<int>(baud_rate_),
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .source_clk = UART_SCLK_DEFAULT,
        };

        ESP_ERROR_CHECK(uart_driver_install(uart_port_, UART1_RX_BUFFER_SIZE, UART1_TX_BUFFER_SIZE, 10, &rx_event_queue_, 0));
        ESP_ERROR_CHECK(uart_param_config(uart_port_, &uart_config));
        ESP_ERROR_CHECK(uart_set_pin(uart_port_, tx_gpio_, rx_gpio_, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    }

    // FreeRTOS 任务入口，转调到对象方法
    static void RxTaskEntry(void* arg) {
        static_cast<UartController*>(arg)->RxTaskLoop();
    }

    // 后台读取串口数据，缓存并解析 K65 协议帧
    void RxTaskLoop() {
        uint8_t* data = static_cast<uint8_t*>(malloc(UART1_RX_BUFFER_SIZE));
        if (data == nullptr) {
            ESP_LOGE(TAG, "Failed to allocate UART1 RX buffer");
            vTaskDelete(nullptr);
            return;
        }

        UartService parser;

        while (true) {
            int length = uart_read_bytes(uart_port_, data, UART1_RX_BUFFER_SIZE, pdMS_TO_TICKS(UART1_READ_TIMEOUT_MS));
            if (length > 0) {
                if (xSemaphoreTake(rx_buffer_mutex_, portMAX_DELAY) == pdTRUE) {
                    last_message_.assign(reinterpret_cast<char*>(data), length);
                    xSemaphoreGive(rx_buffer_mutex_);
                }
                // ESP_LOGI(TAG, "UART1 RX %d bytes", length);

                auto frame = parser.Parse(data, static_cast<size_t>(length));
                if (frame.has_value()) {
                    ESP_LOGI(TAG, "Parsed frame payload: %02X %02X %02X", frame->field1, frame->field2, frame->field3);

                    if (frame->field1 == 0x01 && frame->field2 == 0x02) { //风扇控制指令
                        FanController* fan = FanController::GetInstance();
                        if (fan != nullptr) {
                            bool result = false;
                            if (frame->field3 == 0x01) {
                                result = fan->TurnOnDefaultLevel();
                            } else if (frame->field3 == 0x02) {
                                result = fan->TurnOffDirect();
                            } else {
                                ESP_LOGW(TAG, "Unknown fan control value: 0x%02X", frame->field3);
                            }
                            ESP_LOGI(TAG, "Fan control result: %s", result ? "success" : "failed");
                        } else {
                            ESP_LOGW(TAG, "FanController instance not available");
                        }
                    } else if (frame->field1 == 0x01 && frame->field2 == 0x01) { //灯光控制指令
                        LampController* lamp = LampController::GetInstance();
                        if (lamp != nullptr) {
                            bool result = false;
                            if (frame->field3 == 0x01) {
                                result = lamp->TurnOnDirect();
                            } else if (frame->field3 == 0x02) {
                                result = lamp->TurnOffDirect();
                            } else {
                                ESP_LOGW(TAG, "Unknown lamp control value: 0x%02X", frame->field3);
                            }
                            ESP_LOGI(TAG, "Lamp control result: %s", result ? "success" : "failed");
                        } else {
                            ESP_LOGW(TAG, "LampController instance not available");
                        }
                    }

                    char response[48];
                    snprintf(response, sizeof(response), "PARSED:%02X%02X%02X\r\n", frame->field1, frame->field2, frame->field3);
                    SendMessage(response); //发送解析后的数据到上位机
                } else {
                    ESP_LOGW(TAG, "Discard invalid UART frame");
                }
            }
        }
    }

    // 复制最近一次收到的数据，限制最大长度
    std::string CopyLastMessage(size_t max_length) {
        if (max_length == 0 || max_length > UART1_CONTROLLER_RECEIVE_MAX_LEN) {
            max_length = UART1_CONTROLLER_RECEIVE_MAX_LEN;
        }

        std::string message;
        if (xSemaphoreTake(rx_buffer_mutex_, pdMS_TO_TICKS(UART1_CONTROLLER_MUTEX_TIMEOUT_MS)) == pdTRUE) {
            if (!last_message_.empty()) {
                message = last_message_.substr(0, max_length);
            }
            xSemaphoreGive(rx_buffer_mutex_);
        }
        return message;
    }

    // 向串口发送数据，带互斥保护
    bool SendMessage(const std::string& message) {
        if (message.length() > UART1_CONTROLLER_SEND_MAX_LEN) {
            ESP_LOGW(TAG, "UART1 message too long: %u", static_cast<unsigned>(message.length()));
            return false;
        }

        if (xSemaphoreTake(tx_mutex_, pdMS_TO_TICKS(UART1_CONTROLLER_MUTEX_TIMEOUT_MS)) != pdTRUE) {
            ESP_LOGE(TAG, "UART1 TX mutex timeout");
            return false;
        }

        int written = uart_write_bytes(uart_port_, message.c_str(), message.length());
        if (written < 0) {
            ESP_LOGE(TAG, "uart_write_bytes failed");
            xSemaphoreGive(tx_mutex_);
            return false;
        }
        // ESP_LOGI(TAG, "UART1 TX %d bytes", written);  // 注释掉调试信息
        xSemaphoreGive(tx_mutex_);

        return true;
    }

    // 清空接收缓冲与硬件 FIFO
    void FlushInternal() {
        if (xSemaphoreTake(rx_buffer_mutex_, pdMS_TO_TICKS(UART1_CONTROLLER_MUTEX_TIMEOUT_MS)) == pdTRUE) {
            last_message_.clear();
            uart_flush_input(uart_port_);
            if (rx_event_queue_ != nullptr) {
                xQueueReset(rx_event_queue_);
            }
            xSemaphoreGive(rx_buffer_mutex_);
        }
    }

public:
    // 构造时完成 UART 配置并启动后台接收任务
    UartController(uart_port_t port,
                   gpio_num_t tx_gpio,
                   gpio_num_t rx_gpio,
                   uint32_t baud_rate)
        : uart_port_(port),
          tx_gpio_(tx_gpio),
          rx_gpio_(rx_gpio),
          baud_rate_(baud_rate),
          rx_event_queue_(nullptr),
          tx_mutex_(xSemaphoreCreateMutex()),
          rx_task_handle_(nullptr),
          rx_buffer_mutex_(xSemaphoreCreateMutex()) {
        instance_ = this;
        ConfigureUart();

        BaseType_t result = xTaskCreatePinnedToCore(
            RxTaskEntry,
            "uart0_rx_task",
            UART1_RX_TASK_STACK_SIZE,
            this,
            UART1_RX_TASK_PRIORITY,
            &rx_task_handle_,
            tskNO_AFFINITY
        );
        if (result != pdPASS) {
            ESP_LOGE(TAG, "Failed to create UART0 RX task");
        } else {
            ESP_LOGI(TAG, "UART0 RX task started");
        }

        ESP_LOGI(TAG, "UartController initialized: port=%d TX=%d RX=%d baud=%lu",
                 static_cast<int>(uart_port_), static_cast<int>(tx_gpio_), static_cast<int>(rx_gpio_),
                 static_cast<unsigned long>(baud_rate_));
    }

    // 对外发送接口
    bool Send(const std::string& message) {
        return SendMessage(message);
    }

    // 对外读取最近一次消息的接口
    std::string Receive(size_t max_length = UART1_CONTROLLER_RECEIVE_DEFAULT_LEN) {
        return CopyLastMessage(max_length);
    }

    // 对外提供的清空接口
    void Flush() {
        FlushInternal();
    }

    // 析构时释放资源
    ~UartController() {
        if (rx_task_handle_ != nullptr) {
            vTaskDelete(rx_task_handle_);
        }
        if (tx_mutex_ != nullptr) {
            vSemaphoreDelete(tx_mutex_);
        }
        if (rx_buffer_mutex_ != nullptr) {
            vSemaphoreDelete(rx_buffer_mutex_);
        }
        if (rx_event_queue_ != nullptr) {
            vQueueDelete(rx_event_queue_);
        }

        uart_driver_delete(uart_port_);
    }

    UartController(const UartController&) = delete;
    UartController& operator=(const UartController&) = delete;

    // 获取全局 UART 控制器实例
    static UartController* GetInstance() {
        return instance_;
    }
};

// 为 LampController 提供串口反馈实现（在 UartController 完整定义之后）
inline void LampController::SendLampUartFeedback(bool state) {
    UartController* uart = UartController::GetInstance();
    if (uart != nullptr) {
        const char* uart_msg = state ? "a=1" : "a=2";
        uart->Send(uart_msg);
        ESP_LOGI(TAG, "Sent UART feedback: %s", uart_msg);
    } else {
        ESP_LOGD(TAG, "UART not ready, skip feedback");
    }
}

// 为 FanController 提供串口反馈实现
inline void FanController::SendFanUartFeedback(bool is_on) {
    UartController* uart = UartController::GetInstance();
    if (uart != nullptr) {
        const char* uart_msg = is_on ? "a=5" : "a=6";
        uart->Send(uart_msg);
        ESP_LOGI(TAG, "Sent UART fan feedback: %s", uart_msg);
    } else {
        ESP_LOGD(TAG, "UART not ready, skip fan feedback");
    }
}

#endif // __UART_CONTROLLER_H__
