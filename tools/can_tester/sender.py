"""
CAN message sender with convenience methods for STM32LowLevel commands.

Wraps python-can's Bus object and provides typed, protocol-aware send methods.
"""

from __future__ import annotations

import can
import time
from typing import Optional

try:
    from .protocol import MsgType, ModuleAddress, encode_can_id
    from .codec import encode_payload
except ImportError:
    from protocol import MsgType, ModuleAddress, encode_can_id
    from codec import encode_payload


class CanSender:
    """Send CAN messages to STM32LowLevel modules."""

    def __init__(
        self,
        bus: can.BusABC,
        source: int = ModuleAddress.CENTRAL,
    ):
        self.bus = bus
        self.source = source

    def send_raw(
        self,
        msg_type: int,
        data: bytes,
        destination: int = ModuleAddress.MK2_MOD1,
    ) -> None:
        """Send a raw CAN message."""
        can_id = encode_can_id(self.source, destination, msg_type)
        msg = can.Message(
            arbitration_id=can_id,
            data=data,
            is_extended_id=True,
            is_fd=True,
            bitrate_switch=True,
        )
        self.bus.send(msg)

    def send(
        self,
        msg_type: int,
        destination: int = ModuleAddress.MK2_MOD1,
        **kwargs,
    ) -> None:
        """Encode and send a CAN message.

        Args:
            msg_type: MsgType value.
            destination: Target module address.
            **kwargs: Payload fields matching the PayloadFormat.
        """
        data = encode_payload(msg_type, **kwargs)
        self.send_raw(msg_type, data, destination)

    # ─── Convenience methods ─────────────────────────────────────────────

    def traction(
        self,
        left_rpm: float,
        right_rpm: float,
        destination: int = ModuleAddress.MK2_MOD1,
    ) -> None:
        """Set traction motor speeds."""
        self.send(
            MsgType.MOTOR_SETPOINT,
            destination,
            right_rpm=right_rpm,
            left_rpm=left_rpm,
        )

    def arm_pitch_1a1b(self, theta: float, phi: float) -> None:
        """Set arm J1 differential pitch/yaw."""
        self.send(MsgType.ARM_PITCH_1a1b_SETPOINT, theta=theta, phi=phi)

    def arm_pitch_j2(self, angle: float) -> None:
        """Set arm elbow pitch J2."""
        self.send(MsgType.ARM_PITCH_2_SETPOINT, angle=angle)

    def arm_roll_j3(self, angle: float) -> None:
        """Set arm roll J3."""
        self.send(MsgType.ARM_ROLL_3_SETPOINT, angle=angle)

    def arm_pitch_j4(self, angle: float) -> None:
        """Set arm wrist pitch J4."""
        self.send(MsgType.ARM_PITCH_4_SETPOINT, angle=angle)

    def arm_roll_j5(self, angle: float) -> None:
        """Set arm wrist roll J5."""
        self.send(MsgType.ARM_ROLL_5_SETPOINT, angle=angle)

    def arm_beak(self, close: bool) -> None:
        """Control beak gripper. close=True to close, False to open."""
        # Firmware convention: 0 = close, 1 = open
        self.send(MsgType.ARM_ROLL_6_SETPOINT, command=0 if close else 1)

    def reset_arm(self) -> None:
        """Move arm to calibrated initial position."""
        self.send(MsgType.RESET_ARM)

    def reboot_arm(self) -> None:
        """Reboot all arm Dynamixel motors."""
        self.send(MsgType.REBOOT_ARM)

    def set_home(self, persist: bool = False) -> None:
        """Set current arm position as home. persist=True saves to flash."""
        self.send(MsgType.SET_HOME, persist=1 if persist else 0)

    def reboot_traction(
        self,
        destination: int = ModuleAddress.MK2_MOD1,
    ) -> None:
        """Reboot traction Dynamixel motors."""
        self.send(MsgType.MOTOR_TRACTION_REBOOT, destination)

    def joint_pitch_1a1b(
        self,
        theta: float,
        phi: float,
        destination: int = ModuleAddress.MK2_MOD2,
    ) -> None:
        """Set inter-module joint differential pitch/yaw."""
        self.send(
            MsgType.JOINT_PITCH_1a1b_SETPOINT,
            destination,
            theta=theta,
            phi=phi,
        )

    def joint_roll(
        self,
        angle: float,
        destination: int = ModuleAddress.MK2_MOD2,
    ) -> None:
        """Set inter-module joint roll."""
        self.send(MsgType.JOINT_ROLL_2_SETPOINT, destination, angle=angle)

    def stop_all(self) -> None:
        """Emergency stop: zero all motor speeds."""
        for dest in [ModuleAddress.MK2_MOD1, ModuleAddress.MK2_MOD2, ModuleAddress.MK2_MOD3]:
            self.traction(0.0, 0.0, dest)

    def torque_enable(
        self,
        torque_bitfield: int,
        destination: int = ModuleAddress.MK2_MOD1,
    ) -> None:
        """Enable/disable torque for individual motors via bitfield.

        Bit layout (uint16):
          bit 0  = right traction motor
          bit 1  = left traction motor
          bit 2  = arm/joint motor 1 (J1a / joint-left)
          bit 3  = arm/joint motor 2 (J1b / joint-right)
          bit 4  = arm/joint motor 3 (J2 / joint-roll)
          bit 5  = arm motor 4 (J3)
          bit 6  = arm motor 5 (J4)
          bit 7  = arm motor 6 (J5)
          bit 8  = arm motor 7 (J6 / beak)
          bits 9-15 = unused

        Args:
            torque_bitfield: uint16 bitmask, 1=enable torque, 0=disable.
            destination: Target module address.
        """
        self.send(MsgType.TORQUE_ENABLE_DISABLE, destination, torque_bitfield=torque_bitfield)

    def led_hp_brightness(
        self,
        brightness: int,
        destination: int = ModuleAddress.MK2_MOD1,
    ) -> None:
        """Set LED HP board brightness via PWM.

        Args:
            brightness: 0 (off) to 255 (max brightness).
            destination: Target module address.
        """
        self.send(MsgType.LED_HP_BRIGHTNESS, destination, brightness=brightness)
