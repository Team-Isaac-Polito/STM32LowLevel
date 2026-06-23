"""
Encode and decode CAN message payloads for STM32LowLevel protocol.

Provides high-level encode/decode functions that use the PayloadFormat
descriptors from protocol.py.
"""

from __future__ import annotations

import struct
from typing import Any

try:
    from .protocol import (
        MsgType, PayloadFormat, PAYLOAD_FORMATS, encode_can_id,
        decode_can_id, DecodedID, ModuleAddress, MSG_NAMES,
    )
except ImportError:
    from protocol import (
        MsgType, PayloadFormat, PAYLOAD_FORMATS, encode_can_id,
        decode_can_id, DecodedID, ModuleAddress, MSG_NAMES,
    )


def encode_payload(msg_type: int, **kwargs: Any) -> bytes:
    """Encode keyword arguments into a CAN payload.

    Args:
        msg_type: MsgType value.
        **kwargs: Field values matching the PayloadFormat field names.

    Returns:
        Packed bytes ready for CAN transmission.

    Raises:
        ValueError: If msg_type has no known format or fields are missing.

    Example:
        >>> encode_payload(MsgType.MOTOR_SETPOINT, right_rpm=10.0, left_rpm=10.0)
        b'\\x00\\x00 A\\x00\\x00 A'
    """
    fmt = PAYLOAD_FORMATS.get(msg_type)
    if fmt is None:
        raise ValueError(f"No payload format for msg_type 0x{msg_type:02X}")

    if not fmt.fmt:
        return b""  # No-payload messages (RESET_ARM, REBOOT_ARM, etc.)

    values = []
    for field_name in fmt.fields:
        if field_name not in kwargs:
            raise ValueError(
                f"Missing field '{field_name}' for {MSG_NAMES.get(msg_type, '?')}. "
                f"Required: {fmt.fields}"
            )
        values.append(kwargs[field_name])

    return struct.pack(fmt.fmt, *values)


def decode_payload(msg_type: int, data: bytes) -> dict[str, Any]:
    """Decode a CAN payload into a dict of named fields.

    Args:
        msg_type: MsgType value.
        data: Raw CAN payload bytes.

    Returns:
        Dict mapping field names to decoded values.
        Returns {"raw": data.hex()} for unknown message types.
    """
    fmt = PAYLOAD_FORMATS.get(msg_type)
    if fmt is None:
        return {"raw": data.hex()}

    if not fmt.fmt:
        return {}  # No-payload messages

    try:
        values = struct.unpack(fmt.fmt, data[: fmt.size])
    except struct.error:
        return {"raw": data.hex(), "error": "payload size mismatch"}

    result = dict(zip(fmt.fields, values))
    result["_unit"] = fmt.unit
    return result


def format_decoded(decoded_id: DecodedID, payload: dict[str, Any]) -> str:
    """Format a decoded CAN message into a human-readable string.

    Args:
        decoded_id: Parsed CAN identifier.
        payload: Decoded payload dict from decode_payload().

    Returns:
        Single-line formatted string.
    """
    parts = [
        f"[{decoded_id.source_name} → {decoded_id.destination_name}]",
        decoded_id.msg_name,
    ]

    unit = payload.pop("_unit", "")

    for key, value in payload.items():
        if key.startswith("_"):
            continue
        if isinstance(value, float):
            parts.append(f"{key}={value:.4f}{' ' + unit if unit else ''}")
        else:
            parts.append(f"{key}={value}")

    # Restore unit for potential re-use
    if unit:
        payload["_unit"] = unit

    return "  ".join(parts)
