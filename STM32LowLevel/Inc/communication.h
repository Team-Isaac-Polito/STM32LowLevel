/**
 * @file communication.h
 * @brief CAN bus packet identifier definitions (PDU Format field).
 *
 * All message IDs are in the 0x11–0x93 range as defined by the J1939-based
 * Extended CAN protocol used by RESE.Q.
 *
 * Full specification:
 *   https://docs.teamisaac.it/doc/can-bus-protocol-t40e2NOEqp
 */

#ifndef COMMUNICATION_H
#define COMMUNICATION_H

// Battery (0x1X)
#define BATTERY_VOLTAGE             0x11  ///< Battery voltage    — Float, Volts
#define BATTERY_PERCENT             0x12  ///< Battery SoC        — Float, Percent
#define BATTERY_TEMPERATURE         0x13  ///< Battery temperature — Float, °C

// Traction motors (0x2X)
#define MOTOR_SETPOINT              0x21  ///< Motor speed setpoint — 2×Float, RPM
#define MOTOR_FEEDBACK              0x22  ///< Motor speed feedback — 2×Float, RPM

// Inter-module sensors (0x3X)
#define JOINT_YAW_FEEDBACK          0x32  ///< Yaw  from AS5048B  — Float, degrees
#define JOINT_PITCH_FEEDBACK        0x34  ///< Pitch from IMU     — Float, radians
#define JOINT_ROLL_FEEDBACK         0x36  ///< Roll  from IMU     — Float, radians

// End effector — MK1 only (0x4X)
#define DATA_EE_PITCH_SETPOINT      0x41  ///< EE wrist pitch setpoint    — Int, DXL units
#define DATA_EE_PITCH_FEEDBACK      0x42  ///< EE wrist pitch feedback    — Int, DXL units
#define DATA_EE_HEAD_PITCH_SETPOINT 0x43  ///< EE head pitch setpoint     — Int, DXL units
#define DATA_EE_HEAD_PITCH_FEEDBACK 0x44  ///< EE head pitch feedback     — Int, DXL units
#define DATA_EE_HEAD_ROLL_SETPOINT  0x45  ///< EE head roll  setpoint     — Int, DXL units
#define DATA_EE_HEAD_ROLL_FEEDBACK  0x46  ///< EE head roll  feedback     — Int, DXL units

// Robotic arm — MK2 MOD1 only (0x5X)
#define ARM_PITCH_1a1b_SETPOINT     0x51  ///< Arm J1a/J1b differential setpoint — Float×2, rad
#define ARM_PITCH_1a1b_FEEDBACK     0x52  ///< Arm J1a/J1b differential feedback — Float×2, rad
#define ARM_PITCH_2_SETPOINT        0x53  ///< Arm J2 elbow pitch setpoint       — Float, rad
#define ARM_PITCH_2_FEEDBACK        0x54  ///< Arm J2 elbow pitch feedback       — Float, rad
#define ARM_ROLL_3_SETPOINT         0x55  ///< Arm J3 wrist roll  setpoint       — Float, rad
#define ARM_ROLL_3_FEEDBACK         0x56  ///< Arm J3 wrist roll  feedback       — Float, rad
#define ARM_PITCH_4_SETPOINT        0x57  ///< Arm J4 wrist pitch setpoint       — Float, rad
#define ARM_PITCH_4_FEEDBACK        0x58  ///< Arm J4 wrist pitch feedback       — Float, rad
#define ARM_ROLL_5_SETPOINT         0x59  ///< Arm J5 wrist roll  setpoint       — Float, rad
#define ARM_ROLL_5_FEEDBACK         0x5A  ///< Arm J5 wrist roll  feedback       — Float, rad
#define ARM_ROLL_6_SETPOINT         0x5B  ///< Arm J6 beak  setpoint — Int32: 0=close, 1=open  
#define ARM_ROLL_6_FEEDBACK         0x5C  ///< Arm J6 beak  feedback — Float, rad
#define RESET_ARM                   0x5D  ///< Move arm to calibrated home position (no payload)
#define REBOOT_ARM                  0x5E  ///< Reboot all arm Dynamixel motors    (no payload)
#define SET_HOME                    0x5F  ///< Save home pos — Int32: 0=session, 1=Flash

// Inter-module joint — MK2 MOD2 and MOD3 (0x6X)
#define JOINT_PITCH_1a1b_SETPOINT   0x61  ///< Joint differential pitch/yaw setpoint — Float×2, rad
#define JOINT_PITCH_1a1b_FEEDBACK   0x62  ///< Joint differential pitch/yaw feedback — Float×2, rad
#define JOINT_ROLL_2_SETPOINT       0x63  ///< Joint roll setpoint — Float, rad
#define JOINT_ROLL_2_FEEDBACK       0x64  ///< Joint roll feedback — Float, rad

// Status and control (0x7X)
#define MOTOR_TRACTION_REBOOT       0x71  ///< Reboot traction motors           (no payload)
#define MOTOR_TRACTION_ERROR_STATUS 0x72  ///< Traction HW error bytes — uint8[2]: right, left
#define MOTOR_ARM_ERROR_STATUS      0x73  ///< Arm HW error bytes     — uint8[7]: J1a…J6
#define TORQUE_ENABLE_DISABLE       0x74  ///< Per-motor torque enable/disable  — uint16 bitfield (1=enable, 0=disable per bit)
#define LED_HP_BRIGHTNESS           0x75  ///< LED HP board brightness  — uint8 (0=off, 255=max)

// Arm velocity feedback — MK2 MOD1 only (0x8X)
#define ARM_PITCH_1a1b_FEEDBACK_VEL 0x80  ///< Arm J1a/J1b velocity — Float×2, rad/s
#define ARM_PITCH_2_FEEDBACK_VEL    0x81  ///< Arm J2 velocity       — Float, rad/s
#define ARM_ROLL_3_FEEDBACK_VEL     0x82  ///< Arm J3 velocity       — Float, rad/s
#define ARM_PITCH_4_FEEDBACK_VEL    0x83  ///< Arm J4 velocity       — Float, rad/s
#define ARM_ROLL_5_FEEDBACK_VEL     0x84  ///< Arm J5 velocity       — Float, rad/s
#define ARM_ROLL_6_FEEDBACK_VEL     0x85  ///< Arm J6 velocity       — Float, rad/s

// IMU raw data (0x9X) — optional, debug only
#define IMU_RAW_ACCEL               0x92  ///< Raw accelerometer data (debug)
#define IMU_RAW_GYRO                0x93  ///< Raw gyroscope data     (debug)

#endif /* COMMUNICATION_H */
