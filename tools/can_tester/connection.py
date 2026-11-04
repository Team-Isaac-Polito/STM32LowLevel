"""Shared python-can bus setup for the ISAAC CAN tester."""

from __future__ import annotations

import can


def open_can_bus(
    interface: str,
    channel: str | int,
    bitrate: int = 1_000_000,
    data_bitrate: int = 2_000_000,
) -> can.BusABC:
    """Open a CAN FD bus, normalizing interface-specific CLI arguments.

    Ixxat's Windows VCI backend expects a numeric (zero-based) channel,
    while SocketCAN needs an interface name such as ``can0``.
    """
    if interface == "ixxat":
        try:
            channel = int(channel)
        except (TypeError, ValueError) as exc:
            raise ValueError(
                "Ixxat channel must be a non-negative integer (usually 0)."
            ) from exc
        if channel < 0:
            raise ValueError("Ixxat channel must be a non-negative integer.")

    return can.Bus(
        interface=interface,
        channel=channel,
        bitrate=bitrate,
        data_bitrate=data_bitrate,
        fd=True,
    )
