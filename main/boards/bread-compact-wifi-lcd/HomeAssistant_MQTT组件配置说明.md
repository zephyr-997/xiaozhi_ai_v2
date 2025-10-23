# Home Assistant MQTT 组件配置说明

## 📋 文档说明

本文档详细说明了 ESP32 设备与 Home Assistant 通过 MQTT 自动发现协议集成的配置方法。

**适用版本**: Home Assistant 2023.x+  
**MQTT 版本**: 3.1.1  
**更新日期**: 2025-10-22

---

## 🎯 组件类型对比

Home Assistant 支持多种 MQTT 设备类型，每种类型有不同的配置要求和功能特性。

### 主要组件类型

| 组件类型 | 用途 | 双向控制 | 适用场景 |
|---------|------|---------|---------|
| **Fan** | 风扇控制 | ✅ 是 | 可控制的风扇设备 |
| **Sensor** | 传感器数据 | ❌ 否 | 只读数据（温度、湿度等） |
| **Switch** | 开关控制 | ✅ 是 | 简单的开/关设备 |
| **Light** | 灯光控制 | ✅ 是 | 灯光设备（亮度、颜色） |
| **Binary Sensor** | 二进制传感器 | ❌ 否 | 开/关状态（门、窗） |
| **Climate** | 温控器 | ✅ 是 | 空调、暖气等 |

---

## 🌀 Fan 组件（风扇）

### ✅ 使用场景

- 需要从 Home Assistant 控制风扇开关
- 需要调节风扇速度
- 双向通信（设备→HA，HA→设备）

### 📤 配置主题

```
homeassistant/fan/{设备ID}/{实体ID}/config
```

**示例**: `homeassistant/fan/XZ-ESP32-01/fan/config`

### 🔧 完整配置示例

```json
{
    "unique_id": "XZ-ESP32-01-fan",
    "name": "小智风扇",
    "icon": "mdi:fan",
    
    "command_topic": "XZ-ESP32-01/fan/set",
    "state_topic": "XZ-ESP32-01/fan/state",
    "state_value_template": "{{ value_json.state }}",
    
    "payload_on": "ON",
    "payload_off": "OFF",
    
    "percentage_command_topic": "XZ-ESP32-01/fan/percentage/set",
    "percentage_state_topic": "XZ-ESP32-01/fan/state",
    "percentage_value_template": "{{ value_json.percentage }}",
    
    "speed_range_min": 1,
    "speed_range_max": 3,
    
    "json_attributes_topic": "XZ-ESP32-01/fan/attributes",
    
    "device": {
        "identifiers": ["XZ-ESP32-01"],
        "name": "小智 ESP32",
        "model": "ESP32-S3",
        "manufacturer": "XiaoZhi",
        "sw_version": "2.0.3"
    }
}
```

### 📝 必需字段说明

| 字段 | 类型 | 说明 | 示例 |
|------|------|------|------|
| `unique_id` | String | 全局唯一标识符 | `XZ-ESP32-01-fan` |
| `name` | String | 显示名称 | `小智风扇` |
| `command_topic` | String | **接收控制命令的主题** | `XZ-ESP32-01/fan/set` |
| `state_topic` | String | 发送状态的主题 | `XZ-ESP32-01/fan/state` |

### 🎨 可选字段说明

| 字段 | 类型 | 说明 |
|------|------|------|
| `icon` | String | 图标（MDI 图标库） |
| `state_value_template` | String | 从 JSON 提取状态 |
| `payload_on` | String | 开启时的负载值 |
| `payload_off` | String | 关闭时的负载值 |
| `percentage_command_topic` | String | 接收速度命令的主题 |
| `percentage_state_topic` | String | 发送速度状态的主题 |
| `percentage_value_template` | String | 从 JSON 提取速度 |
| `speed_range_min` | Integer | 最小速度档位 |
| `speed_range_max` | Integer | 最大速度档位 |
| `json_attributes_topic` | String | 额外属性主题 |

### 📊 状态消息格式

**主题**: `XZ-ESP32-01/fan/state`

```json
{
    "state": "ON",
    "percentage": 66
}
```

**字段说明**:
- `state`: 风扇状态（`ON` 或 `OFF`）
- `percentage`: 速度百分比（0-100）

### 📥 命令消息格式（设备需订阅）

**开关命令主题**: `XZ-ESP32-01/fan/set`
```
ON    // 开启风扇
OFF   // 关闭风扇
```

**速度命令主题**: `XZ-ESP32-01/fan/percentage/set`
```
33    // 设置速度为 33%
66    // 设置速度为 66%
100   // 设置速度为 100%
```

### ⚠️ Fan 组件不支持的字段

- ❌ `device_class` - 这是 sensor 专用字段
- ❌ `unit_of_measurement` - sensor 专用
- ❌ `value_template` - 使用 `state_value_template` 代替

### 🔴 重要配置注意事项

#### `speed_range_min` 必须从 1 开始

**❌ 错误配置**（会导致 HA 无法识别设备）:
```json
{
  "speed_range_min": 0,
  "speed_range_max": 3
}
```

**✅ 正确配置**:
```json
{
  "speed_range_min": 1,
  "speed_range_max": 3
}
```

**原因**:
- `speed_range` 定义的是**有效速度档位的范围**
- `0` 不是速度档位，而是**关闭状态**
- 速度范围必须从 `1` 开始

**工作原理**:
- 设备返回 `speed: 0` → HA 识别为 OFF 状态
- 设备返回 `speed: 1-3` → HA 识别为对应档位
- HA 发送 `0` → 关闭风扇
- HA 发送 `1-3` → 对应档位

---

## 📊 Sensor 组件（传感器）

### ✅ 使用场景

- 只需要显示设备状态（单向）
- 不需要从 Home Assistant 控制
- 读取传感器数据（温度、湿度等）

### 📤 配置主题

```
homeassistant/sensor/{设备ID}/{实体ID}/config
```

**示例**: `homeassistant/sensor/XZ-ESP32-01/fan/config`

### 🔧 完整配置示例

```json
{
    "unique_id": "XZ-ESP32-01-fan-status",
    "name": "小智风扇状态",
    "icon": "mdi:fan",
    
    "state_topic": "XZ-ESP32-01/fan/state",
    "value_template": "{{ value_json.state }}",
    
    "json_attributes_topic": "XZ-ESP32-01/fan/attributes",
    
    "device_class": "enum",
    
    "device": {
        "identifiers": ["XZ-ESP32-01"],
        "name": "小智 ESP32",
        "model": "ESP32-S3",
        "manufacturer": "XiaoZhi",
        "sw_version": "2.0.3"
    }
}
```

### 📝 必需字段说明

| 字段 | 类型 | 说明 |
|------|------|------|
| `unique_id` | String | 全局唯一标识符 |
| `name` | String | 显示名称 |
| `state_topic` | String | 发送状态的主题 |

### 🎨 可选字段说明

| 字段 | 类型 | 说明 | 常用值 |
|------|------|------|--------|
| `device_class` | String | 传感器类型 | `temperature`, `humidity`, `enum` 等 |
| `unit_of_measurement` | String | 单位 | `℃`, `%`, `rpm` 等 |
| `value_template` | String | 从 JSON 提取值 | `{{ value_json.state }}` |
| `icon` | String | 图标 | `mdi:thermometer` 等 |
| `json_attributes_topic` | String | 额外属性主题 | - |

### 📊 状态消息格式

**主题**: `XZ-ESP32-01/fan/state`

```json
{
    "state": "ON",
    "speed": 2,
    "percentage": 66
}
```

### 🎯 常用 device_class 值

| device_class | 用途 | 默认单位 |
|--------------|------|---------|
| `temperature` | 温度 | °C |
| `humidity` | 湿度 | % |
| `pressure` | 气压 | hPa |
| `battery` | 电池电量 | % |
| `enum` | 枚举状态 | - |
| `power` | 功率 | W |
| `voltage` | 电压 | V |
| `current` | 电流 | A |

---

## 🔌 Switch 组件（开关）

### ✅ 使用场景

- 简单的开/关控制
- 不需要调速或调光功能
- 继电器、电源开关等

### 🔧 配置示例

```json
{
    "unique_id": "XZ-ESP32-01-relay",
    "name": "小智继电器",
    "icon": "mdi:power-socket",
    
    "command_topic": "XZ-ESP32-01/relay/set",
    "state_topic": "XZ-ESP32-01/relay/state",
    
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

---

## 💡 Light 组件（灯光）

### ✅ 使用场景

- 灯光控制
- 支持亮度调节
- 支持颜色控制（RGB/RGBW）

### 🔧 配置示例（亮度灯）

```json
{
    "unique_id": "XZ-ESP32-01-light",
    "name": "小智灯光",
    "icon": "mdi:lightbulb",
    
    "command_topic": "XZ-ESP32-01/light/set",
    "state_topic": "XZ-ESP32-01/light/state",
    
    "brightness_command_topic": "XZ-ESP32-01/light/brightness/set",
    "brightness_state_topic": "XZ-ESP32-01/light/state",
    "brightness_scale": 100,
    
    "schema": "json",
    
    "device": {
        "identifiers": ["XZ-ESP32-01"],
        "name": "小智 ESP32"
    }
}
```

---

## 🔥 常用图标（MDI）

```
风扇：mdi:fan, mdi:fan-speed-1, mdi:fan-speed-2, mdi:fan-speed-3
灯光：mdi:lightbulb, mdi:lamp, mdi:ceiling-light
温度：mdi:thermometer, mdi:temperature-celsius
湿度：mdi:water-percent
开关：mdi:toggle-switch, mdi:power
继电器：mdi:electric-switch, mdi:power-socket
传感器：mdi:sensor, mdi:motion-sensor
门窗：mdi:door, mdi:window-open, mdi:window-closed
电池：mdi:battery, mdi:battery-charging
```

完整图标库：https://pictogrammers.com/library/mdi/

---

## 📡 Device（设备）配置

所有组件都应包含 `device` 配置，用于在 Home Assistant 中将多个实体关联到同一设备。

### 完整 Device 配置

```json
{
    "device": {
        "identifiers": ["XZ-ESP32-01"],
        "name": "小智 ESP32",
        "model": "ESP32-S3",
        "manufacturer": "XiaoZhi",
        "sw_version": "2.0.3",
        "hw_version": "1.0",
        "configuration_url": "http://192.168.1.100",
        "suggested_area": "客厅"
    }
}
```

### 字段说明

| 字段 | 必需 | 说明 |
|------|------|------|
| `identifiers` | ✅ | 设备唯一标识符（数组） |
| `name` | ✅ | 设备名称 |
| `model` | ⚠️ | 设备型号 |
| `manufacturer` | ⚠️ | 制造商 |
| `sw_version` | ⚠️ | 软件版本 |
| `hw_version` | ❌ | 硬件版本 |
| `configuration_url` | ❌ | 配置页面 URL |
| `suggested_area` | ❌ | 建议区域 |

---

## 🎨 JSON Attributes（额外属性）

通过 `json_attributes_topic` 可以发送设备的额外信息。

### 配置示例

```json
{
    "json_attributes_topic": "XZ-ESP32-01/fan/attributes"
}
```

### 属性消息示例

**主题**: `XZ-ESP32-01/fan/attributes`

```json
{
    "gpio": 8,
    "pwm_freq": 25000,
    "max_speed": 3,
    "chip": "ESP32-S3",
    "duty_resolution": 8,
    "uptime": 3600,
    "wifi_rssi": -45
}
```

这些属性会在 Home Assistant 的设备详情页显示。

---

## 📋 完整示例对比

### 示例场景：风扇设备

#### 方式 1：使用 Fan 组件（双向控制）

**配置主题**: `homeassistant/fan/XZ-ESP32-01/fan/config`

```json
{
    "unique_id": "XZ-ESP32-01-fan",
    "name": "小智风扇",
    "icon": "mdi:fan",
    "command_topic": "XZ-ESP32-01/fan/set",
    "state_topic": "XZ-ESP32-01/fan/state",
    "state_value_template": "{{ value_json.state }}",
    "payload_on": "ON",
    "payload_off": "OFF",
    "percentage_command_topic": "XZ-ESP32-01/fan/percentage/set",
    "percentage_state_topic": "XZ-ESP32-01/fan/state",
    "percentage_value_template": "{{ value_json.percentage }}",
    "speed_range_min": 1,
    "speed_range_max": 3,
    "device": {
        "identifiers": ["XZ-ESP32-01"],
        "name": "小智 ESP32"
    }
}
```

**状态消息**: `XZ-ESP32-01/fan/state`
```json
{"state": "ON", "percentage": 66}
```

**优点**: 
- ✅ 可以从 HA 控制
- ✅ 支持速度调节
- ✅ 标准风扇设备

**缺点**:
- ⚠️ 需要订阅命令主题
- ⚠️ 实现较复杂

---

#### 方式 2：使用 Sensor 组件（只显示）

**配置主题**: `homeassistant/sensor/XZ-ESP32-01/fan/config`

```json
{
    "unique_id": "XZ-ESP32-01-fan-status",
    "name": "小智风扇状态",
    "icon": "mdi:fan",
    "state_topic": "XZ-ESP32-01/fan/state",
    "value_template": "{{ value_json.state }}",
    "device": {
        "identifiers": ["XZ-ESP32-01"],
        "name": "小智 ESP32"
    }
}
```

**状态消息**: `XZ-ESP32-01/fan/state`
```json
{"state": "ON", "speed": 2, "percentage": 66}
```

**优点**:
- ✅ 简单易实现
- ✅ 只需发送状态
- ✅ 不需要订阅

**缺点**:
- ❌ 无法从 HA 控制
- ❌ 只能显示状态

---

## 🔍 调试技巧

### 1. 查看 MQTT 流量

使用 MQTT 客户端订阅：

```bash
# 订阅所有 HA 自动发现消息
mosquitto_sub -h BROKER_IP -p 1883 -u USER -P PASS -t "homeassistant/#" -v

# 订阅设备状态消息
mosquitto_sub -h BROKER_IP -p 1883 -u USER -P PASS -t "XZ-ESP32-01/#" -v
```

### 2. 验证 JSON 格式

使用在线工具验证 JSON 格式：
- https://jsonlint.com/
- https://www.json.cn/

### 3. Home Assistant 日志

查看 HA 日志中的 MQTT 错误：

```yaml
# configuration.yaml
logger:
  default: warning
  logs:
    homeassistant.components.mqtt: debug
```

### 4. 测试自动发现

发送配置后，在 HA 中：
1. 进入 **配置** → **设备与服务**
2. 找到 **MQTT** 集成
3. 查看发现的设备

---

## 📚 参考链接

- [Home Assistant MQTT 官方文档](https://www.home-assistant.io/integrations/mqtt/)
- [MQTT Fan 组件](https://www.home-assistant.io/integrations/fan.mqtt/)
- [MQTT Sensor 组件](https://www.home-assistant.io/integrations/sensor.mqtt/)
- [MQTT Switch 组件](https://www.home-assistant.io/integrations/switch.mqtt/)
- [MQTT Light 组件](https://www.home-assistant.io/integrations/light.mqtt/)
- [MDI 图标库](https://pictogrammers.com/library/mdi/)

---

## ✅ 快速决策指南

```
需要从 Home Assistant 控制设备？
├─ 是 → 使用对应的控制组件
│   ├─ 风扇（带速度调节）→ Fan
│   ├─ 灯光（带亮度/颜色）→ Light
│   └─ 简单开关 → Switch
└─ 否 → 使用传感器组件
    ├─ 数值数据（温度、湿度）→ Sensor
    └─ 开/关状态（门、窗）→ Binary Sensor
```

---

**文档版本**: v1.0  
**最后更新**: 2025-10-22  
**作者**: XiaoZhi Team

