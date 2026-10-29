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
#include <math.h>
#include "font5x7.h"
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
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
char uart_buf[150];

uint8_t motor_run = 0;
uint8_t motor_reverse = 0;  // 0 = вперед, 1 = назад

GPIO_PinState old_run_button  = GPIO_PIN_SET;
GPIO_PinState old_stop_button = GPIO_PIN_SET;
GPIO_PinState old_dir_button = GPIO_PIN_SET;


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#define OLED_ADDR (0x3C << 1)

#define POT_ADC_MIN 0
#define POT_ADC_MAX 4095


/* ============================================
   OLED
   ============================================ */

void OLED_Command(uint8_t cmd)
{
    uint8_t data[2];

    data[0] = 0x00;
    data[1] = cmd;

    HAL_I2C_Master_Transmit(&hi2c1,
                            OLED_ADDR,
                            data,
                            2,
                            HAL_MAX_DELAY);
}


void OLED_Init(void)
{
    HAL_Delay(100);

    OLED_Command(0xAE);

    OLED_Command(0xD5);
    OLED_Command(0x80);

    OLED_Command(0xA8);
    OLED_Command(0x3F);

    OLED_Command(0xD3);
    OLED_Command(0x00);

    OLED_Command(0x40);

    OLED_Command(0x8D);
    OLED_Command(0x14);

    /* Page addressing mode */
    OLED_Command(0x20);
    OLED_Command(0x02);

    OLED_Command(0xA1);
    OLED_Command(0xC8);

    OLED_Command(0xDA);
    OLED_Command(0x12);

    OLED_Command(0x81);
    OLED_Command(0xCF);

    OLED_Command(0xD9);
    OLED_Command(0xF1);

    OLED_Command(0xDB);
    OLED_Command(0x40);

    OLED_Command(0xA4);
    OLED_Command(0xA6);

    OLED_Command(0xAF);

    HAL_Delay(100);
}


void OLED_SetCursor(uint8_t x, uint8_t page)
{
    OLED_Command(0xB0 + page);

    OLED_Command(0x00 + (x & 0x0F));
    OLED_Command(0x10 + ((x >> 4) & 0x0F));
}


void OLED_Clear(void)
{
    uint8_t data[129];

    data[0] = 0x40;

    for (int i = 1; i < 129; i++)
    {
        data[i] = 0x00;
    }

    for (uint8_t page = 0; page < 8; page++)
    {
        OLED_SetCursor(0, page);

        HAL_I2C_Master_Transmit(&hi2c1,
                                OLED_ADDR,
                                data,
                                129,
                                HAL_MAX_DELAY);
    }
}


void OLED_WriteChar(char c)
{
    if (c < 32 || c > 126)
    {
        c = '?';
    }

    uint8_t index = (uint8_t)c - 32;

    uint8_t data[7];

    data[0] = 0x40;

    for (uint8_t i = 0; i < 5; i++)
    {
        data[i + 1] = font5x7[index][i];
    }

    data[6] = 0x00;

    HAL_I2C_Master_Transmit(&hi2c1,
                            OLED_ADDR,
                            data,
                            7,
                            HAL_MAX_DELAY);
}


void OLED_WriteString(const char *str)
{
    while (*str)
    {
        OLED_WriteChar(*str);
        str++;
    }
}


/* ============================================
   ADC
   ============================================ */

uint32_t Read_ADC_Channel(uint32_t channel)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    sConfig.Channel = channel;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;

    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    HAL_ADC_Start(&hadc1);

    HAL_ADC_PollForConversion(&hadc1,
                              HAL_MAX_DELAY);

    uint32_t value = HAL_ADC_GetValue(&hadc1);

    HAL_ADC_Stop(&hadc1);

    return value;
}


/* ============================================
   NTC
   ============================================ */

float NTC_GetTemperature(uint32_t adc)
{
    if (adc == 0 || adc >= 4095)
    {
        return -100.0f;
    }

    const float R_FIXED = 10000.0f;
    const float R0 = 10000.0f;
    const float BETA = 3950.0f;
    const float T0 = 25.0f + 273.15f;

    float r_ntc =
        R_FIXED *
        ((float)adc /
        (4095.0f - (float)adc));

    float temp_k =
        1.0f /
        ((1.0f / T0) +
         (1.0f / BETA) *
         logf(r_ntc / R0));

    return temp_k - 273.15f;
}


/* ============================================
   POT 0...100 %
   ============================================ */

uint32_t PotToPercent(uint32_t adc)
{
    if (adc <= POT_ADC_MIN)
    {
        return 0;
    }

    if (adc >= POT_ADC_MAX)
    {
        return 100;
    }

    return ((adc - POT_ADC_MIN) * 100UL) /
           (POT_ADC_MAX - POT_ADC_MIN);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
	uint32_t Read_ADC_Channel(uint32_t channel)
	{
	    ADC_ChannelConfTypeDef sConfig = {0};

	    sConfig.Channel = channel;
	    sConfig.Rank = 1;
	    sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;

	    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

	    HAL_ADC_Start(&hadc1);
	    HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);

	    uint32_t value = HAL_ADC_GetValue(&hadc1);

	    HAL_ADC_Stop(&hadc1);

	    return value;
	}
	float NTC_GetTemperature(uint32_t adc)
	{
	    if (adc == 0 || adc >= 4095)
	        return -100.0f;

	    const float R_FIXED = 10000.0f;
	    const float R0 = 10000.0f;
	    const float BETA = 3950.0f;
	    const float T0 = 25.0f + 273.15f;

	    float r_ntc = R_FIXED * ((float)adc / (4095.0f - (float)adc));

	    float temp_k = 1.0f /
	        ((1.0f / T0) + (1.0f / BETA) * logf(r_ntc / R0));

	    return temp_k - 273.15f;
	}
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
  MX_I2C1_Init();
  MX_USART2_UART_Init();
  MX_ADC1_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  /* USER CODE BEGIN 2 */
  OLED_Init();
  OLED_Clear();

  motor_run = 0;
  motor_reverse = 0;
  /* LED: стартуем в STOP */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5,  GPIO_PIN_RESET); // GREEN OFF
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4,  GPIO_PIN_RESET); // YELLOW OFF
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_SET);   // RED ON

  /* L298N: мотор остановлен */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);  // IN3
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);  // IN4

  /* Запускаем PWM D3 / PB3 / TIM2_CH2 */
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);

  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 0);


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  /* =========================
	         BUTTONS
	         ========================= */

	      GPIO_PinState run_button =
	          HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_8);   // D7

	      GPIO_PinState stop_button =
	          HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9);   // D8

	      GPIO_PinState dir_button =
	          HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_7);   // D9


	      /* =========================
	         RUN
	         ========================= */

	      if ((run_button == GPIO_PIN_RESET) &&
	          (old_run_button == GPIO_PIN_SET))
	      {
	          motor_run = 1;

	          HAL_Delay(30);
	      }


	      /* =========================
	         STOP
	         ========================= */

	      if ((stop_button == GPIO_PIN_RESET) &&
	          (old_stop_button == GPIO_PIN_SET))
	      {
	          motor_run = 0;

	          HAL_Delay(30);
	      }


	      /* =========================
	         DIRECTION
	         ========================= */

	      if ((dir_button == GPIO_PIN_RESET) &&
	          (old_dir_button == GPIO_PIN_SET))
	      {
	          /*
	           * Сначала убираем PWM,
	           * чтобы не менять направление
	           * на полном ходу.
	           */
	          __HAL_TIM_SET_COMPARE(&htim2,
	                                TIM_CHANNEL_2,
	                                0);

	          HAL_Delay(150);

	          motor_reverse =
	              !motor_reverse;
	      }


	      old_run_button  = run_button;
	      old_stop_button = stop_button;
	      old_dir_button  = dir_button;


	      /* =========================
	         ADC
	         ========================= */

	      uint32_t pot_adc =
	          Read_ADC_Channel(ADC_CHANNEL_0);

	      uint32_t ntc1_adc =
	          Read_ADC_Channel(ADC_CHANNEL_1);

	      uint32_t ntc2_adc =
	          Read_ADC_Channel(ADC_CHANNEL_4);


	      /* =========================
	         POT 0...100 %
	         ========================= */

	      uint32_t set_percent =
	          PotToPercent(pot_adc);


	      /* TIM2 Period = 99 */
	      uint32_t pwm_value =
	          (set_percent * 99UL) / 100UL;


	      /* =========================
	         TEMPERATURE
	         ========================= */

	      float temp1 =
	          NTC_GetTemperature(ntc1_adc);

	      float temp2 =
	          NTC_GetTemperature(ntc2_adc);


	      /* =========================
	         MOTOR CONTROL
	         ========================= */

	      if (motor_run && set_percent > 0)
	      {
	          if (motor_reverse == 0)
	          {
	              /* FORWARD */

	              HAL_GPIO_WritePin(GPIOA,
	                                GPIO_PIN_7,
	                                GPIO_PIN_SET);    // IN3 = 1

	              HAL_GPIO_WritePin(GPIOA,
	                                GPIO_PIN_6,
	                                GPIO_PIN_RESET);  // IN4 = 0
	          }
	          else
	          {
	              /* REVERSE */

	              HAL_GPIO_WritePin(GPIOA,
	                                GPIO_PIN_7,
	                                GPIO_PIN_RESET);  // IN3 = 0

	              HAL_GPIO_WritePin(GPIOA,
	                                GPIO_PIN_6,
	                                GPIO_PIN_SET);    // IN4 = 1
	          }


	          __HAL_TIM_SET_COMPARE(&htim2,
	                                TIM_CHANNEL_2,
	                                pwm_value);
	      }
	      else
	      {
	          /* STOP */

	          __HAL_TIM_SET_COMPARE(&htim2,
	                                TIM_CHANNEL_2,
	                                0);

	          HAL_GPIO_WritePin(GPIOA,
	                            GPIO_PIN_7,
	                            GPIO_PIN_RESET);

	          HAL_GPIO_WritePin(GPIOA,
	                            GPIO_PIN_6,
	                            GPIO_PIN_RESET);
	      }


	      /* =========================
	         LED
	         ========================= */

	      if (motor_run)
	      {
	          /* GREEN = RUN */
	          HAL_GPIO_WritePin(GPIOB,
	                            GPIO_PIN_5,
	                            GPIO_PIN_SET);

	          /* RED OFF */
	          HAL_GPIO_WritePin(GPIOB,
	                            GPIO_PIN_10,
	                            GPIO_PIN_RESET);
	      }
	      else
	      {
	          /* GREEN OFF */
	          HAL_GPIO_WritePin(GPIOB,
	                            GPIO_PIN_5,
	                            GPIO_PIN_RESET);

	          /* RED = STOP */
	          HAL_GPIO_WritePin(GPIOB,
	                            GPIO_PIN_10,
	                            GPIO_PIN_SET);
	      }


	      /*
	       * YELLOW = REVERSE
	       */

	      if (motor_reverse)
	      {
	          HAL_GPIO_WritePin(GPIOB,
	                            GPIO_PIN_4,
	                            GPIO_PIN_SET);
	      }
	      else
	      {
	          HAL_GPIO_WritePin(GPIOB,
	                            GPIO_PIN_4,
	                            GPIO_PIN_RESET);
	      }


	      /* =========================
	         OLED
	         ========================= */

	      char line[22];


	      /* SET */

	      OLED_SetCursor(0, 0);

	      snprintf(line,
	               sizeof(line),
	               "SET: %3lu %%       ",
	               (unsigned long)set_percent);

	      OLED_WriteString(line);


	      /* T1 */

	      OLED_SetCursor(0, 2);

	      snprintf(line,
	               sizeof(line),
	               "T1: %3d C          ",
	               (int)temp1);

	      OLED_WriteString(line);


	      /* T2 */

	      OLED_SetCursor(0, 4);

	      snprintf(line,
	               sizeof(line),
	               "T2: %3d C          ",
	               (int)temp2);

	      OLED_WriteString(line);


	      /* STATE */

	      OLED_SetCursor(0, 6);

	      if (motor_run)
	      {
	          OLED_WriteString(
	              "STATE: RUN          ");
	      }
	      else
	      {
	          OLED_WriteString(
	              "STATE: STOP         ");
	      }


	      /* DIRECTION */

	      OLED_SetCursor(0, 7);

	      if (motor_reverse)
	      {
	          OLED_WriteString(
	              "DIR: REVERSE        ");
	      }
	      else
	      {
	          OLED_WriteString(
	              "DIR: FORWARD        ");
	      }


	      /* =========================
	         UART / PuTTY
	         ========================= */

	      int len =
	          snprintf(
	              uart_buf,
	              sizeof(uart_buf),

	              "SET=%lu%% PWM=%lu/99 "
	              "T1=%d C T2=%d C "
	              "STATE=%s DIR=%s\r\n",

	              (unsigned long)set_percent,
	              (unsigned long)pwm_value,

	              (int)temp1,
	              (int)temp2,

	              motor_run ? "RUN" : "STOP",

	              motor_reverse ?
	              "REVERSE" : "FORWARD"
	          );


	      HAL_UART_Transmit(
	          &huart2,
	          (uint8_t *)uart_buf,
	          len,
	          HAL_MAX_DELAY
	      );


	      HAL_Delay(100);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

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
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 7;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
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

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 83;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 99;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

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
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LD2_Pin|GPIO_PIN_6|GPIO_PIN_7, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10|GPIO_PIN_4|GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LD2_Pin PA6 PA7 */
  GPIO_InitStruct.Pin = LD2_Pin|GPIO_PIN_6|GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB10 PB4 PB5 */
  GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_4|GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PC7 */
  GPIO_InitStruct.Pin = GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PA8 PA9 */
  GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

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
	  // Зеленый
	  	  GPIO_PinState run  = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_8);
	      GPIO_PinState stop = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9);
	      GPIO_PinState mode = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_7);

	      int len = snprintf(uart_buf,
	                         sizeof(uart_buf),
	                         "RUN=%d  STOP=%d  MODE=%d\r\n",
	                         run,
	                         stop,
	                         mode);

	      HAL_UART_Transmit(&huart2,
	                        (uint8_t *)uart_buf,
	                        len,
	                        HAL_MAX_DELAY);

	      HAL_Delay(200);
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
