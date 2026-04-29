/**
 * @file definitions.h
 * @brief Firmware-wide constants: timing, motor IDs, CAN bus, ADC, and
 *        Dynamixel conversion factors.
 *
 * Hardware reference: MK2 V3.0 board (STM32G474RET6).
 *
 * See also:
 *   mod_config.h    — module identity and per-module capability guards
 *   communication.h — CAN bus packet identifier definitions
 */

#ifndef DEFINITIONS_H
#define DEFINITIONS_H

/* -----------------------------------------------------------------------
 * Main loop timings (ms)
 * ----------------------------------------------------------------------- */
#define DT_BAT        1000U  /**< Battery & health check interval:  1 Hz  */
#define DT_TEL          40U  /**< CAN telemetry (sendFeedback) interval: 25 Hz */
#define DT_DXL_CHECK  1000U  /**< Dynamixel error-status poll interval:  1 Hz  */

/* -----------------------------------------------------------------------
 * CAN bus safety
 * ----------------------------------------------------------------------- */
#define CAN_TIMEOUT   1000U  /**< Stop all motors if no CAN received for 1000 ms */

/* -----------------------------------------------------------------------
 * Battery monitoring (ADC1 IN1, PA0 — VBAT_SENSE)
 * Resistor divider: R_top = 100 kΩ, R_bot = 27 kΩ
 * Vout = Vbat × 27 / (100 + 27)  → scale factor for ADC reading
 * ----------------------------------------------------------------------- */
#define BAT_LOW        11.1f  /**< Low-voltage warning threshold (3S LiPo empty) */
#define BAT_NOM        12.6f  /**< Nominal full voltage (3S LiPo) */
#define BAT_R_TOP    100000U  /**< Top resistor of voltage divider (Ω)  */
#define BAT_R_BOT      27000U /**< Bottom resistor of voltage divider (Ω) */
/** Scale factor to recover Vbat from ADC reading:
 *  Vbat = Vadc × (BAT_R_TOP + BAT_R_BOT) / BAT_R_BOT */
#define BAT_SCALE_FACTOR  ((float)(BAT_R_TOP + BAT_R_BOT) / (float)BAT_R_BOT)

/* -----------------------------------------------------------------------
 * eFuse current monitoring (TPS25982)
 * IMON formula: I = V_IMON / (R_IMON × K_IMON)
 *   R_IMON = 511 Ω (RIMON resistor)
 *   K_IMON = 246 µA/A (current mirror ratio, per TPS25982 datasheet SLVSGT5)
 * ----------------------------------------------------------------------- */
#define EFUSE_R_IMON    511U          /**< RIMON resistor (Ω)        */
#define EFUSE_K_IMON    246e-6f       /**< Current mirror ratio (A/A) */

/* -----------------------------------------------------------------------
 * Dynamixel position/velocity unit conversions
 *   4096 DXL units = 2π rad (one full rotation in Extended Position Mode)
 * ----------------------------------------------------------------------- */
#define DXL_TO_RAD   0.00153398f  /**< DXL position units → radians  (2π / 4096) */
#define RAD_TO_DXL   651.899f     /**< Radians → DXL position units  (4096 / 2π) */

/** Dynamixel velocity: 1 RPM = 2π/60 rad/s */
#define DXL_RPM_TO_RAD_S  (2.0f * 3.14159265f / 60.0f)

/* -----------------------------------------------------------------------
 * Traction motor Dynamixel IDs (DXL Bus 1 — USART2)
 * NOTE: variable names mot_Left / mot_Right in PicoLowLevel are swapped
 * relative to physical mounting (legacy naming).  The correct physical
 * assignment is: ID 212 = physical RIGHT motor, ID 114 = physical LEFT motor.
 * ----------------------------------------------------------------------- */
#define SERVO_TRACTION_R_ID   212  /**< Physical right traction motor */
#define SERVO_TRACTION_L_ID   114  /**< Physical left  traction motor */

/* -----------------------------------------------------------------------
 * Joint motor Dynamixel IDs (DXL Bus 2 — USART3, MODC_JOINT modules)
 * ----------------------------------------------------------------------- */
#define SERVO_JOINT_LEFT_ID   100  /**< Left  inter-module joint motor */
#define SERVO_JOINT_RIGHT_ID  120  /**< Right inter-module joint motor */
#define SERVO_JOINT_ROLL_ID   123  /**< Roll  inter-module joint motor */

/* -----------------------------------------------------------------------
 * Absolute encoder (AS5048B on I2C1, shared with LSM6DSL)
 * ----------------------------------------------------------------------- */
#define ABSOLUTE_ENCODER_ADDRESS  0x40U  /**< I2C address of AS5048B */

/* -----------------------------------------------------------------------
 * IMU sensor (LSM6DSL on I2C1, shared with AS5048B)
 * ----------------------------------------------------------------------- */
#define IMU_ADDRESS               0x6AU  /**< I2C address of LSM6DSL (SDO/SA0 = GND) */

/* -----------------------------------------------------------------------
 * Beak gripper state machine constants (motor J6, XL430-W250)
 * Calibrated with dxl_get_position.ino in Extended Position Mode.
 * ----------------------------------------------------------------------- */
#define BEAK_POS_OPEN        (-154)   /**< Open  position (DXL ext-pos units, ~-13.5°) */
#define BEAK_POS_CLOSE         154    /**< Close position (DXL ext-pos units, ~+13.5°) */
#define BEAK_LOAD_THRESHOLD    150    /**< Grip detection load threshold (0.1% units, ~15% torque) */
#define BEAK_POS_TOLERANCE      20    /**< Position tolerance to detect arrival (DXL units, ~1.7°) */
#define BEAK_TIMEOUT_MS       3000U   /**< Max motion time before giving up (ms) */
#define BEAK_HOLD_PWM          250    /**< Hold PWM after grip contact (~28% max PWM) */
#define BEAK_FULL_PWM          885    /**< Full PWM for free movement (100% max PWM)  */
#define BEAK_TEMP_LIMIT         65    /**< Thermal protection: halve hold PWM above this (°C) */
#define BEAK_TEMP_CHECK_MS     500U   /**< Temperature check interval during HOLDING (ms) */

/* -----------------------------------------------------------------------
 * Arm motion profile (shared across all arm joints)
 * ----------------------------------------------------------------------- */
#define ARM_PROFILE_VELOCITY      20   /**< Dynamixel profile velocity (DXL units) */
#define ARM_PROFILE_ACCELERATION  10   /**< Dynamixel profile acceleration (DXL units) */
#define ARM_DE_CAN_DXL            10   /**< Deadband: ignore cmd changes smaller than this (DXL units) */

/* -----------------------------------------------------------------------
 * Default arm home positions (DXL extended-position units)
 * Order: J1a, J1b, J2, J3, J4, J5, J6
 * These are used at startup and overridden by valid Flash-stored values.
 * ----------------------------------------------------------------------- */
#define ARM_HOME_NUM_MOTORS     7
#define ARM_DEFAULT_HOME        {1328, 641, 4101, 3072, 1757, 3612, 144}

/* -----------------------------------------------------------------------
 * Flash storage for home positions (STM32G474, 512 KB Flash)
 * Last page (page 127) at 0x0807F800 — 2 KB per page.
 * Write sequence per RM0440 §3.3: unlock → erase → write 64-bit words → lock.
 * ----------------------------------------------------------------------- */
#define HOME_FLASH_PAGE_ADDR    0x0807F800UL  /**< Start address of last Flash page (Bank 2, page 127) */
#define HOME_FLASH_PAGE_SIZE    2048U         /**< Page size (bytes) — 2 KB in dual-bank mode (DBANK=1) */
#define HOME_FLASH_PAGE_NUM     127U          /**< Page index within Bank 2 (0–127; dual-bank mode) */
#define HOME_FLASH_BANK         FLASH_BANK_2  /**< Must match HOME_FLASH_PAGE_ADDR; pass to HAL_FLASHEx_Erase .Banks */

#endif /* DEFINITIONS_H */
