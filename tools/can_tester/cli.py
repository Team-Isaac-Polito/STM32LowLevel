#!/usr/bin/env python3
"""
Interactive CLI for STM32LowLevel CAN bus testing.

Usage:
    python -m tools.can_tester --interface socketcan --channel can0
    python -m tools.can_tester --interface gs_usb --channel 0   # Innomaker USB2CAN on Windows/Linux
    python -m tools.can_tester --interface slcan --channel COM3  # Serial CAN adapter

Commands:
    send traction <left_rpm> <right_rpm>    Set traction motor speeds
    send arm_j2 <angle_rad>                 Set arm elbow pitch
    send arm_j3 <angle_rad>                 Set arm roll J3
    send arm_j4 <angle_rad>                 Set arm wrist pitch
    send arm_j5 <angle_rad>                 Set arm wrist roll
    send arm_1a1b <theta> <phi>             Set arm differential J1
    send beak open|close                    Control beak gripper
    send reset_arm                          Move arm to home position
    send set_home [permanent]               Set current position as home (default: interim)
    send reboot_arm                         Reboot arm motors
    send reboot_traction                    Reboot traction motors
    stop                                    Emergency stop all motors
    monitor [filter]                        Start monitoring (filter: all|arm|traction|joint|feedback)
    test <name>                             Run test sequence (traction|arm_init|arm_joints|arm_beak|full)
    status                                  Show monitor statistics
    help                                    Show this help
    quit                                    Exit
"""

from __future__ import annotations

import argparse
import sys
import threading
import time
from typing import Optional

import can

from .protocol import ModuleAddress
from .sender import CanSender
from .monitor import CanMonitor, NAMED_FILTERS
from .test_sequences import TESTS


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="STM32LowLevel CAN bus testing CLI",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument(
        "--interface", "-i",
        default="gs_usb",
        help="python-can interface type (default: gs_usb)",
    )
    parser.add_argument(
        "--channel", "-c",
        default="0",
        help="CAN channel (default: 0)",
    )
    parser.add_argument(
        "--bitrate", "-b",
        type=int,
        default=1000000,
        help="CAN arbitration bitrate in bps (default: 1000000 = 1 Mbit/s)",
    )
    parser.add_argument(
        "--data-bitrate", "-D",
        type=int,
        default=2000000,
        help="CAN FD data-phase bitrate in bps (default: 2000000 = 2 Mbit/s)",
    )
    parser.add_argument(
        "--module", "-m",
        default="MK2_MOD1",
        choices=["MK2_MOD1", "MK2_MOD2", "MK2_MOD3"],
        help="Target module (default: MK2_MOD1)",
    )
    return parser.parse_args()


class CLI:
    """Interactive command-line interface for CAN testing."""

    def __init__(self, bus: can.BusABC, target: int):
        self.bus = bus
        self.sender = CanSender(bus)
        self.monitor = CanMonitor(bus)
        self.target = target
        self._monitor_thread: Optional[threading.Thread] = None

    def run(self) -> None:
        """Main REPL loop."""
        print("STM32LowLevel CAN Tester — type 'help' for commands")
        print(f"Target module: {ModuleAddress(self.target).name}")
        print()

        while True:
            try:
                line = input("can> ").strip()
            except (EOFError, KeyboardInterrupt):
                print("\nExiting...")
                break

            if not line:
                continue

            parts = line.split()
            cmd = parts[0].lower()

            try:
                if cmd == "quit" or cmd == "exit" or cmd == "q":
                    break
                elif cmd == "help" or cmd == "?":
                    self._help()
                elif cmd == "send":
                    self._handle_send(parts[1:])
                elif cmd == "stop":
                    self.sender.stop_all()
                    print("Emergency stop sent to all modules.")
                elif cmd == "monitor":
                    self._handle_monitor(parts[1:])
                elif cmd == "test":
                    self._handle_test(parts[1:])
                elif cmd == "status":
                    self._show_status()
                else:
                    print(f"Unknown command: '{cmd}'. Type 'help' for commands.")
            except Exception as e:
                print(f"Error: {e}")

        self.monitor.stop()

    def _help(self) -> None:
        print(__doc__)

    def _handle_send(self, args: list[str]) -> None:
        if not args:
            print("Usage: send <command> [args...]")
            return

        subcmd = args[0].lower()

        if subcmd == "traction" and len(args) == 3:
            left, right = float(args[1]), float(args[2])
            self.sender.traction(left, right, self.target)
            print(f"Traction: left={left} RPM, right={right} RPM")

        elif subcmd == "arm_1a1b" and len(args) == 3:
            theta, phi = float(args[1]), float(args[2])
            self.sender.arm_pitch_1a1b(theta, phi)
            print(f"Arm J1 differential: θ={theta}, φ={phi} rad")

        elif subcmd == "arm_j2" and len(args) == 2:
            angle = float(args[1])
            self.sender.arm_pitch_j2(angle)
            print(f"Arm J2 pitch: {angle} rad")

        elif subcmd == "arm_j3" and len(args) == 2:
            angle = float(args[1])
            self.sender.arm_roll_j3(angle)
            print(f"Arm J3 roll: {angle} rad")

        elif subcmd == "arm_j4" and len(args) == 2:
            angle = float(args[1])
            self.sender.arm_pitch_j4(angle)
            print(f"Arm J4 pitch: {angle} rad")

        elif subcmd == "arm_j5" and len(args) == 2:
            angle = float(args[1])
            self.sender.arm_roll_j5(angle)
            print(f"Arm J5 roll: {angle} rad")

        elif subcmd == "beak":
            if len(args) < 2:
                print("Usage: send beak open|close")
                return
            close = args[1].lower() in ("close", "1", "true")
            self.sender.arm_beak(close=close)
            print(f"Beak: {'close' if close else 'open'}")

        elif subcmd == "reset_arm":
            self.sender.reset_arm()
            print("Reset arm to home position.")

        elif subcmd == "set_home":
            persist = len(args) > 1 and args[1] == "permanent"
            self.sender.set_home(persist=persist)
            mode = "permanent (flash)" if persist else "interim (session)"
            print(f"Set home: {mode}")

        elif subcmd == "reboot_arm":
            self.sender.reboot_arm()
            print("Rebooting arm motors...")

        elif subcmd == "reboot_traction":
            self.sender.reboot_traction(self.target)
            print("Rebooting traction motors...")

        elif subcmd == "joint_1a1b" and len(args) == 3:
            theta, phi = float(args[1]), float(args[2])
            self.sender.joint_pitch_1a1b(theta, phi, self.target)
            print(f"Joint differential: θ={theta}, φ={phi} rad")

        elif subcmd == "joint_roll" and len(args) == 2:
            angle = float(args[1])
            self.sender.joint_roll(angle, self.target)
            print(f"Joint roll: {angle} rad")

        else:
            print(f"Unknown send command: '{subcmd}' with {len(args) - 1} args")
            print("Available: traction, arm_1a1b, arm_j2..j5, beak, reset_arm, "
                  "set_home, reboot_arm, reboot_traction, joint_1a1b, joint_roll")

    def _handle_monitor(self, args: list[str]) -> None:
        filter_name = args[0].lower() if args else "all"
        filter_set = NAMED_FILTERS.get(filter_name)

        if filter_name not in NAMED_FILTERS:
            print(f"Unknown filter: '{filter_name}'")
            print(f"Available: {', '.join(NAMED_FILTERS.keys())}")
            return

        if self._monitor_thread and self._monitor_thread.is_alive():
            self.monitor.stop()
            self._monitor_thread.join(timeout=2.0)

        print(f"Monitoring CAN bus (filter: {filter_name}). Press Ctrl+C to stop.")

        try:
            self.monitor.run(filter_types=filter_set)
        except KeyboardInterrupt:
            self.monitor.stop()
            print("\nMonitor stopped.")

    def _handle_test(self, args: list[str]) -> None:
        if not args:
            print(f"Available tests: {', '.join(TESTS.keys())}")
            return

        test_name = args[0].lower()
        test_fn = TESTS.get(test_name)

        if test_fn is None:
            print(f"Unknown test: '{test_name}'")
            print(f"Available: {', '.join(TESTS.keys())}")
            return

        if self._monitor_thread and self._monitor_thread.is_alive():
            self.monitor.stop()
            self._monitor_thread.join(timeout=2.0)

        self._monitor_thread = threading.Thread(
            target=self.monitor.run,
            kwargs={"quiet": True},
            daemon=True,
        )
        self._monitor_thread.start()
        time.sleep(0.2)  # Let monitor start

        if test_name == "full":
            test_fn(self.sender, self.monitor)
        elif test_name in ("traction", "traction_diff"):
            test_fn(self.sender, self.target)
        else:
            test_fn(self.sender)

        self.monitor.stop()
        self._monitor_thread.join(timeout=2.0)

    def _show_status(self) -> None:
        stats = self.monitor.stats
        if not stats:
            print("No messages received yet. Run 'monitor' first.")
            return

        print("Message counts:")
        for name, count in stats.items():
            print(f"  {name}: {count}")

        print("\nLast values:")
        for name, values in self.monitor.last_values.items():
            vals = {k: v for k, v in values.items() if not k.startswith("_")}
            print(f"  {name}: {vals}")


def main() -> None:
    args = parse_args()

    target_map = {
        "MK2_MOD1": ModuleAddress.MK2_MOD1,
        "MK2_MOD2": ModuleAddress.MK2_MOD2,
        "MK2_MOD3": ModuleAddress.MK2_MOD3,
    }
    target = target_map[args.module]

    print(f"Connecting to CAN bus: interface={args.interface}, "
          f"channel={args.channel}, bitrate={args.bitrate}...")

    try:
        bus = can.Bus(
            interface=args.interface,
            channel=args.channel,
            bitrate=args.bitrate,
            data_bitrate=args.data_bitrate,
            fd=True,
        )
    except Exception as e:
        print(f"Failed to connect: {e}")
        print("\nMake sure:")
        print("  - USB2CAN adapter is connected")
        print("  - On Linux: sudo ip link set can0 up type can bitrate 1000000 dbitrate 2000000 fd on")
        print("  - On Windows: install gs_usb driver for Innomaker USB2CAN")
        sys.exit(1)

    print("Connected.\n")

    try:
        cli = CLI(bus, target)
        cli.run()
    finally:
        bus.shutdown()
        print("CAN bus closed.")


if __name__ == "__main__":
    main()
