/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.cpp
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
#include "adc.h"
#include "cordic.h"
#include "crc.h"
#include "fdcan.h"
#include "fmac.h"
#include "i2c.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "usb.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "mod_config.h"
#include "definitions.h"
#include "communication.h"

// Library headers
#include "Debug.h"
#include "CanWrapper.h"
#include "DynamixelLL.h"
#include "AbsoluteEncoder.h"
#include "IMU.h"
#include "Battery.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

// State machine for the beak/gripper motor (MODC_ARM only).
enum class BeakState : uint8_t { IDLE, CLOSING, OPENING, HOLDING };

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

// CAN
static CanWrapper canW;

// Debug
extern SerialDebug Debug;

// Traction motors (all modules)
static DynamixelLL dxl_traction(USART2, 0);               // sync/broadcast handle
static DynamixelLL mot_left(USART2, SERVO_TRACTION_L_ID);
static DynamixelLL mot_right(USART2, SERVO_TRACTION_R_ID);
static const uint8_t traction_ids[] = {SERVO_TRACTION_R_ID, SERVO_TRACTION_L_ID};
static float speeds_dxl[2] = {0.0f, 0.0f};

// Battery
static Battery battery;

// Health / timing
static uint32_t time_bat  = 0U;
static uint32_t time_tel  = 0U;
static uint32_t time_data = 0U;
static bool     can_active = false;

// LED heartbeat
static bool led_state = false;

// Module-specific peripherals
#ifdef MODC_YAW
static AbsoluteEncoder encoderYaw(ABSOLUTE_ENCODER_ADDRESS);
#endif

#ifdef MODC_IMU
static IMU imu;
#endif

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
extern "C" void SystemClock_Config(void);
extern "C" int  main(void);
/* USER CODE BEGIN PFP */
static void sendFeedback(void);
static void handleSetpoint(uint8_t msg_id, const uint8_t *msg_data);
static void DXL_TRACTION_INIT(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
extern "C" int main(void)
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
  MX_ADC2_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  MX_TIM1_Init();
  MX_TIM3_Init();
  MX_TIM8_Init();
  MX_UART5_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  MX_USB_PCD_Init();
  MX_TIM2_Init();
  MX_CORDIC_Init();
  MX_CRC_Init();
  MX_FMAC_Init();
  MX_I2C3_Init();
  MX_FDCAN2_Init();
  /* USER CODE BEGIN 2 */

  // Debug — must be first so all subsequent prints reach the console
  Debug.setLevel(Level::LOG_INFO);
  Debug.log(Level::LOG_INFO, "[main] Debug ready\n");

  // CAN — begin() releases STBY, so must come after MX_FDCAN2_Init()
  canW.begin();
  Debug.log(Level::LOG_INFO, "[main] CAN ready\n");

  // DXL traction — USART2 DE pin + Dynamixel boot sequence
  DXL_TRACTION_INIT();
  Debug.log(Level::LOG_INFO, "[main] Traction DXL ready\n");

  // 4a. I2C — Yaw encoder (shares I2C1 with IMU)
#ifdef MODC_YAW
  encoderYaw.setZero();
  Debug.log(Level::LOG_INFO, "[main] Yaw encoder ready\n");
#endif

  // 4b. I2C — IMU
#ifdef MODC_IMU
  imu.begin(IMU_ADDRESS);
  if (!imu.checkID()) {
      Debug.log(Level::LOG_WARN, "[main] IMU not found at 0x%02X!\n", IMU_ADDRESS);
  } else {
      imu.enableGyro();
      imu.enableAccel();
      imu.calibrateAccel();
      imu.calibrateGyro();
      Debug.log(Level::LOG_INFO, "[main] IMU ready\n");
  }
#endif

  // 5. ADC — Battery (ADC1 LL)
  battery.begin();
  Debug.log(Level::LOG_INFO, "[main] Battery ready, %.2f V\n", battery.readVoltage());

  Debug.log(Level::LOG_INFO, "[main] Init complete, entering main loop\n");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    uint32_t now = HAL_GetTick();

    // Health checks (1 Hz)
    if (now - time_bat >= DT_BAT) {
        time_bat = now;
        if (!battery.charged()) {
            Debug.log(Level::LOG_WARN, "[main] Low battery: %.2f V\n", battery.readVoltage());
        }
    }

    // Telemetry / feedback (25 Hz)
    if (now - time_tel >= DT_TEL) {
        time_tel = now;
        sendFeedback();
    }

    // Receive CAN setpoint
    uint8_t msg_id;
    uint8_t msg_data[8];
    if (canW.readMessage(&msg_id, msg_data)) {
        time_data = now;
        can_active = true;
        handleSetpoint(msg_id, msg_data);
    } else if (can_active && (now - time_data > CAN_TIMEOUT)) {
        // CAN silence timeout — stop all motors
        can_active = false;
        speeds_dxl[0] = 0.0f;
        speeds_dxl[1] = 0.0f;
        dxl_traction.setGoalVelocity_RPM(speeds_dxl);
        Debug.log(Level::LOG_WARN, "[main] CAN timeout, motors stopped\n");
    }

    // LED heartbeat (toggle every 500 ms)
    if (now % 1000U < 500U) {
        if (!led_state) { LL_GPIO_SetOutputPin(LED_USR_GPIO_Port, LED_USR_Pin); led_state = true; }
    } else {
        if ( led_state) { LL_GPIO_ResetOutputPin(LED_USR_GPIO_Port, LED_USR_Pin); led_state = false; }
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
extern "C" void SystemClock_Config(void)
{
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_4);
  while(LL_FLASH_GetLatency() != LL_FLASH_LATENCY_4)
  {
  }
  LL_PWR_EnableRange1BoostMode();
  LL_RCC_HSE_Enable();
   /* Wait till HSE is ready */
  while(LL_RCC_HSE_IsReady() != 1)
  {
  }

  LL_RCC_HSI48_Enable();
   /* Wait till HSI48 is ready */
  while(LL_RCC_HSI48_IsReady() != 1)
  {
  }

  LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSE, LL_RCC_PLLM_DIV_6, 85, LL_RCC_PLLR_DIV_2);
  LL_RCC_PLL_EnableDomain_SYS();
  LL_RCC_PLL_Enable();
   /* Wait till PLL is ready */
  while(LL_RCC_PLL_IsReady() != 1)
  {
  }

  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_2);
   /* Wait till System clock is ready */
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL)
  {
  }

  /* Insure 1us transition state at intermediate medium speed clock*/
  for (__IO uint32_t i = (170 >> 1); i !=0; i--);

  /* Set AHB prescaler*/
  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
  LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_1);
  LL_SetSystemCoreClock(170000000);

   /* Update the time base */
  if (HAL_InitTick (TICK_INT_PRIORITY) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/**
 * Traction motor initialisation sequence (velocity control mode).
 * Must be called after MX_USART2_UART_Init() (DXL half-duplex on USART2).
 */
static void DXL_TRACTION_INIT(void)
{
    static const uint8_t n = sizeof(traction_ids) / sizeof(traction_ids[0]);

    // Enable RS-485 half-duplex DE mode on USART2 (must precede any DXL packet)
    dxl_traction.begin();
    mot_left.begin();
    mot_right.begin();

    // Disable torque first for safe reconfiguration
    mot_left.setTorqueEnable(false);
    mot_right.setTorqueEnable(false);
    HAL_Delay(10U);

    // Status return level 2 — respond to all instructions
    mot_left.setStatusReturnLevel(2U);
    mot_right.setStatusReturnLevel(2U);
    HAL_Delay(10U);

    // Drive mode: right motor needs reverseMode=true (opposite mounting)
    mot_left.setDriveMode(false, false, false);
    mot_right.setDriveMode(false, false, true);

    // Operating mode 1 = velocity control
    dxl_traction.enableSync(traction_ids, n);
    dxl_traction.setOperatingMode(1U);
    HAL_Delay(10U);

    // Instant velocity response (profile acceleration = 0)
    uint32_t profile_accel[2] = {0U, 0U};
    dxl_traction.setProfileAcceleration(profile_accel);

    // Enable torque
    mot_left.setTorqueEnable(true);
    mot_right.setTorqueEnable(true);
}

/**
 * Send telemetry CAN frames. Populated fully in Issue #12.
 * Stub: reads encoder/IMU data and transmits on the CAN bus.
 */
static void sendFeedback(void)
{
    /* TODO (Issue #12): build and send CAN telemetry frames */
    (void)0;
}

/**
 * Dispatch incoming CAN setpoint message. Populated fully in Issue #11.
 * @param msg_id   CAN message type identifier (bits[23:16] of extended ID).
 * @param msg_data 8-byte payload.
 */
static void handleSetpoint(uint8_t msg_id, const uint8_t *msg_data)
{
    /* TODO (Issue #11): decode CAN setpoint and drive motors/servos */
    (void)msg_id;
    (void)msg_data;
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
