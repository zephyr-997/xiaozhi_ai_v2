#ifndef __DOOR_CONTROLLER_H__
#define __DOOR_CONTROLLER_H__

#include <driver/gpio.h>
#include "mcp_server.h"
#include "esp_log.h"

class DoorController {
private:
    static constexpr const char* TAG = "DoorController";

    gpio_num_t door_gpio_;
    bool is_open_;

    void ConfigureGpio() {
        gpio_config_t config = {
            .pin_bit_mask = (1ULL << door_gpio_),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&config));
        ESP_ERROR_CHECK(gpio_set_level(door_gpio_, 0));
    }

    std::string BuildStateJson() const {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "{\"state\":\"%s\"}", is_open_ ? "open" : "closed");
        return std::string(buffer);
    }

    void RegisterTools() {
        auto& mcp_server = McpServer::GetInstance();

        mcp_server.AddTool(
            "self.door.get_state",
            "查询门的当前状态，返回 state=open/closed",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                ESP_LOGI(TAG, "Get state -> %s", is_open_ ? "open" : "closed");
                return BuildStateJson();
            }
        );

        mcp_server.AddTool(
            "self.door.open",
            "开门",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                is_open_ = true;
                ESP_ERROR_CHECK(gpio_set_level(door_gpio_, 1));
                ESP_LOGI(TAG, "Door opened");
                return true;
            }
        );

        mcp_server.AddTool(
            "self.door.close",
            "关门",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                is_open_ = false;
                ESP_ERROR_CHECK(gpio_set_level(door_gpio_, 0));
                ESP_LOGI(TAG, "Door closed");
                return true;
            }
        );
    }

public:
    explicit DoorController(gpio_num_t door_gpio)
        : door_gpio_(door_gpio), is_open_(false) {
        ConfigureGpio();
        RegisterTools();
        ESP_LOGI(TAG, "Initialized door controller on GPIO %d", door_gpio_);
    }
};

#endif // __DOOR_CONTROLLER_H__

