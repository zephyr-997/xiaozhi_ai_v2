#include "uart_service.h"

#include <cstdlib>

/**
 * @brief 解析原始字节流，验证帧格式并提取载荷。
 *
 * 该函数假定传入的数据以帧头 0x65 开始，之后紧跟三个有效字节，最后是
 * 三个 0xFF 作为帧尾。函数会严格检查帧头和帧尾，如有任一不满足，则认定
 * 帧无效并返回 std::nullopt。
 *
 * @param data   指向 UART 接收缓冲区的指针。
 * @param length 缓冲区可用字节数，必须大于等于 kFrameLength。
 * @return 成功解析时返回包含三个有效字节的 UartServiceFrame；解析失败返回 std::nullopt。
 */
std::optional<UartServiceFrame> UartService::Parse(const uint8_t* data, size_t length) const {
    if (data == nullptr || length < kFrameLength) {
        return std::nullopt;
    }

    if (data[0] != kFrameHeader) {
        return std::nullopt;
    }

    const uint8_t* tail = data + kFrameLength - kFrameTailLength;
    if (!(tail[0] == kFrameTailByte && tail[1] == kFrameTailByte && tail[2] == kFrameTailByte)) {
        return std::nullopt;
    }

    UartServiceFrame frame;
    frame.field1 = data[1];
    frame.field2 = data[2];
    frame.field3 = data[3];
    return frame;
}

/**
 * @brief 解析字节向量，内部复用原始指针版本。
 *
 * 在调用 Parse(const uint8_t*, size_t) 之前会先检查向量是否为空，以避免对
 * buffer.data() 的未定义访问。
 *
 * @param buffer 保存 UART 数据的字节向量。
 * @return 与指针版本相同，成功时返回帧载荷，失败返回 std::nullopt。
 */
std::optional<UartServiceFrame> UartService::Parse(const std::vector<uint8_t>& buffer) const {
    if (buffer.empty()) {
        return std::nullopt;
    }
    return Parse(buffer.data(), buffer.size());
}

/**
 * @brief 将 HEX 字符串转换为字节流并解析。（选用功能）
 *
 * 字符串按两个字符为一组转换为字节；若字符串长度为奇数或包含非法字符，
 * strtoul 会在遇到非法字符时停止并返回转换次数，导致解析失败。
 *
 * @param hex_string 形如 "65020201FFFFFF" 的十六进制字符串。
 * @return 解析成功返回帧载荷，否则返回 std::nullopt。
 */
std::optional<UartServiceFrame> UartService::ParseHexString(const std::string& hex_string) const {
    if (hex_string.size() % 2 != 0) {
        return std::nullopt;
    }

    std::vector<uint8_t> buffer;
    buffer.reserve(hex_string.size() / 2);
    for (size_t i = 0; i < hex_string.size(); i += 2) {
        uint8_t byte = static_cast<uint8_t>(std::strtoul(hex_string.substr(i, 2).c_str(), nullptr, 16));
        buffer.push_back(byte);
    }

    return Parse(buffer);
}

