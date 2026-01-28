/********************************** (C) COPYRIGHT *******************************
 * Copyright (c) 2026 createskyblue@outlook.com MIT
*******************************************************************************/
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

    // 初始化统计信息
    ctx->total_received_bytes = 0;
    ctx->total_dropped_bytes = 0;
    ctx->queue_overflow_count = 0;
    
    // 配置 USART IDLE 中断
    __HAL_UART_CLEAR_IDLEFLAG(ctx->huart);
    __HAL_UART_ENABLE_IT(ctx->huart, UART_IT_IDLE);
    __HAL_DMA_ENABLE_IT(ctx->hdma, DMA_IT_TC | DMA_IT_HT);
    
    // 启动 UART DMA 循环接收
    HAL_UART_Receive_DMA(ctx->huart, ctx->dma_buffer, USART_DMA_BUFFER_SIZE);
}

/**
 * @brief 注册用户自定义的队列操作函数和队列指针
 * @param ctx 指向 USART_DMA_Context 结构体的指针
 * @param user_queue 用户定义的队列指针
 * @param write_func 用户定义的批量写入函数
 * @param available_func 用户定义的可用空间查询函数
 * @note 允许用户自定义自己的队列实现
 */
void USART_RegisterQueueOps(USART_DMA_Context* ctx,
                           void* user_queue,
                           USART_Queue_Write_Func write_func,
                           USART_Queue_Available_Func available_func)
{
    ctx->user_queue = user_queue;
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
        // 没有新数据，只清除 IDLE 标志
        if (RESET != __HAL_UART_GET_FLAG(ctx->huart, UART_FLAG_IDLE)) {
            __HAL_UART_CLEAR_IDLEFLAG(ctx->huart);
        }
        return;
    }

    // 计算需要处理的数据总长度
    uint16_t total_data_len;
    if (thisCount > ctx->last_count) {
        // 线性情况：数据在一个连续块中
        total_data_len = thisCount - ctx->last_count;
    } else {
        // 循环情况：数据环绕缓冲区末尾
        total_data_len = (USART_DMA_BUFFER_SIZE - ctx->last_count) + thisCount;
    }

    // 统计总接收字节数
    ctx->total_received_bytes += total_data_len;

    // 如果没有注册队列操作函数，只更新 last_count
    if (ctx->queue_write == NULL || ctx->queue_available == NULL) {
        ctx->last_count = thisCount;
        if (RESET != __HAL_UART_GET_FLAG(ctx->huart, UART_FLAG_IDLE)) {
            __HAL_UART_CLEAR_IDLEFLAG(ctx->huart);
        }
        return;
    }

    // 尝试写入所有数据
    uint16_t bytes_written = 0;
    uint16_t remaining = total_data_len;

    if (thisCount > ctx->last_count) {
        // 线性情况：数据在一个连续块中
        uint8_t* data_ptr = &ctx->dma_buffer[ctx->last_count];
        uint32_t available = ctx->queue_available(ctx->user_queue);
        uint16_t write_len = (available >= remaining) ? remaining : available;

        if (write_len > 0) {
            write_len = ctx->queue_write(ctx->user_queue, data_ptr, write_len);
            bytes_written += write_len;
        }

        ctx->last_count += bytes_written;
    } else {
        // 循环情况：数据环绕缓冲区末尾
        // 第一部分：从 last_count 到缓冲区末尾
        uint16_t first_part_len = USART_DMA_BUFFER_SIZE - ctx->last_count;
        uint8_t* data_ptr = &ctx->dma_buffer[ctx->last_count];
        uint32_t available = ctx->queue_available(ctx->user_queue);
        uint16_t write_len = (available >= first_part_len) ? first_part_len : available;

        if (write_len > 0) {
            write_len = ctx->queue_write(ctx->user_queue, data_ptr, write_len);
            bytes_written += write_len;
            ctx->last_count = (ctx->last_count + write_len) % USART_DMA_BUFFER_SIZE;
        }

        // 第二部分：从缓冲区开头到 thisCount
        if (bytes_written == first_part_len && remaining > first_part_len) {
            uint16_t second_part_len = thisCount;
            data_ptr = &ctx->dma_buffer[0];
            available = ctx->queue_available(ctx->user_queue);
            write_len = (available >= second_part_len) ? second_part_len : available;

            if (write_len > 0) {
                write_len = ctx->queue_write(ctx->user_queue, data_ptr, write_len);
                bytes_written += write_len;
                ctx->last_count = write_len;
            } else if (write_len == 0 && second_part_len > 0) {
                // 第二部分无法写入，last_count 应指向 0
                ctx->last_count = 0;
            }
        }
    }

    // 更新统计信息
    if (bytes_written < total_data_len) {
        ctx->total_dropped_bytes += (total_data_len - bytes_written);
        ctx->queue_overflow_count++;
    }

    // 清除 IDLE 标志
    if (RESET != __HAL_UART_GET_FLAG(ctx->huart, UART_FLAG_IDLE)) {
        __HAL_UART_CLEAR_IDLEFLAG(ctx->huart);
    }
}

/**
 * @brief 获取接收统计信息
 * @param ctx 指向 USART_DMA_Context 结构体的指针
 * @param total_received 总接收字节数输出指针（可为 NULL）
 * @param total_dropped 总丢弃字节数输出指针（可为 NULL）
 * @param overflow_count 队列溢出次数输出指针（可为 NULL）
 */
void USART_GetStatistics(USART_DMA_Context* ctx,
                        uint32_t* total_received,
                        uint32_t* total_dropped,
                        uint32_t* overflow_count)
{
    if (total_received != NULL) {
        *total_received = ctx->total_received_bytes;
    }
    if (total_dropped != NULL) {
        *total_dropped = ctx->total_dropped_bytes;
    }
    if (overflow_count != NULL) {
        *overflow_count = ctx->queue_overflow_count;
    }
}

/**
 * @brief 重置统计信息
 * @param ctx 指向 USART_DMA_Context 结构体的指针
 */
void USART_ResetStatistics(USART_DMA_Context* ctx)
{
    ctx->total_received_bytes = 0;
    ctx->total_dropped_bytes = 0;
    ctx->queue_overflow_count = 0;
}
