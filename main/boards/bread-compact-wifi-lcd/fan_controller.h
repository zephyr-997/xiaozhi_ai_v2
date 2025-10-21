#ifndef __FAN_CONTROLLER_H__
#define __FAN_CONTROLLER_H__

#include <driver/gpio.h>
#include "mcp_server.h"
#include "esp_log.h"

class FanController {
private:
    static constexpr const char* TAG = "FanController";

    gpio_num_t fan_gpio_;
    bool is_on_;

    void ConfigureGpio() {
        gpio_config_t config = {
            .pin_bit_mask = (1ULL << fan_gpio_),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&config));
        ESP_ERROR_CHECK(gpio_set_level(fan_gpio_, 0));
    }

    std::string BuildStateJson() const {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "{\"state\":\"%s\"}", is_on_ ? "on" : "off");
        return std::string(buffer);
    }

    void RegisterTools() {
        auto& mcp_server = McpServer::GetInstance();

        mcp_server.AddTool(
            "self.fan.get_state",
            "查询风扇当前状态，返回 state=on/off",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                ESP_LOGI(TAG, "Get state -> %s", is_on_ ? "on" : "off");
                return BuildStateJson();
            }
        );

        mcp_server.AddTool(
            "self.fan.turn_on",
            "开风扇",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                is_on_ = true;
                ESP_ERROR_CHECK(gpio_set_level(fan_gpio_, 1));
                ESP_LOGI(TAG, "Fan turned on");
                return true;
            }
        );

        mcp_server.AddTool(
            "self.fan.turn_off",
            "关风扇",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                is_on_ = false;
                ESP_ERROR_CHECK(gpio_set_level(fan_gpio_, 0));
                ESP_LOGI(TAG, "Fan turned off");
                return true;
            }
        );
    }

public:
    explicit FanController(gpio_num_t fan_gpio)
        : fan_gpio_(fan_gpio), is_on_(false) {
        ConfigureGpio();
        RegisterTools();
        ESP_LOGI(TAG, "Initialized fan controller on GPIO %d", fan_gpio_);
    }
};

#endif // __FAN_CONTROLLER_H__

