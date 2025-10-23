# Home Assistant MQTT 组件配置快速参考

## 🎯 核心问题：Fan vs Sensor

### ❓ 为什么 `homeassistant/fan/...` 无效？

**原因**: Fan 组件**不支持** `device_class` 字段，而我们的配置中包含了这个字段。

```json
// ❌ 错误 - Fan 组件会拒绝这个配置
{
    "device_class": "fan",  // ← Fan 组件不支持此字段
    "state_topic": "...",
    ...
}
```

---

## ✅ 解决方案

### 方案 1：使用 Sensor（简单）

**主题**: `homeassistant/sensor/XZ-ESP32-01/fan/config`

**优点**: ✅ 简单，只显示状态  
**缺点**: ❌ 无法从 HA 控制设备

```json
{
    "unique_id": "XZ-ESP32-01-fan-status",
    "name": "小智风扇状态",
    "icon": "mdi:fan",
    "state_topic": "XZ-ESP32-01/fan/state",
    "value_template": "{{ value_json.state }}",
    "device": {...}
}
```

**状态消息**:
```json
{"state": "ON", "speed": 2, "percentage": 66}
```

---

### 方案 2：使用 Fan（完整功能）

**主题**: `homeassistant/fan/XZ-ESP32-01/fan/config`

**优点**: ✅ 可从 HA 控制，支持速度调节  
**缺点**: ⚠️ 需要订阅命令主题

```json
{
    "unique_id": "XZ-ESP32-01-fan",
    "name": "小智风扇",
    "icon": "mdi:fan",
    
    "command_topic": "XZ-ESP32-01/fan/set",           // ⭐ 必需
    "state_topic": "XZ-ESP32-01/fan/state",
    "state_value_template": "{{ value_json.state }}",
    
    "payload_on": "ON",
    "payload_off": "OFF",
    
    "percentage_command_topic": "XZ-ESP32-01/fan/percentage/set",
    "percentage_state_topic": "XZ-ESP32-01/fan/state",
    "percentage_value_template": "{{ value_json.percentage }}",
    
    "speed_range_min": 1,    // ⚠️ 必须从 1 开始（0 是关闭状态，不是档位）
    "speed_range_max": 3,
    
    // ⚠️ 不要包含 device_class
    "device": {...}
}
```

**状态消息**:
```json
{"state": "ON", "percentage": 66}
```

**需订阅的命令**:
- `XZ-ESP32-01/fan/set` → 接收 `ON`/`OFF`
- `XZ-ESP32-01/fan/percentage/set` → 接收 `33`/`66`/`100`

---

## 📋 关键字段对比

| 字段 | Fan | Sensor | 说明 |
|------|-----|--------|------|
| `device_class` | ❌ | ✅ | Fan 不支持 |
| `command_topic` | ✅ 必需 | ❌ | Fan 必需 |
| `value_template` | ❌ | ✅ | Fan 用 `state_value_template` |
| `payload_on/off` | ✅ | ❌ | Fan 专用 |
| `percentage_*` | ✅ | ❌ | Fan 速度控制 |

---

## 🚀 快速决策

```
需要从 Home Assistant 控制风扇？
├─ 是 → 用 Fan 组件（需实现订阅）
└─ 否 → 用 Sensor 组件（简单，只显示）
```

---

## 📝 完整示例代码

### ESP32 配置（config.h）

```cpp
// Sensor 方式（只显示）
#define MQTT_HA_FAN_CONFIG_TOPIC  "homeassistant/sensor/XZ-ESP32-01/fan/config"
#define MQTT_HA_FAN_STATE_TOPIC   "XZ-ESP32-01/fan/state"

// 或 Fan 方式（双向控制）
#define MQTT_HA_FAN_CONFIG_TOPIC     "homeassistant/fan/XZ-ESP32-01/fan/config"
#define MQTT_HA_FAN_STATE_TOPIC      "XZ-ESP32-01/fan/state"
#define MQTT_HA_FAN_COMMAND_TOPIC    "XZ-ESP32-01/fan/set"
#define MQTT_HA_FAN_PERCENTAGE_CMD   "XZ-ESP32-01/fan/percentage/set"
```

### 发送配置（fan_controller.h）

**Sensor 方式**:
```cpp
snprintf(config_json, sizeof(config_json),
    "{"
    "\"unique_id\":\"%s-fan\","
    "\"name\":\"小智风扇状态\","
    "\"icon\":\"mdi:fan\","
    "\"state_topic\":\"%s\","
    "\"value_template\":\"{{ value_json.state }}\","
    "\"device\":{...}"
    "}",
    DEVICE_ID, MQTT_HA_FAN_STATE_TOPIC
);
```

**Fan 方式**:
```cpp
snprintf(config_json, sizeof(config_json),
    "{"
    "\"unique_id\":\"%s-fan\","
    "\"name\":\"小智风扇\","
    "\"icon\":\"mdi:fan\","
    "\"command_topic\":\"XZ-ESP32-01/fan/set\","
    "\"state_topic\":\"%s\","
    "\"state_value_template\":\"{{ value_json.state }}\","
    "\"payload_on\":\"ON\","
    "\"payload_off\":\"OFF\","
    "\"percentage_command_topic\":\"XZ-ESP32-01/fan/percentage/set\","
    "\"percentage_state_topic\":\"%s\","
    "\"percentage_value_template\":\"{{ value_json.percentage }}\","
    "\"speed_range_min\":1,"
    "\"speed_range_max\":3,"
    "\"device\":{...}"
    "}",
    DEVICE_ID, MQTT_HA_FAN_STATE_TOPIC, MQTT_HA_FAN_STATE_TOPIC
);
```

---

## 🔍 调试命令

```bash
# 订阅 HA 自动发现消息
mosquitto_sub -h IP -p 1883 -u USER -P PASS -t "homeassistant/#" -v

# 订阅设备状态
mosquitto_sub -h IP -p 1883 -u USER -P PASS -t "XZ-ESP32-01/#" -v

# 手动发送测试配置
mosquitto_pub -h IP -p 1883 -u USER -P PASS \
    -t "homeassistant/sensor/XZ-ESP32-01/fan/config" \
    -m '{"unique_id":"test","name":"测试","state_topic":"test/state"}'
```

---

## ✅ 推荐做法

**当前场景**（只需显示状态）:
→ **使用 Sensor 组件**

**未来扩展**（需要 HA 控制）:
→ **升级到 Fan 组件** + 实现命令订阅

---

详细文档请查看：`HomeAssistant_MQTT组件配置说明.md`

