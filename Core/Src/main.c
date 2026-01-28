/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "app_drv_serial_rx.h"
#include "app_drv_fifo.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
extern DMA_HandleTypeDef hdma_usart1_rx;
USART_DMA_Context USART1_DMA_Context;
extern DMA_HandleTypeDef hdma_usart2_rx;
USART_DMA_Context USART2_DMA_Context;

// DMA发送状态标志
volatile uint8_t usart1_tx_busy = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// 定义 FIFO 缓冲区大小
#define RX_FIFO_SIZE 256

// 用户自定义的 USART1 和 USART2 的 FIFO 缓冲区
static uint8_t usart1_rx_fifo_buffer[RX_FIFO_SIZE];

// FIFO 实例
static app_drv_fifo_t usart1_rx_fifo;

// 通用的批量队列写入函数（所有串口共用）
uint32_t USART_Queue_Write(void* user_queue, uint8_t* data, uint16_t length)
{
    uint16_t written = length;
    app_drv_fifo_result_t result = app_drv_fifo_write((app_drv_fifo_t*)user_queue, data, &written);
    if (result == APP_DRV_FIFO_RESULT_SUCCESS) {
        return written;
    }
    return 0;
}

// 通用的队列可用空间查询函数（所有串口共用）
uint32_t USART_Queue_Available(void* user_queue)
{
    return (uint32_t)(RX_FIFO_SIZE - app_drv_fifo_length((app_drv_fifo_t*)user_queue));
}

// Printf redirect
int __io_putchar(int ch)
{
  // 等待上一次DMA发送完成
  while (usart1_tx_busy != 0) {
    __NOP();
  }
  
  usart1_tx_busy = 1;
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
  usart1_tx_busy = 0;
  return ch;
}

int _write(int file, char *ptr, int len)
{
  int i;
  for (i = 0; i < len; i++)
  {
    __io_putchar(*ptr++);
  }
  return len;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  
  // 初始化用户自定义的 FIFO 队列
  app_drv_fifo_init(&usart1_rx_fifo, usart1_rx_fifo_buffer, RX_FIFO_SIZE);
  
  // 初始化 USART DMA IDLE 接收
  USART_Rx_DMA_Init(&USART1_DMA_Context, &huart1, &hdma_usart1_rx);
  
  // 设置用户队列指针
  
  // 注册用户自定义队列指针和操作函数
  USART_RegisterQueueOps(&USART1_DMA_Context, &usart1_rx_fifo, USART_Queue_Write, USART_Queue_Available);
  
  printf("USART DMA IDLE Reception initialized\r\n");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

/* USER CODE BEGIN 3 */
    // 检查 USART1 FIFO 中是否有数据
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
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 16;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV4;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

// UART发送完成回调函数
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1) {
    usart1_tx_busy = 0;
  }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
