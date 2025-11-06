#ifndef __LAMP_CONTROLLER_H__
#define __LAMP_CONTROLLER_H__

#include <driver/gpio.h>
#include "mcp_server.h"
#include "mqtt_controller.h"
#include "config.h"
#include "esp_log.h"

// 前向声明，避免循环依赖
class UartController;

class LampController {
private:
    static constexpr const char* TAG = "LampController";
    inline static LampController* instance_ = nullptr;

    gpio_num_t lamp_gpio_;
    bool power_state_;  // true=开, false=关
    bool ha_initialized_;

    void InitializeGpio() {
        // 配置 GPIO 为输出模式
        gpio_config_t config = {
            .pin_bit_mask = (1ULL << lamp_gpio_),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&config));
        gpio_set_level(lamp_gpio_, 0);  // 初始状态为关闭
    }
    
    // 发送 Home Assistant 配置消息（自动发现）
    void PublishHAConfig() {
        MqttController* mqtt = MqttController::GetInstance();
        if (mqtt == nullptr || !mqtt->IsConnected()) {
            ESP_LOGW(TAG, "MQTT not ready, skip HA config");
            return;
        }
        
        // 构建 Home Assistant 自动发现配置 JSON
        char config_json[1024];
        snprintf(config_json, sizeof(config_json),
            "{\n"
            "  \"unique_id\": \"%s-lamp\",\n"
            "  \"name\": \"小智灯光\",\n"
            "  \"icon\": \"mdi:lightbulb\",\n"
            "  \n"
            "  \"command_topic\": \"%s\",\n"
            "  \"state_topic\": \"%s\",\n"
            "  \"state_value_template\": \"{{ value_json.state }}\",\n"
            "  \n"
            "  \"payload_on\": \"ON\",\n"
            "  \"payload_off\": \"OFF\",\n"
            "  \n"
            "  \"device\": {\n"
            "    \"identifiers\": [\"%s\"],\n"
            "    \"name\": \"%s\",\n"
            "    \"model\": \"ESP32-S3\",\n"
            "    \"manufacturer\": \"XiaoZhi\",\n"
            "    \"sw_version\": \"%s\"\n"
            "  }\n"
            "}",
            DEVICE_ID,
            MQTT_HA_LAMP_COMMAND_TOPIC,
            MQTT_HA_LAMP_STATE_TOPIC,
            DEVICE_ID,
            DEVICE_NAME,
            DEVICE_SW_VERSION
        );
        
        mqtt->Publish(MQTT_HA_LAMP_CONFIG_TOPIC, config_json);
        ESP_LOGI(TAG, "Published HA config to %s", MQTT_HA_LAMP_CONFIG_TOPIC);
    }
    
    // 发送灯光状态到 MQTT（符合 HA 标准格式）
    void PublishLampState() {
        MqttController* mqtt = MqttController::GetInstance();
        if (mqtt == nullptr || !mqtt->IsConnected()) {
            ESP_LOGD(TAG, "MQTT not ready, skip state publish");
            return;
        }
        
        char state_json[64];
        const char* state_str = power_state_ ? "ON" : "OFF";
        
        snprintf(state_json, sizeof(state_json),
            "{\"state\":\"%s\"}",
            state_str
        );
        
        mqtt->Publish(MQTT_HA_LAMP_STATE_TOPIC, state_json);
        ESP_LOGI(TAG, "Published lamp state: %s", state_json);
    }

    // 串口反馈函数声明（实现在 uart_controller.h 包含后）
    void SendLampUartFeedback(bool state);

    void SetPower(bool state) {
        power_state_ = state;
        gpio_set_level(lamp_gpio_, state ? 1 : 0);
        
        // 发送 MQTT 状态更新
        PublishLampState();
        
        // 发送串口状态反馈（实际实现在包含 uart_controller.h 后）
        SendLampUartFeedback(state);
        
        ESP_LOGI(TAG, "Lamp %s", state ? "ON" : "OFF");
    }
    

    
    // 处理 Home Assistant 发来的开关命令
    void HandleCommand(const std::string& payload) {
        ESP_LOGI(TAG, "Received command: %s", payload.c_str());
        
        if (payload == "ON") {
            SetPower(true);
            ESP_LOGI(TAG, "Lamp turned ON via MQTT");
        } else if (payload == "OFF") {
            SetPower(false);
            ESP_LOGI(TAG, "Lamp turned OFF via MQTT");
        } else {
            ESP_LOGW(TAG, "Unknown command: %s", payload.c_str());
        }
    }
    
    // 订阅 MQTT 命令主题
    void SubscribeCommands() {
        MqttController* mqtt = MqttController::GetInstance();
        if (mqtt == nullptr || !mqtt->IsConnected()) {
            ESP_LOGW(TAG, "MQTT not ready, skip subscribe");
            return;
        }
        
        // 订阅开关命令
        mqtt->Subscribe(MQTT_HA_LAMP_COMMAND_TOPIC, 
            [this](const std::string& topic, const std::string& payload) {
                this->HandleCommand(payload);
            });
        ESP_LOGI(TAG, "Subscribed to %s", MQTT_HA_LAMP_COMMAND_TOPIC);
    }
    
    std::string BuildStateJson() const {
        char buffer[64];
        const char* state_str = power_state_ ? "ON" : "OFF";
        snprintf(buffer, sizeof(buffer), 
                "{\"state\":\"%s\"}", 
                state_str);
        return std::string(buffer);
    }

    void RegisterTools() {
        auto& mcp_server = McpServer::GetInstance();

        mcp_server.AddTool(
            "self.lamp.get_state",
            "查询灯光当前状态，返回 state(ON/OFF)",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                ESP_LOGI(TAG, "Get state -> %s", power_state_ ? "ON" : "OFF");
                return BuildStateJson();
            }
        );

        mcp_server.AddTool(
            "self.lamp.turn_on",
            "打开灯光",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                InitializeHaIntegration();
                SetPower(true);
                ESP_LOGI(TAG, "Lamp turned on via MCP");
                return true;
            }
        );

        mcp_server.AddTool(
            "self.lamp.turn_off",
            "关闭灯光",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                InitializeHaIntegration();
                SetPower(false);
                ESP_LOGI(TAG, "Lamp turned off via MCP");
                return true;
            }
        );
    }

public:
    explicit LampController(gpio_num_t lamp_gpio)
        : lamp_gpio_(lamp_gpio), power_state_(false), ha_initialized_(false) {
        InitializeGpio();
        RegisterTools();
        ESP_LOGI(TAG, "Initialized lamp controller on GPIO %d", lamp_gpio_);
        instance_ = this;
    }

    /**
     * @brief 获取当前灯光控制器单例。
     */
    static LampController* GetInstance() {
        return instance_;
    }

    /**
     * @brief 直接打开灯光，供外设联动调用。
     */
    bool TurnOnDirect() {
        InitializeHaIntegration();
        SetPower(true);
        ESP_LOGI(TAG, "Lamp turned on via direct command");
        return true;
    }

    /**
     * @brief 直接关闭灯光，供外设联动调用。
     */
    bool TurnOffDirect() {
        InitializeHaIntegration();
        SetPower(false);
        ESP_LOGI(TAG, "Lamp turned off via direct command");
        return true;
    }

    bool InitializeHaIntegration() {
        if (ha_initialized_) {
            return true;
        }

        MqttController* mqtt = MqttController::GetInstance();
        if (mqtt == nullptr || !mqtt->IsConnected()) {
            ESP_LOGW(TAG, "MQTT not ready, skip HA integration for lamp");
            return false;
        }

        PublishHAConfig();
        SubscribeCommands();
        PublishLampState();
        ha_initialized_ = true;
        ESP_LOGI(TAG, "Lamp HA integration initialized");
        return true;
    }
};

#endif // __LAMP_CONTROLLER_H__

