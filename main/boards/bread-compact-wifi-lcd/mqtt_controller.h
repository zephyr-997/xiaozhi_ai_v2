#ifndef __MQTT_CONTROLLER_H__
#define __MQTT_CONTROLLER_H__

#include <mqtt_client.h>
#include "mcp_server.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include <string>
#include <map>
#include <functional>
#include <vector>

class MqttController {
private:
    static constexpr const char* TAG = "MqttController";
    static MqttController* instance_;  // 单例指针
    
    esp_mqtt_client_handle_t mqtt_client_;
    bool is_connected_;
    bool client_started_;  // 跟踪客户端是否已启动
    std::string broker_uri_;
    std::string client_id_;
    std::string username_;
    std::string password_;
    std::string default_topic_;
    std::map<std::string, std::function<void(const std::string&, const std::string&)>> topic_callbacks_;  // 主题回调映射
    std::vector<std::function<void()>> on_connected_callbacks_;

    // MQTT 事件处理回调
    static void MqttEventHandler(void* handler_args, esp_event_base_t base, 
                                 int32_t event_id, void* event_data) {
        MqttController* controller = static_cast<MqttController*>(handler_args);
        esp_mqtt_event_handle_t event = static_cast<esp_mqtt_event_handle_t>(event_data);
        
        switch ((esp_mqtt_event_id_t)event_id) {
            case MQTT_EVENT_CONNECTED:
                ESP_LOGI(TAG, "MQTT connected to broker");
                controller->is_connected_ = true;
                // 重新订阅所有主题
                controller->ResubscribeAll();
                // 触发所有注册的连接回调
                if (!controller->on_connected_callbacks_.empty()) {
                    ESP_LOGI(TAG, "Executing %zu MQTT connected callbacks", controller->on_connected_callbacks_.size());
                    for (const auto& callback : controller->on_connected_callbacks_) {
                        if (callback) {
                            callback();
                        }
                    }
                }
                break;
            case MQTT_EVENT_DISCONNECTED:
                ESP_LOGI(TAG, "MQTT disconnected");
                controller->is_connected_ = false;
                break;
            case MQTT_EVENT_DATA:
                // 接收到消息
                {
                    std::string topic(event->topic, event->topic_len);
                    std::string payload(event->data, event->data_len);
                    ESP_LOGI(TAG, "Received message on [%s]: %s", topic.c_str(), payload.c_str());
                    
                    // 调用回调函数
                    auto it = controller->topic_callbacks_.find(topic);
                    if (it != controller->topic_callbacks_.end()) {
                        it->second(topic, payload);
                    }
                }
                break;
            case MQTT_EVENT_ERROR:
                ESP_LOGE(TAG, "MQTT error");
                break;
            default:
                break;
        }
    }

    // WiFi 事件处理回调 - 等待 WiFi 连接后再启动 MQTT
    static void WifiEventHandler(void* handler_args, esp_event_base_t base,
                                int32_t event_id, void* event_data) {
        MqttController* controller = static_cast<MqttController*>(handler_args);
        
        if (event_id == IP_EVENT_STA_GOT_IP) {
            // WiFi 获取 IP 后，启动 MQTT 客户端
            if (!controller->client_started_ && controller->mqtt_client_ != nullptr) {
                ESP_LOGI(TAG, "WiFi connected with IP, starting MQTT client...");
                esp_mqtt_client_start(controller->mqtt_client_);
                controller->client_started_ = true;
            }
        } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            ESP_LOGW(TAG, "WiFi disconnected");
        }
    }

    void InitializeMqttClient() {
        esp_mqtt_client_config_t mqtt_cfg = {};
        mqtt_cfg.broker.address.uri = broker_uri_.c_str();
        mqtt_cfg.credentials.client_id = client_id_.c_str();
        mqtt_cfg.credentials.username = username_.c_str();
        mqtt_cfg.credentials.authentication.password = password_.c_str();
        
        mqtt_client_ = esp_mqtt_client_init(&mqtt_cfg);
        if (mqtt_client_ == nullptr) {
            ESP_LOGE(TAG, "Failed to initialize MQTT client");
            return;
        }
        
        esp_mqtt_client_register_event(mqtt_client_, MQTT_EVENT_ANY, 
                                       MqttEventHandler, this);
        
        // ⚠️ 关键修改：不在这里立即启动，而是等待 WiFi 事件
        // esp_mqtt_client_start(mqtt_client_);  // 移除此行
        
        // 注册 WiFi 事件监听
        esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, WifiEventHandler, this);
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, WifiEventHandler, this);
        
        ESP_LOGI(TAG, "MQTT client initialized, waiting for WiFi connection...");
    }

    void ResubscribeAll() {
        if (!is_connected_) {
            return;
        }
        
        for (const auto& pair : topic_callbacks_) {
            int msg_id = esp_mqtt_client_subscribe(mqtt_client_, pair.first.c_str(), 0);
            ESP_LOGI(TAG, "Resubscribed to topic: %s (msg_id=%d)", pair.first.c_str(), msg_id);
        }
    }

    bool PublishMessage(const char* topic, const char* message) {
        if (!is_connected_) {
            ESP_LOGW(TAG, "MQTT not connected, cannot publish");
            return false;
        }
        
        int msg_id = esp_mqtt_client_publish(mqtt_client_, topic, message, 0, 1, 0);
        if (msg_id < 0) {
            ESP_LOGE(TAG, "Failed to publish message to topic: %s", topic);
            return false;
        }
        
        ESP_LOGI(TAG, "Published to [%s]: %s (msg_id=%d)", topic, message, msg_id);
        return true;
    }

    void RegisterTools() {
        auto& mcp_server = McpServer::GetInstance();

        // 工具1: 发送简单的 hello 消息
        mcp_server.AddTool(
            "self.mqtt.send_hello",
            "向默认主题发送 'hello' 消息",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                bool success = PublishMessage(default_topic_.c_str(), "hello");
                if (success) {
                    ESP_LOGI(TAG, "Hello message sent successfully");
                    return true;
                } else {
                    ESP_LOGE(TAG, "Failed to send hello message");
                    return false;
                }
            }
        );

        // 工具2: 查询 MQTT 连接状态
        mcp_server.AddTool(
            "self.mqtt.get_status",
            "查询 MQTT 连接状态",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                char buffer[128];
                snprintf(buffer, sizeof(buffer), 
                        "{\"connected\":%s,\"broker\":\"%s\",\"topic\":\"%s\"}", 
                        is_connected_ ? "true" : "false",
                        broker_uri_.c_str(),
                        default_topic_.c_str());
                ESP_LOGI(TAG, "Status query: %s", is_connected_ ? "connected" : "disconnected");
                return std::string(buffer);
            }
        );

        // 工具3: 发送自定义消息（为后续扩展预留）
        mcp_server.AddTool(
            "self.mqtt.send_message",
            "向指定主题发送自定义消息",
            PropertyList({
                Property("topic", kPropertyTypeString),
                Property("message", kPropertyTypeString)
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                std::string topic = properties["topic"].value<std::string>();
                std::string message = properties["message"].value<std::string>();
                
                bool success = PublishMessage(topic.c_str(), message.c_str());
                if (success) {
                    ESP_LOGI(TAG, "Custom message sent to %s", topic.c_str());
                    return true;
                } else {
                    ESP_LOGE(TAG, "Failed to send message to %s", topic.c_str());
                    return false;
                }
            }
        );
    }

public:
    // 获取单例实例
    static MqttController* GetInstance() {
        return instance_;
    }
    
    // 公共发布接口（供其他控制器调用）
    bool Publish(const char* topic, const char* message) {
        return PublishMessage(topic, message);
    }
    
    // 检查是否已连接
    bool IsConnected() const {
        return is_connected_;
    }
    
    // 订阅主题（供其他控制器调用）
    bool Subscribe(const char* topic, std::function<void(const std::string&, const std::string&)> callback) {
        std::string topic_str(topic);
        
        // 保存回调
        topic_callbacks_[topic_str] = callback;
        
        // 如果已连接，立即订阅
        if (is_connected_) {
            int msg_id = esp_mqtt_client_subscribe(mqtt_client_, topic, 0);
            if (msg_id < 0) {
                ESP_LOGE(TAG, "Failed to subscribe to topic: %s", topic);
                return false;
            }
            ESP_LOGI(TAG, "Subscribed to topic: %s (msg_id=%d)", topic, msg_id);
            return true;
        } else {
            ESP_LOGW(TAG, "Not connected, subscription will be done when connected: %s", topic);
            return true;  // 稍后连接时会自动订阅
        }
    }

    void RegisterOnConnected(std::function<void()> callback) {
        if (callback) {
            on_connected_callbacks_.push_back(callback);
            if (is_connected_) {
                ESP_LOGI(TAG, "MQTT already connected, executing callback immediately");
                callback();
            }
        }
    }
    
    explicit MqttController(const char* broker_uri, 
                           const char* client_id,
                           const char* username,
                           const char* password,
                           const char* default_topic)
        : mqtt_client_(nullptr), 
          is_connected_(false),
          client_started_(false),  // 初始化新增的标志
          broker_uri_(broker_uri),
          client_id_(client_id),
          username_(username),
          password_(password),
          default_topic_(default_topic) {
        instance_ = this;  // 设置单例
        InitializeMqttClient();
        RegisterTools();
        ESP_LOGI(TAG, "MQTT Controller initialized - Broker: %s, Client: %s, Topic: %s", 
                 broker_uri, client_id, default_topic);
    }

    ~MqttController() {
        if (mqtt_client_ != nullptr) {
            // 注销事件处理器
            esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, WifiEventHandler);
            esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, WifiEventHandler);
            
            // 只有启动过才需要停止
            if (client_started_) {
                esp_mqtt_client_stop(mqtt_client_);
            }
            esp_mqtt_client_destroy(mqtt_client_);
            ESP_LOGI(TAG, "MQTT client destroyed");
        }
    }

    // 禁用拷贝构造和赋值
    MqttController(const MqttController&) = delete;
    MqttController& operator=(const MqttController&) = delete;
};

// 静态成员初始化
MqttController* MqttController::instance_ = nullptr;

#endif // __MQTT_CONTROLLER_H__

