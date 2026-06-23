"""
Pre-built test sequences for STM32LowLevel CAN bus validation.

Each test function takes a CanSender and CanMonitor and runs a scripted
sequence of commands, printing results.
"""

from __future__ import annotations

import time
import math
from typing import Optional

from .protocol import MsgType, ModuleAddress
from .sender import CanSender
from .monitor import CanMonitor, FILTER_ARM, FILTER_TRACTION


def test_traction_basic(
    sender: CanSender,
    destination: int = ModuleAddress.MK2_MOD1,
    speed: float = 5.0,
    duration: float = 3.0,
) -> None:
    """Basic traction motor test: forward → stop → reverse → stop.

    Args:
        sender: CanSender instance.
        destination: Module to test.
        speed: RPM value to use.
        duration: Seconds per phase.
    """
    print(f"=== Traction motor test (speed={speed} RPM, {duration}s per phase) ===")

    print(f"  Forward ({speed} RPM)...")
    sender.traction(speed, speed, destination)
    time.sleep(duration)

    print("  Stop...")
    sender.traction(0.0, 0.0, destination)
    time.sleep(1.0)

    print(f"  Reverse ({-speed} RPM)...")
    sender.traction(-speed, -speed, destination)
    time.sleep(duration)

    print("  Stop...")
    sender.traction(0.0, 0.0, destination)
    print("=== Traction test complete ===\n")


def test_traction_differential(
    sender: CanSender,
    destination: int = ModuleAddress.MK2_MOD1,
    speed: float = 5.0,
    duration: float = 2.0,
) -> None:
    """Differential traction test: left → right → spin.

    Args:
        sender: CanSender instance.
        destination: Module to test.
        speed: RPM value.
        duration: Seconds per phase.
    """
    print(f"=== Differential traction test ===")

    print(f"  Turn left (right={speed}, left=0)...")
    sender.traction(0.0, speed, destination)
    time.sleep(duration)

    print(f"  Turn right (right=0, left={speed})...")
    sender.traction(speed, 0.0, destination)
    time.sleep(duration)

    print(f"  Spin (right={speed}, left={-speed})...")
    sender.traction(-speed, speed, destination)
    time.sleep(duration)

    print("  Stop...")
    sender.traction(0.0, 0.0, destination)
    print("=== Differential test complete ===\n")


def test_arm_init(sender: CanSender) -> None:
    """Test arm initialization: reset → verify feedback.

    Sends RESET_ARM and waits for feedback messages.
    """
    print("=== Arm initialization test ===")

    print("  Sending RESET_ARM...")
    sender.reset_arm()
    time.sleep(3.0)

    print("  Arm should now be at home position.")
    print("  Check feedback messages for position confirmation.")
    print("=== Arm init test complete ===\n")


def test_arm_joints(
    sender: CanSender,
    amplitude: float = 0.2,
    duration: float = 2.0,
) -> None:
    """Move each arm joint individually by a small amount.

    Args:
        sender: CanSender instance.
        amplitude: Angle in radians to move each joint.
        duration: Seconds to wait between movements.
    """
    print(f"=== Arm joint test (amplitude={amplitude} rad) ===")

    joints = [
        ("J1 (pitch 1a1b)", lambda a: sender.arm_pitch_1a1b(a, 0.0)),
        ("J2 (elbow pitch)", sender.arm_pitch_j2),
        ("J3 (roll)", sender.arm_roll_j3),
        ("J4 (wrist pitch)", sender.arm_pitch_j4),
        ("J5 (wrist roll)", sender.arm_roll_j5),
    ]

    for name, cmd in joints:
        print(f"  Moving {name} to +{amplitude} rad...")
        cmd(amplitude)
        time.sleep(duration)

        print(f"  Moving {name} to 0 rad...")
        cmd(0.0)
        time.sleep(duration)

    print("=== Arm joint test complete ===\n")


def test_arm_beak(sender: CanSender, wait: float = 3.0) -> None:
    """Test beak open/close cycle.

    Args:
        sender: CanSender instance.
        wait: Seconds to wait between open/close.
    """
    print("=== Beak gripper test ===")

    print("  Closing beak...")
    sender.arm_beak(close=True)
    time.sleep(wait)

    print("  Opening beak...")
    sender.arm_beak(close=False)
    time.sleep(wait)

    print("=== Beak test complete ===\n")


def test_arm_reboot(sender: CanSender) -> None:
    """Test arm motor reboot sequence.

    Sends REBOOT_ARM and waits for re-initialization.
    """
    print("=== Arm reboot test ===")

    print("  Sending REBOOT_ARM...")
    sender.reboot_arm()
    print("  Waiting for re-initialization (5s)...")
    time.sleep(5.0)

    print("  Arm should have re-initialized smoothly.")
    print("=== Arm reboot test complete ===\n")


def test_full_diagnostics(
    sender: CanSender,
    monitor: CanMonitor,
    speed: float = 3.0,
) -> None:
    """Run full diagnostic sequence: traction → arm init → arm joints → beak.

    Args:
        sender: CanSender instance.
        monitor: CanMonitor instance (for collecting feedback).
        speed: RPM for traction tests.
    """
    print("=" * 60)
    print("  FULL DIAGNOSTICS")
    print("=" * 60)

    test_traction_basic(sender, speed=speed, duration=2.0)
    test_arm_init(sender)
    test_arm_joints(sender, amplitude=0.15, duration=1.5)
    test_arm_beak(sender, wait=2.0)

    print("=" * 60)
    print("  DIAGNOSTICS COMPLETE")
    print("=" * 60)

    print("\nFeedback summary:")
    for name, count in monitor.stats.items():
        print(f"  {name}: {count} messages")


# ─── Test registry ───────────────────────────────────────────────────────────

TESTS = {
    "traction": test_traction_basic,
    "traction_diff": test_traction_differential,
    "arm_init": test_arm_init,
    "arm_joints": test_arm_joints,
    "arm_beak": test_arm_beak,
    "arm_reboot": test_arm_reboot,
    "full": test_full_diagnostics,
}
