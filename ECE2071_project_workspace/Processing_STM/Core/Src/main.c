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

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim16;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
uint16_t rawSample = 0;	// new sample
uint16_t previousSample = 0;	// previous sample for filter
uint16_t filteredSample = 0;	// cleaned sample

uint8_t mode = 'I';	// 'I' = Idle, 'M' = Manual, 'D' = Distance Trigger
uint8_t command = 0;
uint8_t recording = 0;

uint32_t lastDetectedTime = 0;
uint32_t lastDistanceCheck = 0;

int distance_cm = -1;	// haven't measure yet

static uint8_t haveFirst = 0;	// task 4 packing (0 = no first sample stored)
static uint16_t firstSample = 0;	// store first 12 bit sample until next sample arrive
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM16_Init(void);
/* USER CODE BEGIN PFP */
void delay_us(uint16_t us);
int get_distance_cm(void);
void send_audio_sample(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#define ECHO_TIMEOUT_US 8000	// max wait time 8000 microsecond
#define DISTANCE_THRESHOLD_CM 10	// 10 CM ultrasonic sensor threshold
#define STOP_DELAY_MS 1000	//	"short interval of time" which we put 1 second
#define OUTLIER_THRESHOLD 600	// range is 0-4095, 600 reject large sudden spike

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
  MX_SPI1_Init();
  MX_USART2_UART_Init();
  MX_TIM16_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_Base_Start(&htim16);
  __HAL_SPI_ENABLE(&hspi1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /*USER CODE START 3 */
	  if (HAL_UART_Receive(&huart2, &command, 1, 0) == HAL_OK)	// try to receive command from Python
	  {
		  if (command == 'M')
		  {
			  mode = 'M';
			  recording = 1;
			  rawSample = 2048;	// reset filter midpoint for 12 bit
			  filteredSample = 2048;
			  previousSample = 2048;
			  haveFirst = 0;	// reset packed byte state
		  }
		  else if (command == 'D')
		  {
			  mode = 'D';
			  recording = 0;
			  lastDetectedTime = 0;
 			  lastDistanceCheck = 0;
 			  rawSample = 2048;	// reset filter midpoint for 12 bit
			  filteredSample = 2048;
			  previousSample = 2048;
			  haveFirst = 0;	// reset packed byte state
		  }
		  else if (command == 'I')
		  {
			  mode = 'I';
			  recording = 0;
			  rawSample = 2048;
			  filteredSample = 2048;
			  previousSample = 2048;
			  haveFirst = 0;
		  }
	  }
	  if (mode == 'M')
	  {
		  send_audio_sample();
	  }
	  else if (mode == 'D')
	  {
		  if (HAL_GetTick() - lastDistanceCheck >= 300)	// check distance every 300ms
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
				  haveFirst = 0;	// discard incomplete packed sample
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
  hspi1.Init.DataSize = SPI_DATASIZE_12BIT;
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
  htim16.Init.Prescaler = 0;
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
  huart2.Init.BaudRate = 230400;
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
    static uint8_t outlierStrike = 0;   // Count repeated outliers

    if (__HAL_SPI_GET_FLAG(&hspi1, SPI_FLAG_RXNE) == 0)	//	if no new sample arrive
    {
        return;	// return to main loop
    }

    rawSample = hspi1.Instance->DR;     // Read 16-bit SPI value
    rawSample = rawSample & 0x0FFF;     // Keep 12-bit sample only

    if (__HAL_SPI_GET_FLAG(&hspi1, SPI_FLAG_OVR) != 0)	// if SPI overrun (new sample arrive before old sample was read)
    {
        __HAL_SPI_CLEAR_OVRFLAG(&hspi1);	// clear flag so SPI can receive normally
    }

    int16_t delta = (int16_t)rawSample - (int16_t)previousSample;	// to detect sudden spike

    if (delta > OUTLIER_THRESHOLD || delta < -OUTLIER_THRESHOLD)
    {
        outlierStrike++;

        if (outlierStrike < 5)
        {
            rawSample = previousSample;	//	reject new sample and replace with previous sample
        }
        else
        {
            outlierStrike = 0;	// too many outlier, the sample maybe real signal
        }
    }
    else
    {
        outlierStrike = 0;	// normal sample
    }

    filteredSample = (rawSample + previousSample) / 2;	// moving average filter
    previousSample = rawSample;

    uint16_t sample = filteredSample & 0x0FFF;	// make sure is 12 bit


    /*
     * if send 12 bit sample as 2 byte: 44138 × 2 bytes × 10 UART bit = 882760 bits/s (baud rate overhead 921600)
     * if send 24 bit sample as 3 byte: 44138 × 1.5 bytes × 10 UART bit =  662070 bits/s
     */
    if (!haveFirst)	// pack two 12 bit samples to send
    {
        firstSample = sample;	// this is the first sample
        haveFirst = 1;	// wait for the second one
    }
    else
    {
        uint16_t secondSample = sample;	// this is the second sample

        uint8_t tx[3];	// the package space

        tx[0] = firstSample & 0xFF;	// store lower 8 bit of first sample
        tx[1] = ((firstSample >> 8) & 0x0F) | ((secondSample & 0x0F) << 4);	// store upper 4 bit of first sample and lower 4 bit of second sample
        tx[2] = (secondSample >> 4) & 0xFF;	// store upper 8 bit of second sample

        while (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TXE) == RESET)
        {
        }
        huart2.Instance->TDR = tx[0];	// send first package byte through UART

        while (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TXE) == RESET)
        {
        }
        huart2.Instance->TDR = tx[1];	// send second package byte through UART

        while (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TXE) == RESET)
        {
        }
        huart2.Instance->TDR = tx[2];	// send third package byte through UART

        haveFirst = 0;	// reset packing state
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
