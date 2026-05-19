# CAN Tester — Testing Procedures

This document describes when and how to use the CAN testing tool during
development, integration, and maintenance of the STM32LowLevel firmware.

---

## When to Test

| Scenario | What to test | Tool |
|----------|-------------|------|
| **New firmware upload** | All motors respond, feedback messages arrive | Web dashboard — full diagnostic |
| **Mechanical change** (shaft/arm repositioned) | Arm home position is safe, no collisions | Dashboard — `reset_arm`, monitor feedback |
| **After fixing a bug** | Specific subsystem affected by the fix | Dashboard or CLI — targeted commands |
| **CAN wiring change** | All modules reachable, no bus errors | Dashboard — listen to all three modules |
| **New motor / Dynamixel replaced** | Motor responds to setpoints, error status clear | Dashboard — individual joint commands |
| **Pre-competition check** | End-to-end system health | CLI — `test full` sequence |
| **Debugging intermittent issues** | Monitor traffic patterns over time | Dashboard — leave running with filters |

---

## Procedure 1: Verify Traction Motors

**Purpose**: Confirm both traction motors on a module respond correctly.

1. Power on the module. Connect USB2CAN to the CAN bus.
2. Start dashboard: `python -m tools.can_tester.web_dashboard -i gs_usb -c 0 --port 8080`
3. Select the target module (e.g. MK2_MOD1).
4. Select **Traction Motors** command.
5. Enter small values first: Right RPM = `5`, Left RPM = `5`. Click **Send**.
6. Observe:
   - Motors should spin forward at low speed.
   - `MOTOR_FEEDBACK` messages should appear in the live feed showing actual RPM.
7. Try differential: Right = `20`, Left = `-20` (spin in place).
8. Click **STOP ALL** to halt.
9. Check for `MOTOR_TRACTION_ERROR_STATUS` messages — both bytes should be `0`.

**Expected**: Motors respond within ~100 ms, feedback RPM matches setpoint
within ±10%.

---

## Procedure 2: Verify Robotic Arm (MOD1 Only)

**Purpose**: Confirm all 6 arm joints + beak respond correctly.

> **Safety**: Ensure the arm is free to move. Keep hands clear. Start with
> small angles.

1. Start dashboard, select **MK2_MOD1**.
2. Send **Reset Arm** — arm should move to home position.
3. Monitor `ARM_*_FEEDBACK` messages to confirm positions near zero.
4. Test each joint individually:
   - **Arm J1 (1a1b)**: Theta = `0.1`, Phi = `0.0` → shoulder should yaw slightly.
   - **Arm J2**: Angle = `0.2` → elbow should bend slightly.
   - **Arm J3**: Angle = `0.1` → forearm should rotate slightly.
   - **Arm J4**: Angle = `0.1` → wrist should pitch slightly.
   - **Arm J5**: Angle = `0.1` → wrist should rotate slightly.
5. Send **Beak Close**, then **Beak Open** — gripper should actuate.
6. Send **Reset Arm** to return to home.
7. Check `MOTOR_ARM_ERROR_STATUS` — all 7 bytes should be `0`.

**Expected**: Each joint moves smoothly, feedback matches setpoint within
~0.05 rad. No error flags.

---

## Procedure 3: Verify Inter-Module Joints (MOD2/MOD3)

**Purpose**: Confirm the inter-module joint motors on middle/tail modules work.

1. Start dashboard, select **MK2_MOD2** (or MOD3).
2. Select **Joint Pitch (1a1b)**: Theta = `0.1`, Phi = `0.0`. Send.
3. Observe `JOINT_PITCH_1a1b_FEEDBACK` — should reflect the setpoint.
4. Select **Joint Roll**: Angle = `0.1`. Send.
5. Observe `JOINT_ROLL_2_FEEDBACK`.
6. Return to zero: send both with `0.0`.

**Expected**: Joint responds, feedback matches within ~0.05 rad.

---

## Procedure 4: CAN Bus Health Check

**Purpose**: Verify all modules are alive and communicating.

1. Start dashboard in monitoring mode (no commands needed).
2. Filter: **All**. Watch for periodic messages from each module:
   - `BATTERY_VOLTAGE`, `BATTERY_PERCENT` from each module.
   - `MOTOR_FEEDBACK` if traction is active.
3. Check the **Statistics** panel — each expected message type should
   have a nonzero count.
4. If a module is missing: check CAN wiring, termination resistor (120 Ω),
   and power supply.

---

## Procedure 5: Error Recovery

**Purpose**: Clear motor error states and verify recovery.

1. If `MOTOR_TRACTION_ERROR_STATUS` or `MOTOR_ARM_ERROR_STATUS` shows
   nonzero values:
2. Send **Reboot Traction** (for traction errors) or **Reboot Arm**
   (for arm errors).
3. Wait 2–3 seconds for motors to re-initialize.
4. Send a small setpoint to verify the motor responds.
5. Monitor error status — should return to all zeros.

---

## Procedure 6: Full Diagnostic (CLI)

**Purpose**: Automated end-to-end test of all subsystems.

```bash
python -m tools.can_tester -i gs_usb -c 0
> test full
```

This runs all test sequences in order:
1. Traction forward/stop/reverse
2. Differential turning
3. Arm reset + individual joint sweep
4. Beak open/close
5. Arm motor reboot

Monitor the output for any failures or timeouts.

---

## Procedure 7: Offline Validation

**Purpose**: Verify the tool's protocol layer without hardware.

```bash
cd STM32LowLevel
python -m tools.can_tester.test_dry
```

This validates:
- CAN ID encoding/decoding round-trip
- Payload codec for all message types
- Header parser output matches protocol definitions
- MsgType enum completeness (all 44 message types)

Run this after any change to `protocol.py`, `codec.py`, or `communication.h`.

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---------|-------------|-----|
| "No gs_usb device found" | WinUSB driver not installed | See README.md — Windows setup |
| Dashboard shows no messages | CAN wiring disconnected or wrong bitrate | Check wiring; must be 125 kbps |
| Motors don't respond to setpoints | Module not powered or CAN ID mismatch | Verify module address in module selector |
| `MOTOR_*_ERROR_STATUS` nonzero | Motor in error state (overload, overtemp) | Send reboot command, check mechanical load |
| Messages from unknown address | MK1 module or misconfigured firmware | Check `mod_config.h` build defines |
