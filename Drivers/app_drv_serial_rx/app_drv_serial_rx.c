/**
 ******************************************************************************
 * @file    usart_dma_idle.c
 * @brief   USART DMA IDLE 线中断驱动
 * @note    使用 DMA 循环模式和 IDLE 线中断实现高效的变长数据接收
 ******************************************************************************
 */

#include "app_drv_serial_rx.h"
#include "usart.h"

/**
 * @brief 初始化 USART DMA 接收
 * @param ctx 指向 USART_DMA_Context 结构体的指针
 * @param huart 指向 UART_HandleTypeDef 的指针
 * @param hdma 指向 DMA_HandleTypeDef 的指针
 * @note 配置 DMA 循环模式并使能 IDLE 线中断
 */
void USART_Rx_DMA_Init(USART_DMA_Context* ctx, UART_HandleTypeDef* huart, DMA_HandleTypeDef* hdma)
{
    // 初始化上下文
    ctx->huart = huart;
    ctx->hdma = hdma;
    ctx->last_count = 0;
    ctx->queue_write = NULL;
    ctx->queue_available = NULL;
    
    // 配置 USART IDLE 中断
    __HAL_UART_CLEAR_IDLEFLAG(ctx->huart);
    __HAL_UART_ENABLE_IT(ctx->huart, UART_IT_IDLE);
    __HAL_DMA_ENABLE_IT(ctx->hdma, DMA_IT_TC | DMA_IT_HT);
    
    // 启动 UART DMA 循环接收
    HAL_UART_Receive_DMA(ctx->huart, ctx->dma_buffer, USART_DMA_BUFFER_SIZE);
}

/**
 * @brief 注册用户自定义的队列操作函数
 * @param ctx 指向 USART_DMA_Context 结构体的指针
 * @param write_func 用户定义的批量写入函数
 * @param available_func 用户定义的可用空间查询函数
 * @note 允许用户自定义自己的队列实现
 */
void USART_RegisterQueueOps(USART_DMA_Context* ctx, 
                           USART_Queue_Write_Func write_func, 
                           USART_Queue_Available_Func available_func)
{
    ctx->queue_write = write_func;
    ctx->queue_available = available_func;
}

/**
 * @brief 处理 USART DMA 中断
 * @param ctx 指向 USART_DMA_Context 结构体的指针
 * @note 在 DMA 传输完成/中断回调中调用，用于处理接收到的数据
 */
void USART_Rx_DMA_IRQHandler_Process(USART_DMA_Context* ctx)
{
    // 获取当前缓冲区索引并计算接收到的数据长度
    uint32_t thisCount = USART_DMA_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(ctx->hdma);
    
    if (ctx->last_count == thisCount) {
        // 没有新数据
        if (RESET != __HAL_UART_GET_FLAG(ctx->huart, UART_FLAG_IDLE)) {
            __HAL_UART_CLEAR_IDLEFLAG(ctx->huart);
        }
        return;
    }
    
    // 考虑循环缓冲区计算数据长度
    uint16_t data_len;
    uint8_t* data_ptr;
    
    if (thisCount > ctx->last_count) {
        // 线性情况：数据在一个连续块中
        data_len = thisCount - ctx->last_count;
        data_ptr = &ctx->dma_buffer[ctx->last_count];
        
        // 批量写入用户队列
        if (ctx->queue_write && ctx->queue_available) {
            uint32_t available = ctx->queue_available(ctx->user_queue);
            uint16_t write_len = (available >= data_len) ? data_len : available;
            if (write_len > 0) {
                ctx->queue_write(ctx->user_queue, data_ptr, write_len);
                ctx->last_count = thisCount;
            }
        }
    } else {
        // 循环情况：数据环绕缓冲区末尾
        // 第一部分：从 last_count 到缓冲区末尾
        data_len = USART_DMA_BUFFER_SIZE - ctx->last_count;
        data_ptr = &ctx->dma_buffer[ctx->last_count];
        
        if (ctx->queue_write && ctx->queue_available) {
            uint32_t available = ctx->queue_available(ctx->user_queue);
            uint16_t write_len = (available >= data_len) ? data_len : available;
            if (write_len > 0) {
                ctx->queue_write(ctx->user_queue, data_ptr, write_len);
                ctx->last_count = (ctx->last_count + write_len) % USART_DMA_BUFFER_SIZE;
                
                // 第二部分：从缓冲区开头到 thisCount
                if (ctx->last_count < thisCount) {
                    data_len = thisCount - ctx->last_count;
                    data_ptr = &ctx->dma_buffer[ctx->last_count];
                    available = ctx->queue_available(ctx->user_queue);
                    write_len = (available >= data_len) ? data_len : available;
                    if (write_len > 0) {
                        ctx->queue_write(ctx->user_queue, data_ptr, write_len);
                        ctx->last_count = thisCount;
                    }
                }
            }
        }
    }
    
    if (RESET != __HAL_UART_GET_FLAG(ctx->huart, UART_FLAG_IDLE)) {
        __HAL_UART_CLEAR_IDLEFLAG(ctx->huart);
    }
}
