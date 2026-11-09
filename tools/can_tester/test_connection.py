"""Tests for CAN adapter setup and outgoing frame flags."""

import unittest
from unittest.mock import Mock, patch

from tools.can_tester.connection import open_can_bus
from tools.can_tester.protocol import ModuleAddress, MsgType
from tools.can_tester.sender import CanSender


class ConnectionTests(unittest.TestCase):
    @patch("tools.can_tester.connection.can.Bus")
    def test_ixxat_channel_is_an_integer(self, bus_factory):
        open_can_bus("ixxat", "0", fd=True)

        bus_factory.assert_called_once_with(
            interface="ixxat",
            channel=0,
            bitrate=1_000_000,
            data_bitrate=2_000_000,
            fd=True,
        )

    @patch("tools.can_tester.connection.can.Bus")
    def test_other_channel_names_are_unchanged(self, bus_factory):
        open_can_bus("socketcan", "can0", fd=False)

        self.assertEqual(bus_factory.call_args.kwargs["channel"], "can0")
        self.assertFalse(bus_factory.call_args.kwargs["fd"])

    def test_invalid_ixxat_channel_is_rejected(self):
        for channel in ("can0", "-1"):
            with self.subTest(channel=channel):
                with self.assertRaisesRegex(ValueError, "Ixxat channel"):
                    open_can_bus("ixxat", channel)

    def test_sender_uses_extended_can_fd_with_brs(self):
        bus = Mock()
        CanSender(bus).send_raw(
            MsgType.MOTOR_SETPOINT,
            bytes(range(12)),
            ModuleAddress.MK2_MOD2,
        )

        message = bus.send.call_args.args[0]
        self.assertTrue(message.is_extended_id)
        self.assertTrue(message.is_fd)
        self.assertTrue(message.bitrate_switch)


if __name__ == "__main__":
    unittest.main()
