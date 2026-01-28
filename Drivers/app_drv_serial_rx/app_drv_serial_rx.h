#ifndef USART_DMA_IDLE_H_
#define USART_DMA_IDLE_H_

#include <stdint.h>
#include "main.h"

#define USART_DMA_BUFFER_SIZE  (32)

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
} USART_DMA_Context;

// 初始化和控制函数
void USART_Rx_DMA_Init(USART_DMA_Context* ctx, UART_HandleTypeDef* huart, DMA_HandleTypeDef* hdma);
void USART_Rx_DMA_IRQHandler_Process(USART_DMA_Context* ctx);

// 设置用户自定义队列操作函数和队列指针
void USART_RegisterQueueOps(USART_DMA_Context* ctx,
                           void* user_queue,
                           USART_Queue_Write_Func write_func,
                           USART_Queue_Available_Func available_func);

#endif /* USART_DMA_IDLE_H_ */
