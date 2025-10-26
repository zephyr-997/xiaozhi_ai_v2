#ifndef __UART_SERVICE_H__
#define __UART_SERVICE_H__

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

/** @brief 表示一帧 UART 协议数据的载荷部分。 */
struct UartServiceFrame {
    uint8_t field1;
    uint8_t field2;
    uint8_t field3;
};

/** @brief 提供 UART 帧解析能力的工具类。 */
class UartService {
public:
    static constexpr uint8_t kFrameHeader = 0x65;
    static constexpr uint8_t kFrameTailByte = 0xFF;
    static constexpr size_t kFrameTailLength = 3;
    static constexpr size_t kFramePayloadLength = 3;
    static constexpr size_t kFrameLength = 1 + kFramePayloadLength + kFrameTailLength;

    /** @brief 解析原始字节数组。 */
    std::optional<UartServiceFrame> Parse(const uint8_t* data, size_t length) const;

    /** @brief 解析字节向量中的数据。 */
    std::optional<UartServiceFrame> Parse(const std::vector<uint8_t>& buffer) const;

    /** @brief 解析 HEX 字符串形式的帧数据。 */
    std::optional<UartServiceFrame> ParseHexString(const std::string& hex_string) const;
};

#endif // __UART_SERVICE_H__

