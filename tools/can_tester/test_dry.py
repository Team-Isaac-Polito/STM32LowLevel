"""Dry test — validates all CAN tester logic without hardware."""

import sys

def main():
    errors = 0

    # 1. Import test
    print("=== Import test ===")
    try:
        from tools.can_tester import (
            protocol, codec, sender, monitor, cli,
            test_sequences, web_dashboard, header_parser,
        )
        print("  All modules imported OK")
    except Exception as e:
        print(f"  FAIL: {e}")
        errors += 1
        sys.exit(1)

    # 2. CAN ID encode/decode
    print("\n=== CAN ID encode/decode ===")
    from tools.can_tester.protocol import (
        encode_can_id, decode_can_id, MsgType, ModuleAddress,
    )

    can_id = encode_can_id(
        ModuleAddress.CENTRAL, ModuleAddress.MK2_MOD1,
        MsgType.ARM_PITCH_2_SETPOINT,
    )
    print(f"  encode(CENTRAL -> MK2_MOD1, ARM_PITCH_2_SETPOINT) = 0x{can_id:08X}")

    decoded = decode_can_id(can_id)
    assert decoded.source == ModuleAddress.CENTRAL, f"src mismatch: {decoded.source}"
    assert decoded.destination == ModuleAddress.MK2_MOD1, f"dst mismatch: {decoded.destination}"
    assert decoded.msg_type == MsgType.ARM_PITCH_2_SETPOINT, f"msg mismatch: {decoded.msg_type}"
    print(f"  decode -> {decoded.source_name} -> {decoded.destination_name}: {decoded.msg_name}")
    print("  Round-trip OK")

    # 3. Payload codec
    print("\n=== Payload codec ===")
    from tools.can_tester.codec import encode_payload, decode_payload, format_decoded
    from tools.can_tester.protocol import PAYLOAD_FORMATS, DecodedID

    # Test traction motor setpoint (two floats: right_rpm, left_rpm)
    data = encode_payload(MsgType.MOTOR_SETPOINT, right_rpm=100.0, left_rpm=-50.0)
    vals = decode_payload(MsgType.MOTOR_SETPOINT, data)
    assert abs(vals["right_rpm"] - 100.0) < 0.01, f"right_rpm mismatch: {vals}"
    assert abs(vals["left_rpm"] - (-50.0)) < 0.01, f"left_rpm mismatch: {vals}"
    print(f"  MOTOR_SETPOINT encode -> {data.hex()} -> {vals} OK")

    # Test arm single float
    data2 = encode_payload(MsgType.ARM_PITCH_2_SETPOINT, angle=1.5708)
    vals2 = decode_payload(MsgType.ARM_PITCH_2_SETPOINT, data2)
    assert abs(vals2["angle"] - 1.5708) < 0.001, f"arm mismatch: {vals2}"
    print(f"  ARM_PITCH_2_SETPOINT encode -> {data2.hex()} -> OK")

    # Test no-payload messages
    data3 = encode_payload(MsgType.RESET_ARM)
    assert data3 == b"", f"RESET_ARM should be empty: {data3}"
    vals3 = decode_payload(MsgType.RESET_ARM, data3)
    assert vals3 == {}, f"RESET_ARM decode should be empty: {vals3}"
    print(f"  RESET_ARM (no payload) OK")

    # Test format_decoded
    test_id = decode_can_id(encode_can_id(
        ModuleAddress.CENTRAL, ModuleAddress.MK2_MOD1, MsgType.MOTOR_SETPOINT
    ))
    fmt = format_decoded(test_id, vals)
    print(f"  format_decoded: {fmt}")

    # 4. Header parser — STM32LowLevel MK2 module variants
    print("\n=== Module config variants ===")
    for variant in ["MK2_MOD1", "MK2_MOD2", "MK2_MOD3"]:
        defs = header_parser.parse_mod_config(variant)
        can_id_val = defs.get("CAN_ID", 0)
        print(f"  {variant}: CAN_ID=0x{can_id_val:02X}, {len(defs)} defines")
        assert "CAN_ID" in defs, f"{variant} missing CAN_ID"

    # Verify CAN_ID values match expected module addresses
    assert header_parser.parse_mod_config("MK2_MOD1")["CAN_ID"] == 0x21, \
        "MK2_MOD1 CAN_ID should be 0x21"
    assert header_parser.parse_mod_config("MK2_MOD2")["CAN_ID"] == 0x22, \
        "MK2_MOD2 CAN_ID should be 0x22"
    assert header_parser.parse_mod_config("MK2_MOD3")["CAN_ID"] == 0x23, \
        "MK2_MOD3 CAN_ID should be 0x23"
    print("  CAN_ID values OK")

    # Verify conditional exclusion: MK2_MOD1 should have ARM servo IDs, not JOINT
    mk2_1 = header_parser.parse_mod_config("MK2_MOD1")
    assert "SERVO_ARM_2_PITCH_ID" in mk2_1, "MK2_MOD1 missing SERVO_ARM_2_PITCH_ID"
    print("  MK2_MOD1 servo defines OK")

    # Verify MK2_MOD2/3 do not have arm servo IDs (only joint + yaw)
    mk2_2 = header_parser.parse_mod_config("MK2_MOD2")
    assert "SERVO_ARM_2_PITCH_ID" not in mk2_2, "MK2_MOD2 should not have ARM servo IDs"
    print("  MK2_MOD2 conditional isolation OK")

    # 5. Test sequences — verify importable
    print("\n=== Test sequences ===")
    from tools.can_tester import test_sequences
    seq_funcs = [
        name for name in dir(test_sequences)
        if name.startswith("test_") and callable(getattr(test_sequences, name))
    ]
    for name in seq_funcs:
        print(f"  {name}")
    assert len(seq_funcs) >= 5, f"Expected >= 5 test functions, got {len(seq_funcs)}"
    print(f"  {len(seq_funcs)} test functions OK")

    # 6. MsgType completeness — STM32LowLevel has 44 message types
    print(f"\n=== MsgType enum ===")
    msg_count = len(list(MsgType))
    print(f"  {msg_count} message types loaded")
    assert msg_count == 44, f"Expected 44 message types, got {msg_count}"

    # Spot-check key entries
    assert MsgType.REBOOT_ARM == 0x5E
    assert MsgType.ARM_ROLL_6_FEEDBACK_VEL == 0x85
    assert MsgType.MOTOR_SETPOINT == 0x21
    assert MsgType.IMU_RAW_GYRO == 0x93
    print("  Spot checks OK")

    print(f"\n{'='*40}")
    if errors == 0:
        print("All tests PASSED!")
    else:
        print(f"{errors} test(s) FAILED")
        sys.exit(1)


if __name__ == "__main__":
    main()
