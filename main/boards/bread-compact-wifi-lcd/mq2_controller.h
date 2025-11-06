#ifndef __MQ2_CONTROLLER_H__
#define __MQ2_CONTROLLER_H__

#include <cmath>
#include <string>

#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_log.h>

#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mcp_server.h"
#include "mqtt_controller.h"
#include "uart_controller.h"
#include "fan_controller.h"
#include "lamp_controller.h"

class Mq2Controller {
private:
    static constexpr const char* TAG = "MQ2";

    adc_unit_t adc_unit_;
    adc_channel_t adc_channel_;
    adc_oneshot_unit_handle_t adc_handle_;
    adc_cali_handle_t cali_handle_;
    bool calibrated_;
    uint16_t raw_value_;
    float voltage_;
    float ppm_;
    bool sensor_alert_;
    uint32_t last_read_time_;
    bool last_read_success_;
    TaskHandle_t read_task_handle_;
    bool last_alert_state_;
    bool ha_config_initialized_;

    void InitAdc() {
        adc_oneshot_unit_init_cfg_t init_config = {
            .unit_id = adc_unit_,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle_));

        adc_oneshot_chan_cfg_t chan_config = {
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle_, adc_channel_, &chan_config));

        calibrated_ = false;
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = adc_unit_,
            .chan = adc_channel_,
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };

        if (adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle_) == ESP_OK) {
            calibrated_ = true;
            ESP_LOGI(TAG, "ADC calibration succeeded (curve fitting)");
        } else {
            cali_handle_ = nullptr;
            ESP_LOGW(TAG, "ADC calibration not available, using raw approximation");
        }

        ESP_LOGI(TAG, "ADC initialized on unit %d, channel %d", static_cast<int>(init_config.unit_id), static_cast<int>(adc_channel_));
    }

    bool ReadSensor() {
        uint32_t sum = 0;
        for (int i = 0; i < MQ2_READ_TIMES; i++) {
            int adc_raw = 0;
            ESP_ERROR_CHECK(adc_oneshot_read(adc_handle_, adc_channel_, &adc_raw));
            sum += adc_raw;
            vTaskDelay(pdMS_TO_TICKS(5));
        }
        raw_value_ = static_cast<uint16_t>(sum / MQ2_READ_TIMES);

        int voltage_mv = 0;
        if (calibrated_) {
            esp_err_t ret = adc_cali_raw_to_voltage(cali_handle_, raw_value_, &voltage_mv);
            if (ret != ESP_OK) {
                voltage_mv = static_cast<int>((static_cast<float>(raw_value_) / 4095.0f) * 3300.0f);
            }
        } else {
            voltage_mv = static_cast<int>((static_cast<float>(raw_value_) / 4095.0f) * 3300.0f);
        }
        voltage_ = voltage_mv / 1000.0f;

        static constexpr float kMq2MinVoltage = 0.01f;
        static constexpr float kMq2SupplyVoltage = 5.0f;
        static constexpr float kMq2LoadResistanceKOhm = 1.0f;
        static constexpr float kMq2AdcSaturationVoltage = 3.2f;
        static constexpr float kMq2BaselinePpm = 8.0f;
        static constexpr float kMq2PpmExponent = 2.0f;
        static constexpr float kMq2MaxPpm = 999999.0f;

        if (voltage_ > kMq2MinVoltage) {
            float effective_voltage = voltage_;
            if (effective_voltage >= kMq2AdcSaturationVoltage) {
                ESP_LOGW(TAG, "ADC reading near saturation: %.2fV (raw=%d)", voltage_, raw_value_);
                effective_voltage = kMq2AdcSaturationVoltage;
            }

            float rs = 0.0f;
            if (effective_voltage > kMq2MinVoltage) {
                rs = kMq2LoadResistanceKOhm * (kMq2SupplyVoltage - effective_voltage) / effective_voltage;
            }

            if (rs > 0.0f) {
                float ratio = MQ2_R0_VALUE / rs;
                ppm_ = kMq2BaselinePpm * std::pow(ratio, kMq2PpmExponent);
                if (ppm_ > kMq2MaxPpm) {
                    ppm_ = kMq2MaxPpm;
                }
                sensor_alert_ = (ppm_ > MQ2_ALERT_THRESHOLD);
                ESP_LOGD(TAG, "Derived Rs=%.2fkΩ, ratio=%.2f", rs, ratio);
            } else {
                ppm_ = 0.0f;
                sensor_alert_ = false;
            }
        } else {
            ppm_ = 0.0f;
            sensor_alert_ = false;
        }

        ESP_LOGI(TAG, "MQ-2 Read: Raw=%d, Voltage=%.2fV, PPM=%.2f, Alert=%s",
                 raw_value_, voltage_, ppm_, sensor_alert_ ? "YES" : "NO");

        return true;
    }

    void PublishToMqtt() {
        MqttController* mqtt = MqttController::GetInstance();
        if (mqtt == nullptr || !mqtt->IsConnected()) {
            ESP_LOGD(TAG, "MQTT not ready, skip publishing");
            return;
        }

        char state_json[256];
        snprintf(state_json, sizeof(state_json),
                 "{\"raw\":%d,\"voltage\":%.2f,\"ppm\":%.2f,\"alert\":%s}",
                 raw_value_, voltage_, ppm_, sensor_alert_ ? "true" : "false");

        mqtt->Publish(MQTT_HA_MQ2_STATE_TOPIC, state_json);
        ESP_LOGD(TAG, "Published to MQTT: %s", state_json);
    }

    void PublishToUart() {
        UartController* uart = UartController::GetInstance();
        if (uart == nullptr) {
            ESP_LOGD(TAG, "UART not ready, skip publishing");
            return;
        }

        char line1[64];
        char line2[64];

        snprintf(line1, sizeof(line1), "saver.t3.txt=\"%.0f\"\r\n", ppm_);
        snprintf(line2, sizeof(line2), "main.t2.txt=\"%.0f\"\r\n", ppm_);

        bool success = uart->Send(line1);
        success &= uart->Send(line2);

        if (success) {
            ESP_LOGD(TAG, "Published to UART: PPM=%.2f", ppm_);
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

        char config_json[512];
        snprintf(config_json, sizeof(config_json),
                 "{\n"
                 "  \"unique_id\": \"%s-mq2-smoke\",\n"
                 "  \"name\": \"小智烟雾传感器\",\n"
                 "  \"state_topic\": \"%s\",\n"
                 "  \"value_template\": \"{{ value_json.ppm }}\",\n"
                 "  \"unit_of_measurement\": \"ppm\",\n"
                 "  \"device_class\": \"volatile_organic_compounds\",\n"
                 "  \"icon\": \"mdi:smoke-detector\",\n"
                 "  \"device\": {\n"
                 "    \"identifiers\": [\"%s\"],\n"
                 "    \"name\": \"%s\",\n"
                 "    \"model\": \"ESP32-S3\",\n"
                 "    \"manufacturer\": \"XiaoZhi\",\n"
                 "    \"sw_version\": \"%s\"\n"
                 "  }\n"
                 "}",
                 DEVICE_ID,
                 MQTT_HA_MQ2_STATE_TOPIC,
                 DEVICE_ID,
                 DEVICE_NAME,
                 DEVICE_SW_VERSION);

        ESP_LOGI(TAG, "Publishing smoke sensor config to: %s", MQTT_HA_MQ2_CONFIG_TOPIC);
        mqtt->Publish(MQTT_HA_MQ2_CONFIG_TOPIC, config_json);

        snprintf(config_json, sizeof(config_json),
                 "{\n"
                 "  \"unique_id\": \"%s-mq2-alert\",\n"
                 "  \"name\": \"小智烟雾告警\",\n"
                 "  \"state_topic\": \"%s\",\n"
                 "  \"value_template\": \"{{ value_json.alert }}\",\n"
                 "  \"payload_on\": \"True\",\n"
                 "  \"payload_off\": \"False\",\n"
                 "  \"device_class\": \"smoke\",\n"
                 "  \"device\": {\n"
                 "    \"identifiers\": [\"%s\"]\n"
                 "  }\n"
                 "}",
                 DEVICE_ID,
                 MQTT_HA_MQ2_STATE_TOPIC,
                 DEVICE_ID);

        ESP_LOGI(TAG, "Publishing smoke alert config to: %s", MQTT_HA_MQ2_ALERT_CONFIG_TOPIC);
        mqtt->Publish(MQTT_HA_MQ2_ALERT_CONFIG_TOPIC, config_json);

        ESP_LOGI(TAG, "✓ Published HA discovery config for MQ-2 smoke sensor");
    }

    static void ReadTaskFunction(void* param) {
        auto* controller = static_cast<Mq2Controller*>(param);

        ESP_LOGI(TAG, "Background read task started");

        controller->InitializeHaIntegration();

        while (true) {
            bool success = controller->ReadSensor();

            if (success) {
                controller->last_read_success_ = true;
                controller->last_read_time_ = xTaskGetTickCount() * portTICK_PERIOD_MS;

                controller->PublishToMqtt();
                controller->PublishToUart();
                controller->InitializeHaIntegration();

                bool current_alert = controller->sensor_alert_;

                if (current_alert && !controller->last_alert_state_) {
                    ESP_LOGW(TAG, "Smoke alert detected, activating fan at max speed");
                    FanController* fan = FanController::GetInstance();
                    if (fan != nullptr) {
                        fan->SetSpeedLevel(3);
                    } else {
                        ESP_LOGW(TAG, "FanController not available for smoke alert handling");
                    }

                    LampController* lamp = LampController::GetInstance();
                    if (lamp != nullptr) {
                        lamp->TurnOnDirect();
                    } else {
                        ESP_LOGW(TAG, "LampController not available for smoke alert handling");
                    }
                }

                controller->last_alert_state_ = current_alert;

                ESP_LOGI(TAG, "Background read OK: PPM=%.2f, Alert=%s",
                         controller->ppm_, controller->sensor_alert_ ? "YES" : "NO");
            } else {
                ESP_LOGW(TAG, "Background read failed, will retry in %d ms", MQ2_READ_INTERVAL_MS);
            }

            vTaskDelay(pdMS_TO_TICKS(MQ2_READ_INTERVAL_MS));
        }
    }

    void RegisterTools() {
        auto& mcp_server = McpServer::GetInstance();

        mcp_server.AddTool(
            "self.sensor.read_mq2",
            "读取 MQ-2 烟雾传感器数据（返回最近一次读取的数据）",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                if (last_read_success_) {
                    char buffer[256];
                    snprintf(buffer, sizeof(buffer),
                             "{\"raw\":%d,\"voltage\":%.2f,\"ppm\":%.2f,\"alert\":%s}",
                             raw_value_, voltage_, ppm_, sensor_alert_ ? "true" : "false");
                    return std::string(buffer);
                }
                return "{\"error\":\"no_data\",\"message\":\"Waiting for first read\"}";
            }
        );

        mcp_server.AddTool(
            "self.sensor.get_smoke_ppm",
            "获取当前烟雾浓度 PPM 值",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                if (last_read_success_) {
                    char buffer[32];
                    snprintf(buffer, sizeof(buffer), "%.2f", ppm_);
                    return std::string(buffer);
                }
                return "N/A";
            }
        );

        mcp_server.AddTool(
            "self.sensor.get_smoke_alert",
            "获取烟雾告警状态（是否超过阈值）",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                if (last_read_success_) {
                    return sensor_alert_ ? "ALERT" : "NORMAL";
                }
                return "N/A";
            }
        );
    }

public:
    explicit Mq2Controller(adc_unit_t adc_unit, adc_channel_t adc_channel)
        : adc_unit_(adc_unit),
          adc_channel_(adc_channel),
          adc_handle_(nullptr),
          cali_handle_(nullptr),
          calibrated_(false),
          raw_value_(0),
          voltage_(0.0f),
          ppm_(0.0f),
          sensor_alert_(false),
          last_read_time_(0),
          last_read_success_(false),
          read_task_handle_(nullptr),
          last_alert_state_(false),
          ha_config_initialized_(false) {

        ESP_LOGI(TAG, "Initializing MQ-2 on ADC unit %d channel %d...", adc_unit, adc_channel);
        InitAdc();
        RegisterTools();

        ESP_LOGI(TAG, "Waiting %d ms for MQ-2 to preheat...", MQ2_PREHEAT_TIME_MS);
        vTaskDelay(pdMS_TO_TICKS(MQ2_PREHEAT_TIME_MS));

        ESP_LOGI(TAG, "Performing initial read...");
        bool success = ReadSensor();

        if (success) {
            last_read_success_ = true;
            last_read_time_ = xTaskGetTickCount() * portTICK_PERIOD_MS;
            ESP_LOGI(TAG, "✓ Initial read SUCCESS - PPM=%.2f", ppm_);
        } else {
            ESP_LOGW(TAG, "✗ Initial read failed, background task will retry");
        }

        BaseType_t ret = xTaskCreate(
            ReadTaskFunction,
            "mq2_read",
            MQ2_TASK_STACK_SIZE,
            this,
            MQ2_TASK_PRIORITY,
            &read_task_handle_);

        if (ret == pdPASS) {
            ESP_LOGI(TAG, "✓ MQ-2 background task started (interval: %d ms)", MQ2_READ_INTERVAL_MS);
        } else {
            ESP_LOGE(TAG, "❌ Failed to create background task");
        }

        ESP_LOGI(TAG, "✓ MQ-2 controller initialized on ADC channel %d", adc_channel);
    }

    bool InitializeHaIntegration() {
        if (ha_config_initialized_) {
            return true;
        }

        MqttController* mqtt = MqttController::GetInstance();
        if (mqtt == nullptr || !mqtt->IsConnected()) {
            ESP_LOGW(TAG, "MQTT not ready, skip HA integration for MQ-2");
            return false;
        }

        PublishHAConfig();
        ha_config_initialized_ = true;
        ESP_LOGI(TAG, "MQ-2 HA integration initialized");
        return true;
    }

    ~Mq2Controller() {
        if (read_task_handle_ != nullptr) {
            vTaskDelete(read_task_handle_);
            ESP_LOGI(TAG, "Background task stopped");
        }

        if (calibrated_ && cali_handle_ != nullptr) {
            ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(cali_handle_));
            cali_handle_ = nullptr;
        }
        if (adc_handle_ != nullptr) {
            ESP_ERROR_CHECK(adc_oneshot_del_unit(adc_handle_));
            adc_handle_ = nullptr;
        }
    }
};

#endif // __MQ2_CONTROLLER_H__

