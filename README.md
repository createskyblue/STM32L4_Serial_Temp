# STM32L496 串口接收示例

## 项目简介

STM32L496 高效串口数据接收方案，使用 DMA 循环模式配合空闲中断实现变长数据接收。

### 核心特性

- **DMA 循环模式**: 自动接收数据，无需 CPU 干预
- **空闲中断**: 检测一帧数据结束，实现可变长数据接收
- **DMA 半传输完成/传输完成中断**: 及时处理 DMA 缓冲区数据，避免数据被覆盖
- **批量操作**: 中断中批量写入数据到用户队列，主循环中批量读取处理
- **完全解耦**: 支持用户自定义队列实现

---

## 技术要点

### 接收机制

1. **DMA 循环缓冲区**: 256 字节 DMA 缓冲区，自动循环接收数据
2. **空闲中断**: 串口空闲时触发，表示一帧数据接收完成
3. **DMA 半传输完成中断**: DMA 传输到一半时触发（128 字节），及时处理已接收数据
4. **DMA 传输完成中断**: DMA 传输完成时触发（256 字节），防止 DMA 循环覆盖未处理数据

### 批量操作

- **批量写入**: 中断中一次回调写入多个字节到用户队列
- **批量读取**: 主循环中一次读取多个字节进行处理
- **性能提升**: 减少函数调用次数，提升数据传输效率

### 解耦设计

```
串口驱动 ← 回调函数 → 用户队列
```

**优势**：
- 驱动不依赖特定队列类型
- 用户可自由选择队列实现（FIFO、消息队列等）
- 所有串口共用同一套回调函数，减少代码重复

---

## 快速开始

### 1. 初始化队列

```c
#include "app_drv_serial_rx.h"
#include "app_drv_fifo.h"

#define RX_FIFO_SIZE 256

static uint8_t usart1_rx_fifo_buffer[RX_FIFO_SIZE];
static app_drv_fifo_t usart1_rx_fifo;

app_drv_fifo_init(&usart1_rx_fifo, usart1_rx_fifo_buffer, RX_FIFO_SIZE);
```

### 2. 实现回调函数

```c
// 批量写入（所有串口共用）
uint32_t USART_Queue_Write(void* user_queue, uint8_t* data, uint16_t length)
{
    uint16_t written = length;
    app_drv_fifo_write((app_drv_fifo_t*)user_queue, data, &written);
    return written;
}

// 查询可用空间（所有串口共用）
uint32_t USART_Queue_Available(void* user_queue)
{
    return (uint32_t)(RX_FIFO_SIZE - app_drv_fifo_length((app_drv_fifo_t*)user_queue));
}
```

### 3. 初始化串口

```c
USART_Rx_DMA_Init(&USART1_DMA_Context, &huart1, &hdma_usart1_rx);

// 设置用户队列指针
USART1_DMA_Context.user_queue = &usart1_rx_fifo;

// 注册回调函数
USART_RegisterQueueOps(&USART1_DMA_Context, USART_Queue_Write, USART_Queue_Available);
```

### 4. 处理中断

在 `Core/Src/stm32l4xx_it.c` 中：

```c
void USART1_IRQHandler(void)
{
    USART_Rx_DMA_IRQHandler_Process(&USART1_DMA_Context);
    HAL_UART_IRQHandler(&huart1);
}

void DMA1_Channel5_IRQHandler(void)
{
    USART_Rx_DMA_IRQHandler_Process(&USART1_DMA_Context);
    HAL_DMA_IRQHandler(&hdma_usart1_rx);
}
```

### 5. 读取数据

```c
uint16_t usart1_len = app_drv_fifo_length(&usart1_rx_fifo);
if (usart1_len > 0 && usart1_tx_busy == 0) {
    static uint8_t temp_buf[128];
    uint16_t read_len = (usart1_len > sizeof(temp_buf)) ? sizeof(temp_buf) : usart1_len;
    uint16_t actual_read = read_len;
    app_drv_fifo_result_t result = app_drv_fifo_read(&usart1_rx_fifo, temp_buf, &actual_read);
    if (result == APP_DRV_FIFO_RESULT_SUCCESS && actual_read > 0) {
        // 将接收到的数据回显到 USART1
        usart1_tx_busy = 1;
        HAL_UART_Transmit_DMA(&huart1, temp_buf, actual_read);
    }
}
```

---

## 关键文件说明

### 必须配置的文件

| 文件 | 作用 |
|------|------|
| `Core/Src/main.c` | 队列初始化、回调函数实现、主循环数据处理 |
| `Core/Src/stm32l4xx_it.c` | 中断服务函数，调用 `USART_Rx_DMA_IRQHandler_Process` |

### 驱动文件（无需修改）

| 文件 | 说明 |
|------|------|
| `Drivers/app_drv_serial_rx/app_drv_serial_rx.h` | 驱动头文件，定义接口 |
| `Drivers/app_drv_serial_rx/app_drv_serial_rx.c` | 驱动实现，已针对 STM32L4 优化 |

---

## 平台移植

如果是其他 STM32 系列或 ARM 单片机，需要修改：

### 1. HAL 库适配

修改 `Drivers/app_drv_serial_rx/app_drv_serial_rx.c` 中的 HAL 宏：

```c
// STM32L4 使用这些宏
__HAL_UART_CLEAR_IDLEFLAG(ctx->huart);
__HAL_UART_ENABLE_IT(ctx->huart, UART_IT_IDLE);
__HAL_DMA_ENABLE_IT(ctx->hdma, DMA_IT_TC | DMA_IT_HT);
HAL_UART_Receive_DMA(ctx->huart, ctx->dma_buffer, USART_DMA_BUFFER_SIZE);
```

### 2. 中断函数适配

修改对应平台的 `stm32xxxx_it.c` 文件，确保正确调用：
- USART 中断处理函数
- DMA 中断处理函数

---

## 详细使用说明

### 完整示例代码

查看 `Core/Src/main.c` 获取：
- 队列初始化代码
- 回调函数实现
- 主循环数据处理
- 多串口支持示例

### 中断配置

查看 `Core/Src/stm32l4xx_it.c` 获取：
- USART 中断处理
- DMA 中断处理
- 空闲中断清除

---

## 构建与烧录

```bash
# 构建
cmake --build build/Debug --target all

# 烧录
STM32CubeProgrammer -c port=SWD -w STM32L496_DEMO.hex
```

---

## 项目文件

```
Drivers/app_drv_serial_rx/
├── app_drv_serial_rx.h    # 驱动接口
└── app_drv_serial_rx.c    # 驱动实现（STM32L4 适配）
```

---

**完整示例代码请参考项目源码文件。**
