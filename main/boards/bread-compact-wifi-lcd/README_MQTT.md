# MQTT 功能说明

ESP32 设备通过 MQTT 协议与 Home Assistant 集成，支持设备自动发现和双向控制。

---

## 📚 文档导航

### 主要文档

- **[MQTT_完整指南.md](MQTT_完整指南.md)** - 完整的使用和配置指南（推荐阅读）
- **[MQTT_快速参考.md](MQTT_快速参考.md)** - 单页速查表

### 选择文档

```
需要快速查找配置格式？
└─> MQTT_快速参考.md

想深入了解工作原理？
└─> MQTT_完整指南.md

遇到问题？
└─> MQTT_完整指南.md - 第四章
```

---

## 🚀 快速开始

### 1. 配置 MQTT 服务器

编辑 `config.h`：

```cpp
#define MQTT_URI       "mqtt://你的服务器:1883"
#define MQTT_USERNAME  "admin"
#define MQTT_PASSWORD  "密码"
```

### 2. 测试连接

通过 MCP 工具发送测试消息：

```
"发送 hello"  → 调用 self.mqtt.send_hello
```

### 3. 集成 Home Assistant

开启设备时自动发送配置：

```
"开风扇"  → 调用 self.fan.turn_on
          → 自动发送 HA 自动发现配置
          → Home Assistant 中出现风扇设备

"开灯"    → 调用 self.lamp.turn_on
          → 自动发送 HA 自动发现配置
          → Home Assistant 中出现灯光设备
```

---

## 🎯 核心功能

### MCP 工具

| 工具名 | 功能 | 参数 |
|--------|------|------|
| `self.mqtt.send_hello` | 发送测试消息 | 无 |
| `self.mqtt.get_status` | 查询连接状态 | 无 |
| `self.mqtt.send_message` | 发送自定义消息 | topic, message |
| `self.fan.turn_on` | 开启风扇 + 发送 HA 配置 | 无 |
| `self.fan.set_speed` | 设置风扇速度 | level (0-3) |
| `self.lamp.turn_on` | 开启灯光 + 发送 HA 配置 | 无 |
| `self.lamp.turn_off` | 关闭灯光 | 无 |
| `self.lamp.get_state` | 查询灯光状态 | 无 |

### Home Assistant 集成

- ✅ 自动发现设备
- ✅ 双向控制（HA ↔ 设备）
- ✅ 实时状态同步
- ✅ 风扇速度档位控制（3档）
- ✅ 灯光开关控制

---

## ❓ 常见问题

### Q1: Fan 组件无法创建设备？

**原因**：`speed_range_min` 必须从 1 开始，不能是 0

**解决**：修改配置为 `"speed_range_min": 1`

详见：[MQTT_完整指南.md - 4.2 speed_range_min 配置错误](MQTT_完整指南.md#42-问题speed_range_min-配置错误)

### Q2: 应该用 Fan 还是 Sensor 组件？

**决策**：
- 需要从 HA 控制 → Fan 组件
- 只显示状态 → Sensor 组件

详见：[MQTT_快速参考.md - 快速决策](MQTT_快速参考.md#-快速决策)

### Q3: 如何调试 MQTT 消息？

使用 mosquitto 订阅所有消息：

```bash
mosquitto_sub -h IP -p 1883 -u USER -P PASS -t "#" -v
```

详见：[MQTT_完整指南.md - 第五章](MQTT_完整指南.md#第五章调试与故障排查)

---

## 🔗 相关文件

### 代码文件

- `config.h` - MQTT 配置参数
- `mqtt_controller.h` - MQTT 客户端实现
- `fan_controller.h` - 风扇控制器实现
- `lamp_controller.h` - 灯光控制器实现

### 配置位置

```cpp
// 在 config.h 中定义主题

// 风扇主题
#define MQTT_HA_FAN_CONFIG_TOPIC      "homeassistant/fan/XZ-ESP32-01/fan/config"
#define MQTT_HA_FAN_STATE_TOPIC       "XZ-ESP32-01/fan/state"
#define MQTT_HA_FAN_COMMAND_TOPIC     "XZ-ESP32-01/fan/set"

// 灯光主题
#define MQTT_HA_LAMP_CONFIG_TOPIC     "homeassistant/light/XZ-ESP32-01/lamp/config"
#define MQTT_HA_LAMP_STATE_TOPIC      "XZ-ESP32-01/lamp/state"
#define MQTT_HA_LAMP_COMMAND_TOPIC    "XZ-ESP32-01/lamp/set"
```

---

## 🔧 故障排查

1. **MQTT 无法连接**
   - 检查 WiFi 是否连接
   - 验证服务器地址和端口
   - 确认用户名密码正确

2. **Home Assistant 无设备**
   - 查看 MQTT 集成是否启用
   - 检查配置 JSON 格式是否正确
   - 确认 `speed_range_min` 从 1 开始

3. **状态不同步**
   - 查看设备日志
   - 使用 mosquitto_sub 监控消息
   - 检查主题名称是否匹配

详细故障排查：[MQTT_完整指南.md - 第五章](MQTT_完整指南.md#第五章调试与故障排查)

---

## 📖 外部资源

- [Home Assistant MQTT 文档](https://www.home-assistant.io/integrations/mqtt/)
- [MQTT Fan 组件](https://www.home-assistant.io/integrations/fan.mqtt/)
- [MQTT Light 组件](https://www.home-assistant.io/integrations/light.mqtt/)
- [MQTT 协议规范](https://mqtt.org/)
- [MDI 图标库](https://pictogrammers.com/library/mdi/)

---

**更新日期**: 2025-10-23  
**作者**: XiaoZhi Team

