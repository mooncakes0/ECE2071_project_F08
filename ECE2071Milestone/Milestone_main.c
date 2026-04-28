/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
   #define BUFFER_SIZE 150
   //#define MY_ID "F08_0"
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim16;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
char MY_ID[] = "F08_0";
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM16_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
   char rx_buffer[BUFFER_SIZE]; //message received from PC
   char tx_buffer[BUFFER_SIZE];  //outgoing message after appending MY_ID
   char ring_buffer[BUFFER_SIZE]; //message received from the ring via huart1
   char payload_buffer[BUFFER_SIZE];
   char *last_arrow;
   unsigned int received_checksum;
   uint8_t calculated_checksum=0;
   uint8_t rx_char;
   int rx_index;
   int ishead=0;
   int flag = 1;

   uint8_t calculate_checksum(const char* payload, int length)
     {
   	  uint8_t checksum = (uint8_t)payload[0];
         for (int i = 1; i < length; i++)
         {
             checksum ^= (uint8_t)payload[i];
         }
         return checksum;
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
  MX_USART2_UART_Init();
  MX_TIM16_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  while(flag)
	  {
		  if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE) == SET)
		  {
			  ishead=1;
			  flag=0;
			  break;
		  }
		  else if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE) == SET)
		  {
			  ishead=0;
			  flag=0;
			  break;
		  }
	  }
	  if(ishead == 1)
	  {
		  rx_index = 0;
		  memset(rx_buffer, 0, sizeof(rx_buffer)); // reset the buffer
		  while (1)
		  {
			  HAL_UART_Receive(&huart2, &rx_char, 1, HAL_MAX_DELAY);
			  if (rx_char == '\n')
			  {
				  rx_buffer[rx_index] = '\0';
				  break;  // if "Enter" is detected, it's the sign of the end
			  }
			  if (rx_char != '\r')
			  {
				  if (rx_index < BUFFER_SIZE - 1)
				  {
					  rx_buffer[rx_index++] = rx_char;
				  }
			  }
		  }
		   HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, 1);
		   HAL_Delay(250);

		   snprintf(payload_buffer, sizeof(payload_buffer), "%s->%s", rx_buffer, MY_ID);
		   uint8_t my_checksum = calculate_checksum(payload_buffer, strlen(payload_buffer));
		   snprintf(tx_buffer, sizeof(tx_buffer), "%s->%u\n", payload_buffer, my_checksum);

		   HAL_UART_Transmit(&huart1, (uint8_t *)tx_buffer, strlen(tx_buffer), HAL_MAX_DELAY);

		   HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, 0);

		   rx_index = 0;
		   memset(ring_buffer, 0, sizeof(ring_buffer));
		   while (1)
		   {
			   HAL_UART_Receive(&huart1, &rx_char, 1, HAL_MAX_DELAY);
			   if (rx_char == '\n')
			   {
				   ring_buffer[rx_index] = '\0';
				   break;
			   }
			   if (rx_char != '\r' && rx_index < BUFFER_SIZE - 1)
			   {
				   ring_buffer[rx_index++] = rx_char;
			   }
		   }
		   strcpy(payload_buffer, ring_buffer);
		   char *last_sep = strrchr(payload_buffer, '>');
		   /*if (last_sep == NULL || last_sep == payload_buffer || *(last_sep - 1) != '-')
		   {
			   HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, 1);
			   while (1) {}
		   }*/

		   *(last_sep - 1) = '\0';  // remove the "- before >"

		   sscanf(last_sep + 1, "%u", &received_checksum);

		   calculated_checksum = calculate_checksum(payload_buffer, strlen(payload_buffer));

		   if (calculated_checksum != (uint8_t)received_checksum)
		   {
			   HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, 1);
			   while (1) {}
		   }
		   HAL_UART_Transmit(&huart2, (uint8_t *)ring_buffer, strlen(ring_buffer), HAL_MAX_DELAY);
		   HAL_UART_Transmit(&huart2, (uint8_t *)"\r\n", 2, HAL_MAX_DELAY);
		   flag = 1;
		   ishead=0;
	  }
	  else
	  {
		  rx_index = 0;
		  memset(ring_buffer, 0, sizeof(ring_buffer));
		  while (1)
		  {
			  HAL_UART_Receive(&huart1, &rx_char, 1, HAL_MAX_DELAY);
			  if (rx_char == '\n')
			  {
				  ring_buffer[rx_index] = '\0';
				  break;
			  }
			  if (rx_char != '\r' && rx_index < BUFFER_SIZE - 1)
			  {
				  ring_buffer[rx_index++] = rx_char;
			  }
		  }

		  HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, 1);
		  HAL_Delay(250);

		  strcpy(payload_buffer, ring_buffer);
		  char *last_sep = strrchr(payload_buffer, '>');
		  /*if (last_sep == NULL || last_sep == payload_buffer || *(last_sep - 1) != '-')
		  {
		      HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, 1);
		      while (1) {}
		  }*/
		  *(last_sep - 1) = '\0';
		  sscanf(last_sep + 1, "%u", &received_checksum);
		  calculated_checksum = calculate_checksum(payload_buffer, strlen(payload_buffer));
		  if (calculated_checksum != (uint8_t)received_checksum)
		  {
		      HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, 1);
		      while (1) {}
		  }

		  char temp_payload[BUFFER_SIZE];
		  snprintf(temp_payload, sizeof(temp_payload), "%s->%s", payload_buffer, MY_ID);
		  strcpy(payload_buffer, temp_payload);

		  uint8_t new_checksum = calculate_checksum(payload_buffer, strlen(payload_buffer));

		  //new_checksum ^= 12;//////////////////////////////////////////////////////////////////////////

		  snprintf(tx_buffer, sizeof(tx_buffer), "%s->%u\n", payload_buffer, new_checksum);
		  HAL_UART_Transmit(&huart1, (uint8_t *)tx_buffer, strlen(tx_buffer), HAL_MAX_DELAY);
		  HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, 0);
		  flag = 1;
	  }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE|RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 16;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
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
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable MSI Auto calibration
  */
  HAL_RCCEx_EnableMSIPLLMode();
}

/**
  * @brief TIM16 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM16_Init(void)
{

  /* USER CODE BEGIN TIM16_Init 0 */

  /* USER CODE END TIM16_Init 0 */

  /* USER CODE BEGIN TIM16_Init 1 */

  /* USER CODE END TIM16_Init 1 */
  htim16.Instance = TIM16;
  htim16.Init.Prescaler = 31;
  htim16.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim16.Init.Period = 65535;
  htim16.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim16.Init.RepetitionCounter = 0;
  htim16.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim16) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM16_Init 2 */

  /* USER CODE END TIM16_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LD3_Pin */
  GPIO_InitStruct.Pin = LD3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD3_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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

#ifdef  USE_FULL_ASSERT
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
