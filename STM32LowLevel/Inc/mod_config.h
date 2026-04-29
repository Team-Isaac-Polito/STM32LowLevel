/**
 * @file mod_config.h
 * @brief Module identity and capability guards for MK2 modules.
 *
 * MODULE_DEFINE must be set at compile time to one of the values below:
 *   -DMODULE_DEFINE=MK2_MOD1  (0x21 — HEAD module)
 *   -DMODULE_DEFINE=MK2_MOD2  (0x22 — MIDDLE module)
 *   -DMODULE_DEFINE=MK2_MOD3  (0x23 — TAIL module)
 */

#ifndef MOD_CONFIG_H
#define MOD_CONFIG_H

#ifndef MODULE_DEFINE
#error "MODULE_DEFINE must be set at compile time (e.g. -DMODULE_DEFINE=MK2_MOD1)"
#endif

// Module CAN addresses
#define MK2_MOD1 0x21  ///< MK2 first module  (HEAD)
#define MK2_MOD2 0x22  ///< MK2 second module (MIDDLE)
#define MK2_MOD3 0x23  ///< MK2 third module  (TAIL)

// MK2_MOD1 — HEAD: traction + robotic arm + IMU
#if MODULE_DEFINE == MK2_MOD1

#define CAN_ID                    MK2_MOD1
#define TRACTION_VELOCITY_COEFF     1.0f      ///< Forward velocity scaling (head is reference)
#define TRACTION_VELOCITY_COEFF_REV 0.9f      ///< Reverse velocity scaling (head becomes tail)
#define MODC_ARM                              ///< Module has 6-DOF robotic arm + beak gripper
#define MODC_IMU                              ///< Module has LSM6DSL IMU                     

// Arm Dynamixel motor IDs (DXL Bus 1 — USART2)
#define SERVO_ARM_1a_PITCH_ID   210
#define SERVO_ARM_1b_PITCH_ID   211
#define SERVO_ARM_2_PITCH_ID    112
#define SERVO_ARM_3_ROLL_ID     113
#define SERVO_ARM_4_PITCH_ID    214
#define SERVO_ARM_5_ROLL_ID     215
#define SERVO_ARM_6_BEAK_ID     216  ///< Beak/gripper motor J6 (XL430-W250)

// MK2_MOD2 — MIDDLE: traction + inter-module joint + yaw encoder + IMU
#elif MODULE_DEFINE == MK2_MOD2

#define CAN_ID                    MK2_MOD2
#define TRACTION_VELOCITY_COEFF     0.95f     ///< Forward velocity scaling (middle module)
#define TRACTION_VELOCITY_COEFF_REV 0.95f     ///< Reverse velocity scaling (middle stays)
#define MODC_YAW                              ///< Module has AS5048B yaw encoder 
#define MODC_JOINT                            ///< Module has inter-module joint   
#define MODC_IMU                              ///< Module has LSM6DSL IMU         

// MK2_MOD3 — TAIL: traction + inter-module joint + yaw encoder + IMU
#elif MODULE_DEFINE == MK2_MOD3

#define CAN_ID                    MK2_MOD3
#define TRACTION_VELOCITY_COEFF     0.9f      ///< Forward velocity scaling (tail module)   
#define TRACTION_VELOCITY_COEFF_REV 1.0f      ///< Reverse velocity scaling (tail becomes head) 
#define MODC_YAW                              ///< Module has AS5048B yaw encoder  
#define MODC_JOINT                            ///< Module has inter-module joint   
#define MODC_IMU                              ///< Module has LSM6DSL IMU         

#else
#error "Unknown MODULE_DEFINE value. Valid values: MK2_MOD1 (0x21), MK2_MOD2 (0x22), MK2_MOD3 (0x23)"
#endif

// Default velocity coefficients when not overridden by module config
#ifndef TRACTION_VELOCITY_COEFF
#define TRACTION_VELOCITY_COEFF     1.0f  ///< Default: no scaling        
#endif
#ifndef TRACTION_VELOCITY_COEFF_REV
#define TRACTION_VELOCITY_COEFF_REV 1.0f  ///< Default: no scaling (rev)  
#endif

#endif /* MOD_CONFIG_H */
