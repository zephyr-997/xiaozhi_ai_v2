#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>
#include <esp_adc/adc_oneshot.h>

#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// 如果使用 Duplex I2S 模式，请注释下面一行
#define AUDIO_I2S_METHOD_SIMPLEX

#ifdef AUDIO_I2S_METHOD_SIMPLEX

#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_4
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_5
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_6
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_7
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_15
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_16

#else

#define AUDIO_I2S_GPIO_WS GPIO_NUM_4
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_5
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_6
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_7

#endif


#define BUILTIN_LED_GPIO        GPIO_NUM_48
#define BOOT_BUTTON_GPIO        GPIO_NUM_0
#define TOUCH_BUTTON_GPIO       GPIO_NUM_NC
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_NC
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_NC


#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_42
#define DISPLAY_MOSI_PIN      GPIO_NUM_47
#define DISPLAY_CLK_PIN       GPIO_NUM_21
#define DISPLAY_DC_PIN        GPIO_NUM_40
#define DISPLAY_RST_PIN       GPIO_NUM_45
#define DISPLAY_CS_PIN        GPIO_NUM_41


#ifdef CONFIG_LCD_ST7789_240X320
#define LCD_TYPE_ST7789_SERIAL
#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  320
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false
#define DISPLAY_INVERT_COLOR    true
#define DISPLAY_RGB_ORDER  LCD_RGB_ELEMENT_ORDER_RGB
#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_SPI_MODE 0
#endif

#ifdef CONFIG_LCD_ST7789_240X320_NO_IPS
#define LCD_TYPE_ST7789_SERIAL
#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  320
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false
#define DISPLAY_INVERT_COLOR    false
#define DISPLAY_RGB_ORDER  LCD_RGB_ELEMENT_ORDER_RGB
#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_SPI_MODE 0
#endif

#ifdef CONFIG_LCD_ST7789_170X320
#define LCD_TYPE_ST7789_SERIAL
#define DISPLAY_WIDTH   170
#define DISPLAY_HEIGHT  320
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false
#define DISPLAY_INVERT_COLOR    true
#define DISPLAY_RGB_ORDER  LCD_RGB_ELEMENT_ORDER_RGB
#define DISPLAY_OFFSET_X  35
#define DISPLAY_OFFSET_Y  0
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_SPI_MODE 0
#endif

#ifdef CONFIG_LCD_ST7789_172X320
#define LCD_TYPE_ST7789_SERIAL
#define DISPLAY_WIDTH   172
#define DISPLAY_HEIGHT  320
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false
#define DISPLAY_INVERT_COLOR    true
#define DISPLAY_RGB_ORDER  LCD_RGB_ELEMENT_ORDER_RGB
#define DISPLAY_OFFSET_X  34
#define DISPLAY_OFFSET_Y  0
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_SPI_MODE 0
#endif

#ifdef CONFIG_LCD_ST7789_240X280
#define LCD_TYPE_ST7789_SERIAL
#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  280
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false
#define DISPLAY_INVERT_COLOR    true
#define DISPLAY_RGB_ORDER  LCD_RGB_ELEMENT_ORDER_RGB
#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  20
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_SPI_MODE 0
#endif

#ifdef CONFIG_LCD_ST7789_240X240
#define LCD_TYPE_ST7789_SERIAL
#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  240
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false
#define DISPLAY_INVERT_COLOR    true
#define DISPLAY_RGB_ORDER  LCD_RGB_ELEMENT_ORDER_RGB
#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_SPI_MODE 0
#endif

#ifdef CONFIG_LCD_ST7789_240X240_7PIN
#define LCD_TYPE_ST7789_SERIAL
#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  240
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false
#define DISPLAY_INVERT_COLOR    true
#define DISPLAY_RGB_ORDER  LCD_RGB_ELEMENT_ORDER_RGB
#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_SPI_MODE 3
#endif

#ifdef CONFIG_LCD_ST7789_240X135
#define LCD_TYPE_ST7789_SERIAL
#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  135
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY true
#define DISPLAY_INVERT_COLOR    true
#define DISPLAY_RGB_ORDER  LCD_RGB_ELEMENT_ORDER_RGB
#define DISPLAY_OFFSET_X  40
#define DISPLAY_OFFSET_Y  53
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_SPI_MODE 0
#endif

#ifdef CONFIG_LCD_ST7735_128X160
#define LCD_TYPE_ST7789_SERIAL
#define DISPLAY_WIDTH   128
#define DISPLAY_HEIGHT  160
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y true
#define DISPLAY_SWAP_XY false
#define DISPLAY_INVERT_COLOR    false
#define DISPLAY_RGB_ORDER  LCD_RGB_ELEMENT_ORDER_RGB
#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_SPI_MODE 0
#endif

#ifdef CONFIG_LCD_ST7735_128X128
#define LCD_TYPE_ST7789_SERIAL
#define DISPLAY_WIDTH   128
#define DISPLAY_HEIGHT  128
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y true
#define DISPLAY_SWAP_XY false
#define DISPLAY_INVERT_COLOR  false
#define DISPLAY_RGB_ORDER  LCD_RGB_ELEMENT_ORDER_BGR
#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  32
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_SPI_MODE 0
#endif

#ifdef CONFIG_LCD_ST7796_320X480
#define LCD_TYPE_ST7789_SERIAL
#define DISPLAY_WIDTH   320
#define DISPLAY_HEIGHT  480
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false
#define DISPLAY_INVERT_COLOR    true
#define DISPLAY_RGB_ORDER  LCD_RGB_ELEMENT_ORDER_BGR
#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_SPI_MODE 0
#endif

#ifdef CONFIG_LCD_ST7796_320X480_NO_IPS
#define LCD_TYPE_ST7789_SERIAL
#define DISPLAY_WIDTH   320
#define DISPLAY_HEIGHT  480
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false
#define DISPLAY_INVERT_COLOR    false
#define DISPLAY_RGB_ORDER  LCD_RGB_ELEMENT_ORDER_BGR
#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_SPI_MODE 0
#endif

#ifdef CONFIG_LCD_ILI9341_240X320
#define LCD_TYPE_ILI9341_SERIAL
#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  320
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false
#define DISPLAY_INVERT_COLOR    true
#define DISPLAY_RGB_ORDER  LCD_RGB_ELEMENT_ORDER_BGR
#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_SPI_MODE 0
#endif

#ifdef CONFIG_LCD_ILI9341_240X320_NO_IPS
#define LCD_TYPE_ILI9341_SERIAL
#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  320
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false
#define DISPLAY_INVERT_COLOR    false
#define DISPLAY_RGB_ORDER  LCD_RGB_ELEMENT_ORDER_BGR
#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_SPI_MODE 0
#endif

#ifdef CONFIG_LCD_GC9A01_240X240
#define LCD_TYPE_GC9A01_SERIAL
#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  240
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false
#define DISPLAY_INVERT_COLOR    true
#define DISPLAY_RGB_ORDER  LCD_RGB_ELEMENT_ORDER_BGR
#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_SPI_MODE 0
#endif

#ifdef CONFIG_LCD_CUSTOM
#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  320
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false
#define DISPLAY_INVERT_COLOR    true
#define DISPLAY_RGB_ORDER  LCD_RGB_ELEMENT_ORDER_RGB
#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_SPI_MODE 0
#endif


// ==================== 物联网外设配置 ====================

// 灯光 GPIO 定义
#define LAMP_GPIO GPIO_NUM_1

// 风扇 GPIO 定义
#define FAN_GPIO GPIO_NUM_8

// 温湿度传感器 GPIO 定义
#define DHT11_GPIO GPIO_NUM_3

// DHT11 参数配置
#define DHT11_READ_INTERVAL_MS  5000  // 后台读取间隔 5 秒
#define DHT11_TASK_STACK_SIZE   4096  // 后台任务栈大小
#define DHT11_TASK_PRIORITY     5     // 后台任务优先级

// MQ-2 烟雾传感器 ADC 配置
#define MQ2_ADC_UNIT            ADC_UNIT_2       // GPIO14 对应 ADC2 单元
#define MQ2_ADC_CHANNEL         ADC_CHANNEL_3    // GPIO14 对应 ADC2 通道 3
#define MQ2_READ_TIMES          10               // 多次采样取平均
#define MQ2_READ_INTERVAL_MS    5000             // 后台读取间隔 5 秒
#define MQ2_TASK_STACK_SIZE     4096             // 后台任务栈大小
#define MQ2_TASK_PRIORITY       5                // 后台任务优先级
#define MQ2_PREHEAT_TIME_MS     5000             // 预热时间 5 秒（实际建议更长）
#define MQ2_R0_VALUE            30.0f            // 清洁空气下校准的 R0 值需现场调校
#define MQ2_ALERT_THRESHOLD     100.0f           // 告警阈值 (PPM)

// 窗帘控制器（步进电机）GPIO 定义
#define CURTAIN_IN1_GPIO  GPIO_NUM_9
#define CURTAIN_IN2_GPIO  GPIO_NUM_10
#define CURTAIN_IN3_GPIO  GPIO_NUM_11
#define CURTAIN_IN4_GPIO  GPIO_NUM_12

// 窗帘控制器参数配置
#define CURTAIN_STEPS_PER_TURN    4096    // 28BYJ-48 一圈步数（360°）
#define CURTAIN_TASK_STACK_SIZE   4096    // 后台任务栈大小
#define CURTAIN_TASK_PRIORITY     6       // 后台任务优先级
#define CURTAIN_SPEED_MS          2       // 每步延时(ms)，推荐 2-5ms

// UART1 配置， 烧录已经默认使用U0
#define UART1_TX_GPIO           GPIO_NUM_17
#define UART1_RX_GPIO           GPIO_NUM_18
#define UART1_BAUD_RATE         115200
#define UART1_RX_BUFFER_SIZE    512
#define UART1_TX_BUFFER_SIZE    256
#define UART1_RX_TASK_STACK_SIZE 4096
#define UART1_RX_TASK_PRIORITY   8
#define UART1_READ_TIMEOUT_MS    50
// UART 控制器内部限制
#define UART1_CONTROLLER_SEND_MAX_LEN     256
#define UART1_CONTROLLER_MUTEX_TIMEOUT_MS 1000
#define UART1_CONTROLLER_RECEIVE_MAX_LEN  UART1_RX_BUFFER_SIZE
#define UART1_CONTROLLER_RECEIVE_DEFAULT_LEN 128

// DHT11 MQTT 主题
#define MQTT_HA_DHT11_STATE_TOPIC         "XZ-ESP32-01/sensor/dht11/state"
#define MQTT_HA_DHT11_TEMP_CONFIG_TOPIC   "homeassistant/sensor/XZ-ESP32-01/dht11_temp/config"
#define MQTT_HA_DHT11_HUMI_CONFIG_TOPIC   "homeassistant/sensor/XZ-ESP32-01/dht11_humi/config"

// MQ-2 烟雾传感器 MQTT 主题
#define MQTT_HA_MQ2_STATE_TOPIC           "XZ-ESP32-01/sensor/mq2/state"
#define MQTT_HA_MQ2_CONFIG_TOPIC          "homeassistant/sensor/XZ-ESP32-01/mq2_smoke/config"
#define MQTT_HA_MQ2_ALERT_CONFIG_TOPIC    "homeassistant/binary_sensor/XZ-ESP32-01/mq2_alert/config"

// MQTT 配置 - 连接到MQTT服务器的设置
#define MQTT_URI       "mqtt://106.53.179.231:1883"  // MQTT服务器地址和端口
#define CLIENT_ID      "ESP32-xiaozhi-V2"            // 设备唯一标识符
#define MQTT_USERNAME  "admin"                       // MQTT服务器用户名
#define MQTT_PASSWORD  "azsxdcfv"                    // MQTT服务器密码
#define MQTT_COMMAND_TOPIC    "HA-XZ-01/01/state"    // 统一命令主题

// Home Assistant MQTT 自动发现主题
#define MQTT_HA_FAN_CONFIG_TOPIC            "homeassistant/fan/XZ-ESP32-01/fan/config"  // 风扇配置主题
#define MQTT_HA_FAN_STATE_TOPIC             "XZ-ESP32-01/fan/state"                     // 风扇状态主题
#define MQTT_HA_FAN_COMMAND_TOPIC           "XZ-ESP32-01/fan/set"                       // 风扇开关命令主题（接收）
#define MQTT_HA_FAN_PERCENTAGE_COMMAND_TOPIC "XZ-ESP32-01/fan/percentage/set"           // 风扇速度命令主题（接收）
#define MQTT_HA_FAN_ATTRIBUTES_TOPIC        "XZ-ESP32-01/fan/attributes"                // 风扇属性主题

// 灯光 MQTT 主题
#define MQTT_HA_LAMP_CONFIG_TOPIC           "homeassistant/light/XZ-ESP32-01/lamp/config"  // 灯光配置主题
#define MQTT_HA_LAMP_STATE_TOPIC            "XZ-ESP32-01/lamp/state"                       // 灯光状态主题
#define MQTT_HA_LAMP_COMMAND_TOPIC          "XZ-ESP32-01/lamp/set"                         // 灯光命令主题（接收）

// 窗帘 MQTT 主题
#define MQTT_HA_CURTAIN_CONFIG_TOPIC        "homeassistant/cover/XZ-ESP32-01/curtain/config"  // 窗帘配置主题
#define MQTT_HA_CURTAIN_STATE_TOPIC         "XZ-ESP32-01/curtain/state"                        // 窗帘状态主题
#define MQTT_HA_CURTAIN_COMMAND_TOPIC       "XZ-ESP32-01/curtain/set"                          // 窗帘命令主题（接收）

// 设备信息
#define DEVICE_ID          "XZ-ESP32-01"       // 设备唯一标识符
#define DEVICE_NAME        "小智 ESP32"        // 设备显示名称
#define DEVICE_SW_VERSION  "2.0.3"             // 软件版本

#endif // _BOARD_CONFIG_H_
