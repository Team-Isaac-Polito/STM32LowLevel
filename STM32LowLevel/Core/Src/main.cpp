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

#include <cstring> // memcpy
#include <cstdlib> // abs (integer)

/* USB Device CDC */
#include "usbd_cdc_if.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

// State machine for the beak/gripper motor (MODC_ARM only).
enum class BeakState : uint8_t
{
    IDLE,
    CLOSING,
    OPENING,
    HOLDING
};

// LED identifiers
enum class Led : uint8_t
{
    DXL = 0,
    CANBUS = 1,
    USR = 2,
    POWER = 3
};

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
extern SerialDebug debug;

// Traction motors (all modules)
static DynamixelLL dxlTraction(USART2, 0); // sync/broadcast handle
static DynamixelLL motLeft(USART2, SERVO_TRACTION_L_ID);
static DynamixelLL motRight(USART2, SERVO_TRACTION_R_ID);
static const uint8_t tractionIds[] = {SERVO_TRACTION_R_ID, SERVO_TRACTION_L_ID};
static float speedsDxl[2] = {0.0f, 0.0f};

// Battery
static Battery battery;

// Health / timing
static uint32_t timeBat = 0U;
static uint32_t timeTel = 0U;
static uint32_t timeData = 0U;
static bool canActive = false;

// LED state
static uint32_t ledCanLastToggle = 0U; // debounce timestamp for LED_CAN

// Module-specific peripherals
#ifdef MODC_YAW
static AbsoluteEncoder encoderYaw(ABSOLUTE_ENCODER_ADDRESS);
#endif

#ifdef MODC_IMU
static IMU imu;
#endif

#ifdef MODC_ARM
// Arm Dynamixel motors (all on USART2 bus, position control)
static DynamixelLL armDxl(USART2, 0); // sync handle for J1a/J1b
static DynamixelLL armMot1a(USART2, SERVO_ARM_1a_PITCH_ID);
static DynamixelLL armMot1b(USART2, SERVO_ARM_1b_PITCH_ID);
static DynamixelLL armMot2(USART2, SERVO_ARM_2_PITCH_ID);
static DynamixelLL armMot3(USART2, SERVO_ARM_3_ROLL_ID);
static DynamixelLL armMot4(USART2, SERVO_ARM_4_PITCH_ID);
static DynamixelLL armMot5(USART2, SERVO_ARM_5_ROLL_ID);
static DynamixelLL armMot6(USART2, SERVO_ARM_6_BEAK_ID);
static const uint8_t armIds[] = {SERVO_ARM_1a_PITCH_ID, SERVO_ARM_1b_PITCH_ID};

// Arm home positions (DXL ext-pos units) — default from definitions.h ARM_DEFAULT_HOME
static const int32_t armDefaults[] = ARM_DEFAULT_HOME;
static int32_t armPos0Mot1Lr[2] = {armDefaults[0], armDefaults[1]};
static int32_t armPos0Mot2 = armDefaults[2];
static int32_t armPos0Mot3 = armDefaults[3];
static int32_t armPos0Mot4 = armDefaults[4];
static int32_t armPos0Mot5 = armDefaults[5];
static int32_t armPos0Mot6 = armDefaults[6];

// Arm live setpoint state
static int32_t armPosMot1Lr[2] = {0, 0};
static int32_t armOldPosMot1Lr[2] = {0, 0};
static int32_t armPosMot2 = 0, armOldPosMot2 = 0;
static int32_t armPosMot3 = 0, armOldPosMot3 = 0;
static int32_t armPosMot4 = 0, armOldPosMot4 = 0;
static int32_t armPosMot5 = 0, armOldPosMot5 = 0;

// Beak gripper state machine
static BeakState beakState = BeakState::IDLE;
static uint32_t beakMotionStartMs = 0U;
static uint32_t beakTempCheckMs = 0U; ///< Timestamp of last thermal check in HOLDING
#endif

#ifdef MODC_JOINT
// Joint Dynamixel motors (USART3 bus)
static DynamixelLL jointDxl(USART3, 0); // sync handle for pitch 1a/1b
static DynamixelLL jointMot1L(USART3, SERVO_JOINT_LEFT_ID);
static DynamixelLL jointMot1R(USART3, SERVO_JOINT_RIGHT_ID);
static DynamixelLL jointMot2(USART3, SERVO_JOINT_ROLL_ID);
static const uint8_t jointIds[] = {SERVO_JOINT_LEFT_ID, SERVO_JOINT_RIGHT_ID};

// Joint home positions
static int32_t jointPos0Mot1Lr[2] = {0, 0};
static int32_t jointPos0Mot2 = 0;

// Joint live setpoint state
static int32_t jointPosMot1Lr[2] = {0, 0};
static int32_t jointOldPosMot1Lr[2] = {0, 0};
static int32_t jointPosMot2 = 0;
#endif

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
extern "C" void systemClockConfig(void);
extern "C" int main(void);
/* USER CODE BEGIN PFP */
static void sendFeedback(void);
static void handleSetpoint(uint8_t msgId, const uint8_t* msgData);
static void dxlTractionInit(void);
#ifdef MODC_ARM
static void dxlArmInit(void);
static bool loadHomePositions(void);
static bool saveHomePositions(void);
static void tickBeakStateMachine(uint32_t now);
#endif
#ifdef MODC_JOINT
static void DXL_JOINT_INIT(void);
#endif
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static inline void ledSet(Led led, bool state)
{
    GPIO_TypeDef* port;
    uint32_t pin;
    switch (led)
    {
        case Led::DXL:
            port = LED_DXL_GPIO_Port;
            pin = LED_DXL_Pin;
            break;
        case Led::CANBUS:
            port = LED_CAN_GPIO_Port;
            pin = LED_CAN_Pin;
            break;
        case Led::USR:
            port = LED_USR_GPIO_Port;
            pin = LED_USR_Pin;
            break;
        case Led::POWER:
            port = LED_PWR_GPIO_Port;
            pin = LED_PWR_Pin;
            break;
        default:
            return;
    }
    if (state)
        LL_GPIO_SetOutputPin(port, pin);
    else
        LL_GPIO_ResetOutputPin(port, pin);
}

static inline void ledToggle(Led led)
{
    GPIO_TypeDef* port;
    uint32_t pin;
    switch (led)
    {
        case Led::DXL:
            port = LED_DXL_GPIO_Port;
            pin = LED_DXL_Pin;
            break;
        case Led::CANBUS:
            port = LED_CAN_GPIO_Port;
            pin = LED_CAN_Pin;
            break;
        case Led::USR:
            port = LED_USR_GPIO_Port;
            pin = LED_USR_Pin;
            break;
        case Led::POWER:
            port = LED_PWR_GPIO_Port;
            pin = LED_PWR_Pin;
            break;
        default:
            return;
    }
    LL_GPIO_TogglePin(port, pin);
}

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
    systemClockConfig();

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

    /* USB Device CDC init (must come after MX_USB_PCD_Init) */
    MX_USB_Device_Init();

    /* USER CODE BEGIN 2 */

    /* Debug-mode wait: if this boot was triggered by a DTR auto-reset
     * (SFTRSTF flag set), blink the USR LED and wait for the USB host
     * to reconnect before proceeding with the rest of init. This ensures
     * the serial monitor catches the full boot sequence from the start.
     * On power-on reset (SFTRSTF=0), skip this and boot normally. */
    if (RCC->CSR & RCC_CSR_SFTRSTF)
    {
        /* Blink USR LED while waiting for host */
        RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN;
        GPIOC->MODER &= ~(3U << (2U * 2U));
        GPIOC->MODER |= (1U << (2U * 2U));

        while (!CDC_IsConnected())
        {
            GPIOC->BSRR = (1U << 2U);
            for (volatile int d = 0; d < 50000; d++) {}
            GPIOC->BSRR = (1U << (2U + 16U));
            for (volatile int d = 0; d < 50000; d++) {}
        }

        /* Host connected — turn off LED and proceed */
        GPIOC->BSRR = (1U << (2U + 16U));
    }

    // Debug — must be first so all subsequent prints reach USB CDC
    debug.setLevel(Level::LogDebug);
    debug.log(Level::LogInfo, "[main] Debug ready\n");

    // LEDs — power-on indicator and DXL activity hook
    ledSet(Led::POWER, true);
    DynamixelLL::setActivityCallback([]() {
        ledToggle(Led::DXL);
    });

    // CAN — begin() releases STBY, so must come after MX_FDCAN2_Init()
    canW.begin();
    debug.log(Level::LogInfo, "[main] CAN ready\n");

    // DXL traction — USART2 DE pin + Dynamixel boot sequence
    dxlTractionInit();
    debug.log(Level::LogInfo, "[main] Traction DXL ready\n");

#ifdef MODC_ARM
    dxlArmInit();
    debug.log(Level::LogInfo, "[main] Arm DXL ready\n");
#endif

#ifdef MODC_JOINT
    DXL_JOINT_INIT();
    debug.log(Level::LogInfo, "[main] Joint DXL ready\n");
#endif

    // 4a. I2C — Yaw encoder (shares I2C1 with IMU)
#ifdef MODC_YAW
    encoderYaw.setZero();
    debug.log(Level::LogInfo, "[main] Yaw encoder ready\n");
#endif

    // 4b. I2C — IMU
#ifdef MODC_IMU
    imu.begin(IMU_ADDRESS);
    if (!imu.checkID())
    {
        debug.log(Level::LogWarn, "[main] IMU not found at 0x%02X!\n", IMU_ADDRESS);
    }
    else
    {
        imu.enableGyro();
        imu.enableAccel();
        imu.calibrateAccel();
        imu.calibrateGyro();
        debug.log(Level::LogInfo, "[main] IMU ready\n");
    }
#endif

    // 5. ADC — Battery (ADC1 LL)
    battery.begin();
    debug.log(Level::LogInfo, "[main] Battery ready, %.2f V\n", battery.readVoltage());

    debug.log(Level::LogInfo, "[main] Init complete, entering main loop\n");

    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1)
    {
        uint32_t now = HAL_GetTick();

        // Health checks (1 Hz)
        if (now - timeBat >= DT_BAT)
        {
            timeBat = now;
            if (!battery.charged())
            {
                float v = battery.readVoltage();
                int vWhole = (int)v;
                int vFrac = (int)((v - (float)vWhole) * 100.0f);
                if (vFrac < 0)
                    vFrac = -vFrac;
                debug.log(Level::LogWarn, "[main] Low battery: %d.%02d V\n", vWhole, vFrac);
            }
        }

        // Telemetry / feedback (25 Hz)
        if (now - timeTel >= DT_TEL)
        {
            timeTel = now;
            sendFeedback();
        }

        // Receive CAN setpoint
        uint8_t msgId;
        uint8_t msgData[8];
        if (canW.readMessage(&msgId, msgData))
        {
            timeData = now;
            canActive = true;
            handleSetpoint(msgId, msgData);
            // LED_CAN activity blink — debounced so individual frames stay visible
            if (now - ledCanLastToggle >= 100U)
            {
                ledToggle(Led::CANBUS);
                ledCanLastToggle = now;
            }
        }
        else if (canActive && (now - timeData > CAN_TIMEOUT))
        {
            // CAN silence timeout — stop all motors
            canActive = false;
            speedsDxl[0] = 0.0f;
            speedsDxl[1] = 0.0f;
            dxlTraction.setGoalVelocityRpm(speedsDxl);
            debug.log(Level::LogWarn, "[main] CAN timeout, motors stopped\n");
        }

#ifdef MODC_ARM
        // Beak gripper state machine
        tickBeakStateMachine(now);
#endif // MODC_ARM


        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */
    }
    /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
extern "C" void systemClockConfig(void)
{
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_4);
    while (LL_FLASH_GetLatency() != LL_FLASH_LATENCY_4)
    {}
    LL_PWR_EnableRange1BoostMode();
    LL_RCC_HSE_Enable();
    /* Wait till HSE is ready */
    while (LL_RCC_HSE_IsReady() != 1)
    {}

    LL_RCC_HSI48_Enable();
    /* Wait till HSI48 is ready */
    while (LL_RCC_HSI48_IsReady() != 1)
    {}

    LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSE, LL_RCC_PLLM_DIV_6, 85, LL_RCC_PLLR_DIV_2);
    LL_RCC_PLL_EnableDomain_SYS();
    LL_RCC_PLL_Enable();
    /* Wait till PLL is ready */
    while (LL_RCC_PLL_IsReady() != 1)
    {}

    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_2);
    /* Wait till System clock is ready */
    while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL)
    {}

    /* Insure 1us transition state at intermediate medium speed clock*/
    for (__IO uint32_t i = (170 >> 1); i != 0; i--)
        ;

    /* Set AHB prescaler*/
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
    LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_1);
    LL_SetSystemCoreClock(170000000);

    /* Update the time base */
    if (HAL_InitTick(TICK_INT_PRIORITY) != HAL_OK)
        Error_Handler();
}

/* USER CODE BEGIN 4 */

/**
 * Traction motor initialisation sequence (velocity control mode).
 * Must be called after MX_USART2_UART_Init() (DXL half-duplex on USART2).
 */
static void dxlTractionInit(void)
{
    static const uint8_t n = sizeof(tractionIds) / sizeof(tractionIds[0]);

    dxlTraction.begin();

    // Enable DXL debug on all handles for visibility
    dxlTraction.setDebug(true);
    motLeft.setDebug(true);
    motRight.setDebug(true);

    // Disable torque first for safe reconfiguration
    motLeft.setTorqueEnable(false);
    motRight.setTorqueEnable(false);

    // Status return level 2 — respond to all instructions
    motLeft.setStatusReturnLevel(2U);
    motRight.setStatusReturnLevel(2U);

    // Drive mode: right motor needs reverseMode=true (opposite mounting)
    motLeft.setDriveMode(false, false, false);
    motRight.setDriveMode(false, false, true);

    // Operating mode 1 = velocity control
    dxlTraction.enableSync(tractionIds, n);
    dxlTraction.setOperatingMode(1U);

    // Instant velocity response (profile acceleration = 0)
    dxlTraction.setProfileAcceleration(0U);

    // Enable torque — retry indefinitely until confirmed ON
    uint32_t retryCount = 0U;
    for (;;)
    {
        uint8_t errL = motLeft.setTorqueEnable(true);
        uint8_t errR = motRight.setTorqueEnable(true);
        if (errL == 0U && errR == 0U)
        {
            debug.log(Level::LogInfo, "[DXL] Traction init: torque ON\n");
            break;
        }
        retryCount++;
        if (retryCount % 10U == 0U)
        {
            debug.log(Level::LogWarn, "[TRACTION_INIT] Torque enable still failing (attempt %lu)\n",
                      (unsigned long)retryCount);
        }
        HAL_Delay(10U);
    }
}

#ifdef MODC_ARM
/**
 * Load arm home positions from the last Flash page (HOME_FLASH_PAGE_ADDR).
 * Returns true and overwrites ARM_pos0_* if the magic sentinel is valid;
 * returns false and leaves the compiled defaults untouched otherwise.
 */
static bool loadHomePositions(void)
{
    const uint32_t* p = reinterpret_cast<const uint32_t*>(HOME_FLASH_PAGE_ADDR);

    if (p[7] != HOME_FLASH_MAGIC)
    {
        debug.log(Level::LogInfo, "[Flash] No saved home — using compiled defaults\n");
        return false;
    }

    armPos0Mot1Lr[0] = static_cast<int32_t>(p[0]);
    armPos0Mot1Lr[1] = static_cast<int32_t>(p[1]);
    armPos0Mot2 = static_cast<int32_t>(p[2]);
    armPos0Mot3 = static_cast<int32_t>(p[3]);
    armPos0Mot4 = static_cast<int32_t>(p[4]);
    armPos0Mot5 = static_cast<int32_t>(p[5]);
    armPos0Mot6 = static_cast<int32_t>(p[6]);

    debug.log(Level::LogInfo, "[Flash] Home positions loaded from Flash\n");
    return true;
}

/**
 * Write arm home positions to the last Flash page (HOME_FLASH_PAGE_ADDR).
 * Sequence: unlock → page erase → 4 × DWORD writes → lock.
 */
static bool saveHomePositions(void)
{
    // Pack 7 home positions + sentinel into 8-word (32-byte) buffer
    uint32_t buf[8];
    buf[0] = static_cast<uint32_t>(armPos0Mot1Lr[0]);
    buf[1] = static_cast<uint32_t>(armPos0Mot1Lr[1]);
    buf[2] = static_cast<uint32_t>(armPos0Mot2);
    buf[3] = static_cast<uint32_t>(armPos0Mot3);
    buf[4] = static_cast<uint32_t>(armPos0Mot4);
    buf[5] = static_cast<uint32_t>(armPos0Mot5);
    buf[6] = static_cast<uint32_t>(armPos0Mot6);
    buf[7] = HOME_FLASH_MAGIC;

    HAL_FLASH_Unlock();

    // Erase the target page (Bank 2, page 127, 2 KB)
    FLASH_EraseInitTypeDef eraseInit;
    eraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
    eraseInit.Banks = HOME_FLASH_BANK;
    eraseInit.Page = HOME_FLASH_PAGE_NUM;
    eraseInit.NbPages = 1U;
    uint32_t pageError = 0U;

    if (HAL_FLASHEx_Erase(&eraseInit, &pageError) != HAL_OK)
    {
        HAL_FLASH_Lock();
        debug.log(Level::LogWarn, "[Flash] Erase failed (page error 0x%08lX)\n", pageError);
        return false;
    }

    // Write 4 DWORDs: each DWORD = two consecutive uint32_t values
    // Little-endian: lower 32 bits → addr+0, upper 32 bits → addr+4
    for (uint32_t i = 0U; i < 4U; i++)
    {
        uint64_t dword = static_cast<uint64_t>(buf[i * 2U]) | (static_cast<uint64_t>(buf[i * 2U + 1U]) << 32U);
        uint32_t addr = HOME_FLASH_PAGE_ADDR + (i * 8U);

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr, dword) != HAL_OK)
        {
            HAL_FLASH_Lock();
            debug.log(Level::LogWarn, "[Flash] Write failed at offset %lu\n", i * 8U);
            return false;
        }
    }

    HAL_FLASH_Lock();
    debug.log(Level::LogInfo, "[Flash] Home positions saved to Flash\n");
    return true;
}

/**
 * Initialise all 7 arm Dynamixel motors on USART2.
 * Must be called at startup and after REBOOT_ARM.
 * Enables DE mode, configures sync group, drive modes,
 * extended-position operating mode, profiles, then enables torque.
 */
static void dxlArmInit(void)
{
    // Verbose DXL debug logging for prototype testing
    armDxl.setDebug(true);
    armMot1a.setDebug(true);
    armMot1b.setDebug(true);
    armMot2.setDebug(true);
    armMot3.setDebug(true);
    armMot4.setDebug(true);
    armMot5.setDebug(true);
    armMot6.setDebug(true);

    // Disable torque for safe reconfiguration
    armMot1a.setTorqueEnable(false);
    armMot1b.setTorqueEnable(false);
    armMot2.setTorqueEnable(false);
    armMot3.setTorqueEnable(false);
    armMot4.setTorqueEnable(false);
    armMot5.setTorqueEnable(false);
    armMot6.setTorqueEnable(false);
    HAL_Delay(10U);

    // Status return level 2 — respond to all instructions
    armMot1a.setStatusReturnLevel(2U);
    armMot1b.setStatusReturnLevel(2U);
    armMot2.setStatusReturnLevel(2U);
    armMot3.setStatusReturnLevel(2U);
    armMot4.setStatusReturnLevel(2U);
    armMot5.setStatusReturnLevel(2U);
    armMot6.setStatusReturnLevel(2U);
    HAL_Delay(10U);

    // Register J1a/J1b as sync group for differential control
    armDxl.enableSync(armIds, sizeof(armIds));

    // Drive mode: all arms — normal mode (no reverseMode, no time-based profile)
    armMot1a.setDriveMode(false, false, false);
    armMot1b.setDriveMode(false, false, false);
    armMot2.setDriveMode(false, false, false);
    armMot3.setDriveMode(false, false, false);
    armMot4.setDriveMode(false, false, false);
    armMot5.setDriveMode(false, false, false);
    armMot6.setDriveMode(false, false, false);

    // Operating mode 4 = Extended Position Control Mode
    armDxl.setOperatingMode(4U);
    armMot2.setOperatingMode(4U);
    armMot3.setOperatingMode(4U);
    armMot4.setOperatingMode(4U);
    armMot5.setOperatingMode(4U);
    armMot6.setOperatingMode(4U);
    HAL_Delay(10U);

    // Smooth motion profiles
    armMot1a.setProfileVelocity(ARM_PROFILE_VELOCITY);
    armMot1a.setProfileAcceleration(ARM_PROFILE_ACCELERATION);
    armMot1b.setProfileVelocity(ARM_PROFILE_VELOCITY);
    armMot1b.setProfileAcceleration(ARM_PROFILE_ACCELERATION);
    armMot2.setProfileVelocity(ARM_PROFILE_VELOCITY);
    armMot2.setProfileAcceleration(ARM_PROFILE_ACCELERATION);
    armMot3.setProfileVelocity(ARM_PROFILE_VELOCITY);
    armMot3.setProfileAcceleration(ARM_PROFILE_ACCELERATION);
    armMot4.setProfileVelocity(ARM_PROFILE_VELOCITY);
    armMot4.setProfileAcceleration(ARM_PROFILE_ACCELERATION);
    armMot5.setProfileVelocity(ARM_PROFILE_VELOCITY);
    armMot5.setProfileAcceleration(ARM_PROFILE_ACCELERATION);
    armMot6.setProfileVelocity(ARM_PROFILE_VELOCITY);
    armMot6.setProfileAcceleration(ARM_PROFILE_ACCELERATION);
    HAL_Delay(10U);

    // Set home positions now — before the early-return path — so ARM_pos0_* is
    // always in a deterministic state regardless of whether motors respond.
    // Flash overrides compiled defaults if a valid HOME_FLASH_MAGIC sentinel exists.
    armPos0Mot1Lr[0] = armDefaults[0];
    armPos0Mot1Lr[1] = armDefaults[1];
    armPos0Mot2 = armDefaults[2];
    armPos0Mot3 = armDefaults[3];
    armPos0Mot4 = armDefaults[4];
    armPos0Mot5 = armDefaults[5];
    armPos0Mot6 = armDefaults[6];
    (void)loadHomePositions(); // overrides defaults if Flash contains a valid HOME_FLASH_MAGIC

    // Read current positions before enabling torque to prevent violent startup motion
    int32_t cur1Lr[2];
    int32_t cur2, cur3, cur4, cur5, cur6;
    bool ok = armDxl.getPresentPosition(cur1Lr) == 0 && armMot2.getPresentPosition(cur2) == 0 &&
              armMot3.getPresentPosition(cur3) == 0 && armMot4.getPresentPosition(cur4) == 0 &&
              armMot5.getPresentPosition(cur5) == 0 && armMot6.getPresentPosition(cur6) == 0;

    if (!ok)
    {
        // Position read failed — servos may not be communicating yet.
        // For prototype testing: enable torque anyway using compiled home positions.
        debug.log(Level::LogWarn, "[ARM_INIT] Position read failed — using compiled home positions\n");
        cur1Lr[0] = armPos0Mot1Lr[0];
        cur1Lr[1] = armPos0Mot1Lr[1];
        cur2 = armPos0Mot2;
        cur3 = armPos0Mot3;
        cur4 = armPos0Mot4;
        cur5 = armPos0Mot5;
        cur6 = armPos0Mot6;
    }

    // Pre-load goal = current (or home) so motors hold position when torque enables
    armDxl.setGoalPositionEpcm(cur1Lr);
    armMot2.setGoalPositionEpcm(cur2);
    armMot3.setGoalPositionEpcm(cur3);
    armMot4.setGoalPositionEpcm(cur4);
    armMot5.setGoalPositionEpcm(cur5);
    armMot6.setGoalPositionEpcm(cur6);
    HAL_Delay(10U);

    // Enable torque — retry indefinitely until all motors confirm
    debug.log(Level::LogInfo, "[ARM_INIT] Enabling torque on %u motors...\n", 7U);
    uint32_t retryCount = 0U;
    for (;;)
    {
        uint8_t err = 0U;
        err += (armMot1a.setTorqueEnable(true) != 0U);
        err += (armMot1b.setTorqueEnable(true) != 0U);
        err += (armMot2.setTorqueEnable(true) != 0U);
        err += (armMot3.setTorqueEnable(true) != 0U);
        err += (armMot4.setTorqueEnable(true) != 0U);
        err += (armMot5.setTorqueEnable(true) != 0U);
        err += (armMot6.setTorqueEnable(true) != 0U);
        if (err == 0U)
        {
            debug.log(Level::LogInfo, "[ARM_INIT] Torque enabled OK (attempt %lu)\n",
                      (unsigned long)(retryCount + 1U));
            break;
        }
        retryCount++;
        if (retryCount % 10U == 0U)
        {
            debug.log(Level::LogWarn, "[ARM_INIT] Torque enable still failing (%u errors, attempt %lu)\n",
                      err, (unsigned long)retryCount);
        }
        HAL_Delay(10U);
    }

    if (ok)
        debug.log(Level::LogInfo, "[ARM_INIT] Arm DXL initialised (positions read from servos)\n");
    else
        debug.log(Level::LogWarn, "[ARM_INIT] Arm DXL initialised with home positions (servo read failed)\n");
}

/**
 * Advance the beak gripper state machine one step.
 * Transitions: IDLE → CLOSING → HOLDING → OPENING → IDLE
 * Thermal protection: every BEAK_TEMP_CHECK_MS ms in HOLDING,
 * halves hold PWM if temp ≥ BEAK_TEMP_LIMIT; transitions to IDLE on HW error.
 * @param now HAL_GetTick() snapshot from the current loop iteration.
 */
static void tickBeakStateMachine(uint32_t now)
{
    if (beakState == BeakState::CLOSING)
    {
        int16_t load = 0;
        int32_t pos = 0;
        armMot6.getCurrentLoad(load);
        armMot6.getPresentPosition(pos);

        bool loadDetected = abs(load) >= BEAK_LOAD_THRESHOLD;
        bool posReached = abs(pos - BEAK_POS_CLOSE) <= BEAK_POS_TOLERANCE;
        bool timedOut = (now - beakMotionStartMs) > BEAK_TIMEOUT_MS;

        if (loadDetected || posReached || timedOut)
        {
            // Freeze at current position and reduce PWM to hold gently
            armMot6.getPresentPosition(pos);
            armMot6.setGoalPositionEpcm(pos);
            armMot6.setGoalPWM(BEAK_HOLD_PWM);
            beakState = BeakState::HOLDING;
            beakTempCheckMs = now;
            debug.log(Level::LogInfo,
                      "[beak] CLOSING\xe2\x86\x92HOLDING%s\n",
                      timedOut     ? " (timeout)"
                      : posReached ? " (pos)"
                                   : " (load)");
        }
    }
    else if (beakState == BeakState::OPENING)
    {
        int32_t pos = 0;
        armMot6.getPresentPosition(pos);

        bool posReached = abs(pos - BEAK_POS_OPEN) <= BEAK_POS_TOLERANCE;
        bool timedOut = (now - beakMotionStartMs) > BEAK_TIMEOUT_MS;

        if (posReached || timedOut)
        {
            armMot6.setGoalPositionEpcm(pos);
            armMot6.setGoalPWM(BEAK_FULL_PWM);
            beakState = BeakState::IDLE;
            debug.log(Level::LogInfo, "[beak] OPENING\xe2\x86\x92IDLE%s\n", timedOut ? " (timeout)" : " (pos)");
        }
    }
    else if (beakState == BeakState::HOLDING)
    {
        if ((now - beakTempCheckMs) >= BEAK_TEMP_CHECK_MS)
        {
            beakTempCheckMs = now;
            uint8_t temp = 0U;
            uint8_t hwErr = 0U;
            armMot6.getPresentTemperature(temp);
            armMot6.getHardwareErrorStatus(hwErr);

            if (temp >= BEAK_TEMP_LIMIT)
            {
                armMot6.setGoalPWM(BEAK_HOLD_PWM / 2);
                debug.log(Level::LogWarn,
                          "[beak] Thermal: %u deg"
                          "C >= %u deg"
                          "C limit, PWM halved\n",
                          temp,
                          (uint8_t)BEAK_TEMP_LIMIT);
            }
            if (hwErr != 0U)
            {
                beakState = BeakState::IDLE;
                debug.log(Level::LogWarn, "[beak] HW error 0x%02X in HOLDING \xe2\x86\x92 IDLE\n", hwErr);
            }
        }
    }
    // BeakState::IDLE — nothing to do
}
#endif // MODC_ARM

#ifdef MODC_JOINT
/**
 * Initialise all 3 joint Dynamixel motors on USART3.
 * Must be called at startup. Mirrors MODC_ARM_INIT() pattern.
 */
static void DXL_JOINT_INIT(void)
{
    jointDxl.begin();
    jointMot1L.begin();
    jointMot1R.begin();
    jointMot2.begin();

    jointMot1L.setTorqueEnable(false);
    jointMot1R.setTorqueEnable(false);
    jointMot2.setTorqueEnable(false);
    HAL_Delay(10U);

    jointMot1L.setStatusReturnLevel(2U);
    jointMot1R.setStatusReturnLevel(2U);
    jointMot2.setStatusReturnLevel(2U);
    HAL_Delay(10U);

    jointDxl.enableSync(jointIds, sizeof(jointIds));

    jointMot1L.setDriveMode(false, false, false);
    jointMot1R.setDriveMode(false, false, false);
    jointMot2.setDriveMode(false, false, false);

    jointMot1L.setOperatingMode(4U);
    jointMot1R.setOperatingMode(4U);
    jointMot2.setOperatingMode(4U);
    HAL_Delay(10U);

    jointMot1L.setProfileVelocity(ARM_PROFILE_VELOCITY);
    jointMot1L.setProfileAcceleration(ARM_PROFILE_ACCELERATION);
    jointMot1R.setProfileVelocity(ARM_PROFILE_VELOCITY);
    jointMot1R.setProfileAcceleration(ARM_PROFILE_ACCELERATION);
    jointMot2.setProfileVelocity(ARM_PROFILE_VELOCITY);
    jointMot2.setProfileAcceleration(ARM_PROFILE_ACCELERATION);
    HAL_Delay(10U);

    int32_t cur_1LR[2];
    int32_t cur_2;
    bool ok = jointDxl.getPresentPosition(cur_1LR) == 0 && jointMot2.getPresentPosition(cur_2) == 0;

    if (!ok)
    {
        debug.log(Level::LogWarn, "[JOINT_INIT] Position read failed — torque not enabled\n");
        return;
    }

    jointDxl.setGoalPositionEpcm(cur_1LR);
    jointMot2.setGoalPositionEpcm(cur_2);
    HAL_Delay(10U);

    // Enable torque — retry indefinitely until all motors confirm
    debug.log(Level::LogInfo, "[JOINT_INIT] Enabling torque on %u motors...\n", 3U);
    uint32_t retryCount = 0U;
    for (;;)
    {
        uint8_t err = 0U;
        err += (jointMot1L.setTorqueEnable(true) != 0U);
        err += (jointMot1R.setTorqueEnable(true) != 0U);
        err += (jointMot2.setTorqueEnable(true) != 0U);
        if (err == 0U)
        {
            debug.log(Level::LogInfo, "[JOINT_INIT] Torque enabled OK (attempt %lu)\n",
                      (unsigned long)(retryCount + 1U));
            break;
        }
        retryCount++;
        if (retryCount % 10U == 0U)
        {
            debug.log(Level::LogWarn, "[JOINT_INIT] Torque enable still failing (%u errors, attempt %lu)\n",
                      err, (unsigned long)retryCount);
        }
        HAL_Delay(10U);
    }

    // Reset home positions to zero
    jointPos0Mot1Lr[0] = 0;
    jointPos0Mot1Lr[1] = 0;
    jointPos0Mot2 = 0;

    debug.log(Level::LogInfo, "[JOINT_INIT] Joint DXL initialised\n");
}
#endif // MODC_JOINT

/**
 * Send telemetry CAN frames at DT_TEL cadence.
 */
static void sendFeedback(void)
{
    // --- Traction (all modules) ---
    float speedFb[2] = {0.0f, 0.0f};
    dxlTraction.getPresentVelocityRpm(speedFb);
    uint8_t tractionFrame[8];
    memcpy(&tractionFrame[0], &speedFb[0], 4U); // [0] = right (traction_ids[0] = 212)
    memcpy(&tractionFrame[4], &speedFb[1], 4U); // [1] = left  (traction_ids[1] = 114)
    canW.sendMessage(MOTOR_FEEDBACK, tractionFrame, 8U);

    uint8_t errTraction[2] = {0U, 0U};
    motRight.getHardwareErrorStatus(errTraction[0]);
    motLeft.getHardwareErrorStatus(errTraction[1]);
    canW.sendMessage(MOTOR_TRACTION_ERROR_STATUS, errTraction, 2U);

#ifdef MODC_YAW
    float yawAngle = encoderYaw.readAngle();
    canW.sendMessage(JOINT_YAW_FEEDBACK, &yawAngle, 4U);
#endif

#ifdef MODC_ARM
    static constexpr float RPM_TO_RADS = 2.0f * 3.14159265f / 60.0f;

    // Position feedback — delta from home, converted to radians
    int32_t posf1a1b[2];
    armDxl.getPresentPosition(posf1a1b);
    // slot[0]=phi(negated), slot[1]=theta
    float armPhi = -(float)(((posf1a1b[0] - armPos0Mot1Lr[0]) + (posf1a1b[1] - armPos0Mot1Lr[1])) / 2.0f) * DXL_TO_RAD;
    float armTheta = (float)(((posf1a1b[1] - armPos0Mot1Lr[1]) - (posf1a1b[0] - armPos0Mot1Lr[0])) / 2.0f) * DXL_TO_RAD;
    float arm1a1bFb[2] = {armTheta, armPhi};
    canW.sendMessage(ARM_PITCH_1a1b_FEEDBACK, arm1a1bFb, 8U);

    int32_t posf;
    float posfRad;
    armMot2.getPresentPosition(posf);
    posfRad = (float)(posf - armPos0Mot2) * DXL_TO_RAD;
    canW.sendMessage(ARM_PITCH_2_FEEDBACK, &posfRad, 4U);

    armMot3.getPresentPosition(posf);
    posfRad = (float)(posf - armPos0Mot3) * DXL_TO_RAD;
    canW.sendMessage(ARM_ROLL_3_FEEDBACK, &posfRad, 4U);

    armMot4.getPresentPosition(posf);
    posfRad = (float)(posf - armPos0Mot4) * DXL_TO_RAD;
    canW.sendMessage(ARM_PITCH_4_FEEDBACK, &posfRad, 4U);

    armMot5.getPresentPosition(posf);
    posfRad = (float)(posf - armPos0Mot5) * DXL_TO_RAD;
    canW.sendMessage(ARM_ROLL_5_FEEDBACK, &posfRad, 4U);

    armMot6.getPresentPosition(posf);
    posfRad = (float)(posf - armPos0Mot6) * DXL_TO_RAD;
    canW.sendMessage(ARM_ROLL_6_FEEDBACK, &posfRad, 4U);

    // Velocity feedback (RPM → rad/s), same differential convention as position
    float vel1a1b[2] = {0.0f, 0.0f};
    armDxl.getPresentVelocityRpm(vel1a1b);
    float armPhiVel = -(vel1a1b[0] + vel1a1b[1]) * RPM_TO_RADS;
    float armThetaVel = (vel1a1b[0] - vel1a1b[1]) * RPM_TO_RADS;
    float arm1a1bVel[2] = {armThetaVel, armPhiVel};
    canW.sendMessage(ARM_PITCH_1a1b_FEEDBACK_VEL, arm1a1bVel, 8U);

    float vel;
    armMot2.getPresentVelocityRpm(vel);
    vel *= RPM_TO_RADS;
    canW.sendMessage(ARM_PITCH_2_FEEDBACK_VEL, &vel, 4U);

    armMot3.getPresentVelocityRpm(vel);
    vel *= RPM_TO_RADS;
    canW.sendMessage(ARM_ROLL_3_FEEDBACK_VEL, &vel, 4U);

    armMot4.getPresentVelocityRpm(vel);
    vel *= RPM_TO_RADS;
    canW.sendMessage(ARM_PITCH_4_FEEDBACK_VEL, &vel, 4U);

    armMot5.getPresentVelocityRpm(vel);
    vel *= RPM_TO_RADS;
    canW.sendMessage(ARM_ROLL_5_FEEDBACK_VEL, &vel, 4U);

    armMot6.getPresentVelocityRpm(vel);
    vel *= RPM_TO_RADS;
    canW.sendMessage(ARM_ROLL_6_FEEDBACK_VEL, &vel, 4U);

    // Hardware error status
    uint8_t errArm[7] = {0U, 0U, 0U, 0U, 0U, 0U, 0U};
    armMot1a.getHardwareErrorStatus(errArm[0]);
    armMot1b.getHardwareErrorStatus(errArm[1]);
    armMot2.getHardwareErrorStatus(errArm[2]);
    armMot3.getHardwareErrorStatus(errArm[3]);
    armMot4.getHardwareErrorStatus(errArm[4]);
    armMot5.getHardwareErrorStatus(errArm[5]);
    armMot6.getHardwareErrorStatus(errArm[6]);
    canW.sendMessage(MOTOR_ARM_ERROR_STATUS, errArm, 7U);
#endif // MODC_ARM

#ifdef MODC_IMU
    imu.update();
    float imuRoll = imu.getRoll();
    float imuPitch = imu.getPitch();
    canW.sendMessage(JOINT_ROLL_FEEDBACK, &imuRoll, 4U);
    canW.sendMessage(JOINT_PITCH_FEEDBACK, &imuPitch, 4U);
#endif // MODC_IMU

#ifdef MODC_JOINT
    int32_t jointPosf1a1b[2];
    jointDxl.getPresentPosition(jointPosf1a1b);
    float jointTheta =
        ((float)((jointPosf1a1b[0] - jointPos0Mot1Lr[0]) + (jointPosf1a1b[1] - jointPos0Mot1Lr[1])) / 2.0f) *
        DXL_TO_RAD;
    float jointPhi =
        ((float)((jointPosf1a1b[0] - jointPos0Mot1Lr[0]) - (jointPosf1a1b[1] - jointPos0Mot1Lr[1])) / 2.0f) *
        DXL_TO_RAD;
    float joint1a1bFb[2] = {jointTheta, jointPhi};
    canW.sendMessage(JOINT_PITCH_1a1b_FEEDBACK, joint1a1bFb, 8U);

    int32_t jointPosf2;
    jointMot2.getPresentPosition(jointPosf2);
    float joint_posf_2_rad = (float)(jointPosf2 - jointPos0Mot2) * DXL_TO_RAD;
    canW.sendMessage(JOINT_ROLL_2_FEEDBACK, &joint_posf_2_rad, 4U);
#endif // MODC_JOINT
}

/**
 * Dispatch incoming CAN setpoint message. Populated fully in Issue #11.
 * @param msg_id   CAN message type identifier (bits[23:16] of extended ID).
 * @param msgData 8-byte payload.
 */
static void handleSetpoint(uint8_t msgId, const uint8_t* msgData)
{
    switch (msgId)
    {
        // Traction motors — all modules
        case MOTOR_SETPOINT:
        {
            // Payload layout: bytes[0:3]=right RPM, bytes[4:7]=left RPM.
            // speeds_dxl[0]=left, speeds_dxl[1]=right (matches traction sync-write order).
            memcpy(&speedsDxl[0], msgData, 4);     // right → index 0 → motor 212
            memcpy(&speedsDxl[1], msgData + 4, 4); // left  → index 1 → motor 114

            float coeff =
                ((speedsDxl[0] + speedsDxl[1]) < 0.0f) ? TRACTION_VELOCITY_COEFF_REV : TRACTION_VELOCITY_COEFF;
            speedsDxl[0] *= coeff;
            speedsDxl[1] *= coeff;

            float goal[2] = {speedsDxl[0], speedsDxl[1]};
            dxlTraction.setGoalVelocityRpm(goal);
            debug.log(Level::LogDebug, "[CAN] MOTOR_SETPOINT: L=%.1f R=%.1f RPM\n", speedsDxl[0], speedsDxl[1]);
            break;
        }

#ifdef MODC_ARM // Robotic arm — MODC_ARM modules only
        case ARM_PITCH_1a1b_SETPOINT:
        {
            float theta, phi;
            memcpy(&theta, msgData, 4);
            memcpy(&phi, msgData + 4, 4);

            armPosMot1Lr[0] = (int32_t)((theta * RAD_TO_DXL) - (phi * RAD_TO_DXL)) + armPos0Mot1Lr[0];
            armPosMot1Lr[1] = (int32_t)((theta * RAD_TO_DXL) + (phi * RAD_TO_DXL)) + armPos0Mot1Lr[1];

            if ((abs(armPosMot1Lr[0] - armOldPosMot1Lr[0]) > ARM_DE_CAN_DXL) ||
                (abs(armPosMot1Lr[1] - armOldPosMot1Lr[1]) > ARM_DE_CAN_DXL))
            {
                armDxl.setGoalPositionEpcm(armPosMot1Lr);
                armOldPosMot1Lr[0] = armPosMot1Lr[0];
                armOldPosMot1Lr[1] = armPosMot1Lr[1];
            }
            break;
        }

        case ARM_PITCH_2_SETPOINT:
        {
            float val;
            memcpy(&val, msgData, 4);
            armPosMot2 = (int32_t)(val * RAD_TO_DXL) + armPos0Mot2;
            if (abs(armPosMot2 - armOldPosMot2) > ARM_DE_CAN_DXL)
            {
                armMot2.setGoalPositionEpcm(armPosMot2);
                armOldPosMot2 = armPosMot2;
            }
            break;
        }

        case ARM_ROLL_3_SETPOINT:
        {
            float val;
            memcpy(&val, msgData, 4);
            armPosMot3 = (int32_t)(val * RAD_TO_DXL) + armPos0Mot3;
            if (abs(armPosMot3 - armOldPosMot3) > ARM_DE_CAN_DXL)
            {
                armMot3.setGoalPositionEpcm(armPosMot3);
                armOldPosMot3 = armPosMot3;
            }
            break;
        }

        case ARM_PITCH_4_SETPOINT:
        {
            float val;
            memcpy(&val, msgData, 4);
            armPosMot4 = (int32_t)(val * RAD_TO_DXL) + armPos0Mot4;
            if (abs(armPosMot4 - armOldPosMot4) > ARM_DE_CAN_DXL)
            {
                armMot4.setGoalPositionEpcm(armPosMot4);
                armOldPosMot4 = armPosMot4;
            }
            break;
        }

        case ARM_ROLL_5_SETPOINT:
        {
            float val;
            memcpy(&val, msgData, 4);
            // J5 is mounted inverted — negate offset
            armPosMot5 = armPos0Mot5 - (int32_t)(val * RAD_TO_DXL);
            if (abs(armPosMot5 - armOldPosMot5) > ARM_DE_CAN_DXL)
            {
                armMot5.setGoalPositionEpcm(armPosMot5);
                armOldPosMot5 = armPosMot5;
            }
            break;
        }

        case ARM_ROLL_6_SETPOINT:
        {
            int32_t cmd;
            memcpy(&cmd, msgData, 4);
            if (cmd == 0)
            {
                armMot6.setGoalPWM(BEAK_FULL_PWM);
                armMot6.setGoalPositionEpcm(BEAK_POS_CLOSE);
                beakMotionStartMs = HAL_GetTick();
                beakState = BeakState::CLOSING;
            }
            else if (cmd == 1)
            {
                armMot6.setGoalPWM(BEAK_FULL_PWM);
                armMot6.setGoalPositionEpcm(BEAK_POS_OPEN);
                beakMotionStartMs = HAL_GetTick();
                beakState = BeakState::OPENING;
            }
            break;
        }

        case RESET_ARM:
        {
            // Move all joints to home position
            armDxl.setGoalPositionEpcm(armPos0Mot1Lr);
            armMot2.setGoalPositionEpcm(armPos0Mot2);
            armMot3.setGoalPositionEpcm(armPos0Mot3);
            armMot4.setGoalPositionEpcm(armPos0Mot4);
            armMot5.setGoalPositionEpcm(armPos0Mot5);
            armMot6.setGoalPWM(BEAK_FULL_PWM);
            armMot6.setGoalPositionEpcm(armPos0Mot6);
            beakState = BeakState::IDLE;
            armOldPosMot1Lr[0] = armPos0Mot1Lr[0];
            armOldPosMot1Lr[1] = armPos0Mot1Lr[1];
            armOldPosMot2 = armPos0Mot2;
            armOldPosMot3 = armPos0Mot3;
            armOldPosMot4 = armPos0Mot4;
            armOldPosMot5 = armPos0Mot5;
            debug.log(Level::LogInfo, "[CAN] RESET_ARM: moving to home\n");
            break;
        }

        case REBOOT_ARM:
        {
            armMot1a.reboot();
            armMot1b.reboot();
            armMot2.reboot();
            armMot3.reboot();
            armMot4.reboot();
            armMot5.reboot();
            armMot6.reboot();
            HAL_Delay(2000U); // Wait for motors to come back online (~1.5 s typical)
            // Note: session-only SET_HOME is discarded here; loadHomePositions() inside
            // DXL_ARM_INIT() restores the last Flash-persisted home (or compiled defaults).
            dxlArmInit();
            beakState = BeakState::IDLE;
            debug.log(Level::LogInfo, "[CAN] REBOOT_ARM: all arm motors rebooted and reinitialised\n");
            break;
        }

        case SET_HOME:
        {
            // msgData[0]: 0 = session only, 1 = persist to Flash
            armDxl.getPresentPosition(armPos0Mot1Lr);
            armMot2.getPresentPosition(armPos0Mot2);
            armMot3.getPresentPosition(armPos0Mot3);
            armMot4.getPresentPosition(armPos0Mot4);
            armMot5.getPresentPosition(armPos0Mot5);
            armMot6.getPresentPosition(armPos0Mot6);

            armOldPosMot1Lr[0] = armPos0Mot1Lr[0];
            armOldPosMot1Lr[1] = armPos0Mot1Lr[1];
            armOldPosMot2 = armPos0Mot2;
            armOldPosMot3 = armPos0Mot3;
            armOldPosMot4 = armPos0Mot4;
            armOldPosMot5 = armPos0Mot5;

            if (msgData[0] == 1U)
                (void)saveHomePositions();
            else
                debug.log(Level::LogInfo, "[CAN] SET_HOME: session home updated\n");
            break;
        }
#endif // MODC_ARM

#ifdef MODC_JOINT // Inter-module joint — MODC_JOINT modules only
        case JOINT_PITCH_1a1b_SETPOINT:
        {
            float theta, phi;
            memcpy(&theta, msgData, 4);
            memcpy(&phi, msgData + 4, 4);

            jointPosMot1Lr[0] = (int32_t)((theta * RAD_TO_DXL) + (phi * RAD_TO_DXL)) + jointPos0Mot1Lr[0];
            jointPosMot1Lr[1] = (int32_t)((theta * RAD_TO_DXL) - (phi * RAD_TO_DXL)) + jointPos0Mot1Lr[1];

            jointDxl.setGoalPositionEpcm(jointPosMot1Lr);
            jointOldPosMot1Lr[0] = jointPosMot1Lr[0];
            jointOldPosMot1Lr[1] = jointPosMot1Lr[1];
            break;
        }

        case JOINT_ROLL_2_SETPOINT:
        {
            float val;
            memcpy(&val, msgData, 4);
            jointPosMot2 = jointPos0Mot2 + (int32_t)(val * RAD_TO_DXL);
            jointMot2.setGoalPositionEpcm(jointPosMot2);
            break;
        }
#endif // MODC_JOINT

        // Traction reboot — all modules
        case MOTOR_TRACTION_REBOOT:
            motLeft.reboot();
            motRight.reboot();
            HAL_Delay(2000U); // Wait for motors to come back online (~1.5 s typical)
            dxlTractionInit();
            debug.log(Level::LogInfo, "[CAN] MOTOR_TRACTION_REBOOT: traction motors rebooted\n");
            break;

        default:
            debug.log(Level::LogDebug, "[CAN] Unknown msg_id: 0x%02X\n", msgId);
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
    {}
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
void assert_failed(uint8_t* file, uint32_t line)
{
    /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line number,
       ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
