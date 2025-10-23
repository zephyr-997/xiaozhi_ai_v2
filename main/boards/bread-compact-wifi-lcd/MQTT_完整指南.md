# ESP32 Home Assistant MQTT 完整指南

**版本**: v2.0  
**更新日期**: 2025-10-23  
**适用**: ESP32-S3 + Home Assistant 2023.x+

---

## 📚 目录

1. [快速开始](#第一章快速开始)
2. [MQTT 控制器使用](#第二章mqtt-控制器使用)
3. [Home Assistant 组件配置](#第三章home-assistant-组件配置)
4. [常见问题与解决方案](#第四章常见问题与解决方案)
5. [调试与故障排查](#第五章调试与故障排查)
6. [API 参考](#第六章api-参考)

---

# 第一章：快速开始

## 1.1 什么是 MQTT

MQTT (Message Queuing Telemetry Transport) 是一种轻量级的消息传输协议，非常适合物联网设备。

### 基本概念

```
┌─────────┐    发布消息     ┌──────────┐    订阅主题    ┌─────────────┐
│ ESP32   │ ───────────→   │  Broker  │ ←──────────   │ Home        │
│ 设备    │                 │  服务器  │                │ Assistant   │
└─────────┘ ←───────────   └──────────┘ ───────────→  └─────────────┘
             订阅命令                      发布状态
```

**核心元素**：
- **Broker**（服务器）：消息中转站
- **Topic**（主题）：消息分类（如 `XZ-ESP32-01/fan/state`）
- **Publish**（发布）：发送消息
- **Subscribe**（订阅）：接收消息

---

## 1.2 基本配置

### 修改 config.h

```cpp
// MQTT 连接配置
#define MQTT_URI       "mqtt://106.53.179.231:1883"  // MQTT服务器地址
#define CLIENT_ID      "ESP32-xiaozhi-V2"            // 设备唯一标识
#define MQTT_USERNAME  "admin"                       // 用户名
#define MQTT_PASSWORD  "azsxdcfv"                    // 密码
#define MQTT_COMMAND_TOPIC    "HA-XZ-01/01/state"    // 默认主题

// Home Assistant 自动发现主题
#define MQTT_HA_FAN_CONFIG_TOPIC            "homeassistant/fan/XZ-ESP32-01/fan/config"
#define MQTT_HA_FAN_STATE_TOPIC             "XZ-ESP32-01/fan/state"
#define MQTT_HA_FAN_COMMAND_TOPIC           "XZ-ESP32-01/fan/set"
#define MQTT_HA_FAN_PERCENTAGE_COMMAND_TOPIC "XZ-ESP32-01/fan/percentage/set"

// 设备信息
#define DEVICE_ID          "XZ-ESP32-01"
#define DEVICE_NAME        "小智 ESP32"
#define DEVICE_SW_VERSION  "2.0.3"
```

### 公共 MQTT 服务器（测试用）

如果没有自己的服务器，可以使用公共测试服务器：

| 服务商 | URI | 用户名/密码 |
|--------|-----|------------|
| EMQX | `mqtt://broker.emqx.io:1883` | 留空 |
| Eclipse Mosquitto | `mqtt://test.mosquitto.org:1883` | 留空 |
| HiveMQ | `mqtt://broker.hivemq.com:1883` | 留空 |

---

## 1.3 第一个消息

### 通过 MCP 工具发送

语音控制："发送 hello"

或直接调用工具：
```json
{
  "name": "self.mqtt.send_hello",
  "arguments": {}
}
```

### 查看设备日志

```
I (12345) MqttController: MQTT connected to broker
I (12567) MqttController: Published to [HA-XZ-01/01/state]: hello (msg_id=1)
```

---

## 1.4 Home Assistant 集成流程

```
步骤 1: 开启风扇
  语音: "开风扇"
  │
  ▼
步骤 2: 自动发送配置
  主题: homeassistant/fan/XZ-ESP32-01/fan/config
  内容: {设备自动发现配置}
  │
  ▼
步骤 3: HA 自动创建设备
  设备: 小智 ESP32
  实体: 小智风扇
  │
  ▼
步骤 4: 双向控制生效
  HA → 设备: 控制命令
  设备 → HA: 状态同步
```

---

# 第二章：MQTT 控制器使用

## 2.1 可用的 MCP 工具

### 2.1.1 发送 Hello 消息

**工具名**: `self.mqtt.send_hello`

**功能**: 向默认主题发送 "hello" 测试消息

**参数**: 无

**返回**: `true` (成功) 或 `false` (失败)

**使用场景**: 测试 MQTT 连接是否正常

---

### 2.1.2 查询连接状态

**工具名**: `self.mqtt.get_status`

**功能**: 查询 MQTT 连接状态和配置信息

**参数**: 无

**返回**: JSON 格式状态
```json
{
  "connected": true,
  "broker": "mqtt://106.53.179.231:1883",
  "topic": "HA-XZ-01/01/state"
}
```

---

### 2.1.3 发送自定义消息

**工具名**: `self.mqtt.send_message`

**功能**: 向指定主题发送自定义消息

**参数**:
- `topic` (字符串): MQTT 主题
- `message` (字符串): 消息内容

**返回**: `true` (成功) 或 `false` (失败)

**示例**:
```json
{
  "name": "self.mqtt.send_message",
  "arguments": {
    "topic": "xiaozhi/custom",
    "message": "温度25度"
  }
}
```

---

## 2.2 风扇控制工具

### 2.2.1 开启风扇

**工具名**: `self.fan.turn_on`

**功能**: 
1. 开启风扇（默认二档）
2. 发送 Home Assistant 自动发现配置
3. 订阅 MQTT 命令主题

**执行流程**:
```
self.fan.turn_on 调用
  ├─> SetSpeed(2)                    // 硬件：风扇转动
  ├─> PublishHAConfig()              // 发送 HA 配置
  ├─> SubscribeCommands()            // 订阅命令主题
  └─> PublishFanState()              // 发送当前状态
```

---

### 2.2.2 设置速度

**工具名**: `self.fan.set_speed`

**参数**: `level` (整数, 0-3)
- 0: 关闭
- 1: 一档 (33%)
- 2: 二档 (67%)
- 3: 三档 (100%)

**示例**:
```
"把风扇调到最高档" → level = 3
```

---

### 2.2.3 关闭风扇

**工具名**: `self.fan.turn_off`

**功能**: 关闭风扇（设置为 0 档）

---

## 2.3 语音控制示例

通过小智 AI 语音控制：

| 语音指令 | 调用工具 | 效果 |
|---------|---------|------|
| "发送 hello" | `self.mqtt.send_hello` | 测试 MQTT |
| "查看 MQTT 状态" | `self.mqtt.get_status` | 显示连接状态 |
| "开风扇" | `self.fan.turn_on` | 开风扇 + HA 集成 |
| "把风扇调到最高档" | `self.fan.set_speed` level=3 | 三档 |
| "关风扇" | `self.fan.turn_off` | 关闭 |

---

# 第三章：Home Assistant 组件配置

## 3.1 组件类型对比

Home Assistant 支持多种 MQTT 设备类型：

| 组件类型 | 用途 | 双向控制 | 适用场景 |
|---------|------|---------|---------|
| **Fan** | 风扇控制 | ✅ 是 | 可控制的风扇设备 |
| **Sensor** | 传感器数据 | ❌ 否 | 只读数据（温度、湿度等） |
| **Switch** | 开关控制 | ✅ 是 | 简单的开/关设备 |
| **Light** | 灯光控制 | ✅ 是 | 灯光（亮度、颜色） |
| **Binary Sensor** | 二进制传感器 | ❌ 否 | 开/关状态（门、窗） |

---

## 3.2 Fan 组件（风扇）

### 3.2.1 使用场景

- ✅ 需要从 Home Assistant 控制风扇开关
- ✅ 需要调节风扇速度
- ✅ 需要双向通信（设备↔HA）

---

### 3.2.2 配置主题格式

```
homeassistant/fan/{设备ID}/{实体ID}/config
```

**示例**: `homeassistant/fan/XZ-ESP32-01/fan/config`

---

### 3.2.3 完整配置示例

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
  "percentage_value_template": "{{ value_json.speed }}",
  
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

---

### 3.2.4 必需字段说明

| 字段 | 类型 | 说明 | 示例 |
|------|------|------|------|
| `unique_id` | String | 全局唯一标识符 | `XZ-ESP32-01-fan` |
| `name` | String | 显示名称 | `小智风扇` |
| `command_topic` | String | **接收控制命令的主题** | `XZ-ESP32-01/fan/set` |
| `state_topic` | String | 发送状态的主题 | `XZ-ESP32-01/fan/state` |

---

### 3.2.5 状态消息格式

**主题**: `XZ-ESP32-01/fan/state`

**负载**:
```json
{
  "state": "ON",
  "speed": 2
}
```

**字段说明**:
- `state`: 风扇状态（`ON` 或 `OFF`）
- `speed`: 档位（0-3）

---

### 3.2.6 命令消息格式（设备需订阅）

**开关命令**:
- 主题: `XZ-ESP32-01/fan/set`
- 负载: `ON` 或 `OFF`

**速度命令**:
- 主题: `XZ-ESP32-01/fan/percentage/set`
- 负载: `0` / `1` / `2` / `3`（档位值）

---

### 3.2.7 ⚠️ Fan 组件注意事项

**不支持的字段**:
- ❌ `device_class` - 这是 sensor 专用字段
- ❌ `unit_of_measurement` - sensor 专用
- ❌ `value_template` - 使用 `state_value_template` 代替

**重要配置**:
- ✅ `speed_range_min` **必须从 1 开始**（见 4.2 章节）
- ✅ 必须提供 `command_topic`
- ✅ 必须订阅命令主题

---

## 3.3 Light 组件（灯光）

### 3.3.1 使用场景

- ✅ 需要从 Home Assistant 控制灯光开关
- ✅ 需要双向通信（设备↔HA）
- ✅ 简单的开关控制

---

### 3.3.2 配置主题格式

```
homeassistant/light/{设备ID}/{实体ID}/config
```

**示例**: `homeassistant/light/XZ-ESP32-01/lamp/config`

---

### 3.3.3 完整配置示例

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
    "manufacturer": "XiaoZhi",
    "sw_version": "2.0.3"
  }
}
```

---

### 3.3.4 必需字段说明

| 字段 | 类型 | 说明 | 示例 |
|------|------|------|------|
| `unique_id` | String | 全局唯一标识符 | `XZ-ESP32-01-lamp` |
| `name` | String | 显示名称 | `小智灯光` |
| `command_topic` | String | **接收控制命令的主题** | `XZ-ESP32-01/lamp/set` |
| `state_topic` | String | 发送状态的主题 | `XZ-ESP32-01/lamp/state` |
| `state_value_template` | String | 状态提取模板 | `{{ value_json.state }}` |
| `payload_on` | String | 开启命令 | `ON` |
| `payload_off` | String | 关闭命令 | `OFF` |

---

### 3.3.5 状态消息格式

```json
// 开启状态
{"state": "ON"}

// 关闭状态
{"state": "OFF"}
```

---

### 3.3.6 控制命令格式

Home Assistant 发送：

```
主题: XZ-ESP32-01/lamp/set
消息: "ON"  或  "OFF"
```

---

### 3.3.7 代码实现示例

```cpp
// 订阅命令主题
mqtt->Subscribe(MQTT_HA_LAMP_COMMAND_TOPIC, 
    [this](const std::string& topic, const std::string& payload) {
        if (payload == "ON") {
            // 开灯
            gpio_set_level(lamp_gpio_, 1);
        } else if (payload == "OFF") {
            // 关灯
            gpio_set_level(lamp_gpio_, 0);
        }
        // 发送状态更新
        PublishLampState();
    });
```

---

### 3.3.8 关键要点

- ✅ Light 组件支持基础的开关控制
- ✅ 可选扩展：亮度（brightness）、颜色（RGB）
- ✅ 必须提供 `command_topic`
- ✅ 必须订阅命令主题
- ✅ 图标使用 `mdi:lightbulb` 或 `mdi:lamp`

---

## 3.4 Sensor 组件（传感器）

### 3.4.1 使用场景

- ✅ 只需要显示设备状态（单向）
- ✅ 不需要从 Home Assistant 控制
- ✅ 读取传感器数据（温度、湿度等）

---

### 3.4.2 配置示例

**主题**: `homeassistant/sensor/XZ-ESP32-01/fan/config`

```json
{
  "unique_id": "XZ-ESP32-01-fan-status",
  "name": "小智风扇状态",
  "icon": "mdi:fan",
  
  "state_topic": "XZ-ESP32-01/fan/state",
  "value_template": "{{ value_json.state }}",
  
  "device_class": "enum",
  
  "device": {
    "identifiers": ["XZ-ESP32-01"],
    "name": "小智 ESP32",
    "model": "ESP32-S3",
    "manufacturer": "XiaoZhi"
  }
}
```

---

### 3.4.3 常用 device_class 值

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

## 3.5 Device（设备）配置

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

## 3.6 常用图标（MDI）

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

# 第四章：常见问题与解决方案

## 4.1 问题：Fan 组件无法创建设备

### 问题现象

发送配置到 `homeassistant/fan/XZ-ESP32-01/fan/config` 后，Home Assistant 中没有出现设备。

---

### 根本原因

Fan 组件配置中包含了**不支持的字段** `device_class`。

```json
// ❌ 错误配置
{
  "device_class": "fan",  // ← Fan 组件不支持此字段
  "state_topic": "...",
  ...
}
```

**原因分析**:

| 组件类型 | device_class 支持 | 说明 |
|---------|------------------|------|
| **Sensor** | ✅ 支持 | 用于分类传感器（温度、湿度等） |
| **Binary Sensor** | ✅ 支持 | 用于分类二进制传感器（门、窗等） |
| **Fan** | ❌ **不支持** | Fan 是独立的组件类型，不需要分类 |
| **Light** | ❌ 不支持 | 同样不需要 |
| **Switch** | ❌ 不支持 | 同样不需要 |

---

### 解决方案 A：改用 Sensor 组件（推荐 - 简单）

**适用场景**: 
- ✅ 只需要在 HA 中显示风扇状态
- ✅ 不需要从 HA 控制风扇
- ✅ 通过语音/MCP 控制即可

**修改内容**:

1. **config.h** - 改为 sensor 类型
```cpp
#define MQTT_HA_FAN_CONFIG_TOPIC  "homeassistant/sensor/XZ-ESP32-01/fan/config"
```

2. **fan_controller.h** - 移除不支持的字段
```cpp
snprintf(config_json, sizeof(config_json),
    "{"
    "\"unique_id\":\"%s-fan-status\","
    "\"name\":\"小智风扇状态\","
    "\"icon\":\"mdi:fan\","
    "\"state_topic\":\"%s\","
    "\"value_template\":\"{{ value_json.state }}\","
    "\"device\":{...}"
    "}",
    DEVICE_ID, MQTT_HA_FAN_STATE_TOPIC
);
```

**优点**:
- ✅ 实现简单，无需修改太多代码
- ✅ 不需要订阅 MQTT 命令主题
- ✅ 单向通信（ESP32 → HA）

**缺点**:
- ❌ 无法从 HA 界面控制风扇
- ❌ 无法使用 HA 自动化直接控制

---

### 解决方案 B：使用真正的 Fan 组件（完整功能）

**适用场景**:
- ✅ 需要从 HA 界面控制风扇
- ✅ 需要 HA 自动化控制风扇
- ✅ 需要完整的双向通信

**修改内容**:

1. **确保配置正确**（参见 3.2 章节）
2. **移除 `device_class` 字段**
3. **实现 MQTT 订阅**（已在当前代码中实现）

**优点**:
- ✅ 真正的风扇设备
- ✅ 支持从 HA 控制开关和速度
- ✅ 完整的 HA 集成体验

**缺点**:
- ⚠️ 需要实现 MQTT 订阅功能
- ⚠️ 代码复杂度增加

---

## 4.2 问题：speed_range_min 配置错误

### 问题现象

使用以下配置时，Home Assistant 无法识别设备：

```json
{
  "speed_range_min": 0,
  "speed_range_max": 3
}
```

---

### 根本原因

**`speed_range_min` 必须从 1 开始！**

- ❌ `"speed_range_min": 0` → Home Assistant 无法识别设备
- ✅ `"speed_range_min": 1` → 正常工作

---

### 原因解析

`speed_range_min` 和 `speed_range_max` 定义的是**有效速度档位的范围**。

- **0 不是速度档位**，0 代表**关闭状态**（OFF）
- 速度范围必须从 **1 开始**，表示最低档位

---

### 正确理解

```
speed_range_min: 1, speed_range_max: 3
→ 表示风扇有 3 个速度档位（1档、2档、3档）
→ 关闭状态通过 payload_off: "OFF" 或 speed: 0 控制
```

---

### 设备状态返回

```json
{"state": "OFF", "speed": 0}   // 关闭（0 不在 speed_range 内）
{"state": "ON", "speed": 1}    // 一档（在 speed_range 内）
{"state": "ON", "speed": 2}    // 二档（在 speed_range 内）
{"state": "ON", "speed": 3}    // 三档（在 speed_range 内）
```

---

### Home Assistant 命令

- 发送 `0` → 设备关闭
- 发送 `1-3` → 对应档位

---

### 测试经验

- 使用 `speed_range_min: 0` 时，HA 无法创建设备实体
- 改为 `speed_range_min: 1` 后，设备正常出现并可控制

---

## 4.3 决策指南

### 何时使用 Sensor 组件

```
✅ 只需要显示状态
✅ 不需要从 HA 控制
✅ 通过语音/MCP 控制即可
✅ 实现简单快速
```

### 何时使用 Fan 组件

```
✅ 需要从 HA 界面控制
✅ 需要 HA 自动化（定时、联动）
✅ 需要完整的双向通信
✅ 愿意实现订阅功能
```

---

# 第五章：调试与故障排查

## 5.1 查看 MQTT 流量

### 使用 mosquitto_sub

**订阅所有 HA 自动发现消息**:
```bash
mosquitto_sub -h BROKER_IP -p 1883 -u USER -P PASS -t "homeassistant/#" -v
```

**订阅设备状态消息**:
```bash
mosquitto_sub -h BROKER_IP -p 1883 -u USER -P PASS -t "XZ-ESP32-01/#" -v
```

**订阅所有消息**（调试用）:
```bash
mosquitto_sub -h BROKER_IP -p 1883 -u USER -P PASS -t "#" -v
```

---

## 5.2 验证 JSON 格式

### 在线工具

- https://jsonlint.com/ - JSON 格式验证
- https://www.json.cn/ - 中文 JSON 工具

### 命令行工具

```bash
# 使用 jq 验证和格式化
echo '{"state":"ON","speed":2}' | jq .
```

---

## 5.3 Home Assistant 日志

### 启用 MQTT 调试日志

编辑 `configuration.yaml`:
```yaml
logger:
  default: warning
  logs:
    homeassistant.components.mqtt: debug
```

### 查看日志

**Home Assistant UI**:
1. 设置 → 系统 → 日志
2. 搜索 "mqtt"

**命令行**:
```bash
# Docker
docker logs homeassistant | grep mqtt

# Home Assistant OS
ha logs
```

---

## 5.4 ESP32 设备日志

### 查看串口日志

```bash
idf.py monitor
```

### 关键日志示例

**MQTT 连接成功**:
```
I (12345) MqttController: MQTT connected to broker
```

**发送配置消息**:
```
I (12456) FanController: Published HA config to homeassistant/fan/XZ-ESP32-01/fan/config
```

**发送状态**:
```
I (12567) FanController: Published fan state: {"state":"ON","speed":2}
```

**接收命令**:
```
I (12678) MqttController: Received message on [XZ-ESP32-01/fan/set]: ON
I (12789) FanController: Fan turned ON via MQTT
```

---

## 5.5 常见故障排查

### 问题 1: MQTT 无法连接

**检查清单**:
- ✅ WiFi 是否连接成功
- ✅ Broker 地址和端口是否正确
- ✅ 用户名和密码是否正确
- ✅ Broker 是否在线

**调试命令**:
```bash
# 测试 Broker 连接
mosquitto_sub -h BROKER_IP -p 1883 -u USER -P PASS -t test
```

---

### 问题 2: Home Assistant 无设备

**检查清单**:
- ✅ MQTT 集成是否启用
- ✅ 配置 JSON 格式是否正确
- ✅ `speed_range_min` 是否从 1 开始
- ✅ 是否包含 `device_class` 字段（Fan 不支持）

**验证方法**:
```bash
# 查看是否收到配置消息
mosquitto_sub -h BROKER_IP -p 1883 -u USER -P PASS -t "homeassistant/#" -v
```

---

### 问题 3: 状态不同步

**检查清单**:
- ✅ 主题名称是否匹配
- ✅ JSON 模板是否正确
- ✅ 设备是否真的发送了状态

**调试方法**:
```bash
# 监控状态主题
mosquitto_sub -h BROKER_IP -p 1883 -u USER -P PASS -t "XZ-ESP32-01/fan/state" -v
```

---

### 问题 4: HA 无法控制设备

**检查清单**:
- ✅ 设备是否订阅了命令主题
- ✅ 命令主题名称是否正确
- ✅ 设备网络是否正常

**调试方法**:
```bash
# 手动发送命令测试
mosquitto_pub -h BROKER_IP -p 1883 -u USER -P PASS -t "XZ-ESP32-01/fan/set" -m "ON"
```

---

## 5.6 使用 MQTT 客户端工具

### 桌面工具

- **MQTTX** - 跨平台，现代化界面
  - 下载：https://mqttx.app/
  
- **MQTT Explorer** - 可视化主题树
  - 下载：https://mqtt-explorer.com/

### 网页工具

- **HiveMQ Web Client**
  - 地址：http://www.hivemq.com/demos/websocket-client/

---

# 第六章：API 参考

## 6.1 MCP 工具 API

### MQTT 基础工具

| 工具名 | 参数 | 返回 | 说明 |
|--------|------|------|------|
| `self.mqtt.send_hello` | 无 | boolean | 发送测试消息 |
| `self.mqtt.get_status` | 无 | JSON | 查询连接状态 |
| `self.mqtt.send_message` | topic, message | boolean | 发送自定义消息 |

### 风扇控制工具

| 工具名 | 参数 | 返回 | 说明 |
|--------|------|------|------|
| `self.fan.turn_on` | 无 | boolean | 开启风扇（二档） |
| `self.fan.turn_off` | 无 | boolean | 关闭风扇 |
| `self.fan.set_speed` | level (0-3) | boolean | 设置速度档位 |
| `self.fan.get_state` | 无 | JSON | 查询当前状态 |

---

## 6.2 MQTT 主题列表

### 发布主题（设备 → HA）

| 主题 | 内容 | 说明 |
|------|------|------|
| `homeassistant/fan/XZ-ESP32-01/fan/config` | 自动发现配置 JSON | 设备注册 |
| `XZ-ESP32-01/fan/state` | `{"state":"ON","speed":2}` | 状态上报 |

### 订阅主题（HA → 设备）

| 主题 | 内容 | 说明 |
|------|------|------|
| `XZ-ESP32-01/fan/set` | `ON` / `OFF` | 开关命令 |
| `XZ-ESP32-01/fan/percentage/set` | `0` / `1` / `2` / `3` | 档位命令 |

---

## 6.3 消息格式规范

### 配置消息

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
  "percentage_value_template": "{{ value_json.speed }}",
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

### 状态消息

```json
{
  "state": "ON",
  "speed": 2
}
```

**字段说明**:
- `state`: `"ON"` 或 `"OFF"`
- `speed`: 0-3 档位值

---

## 6.4 代码文件说明

| 文件 | 功能 | 关键类/函数 |
|------|------|-----------|
| `config.h` | MQTT 配置参数 | 主题定义、设备信息 |
| `mqtt_controller.h` | MQTT 客户端 | MqttController 类 |
| `fan_controller.h` | 风扇控制器 | FanController 类 |
| `compact_wifi_board_lcd.cc` | 板型集成 | 控制器初始化 |

---

## 附录：外部资源

### 官方文档

- [Home Assistant MQTT 集成](https://www.home-assistant.io/integrations/mqtt/)
- [MQTT Fan 组件](https://www.home-assistant.io/integrations/fan.mqtt/)
- [MQTT Sensor 组件](https://www.home-assistant.io/integrations/sensor.mqtt/)
- [MQTT 协议规范](https://mqtt.org/mqtt-specification/)

### 图标资源

- [MDI 图标库](https://pictogrammers.com/library/mdi/)
- [Font Awesome](https://fontawesome.com/icons)

### 工具下载

- [MQTTX](https://mqttx.app/)
- [MQTT Explorer](https://mqtt-explorer.com/)
- [mosquitto](https://mosquitto.org/download/)

---

**文档版本**: v2.0  
**最后更新**: 2025-10-23  
**作者**: XiaoZhi Team  
**反馈**: 如发现问题或有改进建议，请提 Issue

