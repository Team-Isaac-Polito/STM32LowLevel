/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g4xx_hal.h"

#include "stm32g4xx_ll_adc.h"
#include "stm32g4xx_ll_cordic.h"
#include "stm32g4xx_ll_crc.h"
#include "stm32g4xx_ll_fmac.h"
#include "stm32g4xx_ll_i2c.h"
#include "stm32g4xx_ll_rcc.h"
#include "stm32g4xx_ll_bus.h"
#include "stm32g4xx_ll_crs.h"
#include "stm32g4xx_ll_system.h"
#include "stm32g4xx_ll_exti.h"
#include "stm32g4xx_ll_cortex.h"
#include "stm32g4xx_ll_utils.h"
#include "stm32g4xx_ll_pwr.h"
#include "stm32g4xx_ll_dma.h"
#include "stm32g4xx_ll_spi.h"
#include "stm32g4xx_ll_tim.h"
#include "stm32g4xx_ll_usart.h"
#include "stm32g4xx_ll_gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define EXP_ENC_L_A_Pin LL_GPIO_PIN_0
#define EXP_ENC_L_A_GPIO_Port GPIOC
#define EXP_ENC_L_B_Pin LL_GPIO_PIN_1
#define EXP_ENC_L_B_GPIO_Port GPIOC
#define LED_USR_Pin LL_GPIO_PIN_2
#define LED_USR_GPIO_Port GPIOC
#define LED_PWR_Pin LL_GPIO_PIN_3
#define LED_PWR_GPIO_Port GPIOC
#define VBAT_SENSE_Pin LL_GPIO_PIN_0
#define VBAT_SENSE_GPIO_Port GPIOA
#define DXL1_DE_Pin LL_GPIO_PIN_1
#define DXL1_DE_GPIO_Port GPIOA
#define DXL1_DATA_Pin LL_GPIO_PIN_2
#define DXL1_DATA_GPIO_Port GPIOA
#define LED_HP_PWM_Pin LL_GPIO_PIN_3
#define LED_HP_PWM_GPIO_Port GPIOA
#define EXP_SPI_NSS_Pin LL_GPIO_PIN_4
#define EXP_SPI_NSS_GPIO_Port GPIOA
#define EXP_SPI_SCK_Pin LL_GPIO_PIN_5
#define EXP_SPI_SCK_GPIO_Port GPIOA
#define EXP_SPI_MISO_Pin LL_GPIO_PIN_6
#define EXP_SPI_MISO_GPIO_Port GPIOA
#define EXP_SPI_MOSI_Pin LL_GPIO_PIN_7
#define EXP_SPI_MOSI_GPIO_Port GPIOA
#define EFUSE1_IMON_Pin LL_GPIO_PIN_4
#define EFUSE1_IMON_GPIO_Port GPIOC
#define EFUSE1_PG_Pin LL_GPIO_PIN_5
#define EFUSE1_PG_GPIO_Port GPIOC
#define EFUSE2_IMON_Pin LL_GPIO_PIN_0
#define EFUSE2_IMON_GPIO_Port GPIOB
#define EFUSE2_PG_Pin LL_GPIO_PIN_1
#define EFUSE2_PG_GPIO_Port GPIOB
#define LED_DXL_Pin LL_GPIO_PIN_2
#define LED_DXL_GPIO_Port GPIOB
#define DXL2_DATA_Pin LL_GPIO_PIN_10
#define DXL2_DATA_GPIO_Port GPIOB
#define CAN_SHDN_Pin LL_GPIO_PIN_11
#define CAN_SHDN_GPIO_Port GPIOB
#define DXL2_DE_Pin LL_GPIO_PIN_14
#define DXL2_DE_GPIO_Port GPIOB
#define CAN_STBY_Pin LL_GPIO_PIN_15
#define CAN_STBY_GPIO_Port GPIOB
#define EXP_PWM1_Pin LL_GPIO_PIN_6
#define EXP_PWM1_GPIO_Port GPIOC
#define EXP_PWM2_Pin LL_GPIO_PIN_7
#define EXP_PWM2_GPIO_Port GPIOC
#define LORA_TX_Pin LL_GPIO_PIN_9
#define LORA_TX_GPIO_Port GPIOA
#define LORA_RX_Pin LL_GPIO_PIN_10
#define LORA_RX_GPIO_Port GPIOA
#define EXP_DIR1_Pin LL_GPIO_PIN_10
#define EXP_DIR1_GPIO_Port GPIOC
#define EXP_DIR2_Pin LL_GPIO_PIN_11
#define EXP_DIR2_GPIO_Port GPIOC
#define UART5_TX_NC_Pin LL_GPIO_PIN_12
#define UART5_TX_NC_GPIO_Port GPIOC
#define UART5_RX_DEBUG_Pin LL_GPIO_PIN_2
#define UART5_RX_DEBUG_GPIO_Port GPIOD
#define ENC_A_Pin LL_GPIO_PIN_4
#define ENC_A_GPIO_Port GPIOB
#define ENC_B_Pin LL_GPIO_PIN_5
#define ENC_B_GPIO_Port GPIOB
#define LED_CAN_Pin LL_GPIO_PIN_6
#define LED_CAN_GPIO_Port GPIOB
#define BOOT_BTN_Pin LL_GPIO_PIN_8
#define BOOT_BTN_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
