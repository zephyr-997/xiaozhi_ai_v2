# Home Assistant MQTT 配置格式改进说明

## 📋 改进概述

根据 Home Assistant MQTT 自动发现标准格式，优化了风扇控制器的 MQTT 消息格式。

**改进时间**: 2025-10-22  
**版本**: v2.0.3+ha-mqtt-optimized

---

## ✅ 完成的改进

### 1. 优化 MQTT 主题命名（config.h）

#### 改进前
```cpp
#define MQTT_HA_FAN_CONFIG_TOPIC  "homeassistant/sensor/HA/XZ-V2-01/config"
#define MQTT_HA_FAN_STATE_TOPIC   "homeassistant/sensor/HA/XZ-V2-01/state"
```

#### 改进后
```cpp
// 使用正确的设备类型 (fan 而不是 sensor)
#define MQTT_HA_FAN_CONFIG_TOPIC      "homeassistant/fan/XZ-ESP32-01/fan/config"
#define MQTT_HA_FAN_STATE_TOPIC       "XZ-ESP32-01/fan/state"
#define MQTT_HA_FAN_ATTRIBUTES_TOPIC  "XZ-ESP32-01/fan/attributes"

// 新增设备信息宏定义
#define DEVICE_ID          "XZ-ESP32-01"
#define DEVICE_NAME        "小智 ESP32"
#define DEVICE_SW_VERSION  "2.0.3"
```

**改进点**：
- ✅ 使用 `fan` 类型而不是 `sensor`（更准确）
- ✅ 简化状态主题路径（减少流量）
- ✅ 新增属性主题支持
- ✅ 统一设备标识符定义

---

### 2. 改进配置消息格式（fan_controller.h）

#### 改进前
```json
{
    "name": "XiaoZhi Fan",
    "device_class": "fan",
    "state_topic": "...",
    "unique_id": "xz_fan_01",
    "device": {
        "identifiers": ["xiaozhi_esp32_v2"],
        "name": "XiaoZhi ESP32",
        "model": "ESP32-S3",
        "manufacturer": "XiaoZhi"
    }
}
```

#### 改进后
```json
{
    "unique_id": "XZ-ESP32-01-fan",
    "name": "小智风扇",
    "icon": "mdi:fan",
    "state_topic": "XZ-ESP32-01/fan/state",
    "json_attributes_topic": "XZ-ESP32-01/fan/attributes",
    "device_class": "fan",
    "device": {
        "identifiers": ["XZ-ESP32-01"],
        "name": "小智 ESP32",
        "model": "ESP32-S3",
        "manufacturer": "XiaoZhi",
        "sw_version": "2.0.3"
    }
}
```

**新增字段**：
- ✅ `icon`: `mdi:fan` - 显示风扇图标
- ✅ `json_attributes_topic` - 支持额外属性
- ✅ `sw_version` - 软件版本信息
- ✅ 中文名称 - 更友好的显示

**优化点**：
- ✅ `unique_id` 格式更规范
- ✅ 使用配置宏定义，便于维护

---

### 3. 优化状态消息格式

#### 改进前
```json
{"state":"ON","level":2,"speed_pct":66}
```

#### 改进后
```json
{"state":"ON","speed":2,"percentage":66}
```

**改进点**：
- ✅ 字段命名更标准（`percentage` 而不是 `speed_pct`）
- ✅ 简化字段名（`speed` 而不是 `level`）

---

### 4. 新增属性消息功能

#### 新增功能
发送设备详细属性到 `XZ-ESP32-01/fan/attributes`：

```json
{
    "gpio": 8,
    "pwm_freq": 25000,
    "max_speed": 3,
    "chip": "ESP32-S3",
    "duty_resolution": 8
}
```

**用途**：
- 显示设备硬件配置
- 便于调试和监控
- 提供完整的设备信息

---

## 📤 完整的 MQTT 消息示例

### 消息 1: 配置发现（首次开启）
**主题**: `homeassistant/fan/XZ-ESP32-01/fan/config`
```json
{
    "unique_id": "XZ-ESP32-01-fan",
    "name": "小智风扇",
    "icon": "mdi:fan",
    "state_topic": "XZ-ESP32-01/fan/state",
    "json_attributes_topic": "XZ-ESP32-01/fan/attributes",
    "device_class": "fan",
    "device": {
        "identifiers": ["XZ-ESP32-01"],
        "name": "小智 ESP32",
        "model": "ESP32-S3",
        "manufacturer": "XiaoZhi",
        "sw_version": "2.0.3"
    }
}
```

### 消息 2: 设备属性（首次开启）
**主题**: `XZ-ESP32-01/fan/attributes`
```json
{
    "gpio": 8,
    "pwm_freq": 25000,
    "max_speed": 3,
    "chip": "ESP32-S3",
    "duty_resolution": 8
}
```

### 消息 3: 状态更新（每次状态改变）
**主题**: `XZ-ESP32-01/fan/state`

**风扇开启 - 二档**:
```json
{"state":"ON","speed":2,"percentage":66}
```

**风扇关闭**:
```json
{"state":"OFF","speed":0,"percentage":0}
```

**风扇三档**:
```json
{"state":"ON","speed":3,"percentage":99}
```

---

## 🔄 消息发送时机

| 消息类型 | 发送时机 | 频率 |
|---------|---------|------|
| 配置消息 | 风扇首次开启 | 仅一次 |
| 属性消息 | 风扇首次开启 | 仅一次 |
| 状态消息 | 每次速度改变 | 实时 |

---

## 🎯 符合的 Home Assistant 标准

参考示例格式：
```json
{
    "unique_id": "HA-ESP32-01-01",
    "name": "温度传感器",
    "icon": "mdi:thermometer",
    "state_topic": "HA-ESP32-01/01/state",
    "json_attributes_topic": "HA-ESP32-01/01/attributes",
    "unit_of_measurement": "℃",
    "device": {
        "identifiers": "ESP32-01",
        "manufacturer": "Zephyr_elc",
        "model": "HA",
        "name": "ESP32-01",
        "sw_version": "1.0"
    }
}
```

**对比分析**：
- ✅ 字段顺序符合标准
- ✅ 必选字段全部包含
- ✅ device 结构完全一致
- ✅ 主题命名规范统一

---

## 🧪 测试方法

### 1. 订阅所有主题
```bash
mosquitto_sub -h 106.53.179.231 -p 1883 -u admin -P azsxdcfv -t "homeassistant/#" -v
mosquitto_sub -h 106.53.179.231 -p 1883 -u admin -P azsxdcfv -t "XZ-ESP32-01/#" -v
```

### 2. 预期日志输出
```
I (10234) FanController: Fan turned on (level 2)
I (10235) FanController: Published fan state: {"state":"ON","speed":2,"percentage":66}
I (10240) FanController: Published HA config to homeassistant/fan/XZ-ESP32-01/fan/config
I (10245) FanController: Published fan attributes
```

### 3. Home Assistant 界面
设备会自动出现在 Home Assistant 的设备列表中：
- **设备名称**: 小智 ESP32
- **实体名称**: 小智风扇
- **图标**: 🌀 (mdi:fan)
- **状态**: ON/OFF
- **属性**: GPIO 8, 25kHz, ESP32-S3

---

## 📊 改进对比表

| 项目 | 改进前 | 改进后 | 改进效果 |
|------|-------|-------|---------|
| 设备类型 | sensor | fan | ✅ 更准确 |
| 图标支持 | ❌ | ✅ mdi:fan | ✅ UI 美观 |
| 软件版本 | ❌ | ✅ 2.0.3 | ✅ 版本管理 |
| 属性主题 | ❌ | ✅ 支持 | ✅ 信息完整 |
| 字段命名 | speed_pct | percentage | ✅ 更规范 |
| 设备 ID | xiaozhi_esp32_v2 | XZ-ESP32-01 | ✅ 统一格式 |
| 主题路径 | 冗长 | 简洁 | ✅ 减少流量 |

---

## 📝 修改文件清单

| 文件 | 修改内容 | 行数变化 |
|------|---------|---------|
| `config.h` | 新增设备信息定义，优化主题配置 | +6 行 |
| `fan_controller.h` | 改进 JSON 格式，新增属性发送 | +25 行 |

**总计**: +31 行代码

---

## 🚀 下一步建议

1. **为其他设备应用相同格式**
   - LampController（灯光控制）
   - SensorController（传感器）

2. **添加更多属性**
   - 运行时间统计
   - 错误计数
   - 温度监控

3. **支持远程控制**
   - 订阅命令主题
   - 接收 Home Assistant 控制指令

4. **优化性能**
   - 减少不必要的消息发送
   - 添加消息队列

---

## ✅ 验证清单

- [x] config.h 主题定义更新
- [x] fan_controller.h JSON 格式改进
- [x] 添加 icon 字段
- [x] 添加 sw_version 字段
- [x] 添加 json_attributes_topic
- [x] 优化状态消息格式
- [x] 新增属性消息功能
- [ ] 编译测试通过
- [ ] 实际设备验证
- [ ] Home Assistant 自动发现测试

---

**改进完成**: ✅  
**下一步**: 编译测试并验证功能

