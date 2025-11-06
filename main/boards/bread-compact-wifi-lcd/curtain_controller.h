#ifndef __CURTAIN_CONTROLLER_H__
#define __CURTAIN_CONTROLLER_H__

#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "mcp_server.h"
#include "config.h"
#include "esp_log.h"

/**
 * @brief 窗帘控制器（基于 28BYJ-48 步进电机 + ULN2003 驱动）
 * 
 * 简化版本，专用于窗帘开关控制：
 * - 开窗帘：逆时针旋转 360°
 * - 关窗帘：顺时针旋转 360°
 * - 非阻塞执行，使用 FreeRTOS 后台任务
 */
class CurtainController {
private:
    static constexpr const char* TAG = "Curtain";
    
    // GPIO 引脚
    gpio_num_t in1_gpio_;
    gpio_num_t in2_gpio_;
    gpio_num_t in3_gpio_;
    gpio_num_t in4_gpio_;
    
    // 四相八拍步进序列
    static constexpr uint8_t STEP_SEQUENCE[8][4] = {
        {1, 0, 0, 0},  // A相
        {1, 1, 0, 0},  // AB相
        {0, 1, 0, 0},  // B相
        {0, 1, 1, 0},  // BC相
        {0, 0, 1, 0},  // C相
        {0, 0, 1, 1},  // CD相
        {0, 0, 0, 1},  // D相
        {1, 0, 0, 1}   // DA相
    };
    
    // 命令类型
    enum CurtainCommand {
        CMD_OPEN,   // 开窗帘（逆时针360°）
        CMD_CLOSE,  // 关窗帘（顺时针360°）
        CMD_STOP    // 停止
    };
    
    // 命令结构体（简化版，无参数）
    struct Command {
        CurtainCommand type;
    };
    
    // FreeRTOS 资源
    QueueHandle_t command_queue_;
    TaskHandle_t curtain_task_;
    
    // 状态变量
    uint8_t current_step_;    // 当前步进序列位置 (0-7)
    bool is_running_;         // 是否正在运行
    enum CurtainState {
        CURTAIN_CLOSED,
        CURTAIN_OPEN,
        CURTAIN_UNKNOWN
    };
    CurtainState current_state_;
    
    /**
     * @brief 初始化 GPIO 引脚。
     */
    void InitializeGpio() {
        gpio_config_t config = {
            .pin_bit_mask = (1ULL << in1_gpio_) | (1ULL << in2_gpio_) | 
                           (1ULL << in3_gpio_) | (1ULL << in4_gpio_),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&config));
        
        // 初始状态：所有相断开
        PowerOff();
    }
    
    /**
     * @brief 执行单步步进。
     * 
     * @param clockwise true=顺时针（关窗帘），false=逆时针（开窗帘）
     */
    void ExecuteStep(bool clockwise) {
        // 更新步进序列索引
        if (clockwise) {
            current_step_ = (current_step_ + 1) % 8;
        } else {
            current_step_ = (current_step_ == 0) ? 7 : (current_step_ - 1);
        }
        
        // 设置 GPIO 电平
        gpio_set_level(in1_gpio_, STEP_SEQUENCE[current_step_][0]);
        gpio_set_level(in2_gpio_, STEP_SEQUENCE[current_step_][1]);
        gpio_set_level(in3_gpio_, STEP_SEQUENCE[current_step_][2]);
        gpio_set_level(in4_gpio_, STEP_SEQUENCE[current_step_][3]);
    }
    
    /**
     * @brief 切断所有相电流，停止电机（减少发热）。
     */
    void PowerOff() {
        gpio_set_level(in1_gpio_, 0);
        gpio_set_level(in2_gpio_, 0);
        gpio_set_level(in3_gpio_, 0);
        gpio_set_level(in4_gpio_, 0);
    }
    
    /**
     * @brief FreeRTOS 后台任务，循环处理窗帘控制命令。
     */
    static void CurtainTaskFunction(void* parameter) {
        CurtainController* controller = static_cast<CurtainController*>(parameter);
        Command cmd;
        
        while (true) {
            // 等待命令
            if (xQueueReceive(controller->command_queue_, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
                
                if (cmd.type == CMD_STOP) {
                    ESP_LOGI(TAG, "Stop command received");
                    xQueueReset(controller->command_queue_);  // 清空队列
                    controller->PowerOff();
                    controller->is_running_ = false;
                    controller->current_state_ = CURTAIN_UNKNOWN;
                    continue;
                }
                
                controller->is_running_ = true;
                
                // 确定方向（打开=逆时针，关闭=顺时针）
                bool clockwise = (cmd.type == CMD_CLOSE);
                const char* action = (cmd.type == CMD_OPEN) ? "Opening" : "Closing";
                
                ESP_LOGI(TAG, "%s curtain (360° rotation)...", action);
                
                // 执行一圈旋转（4096步 = 360°）
                const uint32_t STEPS_PER_TURN = CURTAIN_STEPS_PER_TURN;
                const uint16_t SPEED_MS = CURTAIN_SPEED_MS;
                
                bool aborted = false;
                for (uint32_t i = 0; i < STEPS_PER_TURN; i++) {
                    controller->ExecuteStep(clockwise);
                    vTaskDelay(pdMS_TO_TICKS(SPEED_MS));
                    
                    // 每隔一段步数检查是否有停止命令
                    if (i % 100 == 0) {
                        Command stop_check;
                        if (xQueuePeek(controller->command_queue_, &stop_check, 0) == pdTRUE) {
                            if (stop_check.type == CMD_STOP) {
                                ESP_LOGI(TAG, "Stop detected, aborting");
                                // 移除队列中的停止命令
                                Command discarded;
                                xQueueReceive(controller->command_queue_, &discarded, 0);
                                aborted = true;
                                break;
                            }
                        }
                    }
                }
                
                // 完成后断电
                controller->PowerOff();
                controller->is_running_ = false;
                controller->current_state_ = aborted
                    ? CURTAIN_UNKNOWN
                    : (cmd.type == CMD_OPEN ? CURTAIN_OPEN : CURTAIN_CLOSED);
                ESP_LOGI(TAG, "Curtain operation %s", aborted ? "aborted" : "completed");
            }
        }
    }
    
    /**
     * @brief 创建 FreeRTOS 后台任务。
     */
    void CreateCurtainTask() {
        BaseType_t result = xTaskCreate(
            CurtainTaskFunction,
            "curtain_task",
            CURTAIN_TASK_STACK_SIZE,
            this,
            CURTAIN_TASK_PRIORITY,
            &curtain_task_
        );
        
        if (result != pdPASS) {
            ESP_LOGE(TAG, "Failed to create curtain task");
        } else {
            ESP_LOGI(TAG, "Curtain task created successfully");
        }
    }
    
    /**
     * @brief 发送命令到队列。
     * 
     * @param cmd 命令
     * @return true 成功，false 队列满
     */
    bool SendCommand(const Command& cmd) {
        if (xQueueSend(command_queue_, &cmd, 0) != pdTRUE) {
            ESP_LOGW(TAG, "Command queue full");
            return false;
        }
        return true;
    }
    
    /**
     * @brief 注册 MCP 工具。
     */
    void RegisterTools() {
        auto& mcp_server = McpServer::GetInstance();
        
        // 1. 开窗帘
        mcp_server.AddTool(
            "self.curtain.open",
            "打开窗帘（步进电机逆时针旋转360°）",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                if (is_running_) {
                    ESP_LOGW(TAG, "Curtain is already running");
                    return "curtain_is_running";
                }
                if (current_state_ == CURTAIN_OPEN) {
                    ESP_LOGI(TAG, "Curtain already open");
                    return "already_open";
                }
                Command cmd = {CMD_OPEN};
                bool success = SendCommand(cmd);
                ESP_LOGI(TAG, "Open curtain command -> %s", success ? "OK" : "FAIL");
                return success;
            }
        );
        
        // 2. 关窗帘
        mcp_server.AddTool(
            "self.curtain.close",
            "关闭窗帘（步进电机顺时针旋转360°）",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                if (is_running_) {
                    ESP_LOGW(TAG, "Curtain is already running");
                    return "curtain_is_running";
                }
                if (current_state_ == CURTAIN_CLOSED) {
                    ESP_LOGI(TAG, "Curtain already closed");
                    return "already_closed";
                }
                Command cmd = {CMD_CLOSE};
                bool success = SendCommand(cmd);
                ESP_LOGI(TAG, "Close curtain command -> %s", success ? "OK" : "FAIL");
                return success;
            }
        );
        
        // 3. 停止
        mcp_server.AddTool(
            "self.curtain.stop",
            "停止窗帘运动并切断电机电流",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                Command cmd = {CMD_STOP};
                bool success = SendCommand(cmd);
                ESP_LOGI(TAG, "Stop command -> %s", success ? "OK" : "FAIL");
                return success;
            }
        );
        
        // 4. 查询状态
        mcp_server.AddTool(
            "self.curtain.get_status",
            "查询窗帘控制器状态",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                const char* state_str = "unknown";
                switch (current_state_) {
                    case CURTAIN_OPEN: state_str = "open"; break;
                    case CURTAIN_CLOSED: state_str = "closed"; break;
                    case CURTAIN_UNKNOWN: default: break;
                }

                char status[96];
                snprintf(status, sizeof(status), 
                    "{\"is_running\":%s,\"state\":\"%s\",\"current_step\":%u}",
                    is_running_ ? "true" : "false",
                    state_str,
                    current_step_
                );
                ESP_LOGI(TAG, "Status: %s", status);
                return std::string(status);
            }
        );
    }

public:
    /**
     * @brief 构造函数，初始化窗帘控制器。
     * 
     * @param in1 ULN2003 的 IN1 引脚
     * @param in2 ULN2003 的 IN2 引脚
     * @param in3 ULN2003 的 IN3 引脚
     * @param in4 ULN2003 的 IN4 引脚
     */
    CurtainController(gpio_num_t in1, gpio_num_t in2, gpio_num_t in3, gpio_num_t in4)
        : in1_gpio_(in1), in2_gpio_(in2), in3_gpio_(in3), in4_gpio_(in4),
          current_step_(0), is_running_(false), current_state_(CURTAIN_OPEN) {
        
        // 初始化硬件
        InitializeGpio();
        
        // 创建命令队列（只需要2个槽位：当前命令 + 1个排队命令）
        command_queue_ = xQueueCreate(2, sizeof(Command));
        if (command_queue_ == nullptr) {
            ESP_LOGE(TAG, "Failed to create command queue");
            return;
        }
        
        // 创建后台任务
        CreateCurtainTask();
        
        // 注册 MCP 工具
        RegisterTools();
        
        ESP_LOGI(TAG, "Initialized on GPIO IN1=%d, IN2=%d, IN3=%d, IN4=%d",
                in1, in2, in3, in4);
    }
    
    /**
     * @brief 析构函数，清理资源。
     */
    ~CurtainController() {
        // 删除任务
        if (curtain_task_ != nullptr) {
            vTaskDelete(curtain_task_);
        }
        
        // 删除队列
        if (command_queue_ != nullptr) {
            vQueueDelete(command_queue_);
        }
        
        // 关闭电机
        PowerOff();
        
        ESP_LOGI(TAG, "Controller destroyed");
    }
};

#endif // __CURTAIN_CONTROLLER_H__

