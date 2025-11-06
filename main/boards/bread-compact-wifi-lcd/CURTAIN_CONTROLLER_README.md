# 窗帘控制器使用说明

## 概述

基于 28BYJ-48 步进电机和 ULN2003 驱动板实现的智能窗帘控制器。通过 MCP 协议暴露简洁的开关接口，支持语音/AI 控制。

**旋转方向**：
- 开窗帘：逆时针 360°
- 关窗帘：顺时针 360°

## 硬件连接

### 引脚定义

| ULN2003 引脚 | ESP32-S3 GPIO | 说明 |
|-------------|---------------|------|
| IN1 | GPIO_NUM_9 | 步进相位 A |
| IN2 | GPIO_NUM_10 | 步进相位 B |
| IN3 | GPIO_NUM_11 | 步进相位 C |
| IN4 | GPIO_NUM_12 | 步进相位 D |
| VCC | 5V/12V | 电机独立供电 |
| GND | GND | 与 ESP32 共地 |

### 接线示意图

```
ESP32-S3          ULN2003          28BYJ-48
  GPIO9  -------> IN1
  GPIO10 -------> IN2              [步进电机]
  GPIO11 -------> IN3                 |
  GPIO12 -------> IN4                 |
  GND    -------> GND <-------------> GND
                  VCC <--- 5V 独立电源
```

**⚠️ 重要提示：**
- ✅ 必须为驱动板提供独立的 5V 或 12V 电源（根据电机型号）
- ✅ 驱动板 GND 必须与 ESP32 GND 共地
- ❌ 切勿将电机电源接到 ESP32 电源引脚

---

## MCP 工具接口

### 1. 开窗帘
```
工具名称: self.curtain.open
描述: 打开窗帘（步进电机逆时针旋转 360°）
参数: 无

示例:
AI: "打开窗帘"
→ 调用 self.curtain.open
→ 电机逆时针旋转一圈
智能保护:
- 若窗帘正在运行 → 返回 "curtain_is_running"
- 若窗帘已是打开状态 → 返回 "already_open"
```

### 2. 关窗帘
```
工具名称: self.curtain.close
描述: 关闭窗帘（步进电机顺时针旋转 360°）
参数: 无

示例:
AI: "关闭窗帘"
→ 调用 self.curtain.close
→ 电机顺时针旋转一圈
智能保护:
- 若窗帘正在运行 → 返回 "curtain_is_running"
- 若窗帘已是关闭状态 → 返回 "already_closed"
```

### 3. 停止
```
工具名称: self.curtain.stop
描述: 立即停止窗帘运动并切断电机电流
参数: 无

使用场景:
- 窗帘运行过程中需要紧急停止
- 防止窗帘卡住时继续运转
```

### 4. 查询状态
```
工具名称: self.curtain.get_status
描述: 查询窗帘控制器状态
参数: 无

返回: JSON 格式
{
  "is_running": true/false,    // 是否正在运行
  "state": "open/closed/unknown",  // 当前状态
  "current_step": 0-7          // 当前步进序列位置
}
```

---

## 技术参数

### 电机规格
- **型号**: 28BYJ-48（5V 或 12V）
- **减速比**: 1:64
- **步进角度**: 5.625° / 64 = 0.0879° / 步
- **一圈步数**: 4096 步（360° / 0.0879°）

### 控制参数
- **步进方式**: 四相八拍（高扭矩模式）
- **旋转角度**: 固定 360°（一圈）
- **速度**: 2 ms/步（可在 config.h 中调整为 2-5 ms）
- **旋转时长**: 约 8.2 秒（4096 步 × 2 ms）

### 系统配置
- **FreeRTOS Tick**: 1000 Hz（1 ms 精度）
- **后台任务**: 优先级 6，栈大小 4096 字节
- **命令队列**: 2 个槽位（当前命令 + 1 个排队）
- **执行方式**: 非阻塞（后台任务执行，不影响其他功能）

---

## 使用场景示例

### 场景 1: 语音控制
```
用户: "小智，打开窗帘"
AI: 调用 self.curtain.open
→ 窗帘自动打开（约 8 秒）
AI: "窗帘已打开"
```

### 场景 2: 定时任务
```
用户: "每天早上 7 点打开窗帘"
AI: 设置定时任务
→ 每天 7:00 自动调用 self.curtain.open
```

### 场景 3: 紧急停止
```
窗帘运行中...
用户: "停止！"
AI: 调用 self.curtain.stop
→ 窗帘立即停止
```

---

## 配置修改

如需调整参数，编辑 `config.h`：

```cpp
// GPIO 引脚配置
#define CURTAIN_IN1_GPIO  GPIO_NUM_9   // 修改为您的引脚
#define CURTAIN_IN2_GPIO  GPIO_NUM_10
#define CURTAIN_IN3_GPIO  GPIO_NUM_11
#define CURTAIN_IN4_GPIO  GPIO_NUM_12

// 性能参数
#define CURTAIN_SPEED_MS  2  // 每步延时（ms）
                             // 2ms = 快速（8秒/圈）
                             // 3ms = 中速（12秒/圈）
                             // 5ms = 慢速（20秒/圈）

#define CURTAIN_TASK_STACK_SIZE  4096  // 任务栈大小
#define CURTAIN_TASK_PRIORITY    6     // 任务优先级
```

---

## 故障排查

### 问题 1: 窗帘不动
**可能原因**：
- ❌ 电机电源未连接或电压不足
- ❌ GPIO 引脚接线错误
- ❌ GND 未共地

**解决方法**：
1. 检查独立 5V/12V 电源是否正常
2. 使用万用表测量驱动板输出电压
3. 查看串口日志确认命令是否接收：
   ```
   I (xxxx) Curtain: Opening curtain (360° rotation)...
   ```

### 问题 2: 窗帘抖动或失步
**可能原因**：
- ❌ 速度过快（负载过重）
- ❌ 电源供电不足
- ❌ 机械阻力过大

**解决方法**：
1. 在 `config.h` 中增大 `CURTAIN_SPEED_MS`（如改为 3 或 5）
2. 检查窗帘导轨是否顺畅
3. 更换更大功率的电源

### 问题 3: 方向反了
**解决方法**：
- 交换 `self.curtain.open` 和 `self.curtain.close` 的调用
- 或修改硬件接线：交换 IN1↔IN4, IN2↔IN3

### 问题 4: 360° 不够/太多
**调整方法**：
编辑 `config.h`：
```cpp
// 调整一圈步数（默认 4096）
#define CURTAIN_STEPS_PER_TURN  4096  // 增大 = 多转
                                      // 减小 = 少转
```

实测后微调（例如实际需要 380°）：
```cpp
#define CURTAIN_STEPS_PER_TURN  4324  // 4096 * 380 / 360 ≈ 4324
```

---

## 串口监控

查看运行日志：

```bash
idf.py monitor
```

**关键日志示例**：
```
I (1234) Curtain: Initialized on GPIO IN1=9, IN2=10, IN3=11, IN4=12
I (1240) Curtain: Curtain task created successfully
I (5678) Curtain: Open curtain command -> OK
I (5680) Curtain: Opening curtain (360° rotation)...
I (13880) Curtain: Curtain operation completed
```

---

## 代码架构

### 文件结构
```
main/boards/bread-compact-wifi-lcd/
├── curtain_controller.h        # 窗帘控制器（简化版）
├── config.h                    # 硬件配置
└── compact_wifi_board_lcd.cc   # 板型主类（集成控制器）
```

### 关键特性
✅ **非阻塞执行** - 使用 FreeRTOS 后台任务  
✅ **自动断电** - 旋转完成后切断电流，减少发热  
✅ **命令排队** - 支持 1 个命令排队  
✅ **中途停止** - 可随时发送停止命令  
✅ **简洁接口** - 仅 4 个 MCP 工具（开/关/停/查询）

---

## 与通用步进电机控制器的区别

| 特性 | 通用版 (stepper_motor_controller.h) | 窗帘版 (curtain_controller.h) |
|------|-------------------------------------|------------------------------|
| 代码行数 | ~380 行 | ~220 行 |
| MCP 工具 | 5 个（角度/步数/停止/状态/速度） | 4 个（开/关/停止/状态） |
| 角度控制 | 0-360° 可调 | 固定 360° |
| 步数控制 | 1-8192 步可调 | ❌ 无 |
| 速度调整 | 运行时可调 | 固定（config.h 配置） |
| 命令队列 | 10 个 | 2 个 |
| 适用场景 | 通用步进电机应用 | 专用于窗帘开关 |

**简化优势**：
- ✅ 代码更易理解和维护
- ✅ 减少参数配置，降低使用门槛
- ✅ 专注窗帘场景，接口更直观

---

## 技术支持

- **项目架构**: `architecture.md`
- **MCP 协议**: `docs/mcp-protocol.md`
- **开发规范**: `.cursor/rules/xiaozhi-esp32.mdc`

---

**版本**: V1.0 (简化版)  
**最后更新**: 2025-11-06  
**兼容板型**: bread-compact-wifi-lcd  
**基于**: 方案 B（非阻塞版）

