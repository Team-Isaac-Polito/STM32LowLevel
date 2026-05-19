"""
Real-time CAN bus monitor with protocol-aware decoding.

Listens for CAN messages and prints decoded output to the terminal.
"""

from __future__ import annotations

import can
import time
import sys
from typing import Optional, Callable

from .protocol import decode_can_id, MsgType, MSG_NAMES
from .codec import decode_payload, format_decoded


class CanMonitor:
    """Monitor CAN bus traffic with protocol-aware decoding."""

    def __init__(self, bus: can.BusABC):
        self.bus = bus
        self._running = False
        self._msg_counts: dict[int, int] = {}
        self._last_values: dict[int, dict] = {}
        self._callbacks: list[Callable] = []

    def add_callback(self, callback: Callable) -> None:
        """Register a callback for each received message.

        Callback signature: callback(decoded_id, payload, raw_msg)
        """
        self._callbacks.append(callback)

    def run(
        self,
        filter_types: Optional[set[int]] = None,
        duration: Optional[float] = None,
        quiet: bool = False,
    ) -> None:
        """Start monitoring CAN bus traffic.

        Args:
            filter_types: If set, only show these MsgType values.
            duration: Run for this many seconds, then stop. None = forever.
            quiet: If True, suppress terminal output (callbacks still fire).
        """
        self._running = True
        start = time.time()

        try:
            while self._running:
                if duration and (time.time() - start) >= duration:
                    break

                msg = self.bus.recv(timeout=0.1)
                if msg is None:
                    continue

                decoded_id = decode_can_id(msg.arbitration_id)

                if filter_types and decoded_id.msg_type not in filter_types:
                    continue

                payload = decode_payload(decoded_id.msg_type, bytes(msg.data))

                # Track statistics
                self._msg_counts[decoded_id.msg_type] = (
                    self._msg_counts.get(decoded_id.msg_type, 0) + 1
                )
                self._last_values[decoded_id.msg_type] = payload

                # Fire callbacks
                for cb in self._callbacks:
                    cb(decoded_id, payload, msg)

                if not quiet:
                    ts = f"{msg.timestamp:.3f}" if msg.timestamp else f"{time.time():.3f}"
                    line = format_decoded(decoded_id, payload)
                    print(f"[{ts}] {line}")

        except KeyboardInterrupt:
            pass
        finally:
            self._running = False

    def stop(self) -> None:
        """Stop the monitor loop."""
        self._running = False

    @property
    def stats(self) -> dict[str, int]:
        """Return message count statistics."""
        return {
            MSG_NAMES.get(k, f"0x{k:02X}"): v
            for k, v in sorted(self._msg_counts.items())
        }

    @property
    def last_values(self) -> dict[str, dict]:
        """Return last decoded values for each message type."""
        return {
            MSG_NAMES.get(k, f"0x{k:02X}"): v
            for k, v in self._last_values.items()
        }


# ─── Predefined filters ─────────────────────────────────────────────────────

FILTER_ARM = {
    MsgType.ARM_PITCH_1a1b_SETPOINT, MsgType.ARM_PITCH_1a1b_FEEDBACK,
    MsgType.ARM_PITCH_2_SETPOINT, MsgType.ARM_PITCH_2_FEEDBACK,
    MsgType.ARM_ROLL_3_SETPOINT, MsgType.ARM_ROLL_3_FEEDBACK,
    MsgType.ARM_PITCH_4_SETPOINT, MsgType.ARM_PITCH_4_FEEDBACK,
    MsgType.ARM_ROLL_5_SETPOINT, MsgType.ARM_ROLL_5_FEEDBACK,
    MsgType.ARM_ROLL_6_SETPOINT, MsgType.ARM_ROLL_6_FEEDBACK,
    MsgType.RESET_ARM, MsgType.REBOOT_ARM,
    MsgType.ARM_PITCH_1a1b_FEEDBACK_VEL, MsgType.ARM_PITCH_2_FEEDBACK_VEL,
    MsgType.ARM_ROLL_3_FEEDBACK_VEL, MsgType.ARM_PITCH_4_FEEDBACK_VEL,
    MsgType.ARM_ROLL_5_FEEDBACK_VEL, MsgType.ARM_ROLL_6_FEEDBACK_VEL,
    MsgType.MOTOR_ARM_ERROR_STATUS,
}

FILTER_TRACTION = {
    MsgType.MOTOR_SETPOINT, MsgType.MOTOR_FEEDBACK,
    MsgType.MOTOR_TRACTION_REBOOT, MsgType.MOTOR_TRACTION_ERROR_STATUS,
}

FILTER_JOINT = {
    MsgType.JOINT_PITCH_1a1b_SETPOINT, MsgType.JOINT_PITCH_1a1b_FEEDBACK,
    MsgType.JOINT_ROLL_2_SETPOINT, MsgType.JOINT_ROLL_2_FEEDBACK,
    MsgType.JOINT_YAW_FEEDBACK,
    MsgType.JOINT_PITCH_FEEDBACK, MsgType.JOINT_ROLL_FEEDBACK,
}

FILTER_IMU = {
    MsgType.JOINT_PITCH_FEEDBACK, MsgType.JOINT_ROLL_FEEDBACK,
    MsgType.IMU_RAW_ACCEL, MsgType.IMU_RAW_GYRO,
}

FILTER_FEEDBACK = {
    MsgType.MOTOR_FEEDBACK, MsgType.JOINT_YAW_FEEDBACK,
    MsgType.JOINT_PITCH_FEEDBACK, MsgType.JOINT_ROLL_FEEDBACK,
    MsgType.ARM_PITCH_1a1b_FEEDBACK, MsgType.ARM_PITCH_2_FEEDBACK,
    MsgType.ARM_ROLL_3_FEEDBACK, MsgType.ARM_PITCH_4_FEEDBACK,
    MsgType.ARM_ROLL_5_FEEDBACK, MsgType.ARM_ROLL_6_FEEDBACK,
    MsgType.ARM_PITCH_1a1b_FEEDBACK_VEL, MsgType.ARM_PITCH_2_FEEDBACK_VEL,
    MsgType.ARM_ROLL_3_FEEDBACK_VEL, MsgType.ARM_PITCH_4_FEEDBACK_VEL,
    MsgType.ARM_ROLL_5_FEEDBACK_VEL, MsgType.ARM_ROLL_6_FEEDBACK_VEL,
    MsgType.JOINT_PITCH_1a1b_FEEDBACK, MsgType.JOINT_ROLL_2_FEEDBACK,
    MsgType.MOTOR_TRACTION_ERROR_STATUS, MsgType.MOTOR_ARM_ERROR_STATUS,
}

NAMED_FILTERS = {
    "arm": FILTER_ARM,
    "traction": FILTER_TRACTION,
    "joint": FILTER_JOINT,
    "imu": FILTER_IMU,
    "feedback": FILTER_FEEDBACK,
    "all": None,
}
