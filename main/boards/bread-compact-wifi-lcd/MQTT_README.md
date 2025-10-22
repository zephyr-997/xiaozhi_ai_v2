# MQTT 控制器使用说明

## 概述

通过 MCP 协议控制 ESP32 向 MQTT 服务器发送消息，支持语音控制发送。

## 已实现功能

### 1. 发送 Hello 消息
- **工具名**: `self.mqtt.send_hello`
- **描述**: 向默认主题发送 "hello" 消息
- **参数**: 无
- **返回**: `true` (成功) 或 `false` (失败)

### 2. 查询连接状态
- **工具名**: `self.mqtt.get_status`
- **描述**: 查询 MQTT 连接状态
- **参数**: 无
- **返回**: JSON 格式状态信息
  ```json
  {
    "connected": true,
    "broker": "mqtt://106.53.179.231:1883",
    "topic": "HA-XZ-01/01/state"
  }
  ```

### 3. 发送自定义消息（扩展功能）
- **工具名**: `self.mqtt.send_message`
- **描述**: 向指定主题发送自定义消息
- **参数**:
  - `topic` (字符串): MQTT 主题
  - `message` (字符串): 消息内容
- **返回**: `true` (成功) 或 `false` (失败)

## 配置说明

在 `config.h` 中可以修改 MQTT 连接参数：

```cpp
#define MQTT_URI       "mqtt://106.53.179.231:1883"  // MQTT服务器地址和端口
#define CLIENT_ID      "ESP32-xiaozhi-V2"            // 设备唯一标识符
#define MQTT_USERNAME  "admin"                       // MQTT服务器用户名
#define MQTT_PASSWORD  "azsxdcfv"                    // MQTT服务器密码
#define MQTT_COMMAND_TOPIC    "HA-XZ-01/01/state"    // 统一命令主题
```

### 配置参数说明

- **MQTT_URI**: MQTT 服务器地址，支持 `mqtt://` 或 `mqtts://`（加密连接）
- **CLIENT_ID**: 设备客户端 ID，建议唯一以避免冲突
- **MQTT_USERNAME**: MQTT 服务器认证用户名
- **MQTT_PASSWORD**: MQTT 服务器认证密码
- **MQTT_COMMAND_TOPIC**: 默认发布的主题名称

### 其他常用公共 MQTT 服务器（无需认证）

- **EMQX 公共服务器**:
  - URI: `mqtt://broker.emqx.io:1883`
  - 用户名/密码留空

- **Eclipse Mosquitto**:
  - URI: `mqtt://test.mosquitto.org:1883`
  - 用户名/密码留空

- **HiveMQ 公共服务器**:
  - URI: `mqtt://broker.hivemq.com:1883`
  - 用户名/密码留空

## MCP 协议调用示例

### 1. 发送 Hello 消息

```json
{
  "jsonrpc": "2.0",
  "method": "tools/call",
  "params": {
    "name": "self.mqtt.send_hello",
    "arguments": {}
  },
  "id": 1
}
```

### 2. 查询状态

```json
{
  "jsonrpc": "2.0",
  "method": "tools/call",
  "params": {
    "name": "self.mqtt.get_status",
    "arguments": {}
  },
  "id": 2
}
```

### 3. 发送自定义消息

```json
{
  "jsonrpc": "2.0",
  "method": "tools/call",
  "params": {
    "name": "self.mqtt.send_message",
    "arguments": {
      "topic": "xiaozhi/custom",
      "message": "你好世界"
    }
  },
  "id": 3
}
```

## 语音控制示例

通过小智 AI 语音控制：

- **"发送 hello"** → 调用 `self.mqtt.send_hello`
- **"查看 MQTT 状态"** → 调用 `self.mqtt.get_status`
- **"发送消息到主题 test，内容是温度25度"** → 调用 `self.mqtt.send_message`

## 后续扩展建议

### 1. 订阅主题
添加 MQTT 订阅功能，接收来自服务器的消息：

```cpp
mcp_server.AddTool(
    "self.mqtt.subscribe",
    "订阅 MQTT 主题",
    PropertyList({Property("topic", kPropertyTypeString)}),
    [this](const PropertyList& properties) -> ReturnValue {
        // 实现订阅逻辑
    }
);
```

### 2. QoS 支持
支持不同的服务质量等级（0, 1, 2）：

```cpp
Property("qos", kPropertyTypeInteger, 0, 2)
```

### 3. 保留消息
支持发送保留消息（retain flag）。

### 4. 消息接收回调
接收到 MQTT 消息时触发设备动作（如开灯、启动风扇）。

### 5. SSL/TLS 加密连接
支持安全连接：

```cpp
#define MQTT_BROKER_URI "mqtts://broker.emqx.io:8883"
```

## 调试技巧

### 1. 查看日志
设备日志会显示 MQTT 连接状态和消息发送情况：

```
I (12345) MqttController: MQTT client started, connecting to mqtt://106.53.179.231:1883 as ESP32-xiaozhi-V2
I (12456) MqttController: MQTT connected to broker
I (12567) MqttController: Published to [HA-XZ-01/01/state]: hello (msg_id=1)
```

### 2. 使用 MQTT 客户端工具测试

- **MQTT.fx** (桌面应用)
- **MQTTX** (跨平台)
- **mosquitto_sub** (命令行)

订阅测试主题：
```bash
# 带认证的订阅
mosquitto_sub -h 106.53.179.231 -p 1883 -u admin -P azsxdcfv -t "HA-XZ-01/01/state"

# 订阅所有相关主题
mosquitto_sub -h 106.53.179.231 -p 1883 -u admin -P azsxdcfv -t "HA-XZ-01/#"
```

### 3. 网页在线工具
- [HiveMQ Web Client](http://www.hivemq.com/demos/websocket-client/)
- [MQTTX Web](https://mqttx.app/)

## 注意事项

1. **WiFi 连接**: 确保设备已连接到 WiFi 网络
2. **服务器可达性**: 确保 MQTT 服务器可以访问
3. **内存管理**: 发送大量消息时注意内存使用
4. **连接重试**: 控制器会自动尝试重连

## 文件说明

- `mqtt_controller.h` - MQTT 控制器实现
- `config.h` - MQTT 配置参数
- `compact_wifi_board_lcd.cc` - 板型集成代码

## 编译与烧录

```bash
# 编译
idf.py build

# 烧录
idf.py flash

# 监控日志
idf.py monitor
```

---

**作者**: Xiaozhi ESP32 团队  
**更新**: 2025-10-22

