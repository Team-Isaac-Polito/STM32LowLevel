"""CAN bus setup shared by the CLI and web dashboard."""

from __future__ import annotations

import can


def open_can_bus(
    interface: str,
    channel: str | int,
    bitrate: int = 1_000_000,
    data_bitrate: int = 2_000_000,
    fd: bool = True,
) -> can.BusABC:
    """Open a bus, converting interface-specific command-line values."""
    if interface == "ixxat":
        try:
            channel = int(channel)
        except (TypeError, ValueError) as exc:
            raise ValueError("Ixxat channel must be a non-negative integer") from exc

        if channel < 0:
            raise ValueError("Ixxat channel must be a non-negative integer")

    return can.Bus(
        interface=interface,
        channel=channel,
        bitrate=bitrate,
        data_bitrate=data_bitrate,
        fd=fd,
    )
