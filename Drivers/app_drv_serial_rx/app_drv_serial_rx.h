#ifndef APP_DRV_SERIAL_RX_H_
#define APP_DRV_SERIAL_RX_H_

#include <stdint.h>
#include "main.h"

// 用户可配置的 DMA 缓冲区大小（建议根据实际数据包大小调整，建议至少 64 字节）
#ifndef USART_DMA_BUFFER_SIZE
  #define USART_DMA_BUFFER_SIZE  (64)
#endif

// 用户自定义队列操作函数类型定义（批量操作）
typedef uint32_t (*USART_Queue_Write_Func)(void* user_queue, uint8_t* data, uint16_t length);  // 批量写入队列，返回实际写入长度
typedef uint32_t (*USART_Queue_Available_Func)(void* user_queue);                       // 检查队列可用空间

// USART DMA 上下文结构体
typedef struct {
    UART_HandleTypeDef* huart;
    DMA_HandleTypeDef* hdma;
    uint8_t dma_buffer[USART_DMA_BUFFER_SIZE];
    uint32_t last_count;
    void* user_queue;  // 用户队列指针

    // 用户自定义队列操作函数
    USART_Queue_Write_Func queue_write;      // 批量写入队列
    USART_Queue_Available_Func queue_available; // 检查队列可用空间

    // 错误统计
    uint32_t total_received_bytes;    // 总接收字节数
    uint32_t total_dropped_bytes;     // 因队列满丢弃的字节数
    uint32_t queue_overflow_count;    // 队列溢出次数
} USART_DMA_Context;

// 初始化和控制函数
void USART_Rx_DMA_Init(USART_DMA_Context* ctx, UART_HandleTypeDef* huart, DMA_HandleTypeDef* hdma);
void USART_Rx_DMA_IRQHandler_Process(USART_DMA_Context* ctx);

// 设置用户自定义队列操作函数和队列指针
void USART_RegisterQueueOps(USART_DMA_Context* ctx,
                           void* user_queue,
                           USART_Queue_Write_Func write_func,
                           USART_Queue_Available_Func available_func);

// 获取接收统计信息
void USART_GetStatistics(USART_DMA_Context* ctx,
                        uint32_t* total_received,
                        uint32_t* total_dropped,
                        uint32_t* overflow_count);

// 重置统计信息
void USART_ResetStatistics(USART_DMA_Context* ctx);

#endif /* APP_DRV_SERIAL_RX_H_ */
