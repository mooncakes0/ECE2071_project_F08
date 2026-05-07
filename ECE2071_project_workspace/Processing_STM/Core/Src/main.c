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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ECHO_TIMEOUT_US 30000
#define DISTANCE_THRESHOLD_CM 10
#define STOP_DELAY_MS 1000
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim16;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
uint16_t rawSample = 0;	// new sample
uint16_t previousSample = 0;	// previous sample for filter
uint16_t filteredSample = 0;	// cleaned sample

uint8_t mode = 'M';	// 'M' = Manual, 'D' = Distance Trigger
uint8_t command = 0;
uint8_t recording = 1;

uint32_t lastDetectedTime = 0;
uint32_t lastDistanceCheck = 0;

int distance_cm = -1;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_SPI1_Init(void);
static void MX_TIM16_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
void send_audio_sample(void);
int get_distance_cm(void);
void delay_us(uint16_t us);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#define ECHO_TIMEOUT_US 30000
#define DISTANCE_THRESHOLD_CM 10
#define STOP_DELAY_MS 1000
#define OUTLIER_THRESHOLD 150

void delay_us(uint16_t us) // microsecond delay
{
    __HAL_TIM_SET_COUNTER(&htim16, 0);	// set timer16 to 0

    while (__HAL_TIM_GET_COUNTER(&htim16) < us)	// block the code until time reach
    {
        // wait
    }
}

int get_distance_cm(void)
{
    uint32_t pulseWidth = 0;

    HAL_GPIO_WritePin(Trigger_GPIO_Port, Trigger_Pin, 0);	//	reset trigger pin
    delay_us(2);

    HAL_GPIO_WritePin(Trigger_GPIO_Port, Trigger_Pin, 1);	//	 send trigger pin HIGH for 10 microsecond
    delay_us(10);
    HAL_GPIO_WritePin(Trigger_GPIO_Port, Trigger_Pin, 0);

    __HAL_TIM_SET_COUNTER(&htim16, 0);	// set timer to 0
    while (HAL_GPIO_ReadPin(Echo_GPIO_Port, Echo_Pin) == 0)		// if echo pin is not HIGH
    {
        if (__HAL_TIM_GET_COUNTER(&htim16) > ECHO_TIMEOUT_US)
        {
            return 999;
        }
    }

    __HAL_TIM_SET_COUNTER(&htim16, 0);
    while (HAL_GPIO_ReadPin(Echo_GPIO_Port, Echo_Pin) == GPIO_PIN_SET)	// if echo pin went HIGH and did not went LOW
    {
        if (__HAL_TIM_GET_COUNTER(&htim16) > ECHO_TIMEOUT_US)
        {
            return 999;
        }
    }

    pulseWidth = __HAL_TIM_GET_COUNTER(&htim16);	// get the time of the echo HIGH

    return (int)(pulseWidth / 58);
}

void send_audio_sample(void)
{
    static uint8_t sampleToggle = 0;	// used to send only every second sample
    static uint8_t outlierStrike = 0;	// count repeated outlier count

    if (__HAL_SPI_GET_FLAG(&hspi1, SPI_FLAG_RXNE) == 0)	// check if SPI receive new data (RXNE = Receive buffer Not Empty)
    {
        return;	// no new sample, leave this function
    }

    rawSample = hspi1.Instance->DR;	// read 16 bit of the SPI

    rawSample = rawSample & 0x03FF;	// keep only the 10 bits

    if (__HAL_SPI_GET_FLAG(&hspi1, SPI_FLAG_OVR) != 0)	// if overrun happened (overrun = new data arrive before old data was read)
    {
        __HAL_SPI_CLEAR_OVRFLAG(&hspi1);	// clear overrun flag
    }

    int16_t delta = (int16_t)rawSample - (int16_t)previousSample;	// calculate the difference between new and previous sample

    if (delta > OUTLIER_THRESHOLD || delta < -OUTLIER_THRESHOLD)	// if the differences over the threshold
    {
        outlierStrike++;	// increment outlier count
        if (outlierStrike < 5)
        {
            rawSample = previousSample;	// replace outlier with previous value
        }
        else
        {
            outlierStrike = 0;	// if many spike in a row, accept the value
        }
    }
    else
    {
        outlierStrike = 0;	// if normal, reset outlier count
    }

    filteredSample = (rawSample + previousSample) / 2;	// moving average filter
    previousSample = rawSample;

    sampleToggle = !sampleToggle;	// toggle between 1 and 0 so the sample rate is half
    if (sampleToggle)
    {
        uint8_t uartOut = (uint8_t)(filteredSample >> 2);	//	convert 10bit to 8bit
        HAL_UART_Transmit(&huart2, &uartOut, 1, 1);
    }
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
  MX_ADC1_Init();
  MX_SPI1_Init();
  MX_TIM16_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_Base_Start(&htim16);
  __HAL_SPI_ENABLE(&hspi1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      if (HAL_UART_Receive(&huart2, &command, 1, 0) == HAL_OK)
      {
          if (command == 'M')
          {
              mode = 'M';
              recording = 1;
              previousSample = 512;
          }
          else if (command == 'D')
          {
              mode = 'D';
              recording = 0;
              lastDetectedTime = 0;
              lastDistanceCheck = 0;
              previousSample = 512;
          }
      }
      if (mode == 'M')
      {
          send_audio_sample();
      }
      else if (mode == 'D')
      {
          if (HAL_GetTick() - lastDistanceCheck >= 100)	// check distance every 100ms
          {
              lastDistanceCheck = HAL_GetTick();

              distance_cm = get_distance_cm();

              if (distance_cm > 1 && distance_cm < DISTANCE_THRESHOLD_CM)	// if distance between 1 and 10 cm
              {
                  recording = 1;
                  lastDetectedTime = HAL_GetTick();
              }

              if (recording && (HAL_GetTick() - lastDetectedTime > STOP_DELAY_MS))	// if object have gone for at least 1 second
              {
                  recording = 0;
              }
          }

          if (recording)
          {
              send_audio_sample();
          }
          else
          {
              HAL_Delay(5);
          }
      }
  /*USER CODE START 3 */
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
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc1.Init.Resolution = ADC_RESOLUTION_8B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_6;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_SLAVE;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES_RXONLY;
  hspi1.Init.DataSize = SPI_DATASIZE_16BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

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
  huart2.Init.BaudRate = 921600;
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
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(Trigger_GPIO_Port, Trigger_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : Trigger_Pin */
  GPIO_InitStruct.Pin = Trigger_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(Trigger_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : Echo_Pin */
  GPIO_InitStruct.Pin = Echo_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(Echo_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void send_audio_sample(void)
{
    HAL_SPI_Receive(&hspi1, &rawSample, 1, HAL_MAX_DELAY);

    filteredSample = (uint8_t)(((uint16_t)rawSample + (uint16_t)previousSample) / 2);

    HAL_UART_Transmit(&huart2, &filteredSample, 1, HAL_MAX_DELAY);

    previousSample = rawSample;
}


int get_distance_cm(void)
{
    uint32_t pulseWidth = 0;

    HAL_GPIO_WritePin(Trigger_GPIO_Port, Trigger_Pin, GPIO_PIN_RESET);
    delay_us(2);

    HAL_GPIO_WritePin(Trigger_GPIO_Port, Trigger_Pin, GPIO_PIN_SET);
    delay_us(10);
    HAL_GPIO_WritePin(Trigger_GPIO_Port, Trigger_Pin, GPIO_PIN_RESET);

    __HAL_TIM_SET_COUNTER(&htim16, 0);
    while (HAL_GPIO_ReadPin(Echo_GPIO_Port, Echo_Pin) == GPIO_PIN_RESET)
    {
        if (__HAL_TIM_GET_COUNTER(&htim16) > ECHO_TIMEOUT_US)
        {
            return 999;
        }
    }

    __HAL_TIM_SET_COUNTER(&htim16, 0);
    while (HAL_GPIO_ReadPin(Echo_GPIO_Port, Echo_Pin) == GPIO_PIN_SET)
    {
        if (__HAL_TIM_GET_COUNTER(&htim16) > ECHO_TIMEOUT_US)
        {
            return 999;
        }
    }

    pulseWidth = __HAL_TIM_GET_COUNTER(&htim16);

    return (int)(pulseWidth / 58);
}


void delay_us(uint16_t us)
{
    __HAL_TIM_SET_COUNTER(&htim16, 0);

    while (__HAL_TIM_GET_COUNTER(&htim16) < us)
    {
        // wait
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
