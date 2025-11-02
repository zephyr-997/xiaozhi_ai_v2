#ifndef __DHT11_CONTROLLER_H__
#define __DHT11_CONTROLLER_H__

#include <driver/gpio.h>
#include "mcp_server.h"
#include "mqtt_controller.h"
#include "config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"
#include "uart_controller.h" 


class Dht11Controller {
private:
    static constexpr const char* TAG = "DHT11";
    
    gpio_num_t gpio_pin_;
    float temperature_;       // 摄氏度
    float humidity_;          // 相对湿度 %
    uint32_t last_read_time_; // 上次读取时间
    bool last_read_success_;  // 上次读取是否成功
    TaskHandle_t read_task_handle_;  // 后台读取任务句柄
    
    // ==================== DHT11 时序控制 ====================
    
    // 设置 GPIO 模式
    void SetGpioMode(gpio_mode_t mode) {
        gpio_config_t config = {
            .pin_bit_mask = (1ULL << gpio_pin_),
            .mode = mode,
            .pull_up_en = GPIO_PULLUP_ENABLE,  // DHT11 需要上拉
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&config);
    }
    
    // 微秒级延时（使用 ROM 函数）
    void DelayUs(uint32_t us) {
        ets_delay_us(us);
    }
    
    // 等待 GPIO 变为指定电平，返回等待的微秒数
    int WaitForLevel(int level, int timeout_us) {
        int elapsed = 0;
        while (gpio_get_level(gpio_pin_) != level) {
            if (elapsed > timeout_us) {
                return -1;  // 超时
            }
            DelayUs(1);
            elapsed++;
        }
        return elapsed;
    }
    
    // 读取 DHT11 传感器数据（核心函数）
    bool ReadSensor() {
        uint8_t data[5] = {0};  // 40 位数据 = 5 字节
        
        // 检查初始状态
        int initial_level = gpio_get_level(gpio_pin_);
        ESP_LOGD(TAG, "Initial GPIO level: %d", initial_level);
        
        // 1. 发送起始信号（主机拉低至少 18ms）
        SetGpioMode(GPIO_MODE_OUTPUT);
        gpio_set_level(gpio_pin_, 0);
        ESP_LOGD(TAG, "Sent start signal (LOW for 20ms)");
        vTaskDelay(pdMS_TO_TICKS(20));  // 拉低 20ms
        
        // 2. 主机释放总线（拉高）
        gpio_set_level(gpio_pin_, 1);
        DelayUs(30);  // 等待 20-40us
        
        // 3. 切换为输入模式，准备接收数据
        SetGpioMode(GPIO_MODE_INPUT);
        DelayUs(10);  // 给 GPIO 模式切换一些时间
        
        // 检查切换后的电平
        int after_switch_level = gpio_get_level(gpio_pin_);
        ESP_LOGD(TAG, "After switch to input, GPIO level: %d", after_switch_level);
        
        // 4. 等待 DHT11 响应（拉低 80us）
        int wait_low = WaitForLevel(0, 100);
        if (wait_low < 0) {
            ESP_LOGE(TAG, "No response signal (low) - timeout after 100us");
            ESP_LOGE(TAG, "Check: 1) Pull-up resistor 2) Sensor power 3) Wiring");
            ESP_LOGD(TAG, "Current GPIO level: %d", gpio_get_level(gpio_pin_));
            return false;
        }
        ESP_LOGD(TAG, "Response LOW detected after %d us", wait_low);
        
        // 5. DHT11 拉高 80us
        int wait_high = WaitForLevel(1, 100);
        if (wait_high < 0) {
            ESP_LOGE(TAG, "No response signal (high) - timeout after 100us");
            return false;
        }
        ESP_LOGD(TAG, "Response HIGH detected after %d us", wait_high);
        
        // 6. 读取 40 位数据（进入临界区，禁用任务调度）
        portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
        portENTER_CRITICAL(&mux);
        
        bool read_success = true;
        int error_bit = -1;
        int error_type = 0;  // 0=无错误, 1=start超时, 2=data超时
        
        for (int i = 0; i < 40; i++) {
            // 等待数据位起始（拉低 50us）
            if (WaitForLevel(0, 150) < 0) {  // 增加到150us
                error_bit = i;
                error_type = 1;
                read_success = false;
                break;
            }
            
            // 等待数据位（拉高 26-28us = 0，拉高 70us = 1）
            if (WaitForLevel(1, 150) < 0) {  // 增加到150us
                error_bit = i;
                error_type = 2;
                read_success = false;
                break;
            }
            
            DelayUs(40);  // 延时 40us 后采样
            
            // 如果此时是高电平，则为 1
            if (gpio_get_level(gpio_pin_) == 1) {
                data[i / 8] |= (1 << (7 - (i % 8)));
            }
        }
        
        // 统一退出临界区
        portEXIT_CRITICAL(&mux);
        
        // 退出临界区后再打印日志
        if (!read_success) {
            if (error_type == 1) {
                ESP_LOGE(TAG, "Timeout waiting for bit %d start (read %d/40 bits)", error_bit, error_bit);
            } else if (error_type == 2) {
                ESP_LOGE(TAG, "Timeout waiting for bit %d data (read %d/40 bits)", error_bit, error_bit);
            }
            return false;
        }
        
        // 7. 校验和验证
        uint8_t checksum = data[0] + data[1] + data[2] + data[3];
        if (checksum != data[4]) {
            ESP_LOGE(TAG, "Checksum error: %d != %d", checksum, data[4]);
            return false;
        }
        
        // 8. 解析数据（DHT11 只有整数部分）
        humidity_ = data[0];           // 湿度整数
        temperature_ = data[2];        // 温度整数
        
        ESP_LOGI(TAG, "Read success: Temp=%.1f°C, Humi=%.1f%%", temperature_, humidity_);
        return true;
    }
    
    // ==================== MQTT 发布 ====================
    
    void PublishToMqtt() {
        MqttController* mqtt = MqttController::GetInstance();
        if (mqtt == nullptr || !mqtt->IsConnected()) {
            ESP_LOGD(TAG, "MQTT not ready, skip publishing");
            return;
        }
        
        // 发布状态数据
        char state_json[128];
        snprintf(state_json, sizeof(state_json),
                "{\"temperature\":%.1f,\"humidity\":%.1f}",
                temperature_, humidity_);
        
        mqtt->Publish(MQTT_HA_DHT11_STATE_TOPIC, state_json);
        ESP_LOGD(TAG, "Published to MQTT: %s", state_json);
    }
    
    /**
     * @brief 通过串口发送温湿度数据到上位机。
     *
     * 发送格式：
     * - saver.t2.txt="湿度值"
     * - saver.t1.txt="温度值"
     * - main.t1.txt="温度值"
     * 
     * 每行数据以 \r\n 结尾。
     */
    void PublishToUart() {
        // 获取 UartController 单例（需要在 esp32_bread_board_lcd.cc 中包含 uart_controller.h）
        UartController* uart = UartController::GetInstance();
        if (uart == nullptr) {
            ESP_LOGD(TAG, "UART not ready, skip publishing");
            return;
        }
        
        // 准备三行数据
        char line1[64];
        char line2[64];
        char line3[64];
        
        snprintf(line1, sizeof(line1), "saver.t2.txt=\"%d\"\r\n", static_cast<int>(humidity_));
        snprintf(line2, sizeof(line2), "saver.t1.txt=\"%d\"\r\n", static_cast<int>(temperature_));
        snprintf(line3, sizeof(line3), "main.t1.txt=\"%d\"\r\n", static_cast<int>(temperature_));
        
        // 发送到串口
        bool success = uart->Send(line1);
        success &= uart->Send(line2);
        success &= uart->Send(line3);
        
        if (success) {
            ESP_LOGD(TAG, "Published to UART: Temp=%.1f°C, Humi=%.1f%%", temperature_, humidity_);
        } else {
            ESP_LOGW(TAG, "Failed to publish to UART");
        }
    }
    
    void PublishHAConfig() {
        MqttController* mqtt = MqttController::GetInstance();
        if (mqtt == nullptr) {
            ESP_LOGW(TAG, "MQTT instance not available, skip HA config");
            return;
        }
        
        if (!mqtt->IsConnected()) {
            ESP_LOGW(TAG, "MQTT not connected yet, skip HA config");
            return;
        }
        
        ESP_LOGI(TAG, "Publishing HA discovery config...");
        
        // Home Assistant 温度传感器自动发现配置
        char config_json[512];
        snprintf(config_json, sizeof(config_json),
            "{\n"
            "  \"unique_id\": \"%s-dht11-temp\",\n"
            "  \"name\": \"小智温度\",\n"
            "  \"state_topic\": \"%s\",\n"
            "  \"value_template\": \"{{ value_json.temperature }}\",\n"
            "  \"unit_of_measurement\": \"°C\",\n"
            "  \"device_class\": \"temperature\",\n"
            "  \"device\": {\n"
            "    \"identifiers\": [\"%s\"],\n"
            "    \"name\": \"%s\",\n"
            "    \"model\": \"ESP32-S3\",\n"
            "    \"manufacturer\": \"XiaoZhi\",\n"
            "    \"sw_version\": \"%s\"\n"
            "  }\n"
            "}",
            DEVICE_ID,
            MQTT_HA_DHT11_STATE_TOPIC,
            DEVICE_ID,
            DEVICE_NAME,
            DEVICE_SW_VERSION
        );
        
        ESP_LOGI(TAG, "Publishing temperature config to: %s", MQTT_HA_DHT11_TEMP_CONFIG_TOPIC);
        mqtt->Publish(MQTT_HA_DHT11_TEMP_CONFIG_TOPIC, config_json);
        
        // Home Assistant 湿度传感器自动发现配置
        snprintf(config_json, sizeof(config_json),
            "{\n"
            "  \"unique_id\": \"%s-dht11-humi\",\n"
            "  \"name\": \"小智湿度\",\n"
            "  \"state_topic\": \"%s\",\n"
            "  \"value_template\": \"{{ value_json.humidity }}\",\n"
            "  \"unit_of_measurement\": \"%%\",\n"
            "  \"device_class\": \"humidity\",\n"
            "  \"device\": {\n"
            "    \"identifiers\": [\"%s\"]\n"
            "  }\n"
            "}",
            DEVICE_ID,
            MQTT_HA_DHT11_STATE_TOPIC,
            DEVICE_ID
        );
        
        ESP_LOGI(TAG, "Publishing humidity config to: %s", MQTT_HA_DHT11_HUMI_CONFIG_TOPIC);
        mqtt->Publish(MQTT_HA_DHT11_HUMI_CONFIG_TOPIC, config_json);
        
        ESP_LOGI(TAG, "✓ Published HA discovery config for temperature and humidity");
    }
    
    // ==================== 后台读取任务 ====================
    
    static void ReadTaskFunction(void* param) {
        auto* controller = static_cast<Dht11Controller*>(param);
        
        ESP_LOGI(TAG, "Background read task started");
        
        // 等待 MQTT 连接成功后发布 HA 配置（最多等待 30 秒）
        bool ha_config_published = false;
        ESP_LOGI(TAG, "Waiting for MQTT connection to publish HA config...");
        
        for (int i = 0; i < 30; i++) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            
            MqttController* mqtt = MqttController::GetInstance();
            if (mqtt != nullptr && mqtt->IsConnected()) {
                ESP_LOGI(TAG, "MQTT connected after %d seconds, publishing HA config now", i + 1);
                controller->PublishHAConfig();
                ha_config_published = true;
                break;
            }
            
            // 每 5 秒打印一次等待状态
            if ((i + 1) % 5 == 0) {
                ESP_LOGI(TAG, "Still waiting for MQTT... (%d/30s)", i + 1);
            }
        }
        
        if (!ha_config_published) {
            ESP_LOGW(TAG, "Timeout waiting for MQTT (30s), HA config not published - will retry on next read cycle");
        }
        
        while (true) {
            // 读取温湿度
            bool success = controller->ReadSensor();
            
            if (success) {
                controller->last_read_success_ = true;
                controller->last_read_time_ = xTaskGetTickCount() * portTICK_PERIOD_MS;
                
                // 发送到 MQTT
                controller->PublishToMqtt();
                
                // 发送到 UART
                controller->PublishToUart();
                
                ESP_LOGI(TAG, "Background read OK: Temp=%.1f°C, Humi=%.1f%%", 
                         controller->temperature_, controller->humidity_);
                
                // 如果之前没有发布 HA 配置，现在尝试发布
                if (!ha_config_published) {
                    MqttController* mqtt = MqttController::GetInstance();
                    if (mqtt != nullptr && mqtt->IsConnected()) {
                        ESP_LOGI(TAG, "MQTT now available, publishing HA config (retry)");
                        controller->PublishHAConfig();
                        ha_config_published = true;
                    }
                }
            } else {
                ESP_LOGW(TAG, "Background read failed, will retry in %d ms", DHT11_READ_INTERVAL_MS);
            }
            
            // 等待指定间隔
            vTaskDelay(pdMS_TO_TICKS(DHT11_READ_INTERVAL_MS));
        }
    }
    
    // ==================== MCP 工具注册 ====================
    
    void RegisterTools() {
        auto& mcp_server = McpServer::GetInstance();
        
        // GPIO 测试工具（调试用）
        mcp_server.AddTool(
            "self.sensor.test_gpio",
            "测试 DHT11 的 GPIO 引脚状态",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                int level = gpio_get_level(gpio_pin_);
                char buffer[128];
                snprintf(buffer, sizeof(buffer),
                        "{\"gpio\":%d,\"level\":%d,\"description\":\"%s\"}",
                        gpio_pin_, level, level ? "HIGH (pulled up)" : "LOW (grounded or sensor responding)");
                return std::string(buffer);
            }
        );
        
        // 读取温湿度工具（返回缓存数据）
        mcp_server.AddTool(
            "self.sensor.read_dht11",
            "读取 DHT11 温湿度传感器数据（返回最近一次读取的数据）",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                if (last_read_success_) {
                    char buffer[128];
                    snprintf(buffer, sizeof(buffer),
                            "{\"temperature\":%.1f,\"humidity\":%.1f,\"unit\":\"celsius\"}",
                            temperature_, humidity_);
                    return std::string(buffer);
                } else {
                    return "{\"error\":\"no_data\",\"message\":\"Waiting for first read\"}";
                }
            }
        );
        
        // 仅获取温度（返回缓存数据）
        mcp_server.AddTool(
            "self.sensor.get_temperature",
            "获取当前温度值（摄氏度）",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                if (last_read_success_) {
                    char buffer[32];
                    snprintf(buffer, sizeof(buffer), "%.1f", temperature_);
                    return std::string(buffer);
                } else {
                    return "N/A";
                }
            }
        );
        
        // 仅获取湿度（返回缓存数据）
        mcp_server.AddTool(
            "self.sensor.get_humidity",
            "获取当前湿度值（百分比）",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                if (last_read_success_) {
                    char buffer[32];
                    snprintf(buffer, sizeof(buffer), "%.1f", humidity_);
                    return std::string(buffer);
                } else {
                    return "N/A";
                }
            }
        );
    }
    
public:
    explicit Dht11Controller(gpio_num_t pin)
        : gpio_pin_(pin),
          temperature_(0),
          humidity_(0),
          last_read_time_(0),
          last_read_success_(false),
          read_task_handle_(nullptr) {
        
        ESP_LOGI(TAG, "Initializing DHT11 on GPIO %d...", pin);
        SetGpioMode(GPIO_MODE_INPUT);
        RegisterTools();
        
        // DHT11 上电稳定时间
        ESP_LOGI(TAG, "Waiting 2s for DHT11 to stabilize...");
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        // 首次读取（同步）
        ESP_LOGI(TAG, "Performing initial read...");
        bool success = ReadSensor();
        
        if (success) {
            last_read_success_ = true;
            last_read_time_ = xTaskGetTickCount() * portTICK_PERIOD_MS;
            ESP_LOGI(TAG, "✓ Initial read SUCCESS - Temp=%.1f°C, Humi=%.1f%%", 
                     temperature_, humidity_);
        } else {
            ESP_LOGW(TAG, "✗ Initial read failed, background task will retry");
        }
        
        // 创建后台读取任务
        BaseType_t ret = xTaskCreate(
            ReadTaskFunction,
            "dht11_read",
            DHT11_TASK_STACK_SIZE,
            this,
            DHT11_TASK_PRIORITY,
            &read_task_handle_
        );
        
        if (ret == pdPASS) {
            ESP_LOGI(TAG, "✓ DHT11 background task started (interval: %d ms)", DHT11_READ_INTERVAL_MS);
        } else {
            ESP_LOGE(TAG, "❌ Failed to create background task");
        }
        
        ESP_LOGI(TAG, "✓ DHT11 controller initialized on GPIO %d", pin);
    }
    
    ~Dht11Controller() {
        if (read_task_handle_) {
            vTaskDelete(read_task_handle_);
            ESP_LOGI(TAG, "Background task stopped");
        }
    }
};

#endif // __DHT11_CONTROLLER_H__

