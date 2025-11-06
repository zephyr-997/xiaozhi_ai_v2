#ifndef __FAN_CONTROLLER_H__
#define __FAN_CONTROLLER_H__

#include <cstdlib>
#include <driver/gpio.h>
#include <driver/ledc.h>
#include "mcp_server.h"
#include "mqtt_controller.h"
#include "config.h"
#include "esp_log.h"

// 前向声明，避免与 uart_controller.h 循环依赖
class UartController;

class FanController {
private:
    static constexpr const char* TAG = "FanController";
    inline static FanController* instance_ = nullptr;

    gpio_num_t fan_gpio_;
    uint8_t speed_level_;  // 0=关闭, 1=一档(33%), 2=二档(67%), 3=三档(100%)
    bool ha_initialized_;
    
    static constexpr uint8_t SPEED_OFF = 0;
    static constexpr uint8_t SPEED_LOW = 85;     // 一档 ~33%
    static constexpr uint8_t SPEED_MED = 170;    // 二档 ~67%
    static constexpr uint8_t SPEED_HIGH = 255;   // 三档 100%

    void ConfigurePwm() {
        // 配置 LEDC 定时器
        ledc_timer_config_t timer_config = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .duty_resolution = LEDC_TIMER_8_BIT,  // 8位分辨率 (0-255)
            .timer_num = LEDC_TIMER_1,
            .freq_hz = 25000,  // 25kHz PWM 频率，适合风扇控制
            .clk_cfg = LEDC_AUTO_CLK
        };
        ESP_ERROR_CHECK(ledc_timer_config(&timer_config));

        // 配置 LEDC 通道
        ledc_channel_config_t channel_config = {
            .gpio_num = fan_gpio_,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = LEDC_CHANNEL_1,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = LEDC_TIMER_1,
            .duty = 0,  // 初始占空比为 0
            .hpoint = 0
        };
        ESP_ERROR_CHECK(ledc_channel_config(&channel_config));
    }
    
    // 发送 Home Assistant 配置消息（自动发现）
    void PublishHAConfig() {
        MqttController* mqtt = MqttController::GetInstance();
        if (mqtt == nullptr || !mqtt->IsConnected()) {
            ESP_LOGW(TAG, "MQTT not ready, skip HA config");
            return;
        }
        
        // 构建 Home Assistant 自动发现配置 JSON（格式化，便于调试）
        char config_json[1024];
        snprintf(config_json, sizeof(config_json),
            "{\n"
            "  \"unique_id\": \"%s-fan\",\n"
            "  \"name\": \"小智风扇\",\n"
            "  \"icon\": \"mdi:fan\",\n"
            "  \n"
            "  \"command_topic\": \"%s\",\n"
            "  \"state_topic\": \"%s\",\n"
            "  \"state_value_template\": \"{{ value_json.state }}\",\n"
            "  \n"
            "  \"payload_on\": \"ON\",\n"
            "  \"payload_off\": \"OFF\",\n"
            "  \n"
            "  \"percentage_command_topic\": \"%s\",\n"
            "  \"percentage_state_topic\": \"%s\",\n"
            "  \"percentage_value_template\": \"{{ value_json.speed }}\",\n"
            "  \n"
            "  \"speed_range_min\": 1,\n"
            "  \"speed_range_max\": 3,\n"
            "  \n"
            // "  \"json_attributes_topic\": \"%s\",\n"  // 实体属性（可选，已注释）
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
            MQTT_HA_FAN_COMMAND_TOPIC,
            MQTT_HA_FAN_STATE_TOPIC,
            MQTT_HA_FAN_PERCENTAGE_COMMAND_TOPIC,
            MQTT_HA_FAN_STATE_TOPIC,
            // MQTT_HA_FAN_ATTRIBUTES_TOPIC,  // 实体属性主题（已注释）
            DEVICE_ID,
            DEVICE_NAME,
            DEVICE_SW_VERSION
        );
        
        mqtt->Publish(MQTT_HA_FAN_CONFIG_TOPIC, config_json);
        ESP_LOGI(TAG, "Published HA config to %s", MQTT_HA_FAN_CONFIG_TOPIC);
    }
    
    // 发送风扇状态到 MQTT（符合 HA 标准格式）
   
    void PublishFanState() {
        MqttController* mqtt = MqttController::GetInstance();
        if (mqtt == nullptr || !mqtt->IsConnected()) {
            ESP_LOGD(TAG, "MQTT not ready, skip state publish");
            return;
        }
        
        char state_json[128];
        const char* state_str = (speed_level_ == 0) ? "OFF" : "ON";
        
        // 返回档位 (0-3)，与 HA 发送的格式保持一致
        snprintf(state_json, sizeof(state_json),
            "{\"state\":\"%s\",\"speed\":%d}",
            state_str, speed_level_
        );
        
        mqtt->Publish(MQTT_HA_FAN_STATE_TOPIC, state_json);
        ESP_LOGI(TAG, "Published fan state: %s", state_json);
    }
    
    // 发送风扇属性到 MQTT（实体属性 - 可选，已注释）
    /*
    void PublishFanAttributes() {
        MqttController* mqtt = MqttController::GetInstance();
        if (mqtt == nullptr || !mqtt->IsConnected()) {
            ESP_LOGD(TAG, "MQTT not ready, skip attributes publish");
            return;
        }
        
        char attr_json[512];
        snprintf(attr_json, sizeof(attr_json),
            "{\n"
            "  \"gpio\": %d,\n"
            "  \"pwm_freq\": 25000,\n"
            "  \"speed_levels\": \"0=关闭, 1=一档(33%%), 2=二档(67%%), 3=三档(100%%)\",\n"
            "  \"speed_range\": \"0-3\",\n"
            "  \"chip\": \"ESP32-S3\",\n"
            "  \"duty_resolution\": 8\n"
            "}",
            fan_gpio_
        );
        
        mqtt->Publish(MQTT_HA_FAN_ATTRIBUTES_TOPIC, attr_json);
        ESP_LOGD(TAG, "Published fan attributes");
    }*/

    // 发送串口反馈（在 uart_controller.h 中实现）
    void SendFanUartFeedback(bool is_on);

    void SetSpeed(uint8_t level) {
        if (level > 3) {
            level = 3;
        }
        speed_level_ = level;
        
        uint8_t duty = SPEED_OFF;
        switch (level) {
            case 1: duty = SPEED_LOW; break;
            case 2: duty = SPEED_MED; break;
            case 3: duty = SPEED_HIGH; break;
            default: duty = SPEED_OFF; break;
        }
        
        ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty));
        ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1));
        
        // 发送 MQTT 状态更新
        PublishFanState();

        // 发送串口状态反馈
        SendFanUartFeedback(speed_level_ > 0);
    }
    
    // 处理 Home Assistant 发来的开关命令
    void HandleCommand(const std::string& payload) {
        ESP_LOGI(TAG, "Received command: %s", payload.c_str());
        
        if (payload == "ON") {
            SetSpeed(2);  // 默认二档
            ESP_LOGI(TAG, "Fan turned ON via MQTT");
        } else if (payload == "OFF") {
            SetSpeed(0);
            ESP_LOGI(TAG, "Fan turned OFF via MQTT");
        } else {
            ESP_LOGW(TAG, "Unknown command: %s", payload.c_str());
        }
    }
    
    // 处理 Home Assistant 发来的速度命令（直接接收档位 0-3）
    void HandlePercentageCommand(const std::string& payload) {
        ESP_LOGI(TAG, "Received speed level: %s", payload.c_str());
        
        int level = atoi(payload.c_str());
        
        // HA 发送的是档位值：0=关闭, 1=一档, 2=二档, 3=三档
        if (level < 0 || level > 3) {
            ESP_LOGW(TAG, "Invalid speed level: %d, must be 0-3", level);
            return;
        }
        
        SetSpeed(level);
        ESP_LOGI(TAG, "Fan speed set to level %d", level);
    }
    
    // 订阅 MQTT 命令主题
    void SubscribeCommands() {
        MqttController* mqtt = MqttController::GetInstance();
        if (mqtt == nullptr || !mqtt->IsConnected()) {
            ESP_LOGW(TAG, "MQTT not ready, skip subscribe");
            return;
        }
        
        // 订阅开关命令
        mqtt->Subscribe(MQTT_HA_FAN_COMMAND_TOPIC, 
            [this](const std::string& topic, const std::string& payload) {
                this->HandleCommand(payload);
            });
        ESP_LOGI(TAG, "Subscribed to %s", MQTT_HA_FAN_COMMAND_TOPIC);
        
        // 订阅速度命令
        mqtt->Subscribe(MQTT_HA_FAN_PERCENTAGE_COMMAND_TOPIC,
            [this](const std::string& topic, const std::string& payload) {
                this->HandlePercentageCommand(payload);
            });
        ESP_LOGI(TAG, "Subscribed to %s", MQTT_HA_FAN_PERCENTAGE_COMMAND_TOPIC);
    }
    
    std::string BuildStateJson() const {
        char buffer[128];
        const char* state_str = (speed_level_ == 0) ? "OFF" : "ON";
        snprintf(buffer, sizeof(buffer), 
                "{\"state\":\"%s\",\"speed\":%d}", 
                state_str, speed_level_);
        return std::string(buffer);
    }

    void RegisterTools() {
        auto& mcp_server = McpServer::GetInstance();

        mcp_server.AddTool(
            "self.fan.get_state",
            "查询风扇当前状态，返回 state(ON/OFF) 和 speed(0-3档位)",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                ESP_LOGI(TAG, "Get state -> speed level %d", speed_level_);
                return BuildStateJson();
            }
        );

        mcp_server.AddTool(
            "self.fan.set_speed",
            "设置风扇档位：0=关闭, 1=一档(33%), 2=二档(67%), 3=三档(100%)",
            PropertyList({Property("level", kPropertyTypeInteger, 0, 3)}),
            [this](const PropertyList& properties) -> ReturnValue {
                int level = properties["level"].value<int>();
                InitializeHaIntegration();
                SetSpeed(level);
                ESP_LOGI(TAG, "Fan set to level %d", level);
                return true;
            }
        );

        mcp_server.AddTool(
            "self.fan.turn_on",
            "开风扇，默认二档(67%)",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                InitializeHaIntegration();
                SetSpeed(2);  // 默认二档
                ESP_LOGI(TAG, "Fan turned on (level 2)");
                return true;
            }
        );

        mcp_server.AddTool(
            "self.fan.turn_off",
            "关风扇",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                InitializeHaIntegration();
                SetSpeed(0);
                ESP_LOGI(TAG, "Fan turned off");
                return true;
            }
        );
    }

public:
    explicit FanController(gpio_num_t fan_gpio)
        : fan_gpio_(fan_gpio), speed_level_(0), ha_initialized_(false) {
        ConfigurePwm();
        RegisterTools();
        ESP_LOGI(TAG, "Initialized 3-speed PWM fan controller on GPIO %d (25kHz)", fan_gpio_);
        instance_ = this;
    }

    /**
     * @brief 获取当前风扇控制器单例。
     */
    static FanController* GetInstance() {
        return instance_;
    }

    /**
     * @brief 设置风扇档位，用于外设联动。
     *
     * @param level 目标档位，0 表示关闭，1-3 表示不同风速。
     * @return true 始终返回成功。
     */
    bool SetSpeedLevel(uint8_t level) {
        InitializeHaIntegration();
        SetSpeed(level);
        ESP_LOGI(TAG, "Fan speed set via direct command to level %d", speed_level_);
        return true;
    }

    /**
     * @brief 以默认二档速度开启风扇，用于外设联动。
     */
    bool TurnOnDefaultLevel() {
        InitializeHaIntegration();
        SetSpeed(2);
        ESP_LOGI(TAG, "Fan turned on (default level 2) via direct command");
        return true;
    }

    /**
     * @brief 关闭风扇，用于外设联动。
     */
    bool TurnOffDirect() {
        InitializeHaIntegration();
        SetSpeed(0);
        ESP_LOGI(TAG, "Fan turned off via direct command");
        return true;
    }

    bool InitializeHaIntegration() {
        if (ha_initialized_) {
            return true;
        }

        MqttController* mqtt = MqttController::GetInstance();
        if (mqtt == nullptr || !mqtt->IsConnected()) {
            ESP_LOGW(TAG, "MQTT not ready, skip HA integration for fan");
            return false;
        }

        PublishHAConfig();
        SubscribeCommands();
        PublishFanState();
        ha_initialized_ = true;
        ESP_LOGI(TAG, "Fan HA integration initialized");
        return true;
    }
};

#endif // __FAN_CONTROLLER_H__

