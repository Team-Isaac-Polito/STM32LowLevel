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

#include <cstring>   // memcpy
#include <cstdlib>   // abs (integer)
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

#ifdef MODC_ARM
// Arm Dynamixel motors (all on USART2 bus, position control)
static DynamixelLL ARM_dxl(USART2, 0);                              // sync handle for J1a/J1b
static DynamixelLL ARM_mot_1a(USART2, SERVO_ARM_1a_PITCH_ID);
static DynamixelLL ARM_mot_1b(USART2, SERVO_ARM_1b_PITCH_ID);
static DynamixelLL ARM_mot_2(USART2,  SERVO_ARM_2_PITCH_ID);
static DynamixelLL ARM_mot_3(USART2,  SERVO_ARM_3_ROLL_ID);
static DynamixelLL ARM_mot_4(USART2,  SERVO_ARM_4_PITCH_ID);
static DynamixelLL ARM_mot_5(USART2,  SERVO_ARM_5_ROLL_ID);
static DynamixelLL ARM_mot_6(USART2,  SERVO_ARM_6_BEAK_ID);
static const uint8_t arm_ids[] = {SERVO_ARM_1a_PITCH_ID, SERVO_ARM_1b_PITCH_ID};

// Arm home positions (DXL ext-pos units) — default from definitions.h ARM_DEFAULT_HOME
static const int32_t arm_defaults[] = ARM_DEFAULT_HOME;
static int32_t ARM_pos0_mot_1LR[2] = {arm_defaults[0], arm_defaults[1]};
static int32_t ARM_pos0_mot_2  = arm_defaults[2];
static int32_t ARM_pos0_mot_3  = arm_defaults[3];
static int32_t ARM_pos0_mot_4  = arm_defaults[4];
static int32_t ARM_pos0_mot_5  = arm_defaults[5];
static int32_t ARM_pos0_mot_6  = arm_defaults[6];

// Arm live setpoint state
static int32_t ARM_pos_mot_1LR[2] = {0, 0};
static int32_t ARM_old_pos_mot_1LR[2] = {0, 0};
static int32_t ARM_pos_mot_2 = 0, ARM_old_pos_mot_2 = 0;
static int32_t ARM_pos_mot_3 = 0, ARM_old_pos_mot_3 = 0;
static int32_t ARM_pos_mot_4 = 0, ARM_old_pos_mot_4 = 0;
static int32_t ARM_pos_mot_5 = 0, ARM_old_pos_mot_5 = 0;
static int32_t ARM_pos_mot_6 = 0, ARM_old_pos_mot_6 = 0;

// Beak gripper state machine
static BeakState beak_state = BeakState::IDLE;
static uint32_t  beak_motion_start_ms = 0U;
#endif

#ifdef MODC_JOINT
// Joint Dynamixel motors (USART3 bus)
static DynamixelLL JOINT_dxl(USART3,  0);                             // sync handle for pitch 1a/1b
static DynamixelLL JOINT_mot_1L(USART3, SERVO_JOINT_LEFT_ID);
static DynamixelLL JOINT_mot_1R(USART3, SERVO_JOINT_RIGHT_ID);
static DynamixelLL JOINT_mot_2(USART3,  SERVO_JOINT_ROLL_ID);
static const uint8_t joint_ids[] = {SERVO_JOINT_LEFT_ID, SERVO_JOINT_RIGHT_ID};

// Joint home positions
static int32_t JOINT_pos0_mot_1LR[2] = {0, 0};
static int32_t JOINT_pos0_mot_2 = 0;

// Joint live setpoint state
static int32_t JOINT_pos_mot_1LR[2] = {0, 0};
static int32_t JOINT_old_pos_mot_1LR[2] = {0, 0};
static int32_t JOINT_pos_mot_2 = 0;
#endif

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
extern "C" void SystemClock_Config(void);
extern "C" int  main(void);
/* USER CODE BEGIN PFP */
static void sendFeedback(void);
static void handleSetpoint(uint8_t msg_id, const uint8_t *msg_data);
static void DXL_TRACTION_INIT(void);
#ifdef MODC_ARM
static void DXL_ARM_INIT(void);
#endif
#ifdef MODC_JOINT
static void DXL_JOINT_INIT(void);
#endif
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

#ifdef MODC_ARM
  DXL_ARM_INIT();
  Debug.log(Level::LOG_INFO, "[main] Arm DXL ready\n");
#endif

#ifdef MODC_JOINT
  DXL_JOINT_INIT();
  Debug.log(Level::LOG_INFO, "[main] Joint DXL ready\n");
#endif

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

#ifdef MODC_ARM
/**
 * Initialise all 7 arm Dynamixel motors on USART2.
 * Must be called at startup and after REBOOT_ARM.
 * Mirrors PicoLowLevel MODC_ARM_INIT(): enables DE mode, configures sync group,
 * drive modes, extended-position operating mode, profiles, then enables torque.
 */
static void DXL_ARM_INIT(void)
{
    // Enable RS-485 DE mode on USART2 (idempotent — traction already called begin())
    ARM_dxl.begin();
    ARM_mot_1a.begin();
    ARM_mot_1b.begin();
    ARM_mot_2.begin();
    ARM_mot_3.begin();
    ARM_mot_4.begin();
    ARM_mot_5.begin();
    ARM_mot_6.begin();

    // Disable torque for safe reconfiguration
    ARM_mot_1a.setTorqueEnable(false);
    ARM_mot_1b.setTorqueEnable(false);
    ARM_mot_2.setTorqueEnable(false);
    ARM_mot_3.setTorqueEnable(false);
    ARM_mot_4.setTorqueEnable(false);
    ARM_mot_5.setTorqueEnable(false);
    ARM_mot_6.setTorqueEnable(false);
    HAL_Delay(10U);

    // Status return level 2 — respond to all instructions
    ARM_mot_1a.setStatusReturnLevel(2U);
    ARM_mot_1b.setStatusReturnLevel(2U);
    ARM_mot_2.setStatusReturnLevel(2U);
    ARM_mot_3.setStatusReturnLevel(2U);
    ARM_mot_4.setStatusReturnLevel(2U);
    ARM_mot_5.setStatusReturnLevel(2U);
    ARM_mot_6.setStatusReturnLevel(2U);
    HAL_Delay(10U);

    // Register J1a/J1b as sync group for differential control
    ARM_dxl.enableSync(arm_ids, sizeof(arm_ids));

    // Drive mode: all arms — normal mode (no reverseMode, no time-based profile)
    ARM_mot_1a.setDriveMode(false, false, false);
    ARM_mot_1b.setDriveMode(false, false, false);
    ARM_mot_2.setDriveMode(false, false, false);
    ARM_mot_3.setDriveMode(false, false, false);
    ARM_mot_4.setDriveMode(false, false, false);
    ARM_mot_5.setDriveMode(false, false, false);
    ARM_mot_6.setDriveMode(false, false, false);

    // Operating mode 4 = Extended Position Control Mode
    ARM_dxl.setOperatingMode(4U);
    ARM_mot_2.setOperatingMode(4U);
    ARM_mot_3.setOperatingMode(4U);
    ARM_mot_4.setOperatingMode(4U);
    ARM_mot_5.setOperatingMode(4U);
    ARM_mot_6.setOperatingMode(4U);
    HAL_Delay(10U);

    // Smooth motion profiles
    ARM_mot_1a.setProfileVelocity(ARM_PROFILE_VELOCITY);
    ARM_mot_1a.setProfileAcceleration(ARM_PROFILE_ACCELERATION);
    ARM_mot_1b.setProfileVelocity(ARM_PROFILE_VELOCITY);
    ARM_mot_1b.setProfileAcceleration(ARM_PROFILE_ACCELERATION);
    ARM_mot_2.setProfileVelocity(ARM_PROFILE_VELOCITY);
    ARM_mot_2.setProfileAcceleration(ARM_PROFILE_ACCELERATION);
    ARM_mot_3.setProfileVelocity(ARM_PROFILE_VELOCITY);
    ARM_mot_3.setProfileAcceleration(ARM_PROFILE_ACCELERATION);
    ARM_mot_4.setProfileVelocity(ARM_PROFILE_VELOCITY);
    ARM_mot_4.setProfileAcceleration(ARM_PROFILE_ACCELERATION);
    ARM_mot_5.setProfileVelocity(ARM_PROFILE_VELOCITY);
    ARM_mot_5.setProfileAcceleration(ARM_PROFILE_ACCELERATION);
    ARM_mot_6.setProfileVelocity(ARM_PROFILE_VELOCITY);
    ARM_mot_6.setProfileAcceleration(ARM_PROFILE_ACCELERATION);
    HAL_Delay(10U);

    // Read current positions before enabling torque to prevent violent startup motion
    int32_t cur_1LR[2];
    int32_t cur_2, cur_3, cur_4, cur_5, cur_6;
    bool ok =
        ARM_dxl.getPresentPosition(cur_1LR) == 0 &&
        ARM_mot_2.getPresentPosition(cur_2) == 0 &&
        ARM_mot_3.getPresentPosition(cur_3) == 0 &&
        ARM_mot_4.getPresentPosition(cur_4) == 0 &&
        ARM_mot_5.getPresentPosition(cur_5) == 0 &&
        ARM_mot_6.getPresentPosition(cur_6) == 0;

    if (!ok) {
        Debug.log(Level::LOG_WARN, "[ARM_INIT] Position read failed — torque not enabled\n");
        return;
    }

    // Pre-load goal = current so motors hold position when torque enables
    ARM_dxl.setGoalPosition_EPCM(cur_1LR);
    ARM_mot_2.setGoalPosition_EPCM(cur_2);
    ARM_mot_3.setGoalPosition_EPCM(cur_3);
    ARM_mot_4.setGoalPosition_EPCM(cur_4);
    ARM_mot_5.setGoalPosition_EPCM(cur_5);
    ARM_mot_6.setGoalPosition_EPCM(cur_6);
    HAL_Delay(10U);

    // Enable torque
    ARM_mot_1a.setTorqueEnable(true);
    ARM_mot_1b.setTorqueEnable(true);
    ARM_mot_2.setTorqueEnable(true);
    ARM_mot_3.setTorqueEnable(true);
    ARM_mot_4.setTorqueEnable(true);
    ARM_mot_5.setTorqueEnable(true);
    ARM_mot_6.setTorqueEnable(true);

    // Reset home positions to compiled defaults
    // TODO (Issue #14): load from Flash if a valid saved home exists
    const int32_t defaults[] = ARM_DEFAULT_HOME;
    ARM_pos0_mot_1LR[0] = defaults[0];
    ARM_pos0_mot_1LR[1] = defaults[1];
    ARM_pos0_mot_2  = defaults[2];
    ARM_pos0_mot_3  = defaults[3];
    ARM_pos0_mot_4  = defaults[4];
    ARM_pos0_mot_5  = defaults[5];
    ARM_pos0_mot_6  = defaults[6];

    Debug.log(Level::LOG_INFO, "[ARM_INIT] Arm DXL initialised\n");
}
#endif // MODC_ARM

#ifdef MODC_JOINT
/**
 * Initialise all 3 joint Dynamixel motors on USART3.
 * Must be called at startup. Mirrors MODC_ARM_INIT() pattern.
 */
static void DXL_JOINT_INIT(void)
{
    JOINT_dxl.begin();
    JOINT_mot_1L.begin();
    JOINT_mot_1R.begin();
    JOINT_mot_2.begin();

    JOINT_mot_1L.setTorqueEnable(false);
    JOINT_mot_1R.setTorqueEnable(false);
    JOINT_mot_2.setTorqueEnable(false);
    HAL_Delay(10U);

    JOINT_mot_1L.setStatusReturnLevel(2U);
    JOINT_mot_1R.setStatusReturnLevel(2U);
    JOINT_mot_2.setStatusReturnLevel(2U);
    HAL_Delay(10U);

    JOINT_dxl.enableSync(joint_ids, sizeof(joint_ids));

    JOINT_mot_1L.setDriveMode(false, false, false);
    JOINT_mot_1R.setDriveMode(false, false, false);
    JOINT_mot_2.setDriveMode(false, false, false);

    JOINT_mot_1L.setOperatingMode(4U);
    JOINT_mot_1R.setOperatingMode(4U);
    JOINT_mot_2.setOperatingMode(4U);
    HAL_Delay(10U);

    JOINT_mot_1L.setProfileVelocity(ARM_PROFILE_VELOCITY);
    JOINT_mot_1L.setProfileAcceleration(ARM_PROFILE_ACCELERATION);
    JOINT_mot_1R.setProfileVelocity(ARM_PROFILE_VELOCITY);
    JOINT_mot_1R.setProfileAcceleration(ARM_PROFILE_ACCELERATION);
    JOINT_mot_2.setProfileVelocity(ARM_PROFILE_VELOCITY);
    JOINT_mot_2.setProfileAcceleration(ARM_PROFILE_ACCELERATION);
    HAL_Delay(10U);

    int32_t cur_1LR[2];
    int32_t cur_2;
    bool ok =
        JOINT_dxl.getPresentPosition(cur_1LR) == 0 &&
        JOINT_mot_2.getPresentPosition(cur_2) == 0;

    if (!ok) {
        Debug.log(Level::LOG_WARN, "[JOINT_INIT] Position read failed — torque not enabled\n");
        return;
    }

    JOINT_dxl.setGoalPosition_EPCM(cur_1LR);
    JOINT_mot_2.setGoalPosition_EPCM(cur_2);
    HAL_Delay(10U);

    JOINT_mot_1L.setTorqueEnable(true);
    JOINT_mot_1R.setTorqueEnable(true);
    JOINT_mot_2.setTorqueEnable(true);

    // Reset home positions to zero
    JOINT_pos0_mot_1LR[0] = 0;
    JOINT_pos0_mot_1LR[1] = 0;
    JOINT_pos0_mot_2      = 0;

    Debug.log(Level::LOG_INFO, "[JOINT_INIT] Joint DXL initialised\n");
}
#endif // MODC_JOINT

/**
 * Send telemetry CAN frames at DT_TEL cadence.
 */
static void sendFeedback(void)
{
    // --- Traction (all modules) ---
    float speed_fb[2] = {0.0f, 0.0f};
    dxl_traction.getPresentVelocity_RPM(speed_fb);
    uint8_t traction_frame[8];
    memcpy(&traction_frame[0], &speed_fb[0], 4U);   // [0] = right (traction_ids[0] = 212)
    memcpy(&traction_frame[4], &speed_fb[1], 4U);   // [1] = left  (traction_ids[1] = 114)
    canW.sendMessage(MOTOR_FEEDBACK, traction_frame, 8U);

    uint8_t err_traction[2] = {0U, 0U};
    mot_right.getHardwareErrorStatus(err_traction[0]);
    mot_left.getHardwareErrorStatus(err_traction[1]);
    canW.sendMessage(MOTOR_TRACTION_ERROR_STATUS, err_traction, 2U);

#ifdef MODC_YAW
    float yaw_angle = encoderYaw.readAngle();
    canW.sendMessage(JOINT_YAW_FEEDBACK, &yaw_angle, 4U);
#endif

#ifdef MODC_ARM
    static constexpr float RPM_TO_RADS = 2.0f * 3.14159265f / 60.0f;

    // Position feedback — delta from home, converted to radians
    int32_t posf_1a1b[2];
    ARM_dxl.getPresentPosition(posf_1a1b);
    // Mirror PicoLowLevel convention: slot[0]=phi(negated), slot[1]=theta
    float arm_phi   = -(float)(((posf_1a1b[0] - ARM_pos0_mot_1LR[0]) + (posf_1a1b[1] - ARM_pos0_mot_1LR[1])) / 2.0f) * DXL_TO_RAD;
    float arm_theta = (float)(((posf_1a1b[1] - ARM_pos0_mot_1LR[1]) - (posf_1a1b[0] - ARM_pos0_mot_1LR[0])) / 2.0f) * DXL_TO_RAD;
    float arm_1a1b_fb[2] = {arm_theta, arm_phi};
    canW.sendMessage(ARM_PITCH_1a1b_FEEDBACK, arm_1a1b_fb, 8U);

    int32_t posf; float posf_rad;
    ARM_mot_2.getPresentPosition(posf);
    posf_rad = (float)(posf - ARM_pos0_mot_2) * DXL_TO_RAD;
    canW.sendMessage(ARM_PITCH_2_FEEDBACK, &posf_rad, 4U);

    ARM_mot_3.getPresentPosition(posf);
    posf_rad = (float)(posf - ARM_pos0_mot_3) * DXL_TO_RAD;
    canW.sendMessage(ARM_ROLL_3_FEEDBACK, &posf_rad, 4U);

    ARM_mot_4.getPresentPosition(posf);
    posf_rad = (float)(posf - ARM_pos0_mot_4) * DXL_TO_RAD;
    canW.sendMessage(ARM_PITCH_4_FEEDBACK, &posf_rad, 4U);

    ARM_mot_5.getPresentPosition(posf);
    posf_rad = (float)(posf - ARM_pos0_mot_5) * DXL_TO_RAD;
    canW.sendMessage(ARM_ROLL_5_FEEDBACK, &posf_rad, 4U);

    ARM_mot_6.getPresentPosition(posf);
    posf_rad = (float)(posf - ARM_pos0_mot_6) * DXL_TO_RAD;
    canW.sendMessage(ARM_ROLL_6_FEEDBACK, &posf_rad, 4U);

    // Velocity feedback (RPM → rad/s), same differential convention as position
    float vel_1a1b[2] = {0.0f, 0.0f};
    ARM_dxl.getPresentVelocity_RPM(vel_1a1b);
    float arm_phi_vel   = -(vel_1a1b[0] + vel_1a1b[1]) * RPM_TO_RADS;
    float arm_theta_vel =  (vel_1a1b[0] - vel_1a1b[1]) * RPM_TO_RADS;
    float arm_1a1b_vel[2] = {arm_theta_vel, arm_phi_vel};
    canW.sendMessage(ARM_PITCH_1a1b_FEEDBACK_VEL, arm_1a1b_vel, 8U);

    float vel;
    ARM_mot_2.getPresentVelocity_RPM(vel); vel *= RPM_TO_RADS;
    canW.sendMessage(ARM_PITCH_2_FEEDBACK_VEL, &vel, 4U);

    ARM_mot_3.getPresentVelocity_RPM(vel); vel *= RPM_TO_RADS;
    canW.sendMessage(ARM_ROLL_3_FEEDBACK_VEL, &vel, 4U);

    ARM_mot_4.getPresentVelocity_RPM(vel); vel *= RPM_TO_RADS;
    canW.sendMessage(ARM_PITCH_4_FEEDBACK_VEL, &vel, 4U);

    ARM_mot_5.getPresentVelocity_RPM(vel); vel *= RPM_TO_RADS;
    canW.sendMessage(ARM_ROLL_5_FEEDBACK_VEL, &vel, 4U);

    ARM_mot_6.getPresentVelocity_RPM(vel); vel *= RPM_TO_RADS;
    canW.sendMessage(ARM_ROLL_6_FEEDBACK_VEL, &vel, 4U);

    // Hardware error status
    uint8_t err_arm[7] = {0U, 0U, 0U, 0U, 0U, 0U, 0U};
    ARM_mot_1a.getHardwareErrorStatus(err_arm[0]);
    ARM_mot_1b.getHardwareErrorStatus(err_arm[1]);
    ARM_mot_2.getHardwareErrorStatus(err_arm[2]);
    ARM_mot_3.getHardwareErrorStatus(err_arm[3]);
    ARM_mot_4.getHardwareErrorStatus(err_arm[4]);
    ARM_mot_5.getHardwareErrorStatus(err_arm[5]);
    ARM_mot_6.getHardwareErrorStatus(err_arm[6]);
    canW.sendMessage(MOTOR_ARM_ERROR_STATUS, err_arm, 7U);
#endif // MODC_ARM

#ifdef MODC_IMU
    imu.update();
    float imu_roll  = imu.getRoll();
    float imu_pitch = imu.getPitch();
    canW.sendMessage(JOINT_ROLL_FEEDBACK, &imu_roll, 4U);
    canW.sendMessage(JOINT_PITCH_FEEDBACK, &imu_pitch, 4U);
#endif // MODC_IMU

#ifdef MODC_JOINT
    int32_t joint_posf_1a1b[2];
    JOINT_dxl.getPresentPosition(joint_posf_1a1b);
    float joint_theta = ((float)((joint_posf_1a1b[0] - JOINT_pos0_mot_1LR[0]) + (joint_posf_1a1b[1] - JOINT_pos0_mot_1LR[1])) / 2.0f) * DXL_TO_RAD;
    float joint_phi   = ((float)((joint_posf_1a1b[0] - JOINT_pos0_mot_1LR[0]) - (joint_posf_1a1b[1] - JOINT_pos0_mot_1LR[1])) / 2.0f) * DXL_TO_RAD;
    float joint_1a1b_fb[2] = {joint_theta, joint_phi};
    canW.sendMessage(JOINT_PITCH_1a1b_FEEDBACK, joint_1a1b_fb, 8U);

    int32_t joint_posf_2;
    JOINT_mot_2.getPresentPosition(joint_posf_2);
    float joint_posf_2_rad = (float)(joint_posf_2 - JOINT_pos0_mot_2) * DXL_TO_RAD;
    canW.sendMessage(JOINT_ROLL_2_FEEDBACK, &joint_posf_2_rad, 4U);
#endif // MODC_JOINT
}

/**
 * Dispatch incoming CAN setpoint message. Populated fully in Issue #11.
 * @param msg_id   CAN message type identifier (bits[23:16] of extended ID).
 * @param msg_data 8-byte payload.
 */
static void handleSetpoint(uint8_t msg_id, const uint8_t *msg_data)
{
    switch (msg_id)
    {
    // Traction motors — all modules
    case MOTOR_SETPOINT:
    {
        // Payload layout matches PicoLowLevel: bytes[0:3]=right RPM, bytes[4:7]=left RPM.
        // speeds_dxl[0]=left, speeds_dxl[1]=right (matches traction sync-write order).
        memcpy(&speeds_dxl[0], msg_data,     4);   // right → index 0 → motor 212
        memcpy(&speeds_dxl[1], msg_data + 4, 4);   // left  → index 1 → motor 114

        float coeff = ((speeds_dxl[0] + speeds_dxl[1]) < 0.0f)
            ? TRACTION_VELOCITY_COEFF_REV
            : TRACTION_VELOCITY_COEFF;
        speeds_dxl[0] *= coeff;
        speeds_dxl[1] *= coeff;

        float goal[2] = {speeds_dxl[0], speeds_dxl[1]};
        dxl_traction.setGoalVelocity_RPM(goal);
        Debug.log(Level::LOG_DEBUG, "[CAN] MOTOR_SETPOINT: L=%.1f R=%.1f RPM\n",
                  speeds_dxl[0], speeds_dxl[1]);
        break;
    }

#ifdef MODC_ARM // Robotic arm — MODC_ARM modules only
    case ARM_PITCH_1a1b_SETPOINT:
    {
        float theta, phi;
        memcpy(&theta, msg_data,     4);
        memcpy(&phi,   msg_data + 4, 4);

        ARM_pos_mot_1LR[0] = (int32_t)((theta * RAD_TO_DXL) - (phi * RAD_TO_DXL)) + ARM_pos0_mot_1LR[0];
        ARM_pos_mot_1LR[1] = (int32_t)((theta * RAD_TO_DXL) + (phi * RAD_TO_DXL)) + ARM_pos0_mot_1LR[1];

        if ((abs(ARM_pos_mot_1LR[0] - ARM_old_pos_mot_1LR[0]) > ARM_DE_CAN_DXL) ||
            (abs(ARM_pos_mot_1LR[1] - ARM_old_pos_mot_1LR[1]) > ARM_DE_CAN_DXL))
        {
            ARM_dxl.setGoalPosition_EPCM(ARM_pos_mot_1LR);
            ARM_old_pos_mot_1LR[0] = ARM_pos_mot_1LR[0];
            ARM_old_pos_mot_1LR[1] = ARM_pos_mot_1LR[1];
        }
        break;
    }

    case ARM_PITCH_2_SETPOINT:
    {
        float val;
        memcpy(&val, msg_data, 4);
        ARM_pos_mot_2 = (int32_t)(val * RAD_TO_DXL) + ARM_pos0_mot_2;
        if (abs(ARM_pos_mot_2 - ARM_old_pos_mot_2) > ARM_DE_CAN_DXL)
        {
            ARM_mot_2.setGoalPosition_EPCM(ARM_pos_mot_2);
            ARM_old_pos_mot_2 = ARM_pos_mot_2;
        }
        break;
    }

    case ARM_ROLL_3_SETPOINT:
    {
        float val;
        memcpy(&val, msg_data, 4);
        ARM_pos_mot_3 = (int32_t)(val * RAD_TO_DXL) + ARM_pos0_mot_3;
        if (abs(ARM_pos_mot_3 - ARM_old_pos_mot_3) > ARM_DE_CAN_DXL)
        {
            ARM_mot_3.setGoalPosition_EPCM(ARM_pos_mot_3);
            ARM_old_pos_mot_3 = ARM_pos_mot_3;
        }
        break;
    }

    case ARM_PITCH_4_SETPOINT:
    {
        float val;
        memcpy(&val, msg_data, 4);
        ARM_pos_mot_4 = (int32_t)(val * RAD_TO_DXL) + ARM_pos0_mot_4;
        if (abs(ARM_pos_mot_4 - ARM_old_pos_mot_4) > ARM_DE_CAN_DXL)
        {
            ARM_mot_4.setGoalPosition_EPCM(ARM_pos_mot_4);
            ARM_old_pos_mot_4 = ARM_pos_mot_4;
        }
        break;
    }

    case ARM_ROLL_5_SETPOINT:
    {
        float val;
        memcpy(&val, msg_data, 4);
        // J5 is mounted inverted — negate offset
        ARM_pos_mot_5 = ARM_pos0_mot_5 - (int32_t)(val * RAD_TO_DXL);
        if (abs(ARM_pos_mot_5 - ARM_old_pos_mot_5) > ARM_DE_CAN_DXL)
        {
            ARM_mot_5.setGoalPosition_EPCM(ARM_pos_mot_5);
            ARM_old_pos_mot_5 = ARM_pos_mot_5;
        }
        break;
    }

    case ARM_ROLL_6_SETPOINT:
    {
        int32_t cmd;
        memcpy(&cmd, msg_data, 4);
        if (cmd == 0)
        {
            ARM_mot_6.setGoalPWM(BEAK_FULL_PWM);
            ARM_mot_6.setGoalPosition_EPCM(BEAK_POS_CLOSE);
            beak_motion_start_ms = HAL_GetTick();
            beak_state = BeakState::CLOSING;
        }
        else if (cmd == 1)
        {
            ARM_mot_6.setGoalPWM(BEAK_FULL_PWM);
            ARM_mot_6.setGoalPosition_EPCM(BEAK_POS_OPEN);
            beak_motion_start_ms = HAL_GetTick();
            beak_state = BeakState::OPENING;
        }
        break;
    }

    case RESET_ARM:
    {
        // Move all joints to home position
        ARM_dxl.setGoalPosition_EPCM(ARM_pos0_mot_1LR);
        ARM_mot_2.setGoalPosition_EPCM(ARM_pos0_mot_2);
        ARM_mot_3.setGoalPosition_EPCM(ARM_pos0_mot_3);
        ARM_mot_4.setGoalPosition_EPCM(ARM_pos0_mot_4);
        ARM_mot_5.setGoalPosition_EPCM(ARM_pos0_mot_5);
        ARM_mot_6.setGoalPosition_EPCM(ARM_pos0_mot_6);
        ARM_old_pos_mot_1LR[0] = ARM_pos0_mot_1LR[0];
        ARM_old_pos_mot_1LR[1] = ARM_pos0_mot_1LR[1];
        ARM_old_pos_mot_2 = ARM_pos0_mot_2;
        ARM_old_pos_mot_3 = ARM_pos0_mot_3;
        ARM_old_pos_mot_4 = ARM_pos0_mot_4;
        ARM_old_pos_mot_5 = ARM_pos0_mot_5;
        ARM_old_pos_mot_6 = ARM_pos0_mot_6;
        Debug.log(Level::LOG_INFO, "[CAN] RESET_ARM: moving to home\n");
        break;
    }

    case REBOOT_ARM:
    {
        ARM_mot_1a.reboot();
        ARM_mot_1b.reboot();
        ARM_mot_2.reboot();
        ARM_mot_3.reboot();
        ARM_mot_4.reboot();
        ARM_mot_5.reboot();
        ARM_mot_6.reboot();
        HAL_Delay(2000U);   // Wait for motors to come back online (~1.5 s typical)
        DXL_ARM_INIT();
        Debug.log(Level::LOG_INFO, "[CAN] REBOOT_ARM: all arm motors rebooted and reinitialised\n");
        break;
    }

    case SET_HOME:
    {
        // msg_data[0]: 0 = session only, 1 = persist to Flash
        ARM_dxl.getPresentPosition(ARM_pos0_mot_1LR);
        ARM_mot_2.getPresentPosition(ARM_pos0_mot_2);
        ARM_mot_3.getPresentPosition(ARM_pos0_mot_3);
        ARM_mot_4.getPresentPosition(ARM_pos0_mot_4);
        ARM_mot_5.getPresentPosition(ARM_pos0_mot_5);
        ARM_mot_6.getPresentPosition(ARM_pos0_mot_6);

        ARM_old_pos_mot_1LR[0] = ARM_pos0_mot_1LR[0];
        ARM_old_pos_mot_1LR[1] = ARM_pos0_mot_1LR[1];
        ARM_old_pos_mot_2 = ARM_pos0_mot_2;
        ARM_old_pos_mot_3 = ARM_pos0_mot_3;
        ARM_old_pos_mot_4 = ARM_pos0_mot_4;
        ARM_old_pos_mot_5 = ARM_pos0_mot_5;
        ARM_old_pos_mot_6 = ARM_pos0_mot_6;  // beak home also reset

        if (msg_data[0] == 1U)
        {
            // TODO (Issue #14): persist home positions to Flash
            Debug.log(Level::LOG_INFO, "[CAN] SET_HOME: Flash persistence pending Issue #14\n");
        }
        else
        {
            Debug.log(Level::LOG_INFO, "[CAN] SET_HOME: session home updated\n");
        }
        break;
    }
#endif // MODC_ARM


#ifdef MODC_JOINT // Inter-module joint — MODC_JOINT modules only
    case JOINT_PITCH_1a1b_SETPOINT:
    {
        float theta, phi;
        memcpy(&theta, msg_data,     4);
        memcpy(&phi,   msg_data + 4, 4);

        JOINT_pos_mot_1LR[0] = (int32_t)((theta * RAD_TO_DXL) + (phi * RAD_TO_DXL)) + JOINT_pos0_mot_1LR[0];
        JOINT_pos_mot_1LR[1] = (int32_t)((theta * RAD_TO_DXL) - (phi * RAD_TO_DXL)) + JOINT_pos0_mot_1LR[1];

        JOINT_dxl.setGoalPosition_EPCM(JOINT_pos_mot_1LR);
        JOINT_old_pos_mot_1LR[0] = JOINT_pos_mot_1LR[0];
        JOINT_old_pos_mot_1LR[1] = JOINT_pos_mot_1LR[1];
        break;
    }

    case JOINT_ROLL_2_SETPOINT:
    {
        float val;
        memcpy(&val, msg_data, 4);
        JOINT_pos_mot_2 = JOINT_pos0_mot_2 + (int32_t)(val * RAD_TO_DXL);
        JOINT_mot_2.setGoalPosition_EPCM(JOINT_pos_mot_2);
        break;
    }
#endif // MODC_JOINT

    // Traction reboot — all modules
    case MOTOR_TRACTION_REBOOT:
        mot_left.reboot();
        mot_right.reboot();
        HAL_Delay(2000U);   // Wait for motors to come back online (~1.5 s typical)
        DXL_TRACTION_INIT();
        Debug.log(Level::LOG_INFO, "[CAN] MOTOR_TRACTION_REBOOT: traction motors rebooted\n");
        break;

    default:
        Debug.log(Level::LOG_DEBUG, "[CAN] Unknown msg_id: 0x%02X\n", msg_id);
        break;
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
