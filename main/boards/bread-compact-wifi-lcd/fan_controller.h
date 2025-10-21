#ifndef __FAN_CONTROLLER_H__
#define __FAN_CONTROLLER_H__

#include <driver/gpio.h>
#include <driver/ledc.h>
#include "mcp_server.h"
#include "esp_log.h"

class FanController {
private:
    static constexpr const char* TAG = "FanController";

    gpio_num_t fan_gpio_;
    uint8_t speed_level_;  // 0=关闭, 1=一档(85), 2=二档(170), 3=三档(255)
    
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
    }
    
    std::string BuildStateJson() const {
        char buffer[64];
        const char* state_str = (speed_level_ == 0) ? "off" : "on";
        snprintf(buffer, sizeof(buffer), 
                "{\"state\":\"%s\",\"level\":%d}", 
                state_str, speed_level_);
        return std::string(buffer);
    }

    void RegisterTools() {
        auto& mcp_server = McpServer::GetInstance();

        mcp_server.AddTool(
            "self.fan.get_state",
            "查询风扇当前状态，返回 state(on/off) 和 level(0-3档位)",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                ESP_LOGI(TAG, "Get state -> level %d", speed_level_);
                return BuildStateJson();
            }
        );

        mcp_server.AddTool(
            "self.fan.set_speed",
            "设置风扇档位：1=一档(33%), 2=二档(67%), 3=三档(100%)",
            PropertyList({Property("level", kPropertyTypeInteger, 1, 3)}),
            [this](const PropertyList& properties) -> ReturnValue {
                int level = properties["level"].value<int>();
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
                SetSpeed(0);
                ESP_LOGI(TAG, "Fan turned off");
                return true;
            }
        );
    }

public:
    explicit FanController(gpio_num_t fan_gpio)
        : fan_gpio_(fan_gpio), speed_level_(0) {
        ConfigurePwm();
        RegisterTools();
        ESP_LOGI(TAG, "Initialized 3-speed PWM fan controller on GPIO %d (25kHz)", fan_gpio_);
    }
};

#endif // __FAN_CONTROLLER_H__

