# 纯文本对话功能实现说明

本文档详细说明了如何在小智 ESP32 项目中实现纯文本驱动的语音回复功能。该功能允许设备通过串口（或 MQTT）接收文本指令，直接发送给服务器进行处理，并播放服务器返回的 TTS 音频。

## 1. 功能概述

原有的语音交互流程是：`麦克风录音 -> 上传音频 -> 服务器 ASR 识别 -> LLM 处理 -> TTS 生成 -> 下发音频 -> 播放`。

新增的文本交互流程是：`接收文本指令 -> 模拟 ASR 结果 -> 上传文本指令 -> LLM 处理 -> TTS 生成 -> 下发音频 -> 播放`。

核心难点在于：
1.  **协议适配**：服务器没有专门的“纯文本”接口，需要伪装成“语音识别结果” (`type: listen`, `state: detect`)。
2.  **连接管理**：在未唤醒（Idle）状态下，需要自动建立 MQTT 和 UDP 连接。
3.  **NAT 穿透**：在不发送麦克风音频的情况下，需要主动发送 UDP 打洞包，确保服务器的 TTS 音频能穿透防火墙到达设备。

## 2. 代码修改清单

### 2.1 协议层接口定义 (`main/protocols/protocol.h`)

在 `Protocol` 基类中增加了发送文本和查询连接状态的接口。

- **新增虚函数**：
    - `virtual bool SendText(const std::string& text) = 0;`：用于发送用户文本。
    - `virtual bool IsConnected() const = 0;`：用于检查连接状态。
- **修改**：
    - 将原有的 `SendText` 重命名为 `SendRawText` (protected)，避免命名冲突。

### 2.2 MQTT 协议实现 (`main/protocols/mqtt_protocol.h/cc`)

这是修改的核心部分，主要解决了自动连接和 UDP 打洞问题。

- **实现 `IsConnected`**：复用 `IsAudioChannelOpened` 逻辑。
- **重写 `SendText`**：
    1.  **自动重连**：如果当前未连接，调用 `OpenAudioChannel` 建立会话。
    2.  **UDP 打洞 (Hole Punching)**：
        - 在发送文本指令前，构造并发送包含 Opus 静音帧 (`0xF8, 0xFF, 0xFE`) 的 UDP 包。
        - **策略调整**：始终连续发送 5 次（间隔 20ms），无论当前连接状态如何。这是为了防止在长连接 Idle 状态下路由器 NAT 映射过期。
        - **延时调整**：增加 100ms 延时，确保 UDP 包穿透防火墙且服务器回包路径畅通后，再发送 MQTT 指令触发 TTS。
    3.  **协议封装**：
        - 构造 JSON：`{"type": "listen", "state": "detect", "text": "..."}`。
        - 这种格式模拟了设备端完成了语音识别（ASR）并上报结果，服务器会直接接手处理。

### 2.3 WebSocket 协议实现 (`main/protocols/websocket_protocol.h/cc`)

同步更新了 WebSocket 协议以支持相同功能（虽然当前主要使用 MQTT）。

- **实现 `IsConnected`**。
- **重写 `SendText`**：增加了自动重连逻辑和协议封装（`type: listen`）。

### 2.4 应用层逻辑 (`main/application.h/cc`)

在应用层提供了统一的调用入口。

- **新增 `ChatWithText` 方法**：
    - 接收文本字符串。
    - 强制停止当前的录音状态（如果正在录音）。
    - 更新屏幕显示（显示用户输入的文本）。
    - 调用协议层的 `SendText` 发送数据。
    - **关键修改**：移除了对 `IsConnected` 的前置检查，允许在未连接状态下调用，由协议层负责自动重连。
    - **状态控制**：引入 `should_listen_after_talk_` 标志。对于文本对话，设置为 `false`，使得设备在播放完回复后自动返回 Idle 状态，而不是进入聆听模式。

### 2.5 触发入口 (`main/boards/.../uart_controller.h`)

为了测试功能，在串口控制器中增加了指令解析。

- **解析 `chat:` 指令**：
    - 监听串口输入。
    - 当收到以 `chat:` 开头的字符串时（如 `chat:你好`），提取后续内容。
    - 调用 `Application::GetInstance().ChatWithText(...)` 触发对话。

### 2.6 MQTT 触发入口 (`main/boards/.../mqtt_controller.h`)

实现了通过 MQTT 订阅主题来接收文本指令。

- **订阅主题**：`MQTT_CHAT_TOPIC` (在 `config.h` 中定义，默认为 `XZ-ESP32-01/chat/information`)。
- **处理逻辑**：
    - 收到消息后，直接提取 Payload。
    - 调用 `Application::GetInstance().ChatWithText(...)` 触发对话。

## 3. 使用方法

### 3.1 串口触发

1.  **连接设备**：使用 USB 连接 ESP32，打开串口调试工具。
2.  **设置编码**：**必须确保串口工具发送的编码为 UTF-8**，否则中文会乱码。
3.  **发送指令**：
    - 发送 `chat:你好`
    - 发送 `chat:讲个笑话`

### 3.2 MQTT 触发

1.  **连接 MQTT**：使用 MQTT 客户端（如 MQTTX）连接到设备所在的 Broker。
2.  **发布消息**：
    - Topic: `XZ-ESP32-01/chat/information` (或你在 `config.h` 中修改的主题)
    - Payload: `你好`
3.  **预期结果**：
    - 屏幕显示“你好”。
    - 设备状态变为“思考中” -> “说话中”。
    - 设备播放服务器返回的语音回复。
    - 播放结束后，设备自动返回 Idle 状态，不会进入聆听模式。

## 4. 常见问题

- **有回复文本但无声音（卡在 Speaking 前）**：
    - **原因**：UDP 打洞失败或路由器 NAT 映射过期，导致服务器发回的音频流被拦截。
    - **解决**：已在 `MqttProtocol::SendText` 中增强了打洞逻辑（发送 5 个包 + 100ms 延时）。如果仍然遇到问题，请检查网络防火墙设置。
- **中文乱码**：串口工具发送了 GBK 编码。请切换为 UTF-8。
- **无反应**：检查设备是否已联网（显示 Standby 或 Idle）。

