# ESP32 Home Assistant MQTT 集成指南

本文档整合了 MQTT 配置、Home Assistant (HA) 集成规范、MCP 工具使用及故障排查指南。

## 1. 快速入门

### 1.1 核心流程
1.  **配置**：在 `config.h` 中填入 MQTT Broker 信息及主题定义。
2.  **连接**：设备启动后连接 MQTT，可通过语音指令 "发送 hello" 测试。
3.  **发现**：通过调用 `fan.turn_on` 或 `lamp.turn_on` 触发 HA 自动发现配置发送。
4.  **控制**：HA 界面出现设备，支持双向状态同步与控制。

### 1.2 核心配置 (`config.h`)

```cpp
// --- MQTT 连接信息 ---
#define MQTT_URI       "mqtt://your_broker_ip:1883"
#define MQTT_CLIENT_ID "ESP32-xiaozhi-V2"
#define MQTT_USERNAME  "your_username"
#define MQTT_PASSWORD  "your_password"

// --- 风扇 (Fan) 组件主题 ---
// 1. 自动发现配置主题 (发布)
#define MQTT_HA_FAN_CONFIG_TOPIC        "homeassistant/fan/XZ-ESP32-01/fan/config"
// 2. 状态反馈主题 (发布) {"state":"ON", "speed":2}
#define MQTT_HA_FAN_STATE_TOPIC         "XZ-ESP32-01/fan/state"
// 3. 开关命令主题 (订阅) Payload: "ON" / "OFF"
#define MQTT_HA_FAN_COMMAND_TOPIC       "XZ-ESP32-01/fan/set"
// 4. 数值速度命令主题 (订阅) Payload: 1/2/3
#define MQTT_HA_FAN_PERCENTAGE_CMD      "XZ-ESP32-01/fan/percentage/set"

// --- 灯光 (Light) 组件主题 ---
#define MQTT_HA_LAMP_CONFIG_TOPIC       "homeassistant/light/XZ-ESP32-01/lamp/config"
#define MQTT_HA_LAMP_STATE_TOPIC        "XZ-ESP32-01/lamp/state"
#define MQTT_HA_LAMP_COMMAND_TOPIC      "XZ-ESP32-01/lamp/set"

// --- 设备信息 ---
#define DEVICE_ID          "XZ-ESP32-01"
#define DEVICE_NAME        "小智 ESP32"
#define DEVICE_SW_VERSION  "2.0.3"
```

## 2. MCP 工具清单

设备通过 MCP 协议暴露以下工具供 AI 调用，实现语音控制：

| 工具名 | 功能 | 参数示例 | 说明 |
|:---|:---|:---|:---|
| `mqtt.send_hello` | 连接测试 | `{}` | 向状态主题发送 "hello" |
| `mqtt.get_status` | 查询状态 | `{}` | 返回 MQTT 连接状态与 Broker 信息 |
| `mqtt.send_message`| 自定义消息 | `{"topic":"t","message":"m"}` | 发送任意消息 |
| `fan.turn_on` | 开风扇 | `{}` | 开启风扇(默认2档)+发送 HA 发现配置 |
| `fan.turn_off` | 关风扇 | `{}` | 关闭风扇 |
| `fan.set_speed` | 设风速 | `{"level": 3}` | 0=关, 1-3=低/中/高 |
| `lamp.turn_on` | 开灯 | `{}` | 开启灯光+发送 HA 发现配置 |
| `lamp.turn_off` | 关灯 | `{}` | 关闭灯光 |
| `lamp.get_state` | 查灯状态 | `{}` | 返回 `{"state":"ON"}` |

## 3. Home Assistant 集成规范

### 3.1 风扇 (Fan) 组件
**注意**：`Fan` 组件配置有两个常见陷阱，需严格遵守：
1.  **切勿包含** `device_class` 字段（Fan 组件不支持）。
2.  `speed_range_min` 必须设置为 **1**（0 代表 OFF，不是速度档位）。

**Discovery Topic**: `homeassistant/fan/<DEVICE_ID>/fan/config`

**Payload 模板**:
```json
{
  "unique_id": "XZ-ESP32-01-fan",
  "name": "小智风扇",
  "icon": "mdi:fan",
  "command_topic": "XZ-ESP32-01/fan/set",
  "state_topic": "XZ-ESP32-01/fan/state",
  "state_value_template": "{{ value_json.state }}",
  "percentage_command_topic": "XZ-ESP32-01/fan/percentage/set",
  "percentage_state_topic": "XZ-ESP32-01/fan/state",
  "percentage_value_template": "{{ value_json.speed }}",
  "payload_on": "ON",
  "payload_off": "OFF",
  "speed_range_min": 1,
  "speed_range_max": 3,
  "device": {
    "identifiers": ["XZ-ESP32-01"],
    "name": "小智 ESP32",
    "model": "ESP32-S3",
    "manufacturer": "XiaoZhi",
    "sw_version": "2.0.3"
  }
}
```

**状态反馈 (State Payload)**:
```json
{
  "state": "ON",
  "speed": 2
}
```

### 3.2 灯光 (Light) 组件
**Discovery Topic**: `homeassistant/light/<DEVICE_ID>/lamp/config`

**Payload 模板**:
```json
{
  "unique_id": "XZ-ESP32-01-lamp",
  "name": "小智灯光",
  "icon": "mdi:lightbulb",
  "command_topic": "XZ-ESP32-01/lamp/set",
  "state_topic": "XZ-ESP32-01/lamp/state",
  "state_value_template": "{{ value_json.state }}",
  "payload_on": "ON",
  "payload_off": "OFF",
  "device": {
    "identifiers": ["XZ-ESP32-01"],
    "name": "小智 ESP32",
    "model": "ESP32-S3",
    "manufacturer": "XiaoZhi"
  }
}
```

## 4. 故障排查

### 4.1 常见问题速查表

| 现象 | 可能原因 | 解决方案 |
|:---|:---|:---|
| **HA 未发现设备** | 1. 未触发自动发现<br>2. 配置 JSON 格式错误 | 1. 语音控制"开风扇"或"开灯"触发发送<br>2. 使用在线 JSON 工具校验 Payload |
| **HA 显示设备但不可用** | `unique_id` 冲突或变动 | 删除 HA 中的旧实体，重启 HA |
| **风扇无法创建实体** | 配置包含 `device_class` | 移除 JSON 中的 `device_class` 字段 |
| **风扇档位异常** | `speed_range_min` 为 0 | 确保 `speed_range_min` 设为 1 |
| **HA 无法控制设备** | 设备未订阅命令主题 | 检查代码中是否调用了 `Subscribe(COMMAND_TOPIC)` |
| **状态不同步** | 主题不匹配 | 核对 `config.h` 与 HA 配置中的 Topic 是否一致 |

### 4.2 调试工具
使用 `mosquitto_sub` 命令行工具监控 MQTT 流量：

```bash
# 1. 监听所有 HA 自动发现消息 (检查配置发送是否成功)
mosquitto_sub -h <BROKER_IP> -t "homeassistant/#" -v

# 2. 监听设备所有相关消息 (检查命令接收和状态上报)
mosquitto_sub -h <BROKER_IP> -t "XZ-ESP32-01/#" -v

# 3. 手动发送命令测试设备响应
mosquitto_pub -h <BROKER_IP> -t "XZ-ESP32-01/fan/set" -m "ON"
```
