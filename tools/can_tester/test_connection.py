"""Hardware-free regression checks for CAN bus configuration and FD flags."""

from __future__ import annotations

import unittest
from unittest.mock import patch

import can

from tools.can_tester.connection import open_can_bus
from tools.can_tester.protocol import ModuleAddress, MsgType, decode_can_id
from tools.can_tester.sender import CanSender


class BusConfigurationTests(unittest.TestCase):
    @patch("tools.can_tester.connection.can.Bus")
    def test_ixxat_channel_is_integer_and_fd_enabled(self, bus_factory):
        open_can_bus("ixxat", "0")
        bus_factory.assert_called_once_with(
            interface="ixxat",
            channel=0,
            bitrate=1_000_000,
            data_bitrate=2_000_000,
            fd=True,
        )

    @patch("tools.can_tester.connection.can.Bus")
    def test_socketcan_channel_name_is_preserved(self, bus_factory):
        open_can_bus("socketcan", "can0")
        self.assertEqual(bus_factory.call_args.kwargs["channel"], "can0")

    @patch("tools.can_tester.connection.can.Bus")
    def test_existing_gs_usb_channel_is_preserved(self, bus_factory):
        open_can_bus("gs_usb", "0")
        self.assertEqual(bus_factory.call_args.kwargs["channel"], "0")

    def test_non_numeric_ixxat_channel_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "Ixxat channel"):
            open_can_bus("ixxat", "can0")

    def test_negative_ixxat_channel_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "non-negative"):
            open_can_bus("ixxat", "-1")


class FrameFormatTests(unittest.TestCase):
    def test_sender_creates_extended_fd_brs_frame(self):
        class CapturingBus:
            def __init__(self):
                self.messages = []

            def send(self, msg):
                self.messages.append(msg)

        bus = CapturingBus()
        sender = CanSender(bus)
        payload = bytes(range(12))
        sender.send_raw(MsgType.MOTOR_SETPOINT, payload, ModuleAddress.MK2_MOD2)

        self.assertEqual(len(bus.messages), 1)
        msg = bus.messages[0]
        self.assertIsInstance(msg, can.Message)
        self.assertTrue(msg.is_extended_id)
        self.assertTrue(msg.is_fd)
        self.assertTrue(msg.bitrate_switch)
        self.assertEqual(bytes(msg.data), payload)
        decoded = decode_can_id(msg.arbitration_id)
        self.assertEqual(decoded.source, ModuleAddress.CENTRAL)
        self.assertEqual(decoded.destination, ModuleAddress.MK2_MOD2)
        self.assertEqual(decoded.msg_type, MsgType.MOTOR_SETPOINT)


if __name__ == "__main__":
    unittest.main()
