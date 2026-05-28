"""
CAN bus protocol definitions for STM32LowLevel firmware.

Defines the authoritative MsgType enum and CAN ID encoding/decoding.
At import time, optionally validates against communication.h via
header_parser — if the header has new or changed defines, a warning
is printed so the developer can update this file.

Extended CAN (29-bit) identifiers based on J1939:
    - SA  [0:7]   Source Address (module ID)
    - PS  [8:15]  Destination Address
    - PF  [16:23] PDU Format (message type)
    - EFF bit[31]  Extended Frame Flag (set by CAN driver)

Identifier encoding (little-endian bytes):
    byte0 = source_address
    byte1 = destination_address
    byte2 = message_type (PDU Format)
    byte3 = 0x00 (set by CAN driver for EFF)
"""

from __future__ import annotations

import struct
import warnings
from enum import IntEnum
from dataclasses import dataclass, field
from typing import Optional


# ─── Module addresses ────────────────────────────────────────────────────────

class ModuleAddress(IntEnum):
    """Module CAN addresses. Format: 0xYX where Y=version, X=ordinal."""
    CENTRAL = 0x00   # Jetson / PC
    MK2_MOD1 = 0x21  # Head module (arm)
    MK2_MOD2 = 0x22  # Middle module (joint)
    MK2_MOD3 = 0x23  # Tail module (joint)


# ─── Message types (authoritative — manually synced with communication.h) ───

class MsgType(IntEnum):
    """CAN message types. Keep in sync with STM32LowLevel/Inc/communication.h."""

    # Battery
    BATTERY_VOLTAGE = 0x11
    BATTERY_PERCENT = 0x12
    BATTERY_TEMPERATURE = 0x13

    # Traction
    MOTOR_SETPOINT = 0x21
    MOTOR_FEEDBACK = 0x22

    # Joint / IMU orientation feedback
    JOINT_YAW_FEEDBACK = 0x32
    JOINT_PITCH_FEEDBACK = 0x34
    JOINT_ROLL_FEEDBACK = 0x36

    # End Effector (MK1 legacy)
    DATA_EE_PITCH_SETPOINT = 0x41
    DATA_EE_PITCH_FEEDBACK = 0x42
    DATA_EE_HEAD_PITCH_SETPOINT = 0x43
    DATA_EE_HEAD_PITCH_FEEDBACK = 0x44
    DATA_EE_HEAD_ROLL_SETPOINT = 0x45
    DATA_EE_HEAD_ROLL_FEEDBACK = 0x46

    # Robotic arm (MOD1 only)
    ARM_PITCH_1a1b_SETPOINT = 0x51
    ARM_PITCH_1a1b_FEEDBACK = 0x52
    ARM_PITCH_2_SETPOINT = 0x53
    ARM_PITCH_2_FEEDBACK = 0x54
    ARM_ROLL_3_SETPOINT = 0x55
    ARM_ROLL_3_FEEDBACK = 0x56
    ARM_PITCH_4_SETPOINT = 0x57
    ARM_PITCH_4_FEEDBACK = 0x58
    ARM_ROLL_5_SETPOINT = 0x59
    ARM_ROLL_5_FEEDBACK = 0x5A
    ARM_ROLL_6_SETPOINT = 0x5B
    ARM_ROLL_6_FEEDBACK = 0x5C
    RESET_ARM = 0x5D
    REBOOT_ARM = 0x5E
    SET_HOME = 0x5F

    # Inter-module joints (MOD2/MOD3)
    JOINT_PITCH_1a1b_SETPOINT = 0x61
    JOINT_PITCH_1a1b_FEEDBACK = 0x62
    JOINT_ROLL_2_SETPOINT = 0x63
    JOINT_ROLL_2_FEEDBACK = 0x64

    # Motor control / status
    MOTOR_TRACTION_REBOOT = 0x71
    MOTOR_TRACTION_ERROR_STATUS = 0x72
    MOTOR_ARM_ERROR_STATUS = 0x73
    TORQUE_ENABLE_DISABLE = 0x74

    # Arm velocity feedback
    ARM_PITCH_1a1b_FEEDBACK_VEL = 0x80
    ARM_PITCH_2_FEEDBACK_VEL = 0x81
    ARM_ROLL_3_FEEDBACK_VEL = 0x82
    ARM_PITCH_4_FEEDBACK_VEL = 0x83
    ARM_ROLL_5_FEEDBACK_VEL = 0x84
    ARM_ROLL_6_FEEDBACK_VEL = 0x85

    # IMU raw data (debug)
    IMU_RAW_ACCEL = 0x92
    IMU_RAW_GYRO = 0x93


# ─── Validate against communication.h (optional) ────────────────────────────

try:
    from .header_parser import parse_communication_header
    _header_defines = parse_communication_header()

    # Check for defines in the header that are missing or different here
    _py_defines = {m.name: m.value for m in MsgType}
    _missing = {k: v for k, v in _header_defines.items() if k not in _py_defines}
    _changed = {
        k: (v, _py_defines[k])
        for k, v in _header_defines.items()
        if k in _py_defines and v != _py_defines[k]
    }
    _extra = {k: v for k, v in _py_defines.items() if k not in _header_defines}
    if _missing or _changed or _extra:
        parts = []
        if _missing:
            parts.append(f"New defines in communication.h not in protocol.py: {_missing}")
        if _changed:
            parts.append(f"Changed values: {_changed}")
        if _extra:
            parts.append(f"Python-only entries not in communication.h: {_extra}")
        warnings.warn(
            "protocol.py is out of sync with communication.h — "
            + "; ".join(parts),
            stacklevel=2,
        )
except (FileNotFoundError, ImportError):
    pass  # Running outside the repo — skip validation


# ─── Human-readable names ───────────────────────────────────────────────────

MSG_NAMES: dict[int, str] = {m.value: m.name for m in MsgType}
MODULE_NAMES: dict[int, str] = {m.value: m.name for m in ModuleAddress}


# ─── Payload format descriptors ─────────────────────────────────────────────

@dataclass
class PayloadFormat:
    """Describes the encoding of a CAN message payload."""
    fmt: str            # struct format string (little-endian)
    fields: list[str]   # field names
    unit: str = ""      # unit label
    size: int = field(init=False)

    def __post_init__(self):
        self.size = struct.calcsize(self.fmt)


# Maps MsgType → PayloadFormat
PAYLOAD_FORMATS: dict[int, PayloadFormat] = {
    # Traction
    MsgType.MOTOR_SETPOINT: PayloadFormat("<ff", ["right_rpm", "left_rpm"], "RPM"),
    MsgType.MOTOR_FEEDBACK: PayloadFormat("<ff", ["left_rpm", "right_rpm"], "RPM"),

    # Battery
    MsgType.BATTERY_VOLTAGE: PayloadFormat("<f", ["voltage"], "V"),
    MsgType.BATTERY_PERCENT: PayloadFormat("<f", ["percent"], "%"),
    MsgType.BATTERY_TEMPERATURE: PayloadFormat("<f", ["temperature"], "°C"),

    # Joint / IMU orientation feedback
    MsgType.JOINT_YAW_FEEDBACK: PayloadFormat("<f", ["angle"], "deg"),
    MsgType.JOINT_PITCH_FEEDBACK: PayloadFormat("<f", ["pitch"], "rad"),
    MsgType.JOINT_ROLL_FEEDBACK: PayloadFormat("<f", ["roll"], "rad"),

    # Arm position setpoints / feedback
    MsgType.ARM_PITCH_1a1b_SETPOINT: PayloadFormat("<ff", ["theta", "phi"], "rad"),
    MsgType.ARM_PITCH_1a1b_FEEDBACK: PayloadFormat("<ff", ["theta", "phi"], "rad"),
    MsgType.ARM_PITCH_2_SETPOINT: PayloadFormat("<f", ["angle"], "rad"),
    MsgType.ARM_PITCH_2_FEEDBACK: PayloadFormat("<f", ["angle"], "rad"),
    MsgType.ARM_ROLL_3_SETPOINT: PayloadFormat("<f", ["angle"], "rad"),
    MsgType.ARM_ROLL_3_FEEDBACK: PayloadFormat("<f", ["angle"], "rad"),
    MsgType.ARM_PITCH_4_SETPOINT: PayloadFormat("<f", ["angle"], "rad"),
    MsgType.ARM_PITCH_4_FEEDBACK: PayloadFormat("<f", ["angle"], "rad"),
    MsgType.ARM_ROLL_5_SETPOINT: PayloadFormat("<f", ["angle"], "rad"),
    MsgType.ARM_ROLL_5_FEEDBACK: PayloadFormat("<f", ["angle"], "rad"),
    MsgType.ARM_ROLL_6_SETPOINT: PayloadFormat("<i", ["command"], "0=close(PWM-limited hold),1=open"),
    MsgType.ARM_ROLL_6_FEEDBACK: PayloadFormat("<f", ["angle"], "rad"),

    # Arm commands (no payload)
    MsgType.RESET_ARM: PayloadFormat("", [], ""),
    MsgType.REBOOT_ARM: PayloadFormat("", [], ""),
    MsgType.SET_HOME: PayloadFormat("<i", ["persist"], "0=interim,1=permanent"),

    # Arm velocity feedback
    MsgType.ARM_PITCH_1a1b_FEEDBACK_VEL: PayloadFormat("<ff", ["theta_vel", "phi_vel"], "rad/s"),
    MsgType.ARM_PITCH_2_FEEDBACK_VEL: PayloadFormat("<f", ["velocity"], "rad/s"),
    MsgType.ARM_ROLL_3_FEEDBACK_VEL: PayloadFormat("<f", ["velocity"], "rad/s"),
    MsgType.ARM_PITCH_4_FEEDBACK_VEL: PayloadFormat("<f", ["velocity"], "rad/s"),
    MsgType.ARM_ROLL_5_FEEDBACK_VEL: PayloadFormat("<f", ["velocity"], "rad/s"),
    MsgType.ARM_ROLL_6_FEEDBACK_VEL: PayloadFormat("<f", ["velocity"], "rad/s"),

    # Inter-module joints
    MsgType.JOINT_PITCH_1a1b_SETPOINT: PayloadFormat("<ff", ["theta", "phi"], "rad"),
    MsgType.JOINT_PITCH_1a1b_FEEDBACK: PayloadFormat("<ff", ["theta", "phi"], "rad"),
    MsgType.JOINT_ROLL_2_SETPOINT: PayloadFormat("<f", ["angle"], "rad"),
    MsgType.JOINT_ROLL_2_FEEDBACK: PayloadFormat("<f", ["angle"], "rad"),

    # Status
    MsgType.MOTOR_TRACTION_ERROR_STATUS: PayloadFormat("<BB", ["motor_right", "motor_left"], "error_bits"),
    MsgType.MOTOR_ARM_ERROR_STATUS: PayloadFormat("<BBBBBBB", [
        "mot_1L", "mot_1R", "mot_2", "mot_3", "mot_4", "mot_5", "mot_6"
    ], "error_bits"),

    # Traction control (no payload)
    MsgType.MOTOR_TRACTION_REBOOT: PayloadFormat("", [], ""),

    # Torque enable/disable — uint16 bitfield
    MsgType.TORQUE_ENABLE_DISABLE: PayloadFormat("<H", ["torque_bitfield"], "bits"),

    # IMU raw data (debug)
    MsgType.IMU_RAW_ACCEL: PayloadFormat("<fff", ["ax", "ay", "az"], "g"),
    MsgType.IMU_RAW_GYRO: PayloadFormat("<fff", ["gx", "gy", "gz"], "dps"),

    # End effector — MK1 legacy
    MsgType.DATA_EE_PITCH_SETPOINT: PayloadFormat("<i", ["position"], "DU"),
    MsgType.DATA_EE_PITCH_FEEDBACK: PayloadFormat("<i", ["position"], "DU"),
    MsgType.DATA_EE_HEAD_PITCH_SETPOINT: PayloadFormat("<i", ["position"], "DU"),
    MsgType.DATA_EE_HEAD_PITCH_FEEDBACK: PayloadFormat("<i", ["position"], "DU"),
    MsgType.DATA_EE_HEAD_ROLL_SETPOINT: PayloadFormat("<i", ["position"], "DU"),
    MsgType.DATA_EE_HEAD_ROLL_FEEDBACK: PayloadFormat("<i", ["position"], "DU"),
}


# ─── CAN ID encoding / decoding ─────────────────────────────────────────────

def encode_can_id(
    source: int,
    destination: int,
    msg_type: int,
) -> int:
    """Build a 29-bit Extended CAN identifier.

    Layout (little-endian byte view):
        byte0 = source address  [bits 0:7]
        byte1 = destination     [bits 8:15]
        byte2 = msg_type (PF)   [bits 16:23]
        byte3 = 0               [bits 24:28]
    """
    return (msg_type << 16) | (destination << 8) | source


@dataclass
class DecodedID:
    """Parsed fields from a 29-bit CAN identifier."""
    raw: int
    source: int
    destination: int
    msg_type: int
    source_name: str
    destination_name: str
    msg_name: str


def decode_can_id(can_id: int) -> DecodedID:
    """Parse a 29-bit Extended CAN identifier into its component fields."""
    source = can_id & 0xFF
    destination = (can_id >> 8) & 0xFF
    msg_type = (can_id >> 16) & 0xFF

    return DecodedID(
        raw=can_id,
        source=source,
        destination=destination,
        msg_type=msg_type,
        source_name=MODULE_NAMES.get(source, f"0x{source:02X}"),
        destination_name=MODULE_NAMES.get(destination, f"0x{destination:02X}"),
        msg_name=MSG_NAMES.get(msg_type, f"UNKNOWN_0x{msg_type:02X}"),
    )
