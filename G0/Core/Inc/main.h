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
#include "stm32g0xx_hal.h"

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
#define MODE_SW_L_Pin GPIO_PIN_1
#define MODE_SW_L_GPIO_Port GPIOA
#define MODE_SW_H_Pin GPIO_PIN_5
#define MODE_SW_H_GPIO_Port GPIOA
#define LED_3_Pin GPIO_PIN_11
#define LED_3_GPIO_Port GPIOB
#define FTSW_R_Pin GPIO_PIN_12
#define FTSW_R_GPIO_Port GPIOB
#define LED_4_Pin GPIO_PIN_13
#define LED_4_GPIO_Port GPIOB
#define FTSW_L_Pin GPIO_PIN_14
#define FTSW_L_GPIO_Port GPIOB
#define LED_5_Pin GPIO_PIN_15
#define LED_5_GPIO_Port GPIOB
#define LED_6_Pin GPIO_PIN_8
#define LED_6_GPIO_Port GPIOA
#define SPI1_CS_Pin GPIO_PIN_15
#define SPI1_CS_GPIO_Port GPIOA
#define LED_2_PWM_Pin GPIO_PIN_0
#define LED_2_PWM_GPIO_Port GPIOD
#define LED_1_PWM_Pin GPIO_PIN_9
#define LED_1_PWM_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
