#!/usr/bin/env python3
"""
Web dashboard for real-time CAN bus monitoring.

Provides a browser-based UI at http://localhost:8080 with:
- Live message feed via Server-Sent Events (SSE)
- Message statistics
- Send command panel
- Auto-reconnecting real-time display

Usage:
    python -m tools.can_tester.web_dashboard --interface gs_usb --channel 0
    python -m tools.can_tester.web_dashboard --interface socketcan --channel can0
"""

from __future__ import annotations

import argparse
import json
import sys
import threading
import time
from queue import Queue, Empty, Full

from flask import Flask, Response, request, jsonify, render_template_string, make_response

import can

try:
    from .protocol import ModuleAddress, MsgType, encode_can_id, decode_can_id, MSG_NAMES
    from .codec import encode_payload, decode_payload, format_decoded
    from .sender import CanSender
    from .monitor import CanMonitor, NAMED_FILTERS
except ImportError:
    from protocol import ModuleAddress, MsgType, encode_can_id, decode_can_id, MSG_NAMES
    from codec import encode_payload, decode_payload, format_decoded
    from sender import CanSender
    from monitor import CanMonitor, NAMED_FILTERS

app = Flask(__name__)

# Global state (initialized in main)
bus: can.BusABC = None  # type: ignore
sender: CanSender = None  # type: ignore
monitor: CanMonitor = None  # type: ignore
event_queues: list[Queue] = []
event_queues_lock = threading.Lock()

# Track last received message for status endpoint
_last_recv_msg = None
_last_recv_lock = threading.Lock()

# Message buffer for polling endpoint
_msg_buffer = []
_msg_buffer_lock = threading.Lock()
_msg_id_counter = 0



# Accumulated positions for relative mode (per-command)
_positions: dict[str, list[float]] = {}


# ─── HTML template ───────────────────────────────────────────────────────────

DASHBOARD_HTML = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>STM32LowLevel CAN Dashboard</title>
<style>
:root { --bg: #1a1a2e; --card: #16213e; --accent: #0f3460; --text: #e0e0e0;
        --green: #4ecca3; --red: #e74c3c; --yellow: #f1c40f; --blue: #3498db; }
* { margin: 0; padding: 0; box-sizing: border-box; }
body { font-family: 'Segoe UI', system-ui, sans-serif; background: var(--bg);
       color: var(--text); padding: 16px; }
h1 { color: var(--green); margin-bottom: 16px; font-size: 1.5rem; }
h2 { color: var(--blue); margin-bottom: 8px; font-size: 1.1rem; }
.grid { display: grid; grid-template-columns: 1fr 1fr; gap: 16px; }
.card { background: var(--card); border-radius: 8px; padding: 16px; }
#feed { height: 400px; overflow-y: auto; font-family: 'Cascadia Code', monospace;
        font-size: 0.85rem; line-height: 1.5; }
#feed .msg { padding: 2px 4px; border-bottom: 1px solid rgba(255,255,255,0.05); }
#feed .msg:hover { background: rgba(255,255,255,0.05); }
.src { color: var(--green); } .dst { color: var(--yellow); }
.type { color: var(--blue); font-weight: bold; } .val { color: #ccc; }
#stats table { width: 100%; border-collapse: collapse; }
#stats td { padding: 4px 8px; border-bottom: 1px solid rgba(255,255,255,0.05); }
#stats td:last-child { text-align: right; font-family: monospace; }
.send-form { display: flex; gap: 8px; flex-wrap: wrap; align-items: center; }
.send-form input, .send-form select { background: var(--accent); color: var(--text);
  border: 1px solid rgba(255,255,255,0.1); padding: 6px 10px; border-radius: 4px; }
.send-form button { background: var(--green); color: var(--bg); border: none;
  padding: 6px 16px; border-radius: 4px; cursor: pointer; font-weight: bold; }
.send-form button:hover { opacity: 0.9; }
.send-form button.danger { background: var(--red); }
.status-dot { display: inline-block; width: 8px; height: 8px; border-radius: 50%;
  margin-right: 6px; }
.status-dot.on { background: var(--green); } .status-dot.off { background: var(--red); }
#connection { font-size: 0.85rem; margin-bottom: 12px; }
.filter-bar { margin-bottom: 8px; }
.filter-bar button { background: var(--accent); color: var(--text); border: 1px solid
  rgba(255,255,255,0.1); padding: 4px 10px; border-radius: 4px; cursor: pointer;
  margin-right: 4px; font-size: 0.8rem; }
.filter-bar button.active { background: var(--green); color: var(--bg); }
.feed-controls { display: flex; align-items: center; gap: 8px; margin-top: 6px; }
.feed-controls input, .feed-controls select { background: var(--accent); color: var(--text);
  border: 1px solid rgba(255,255,255,0.1); padding: 4px 8px; border-radius: 4px;
  font-size: 0.8rem; }
.feed-controls input::placeholder { color: #888; }
.feed-controls button { background: var(--accent); color: var(--text);
  border: 1px solid rgba(255,255,255,0.1); padding: 4px 8px; border-radius: 4px;
  cursor: pointer; font-size: 0.8rem; }
.feed-controls button:hover { background: rgba(255,255,255,0.1); }
#latestValues table { width: 100%; border-collapse: collapse; }
#latestValues td { padding: 4px 8px; border-bottom: 1px solid rgba(255,255,255,0.05); }
#latestValues td:first-child { color: var(--blue); white-space: nowrap; }
#latestValues td:last-child { font-family: monospace; }
</style>
</head>
<body>
<h1>STM32LowLevel CAN Dashboard</h1>
<div id="connection"><span class="status-dot off" id="connDot"></span>
  <span id="connText">Waiting for CAN data...</span></div>
<div class="grid">
  <div class="card">
    <h2>Live Feed</h2>
    <div class="filter-bar" id="filterBar">
      <button class="active" data-filter="all">All</button>
      <button data-filter="arm">Arm</button>
      <button data-filter="traction">Traction</button>
      <button data-filter="joint">Joint</button>
    </div>
    <div id="feed"></div>
    <div style="display:flex;align-items:center;gap:8px;margin-top:6px">
      <input id="filterSearch" type="text" placeholder="Search text..." style="flex:1;background:var(--accent);color:var(--text);border:1px solid rgba(255,255,255,0.1);padding:4px 8px;border-radius:4px;font-size:0.8rem" oninput="applyFilters()">
      <input id="filterCanId" type="text" placeholder="PF hex (msg type)" title="Filter by PDU Format (message type) in hex. Exact match. Comma-separated for multiple. E.g.: 22 = Motor Feedback, 21 = Motor Setpoint, 64 = Joint Roll, 12 = Battery Voltage" style="width:160px;background:var(--accent);color:var(--text);border:1px solid rgba(255,255,255,0.1);padding:4px 8px;border-radius:4px;font-size:0.8rem;font-family:monospace" oninput="applyFilters()">
      <button id="pauseBtn" onclick="togglePause()" title="Pause/Resume feed" style="background:var(--accent);color:var(--text);border:1px solid rgba(255,255,255,0.1);padding:4px 10px;border-radius:4px;cursor:pointer;font-size:0.8rem">⏸ Pause</button>
      <button onclick="clearFeed()" title="Clear feed" style="background:var(--accent);color:var(--text);border:1px solid rgba(255,255,255,0.1);padding:4px 10px;border-radius:4px;cursor:pointer;font-size:0.8rem">✕ Clear</button>
    </div>
    <div id="moduleFilterRow" style="display:flex;align-items:center;gap:4px;margin-top:4px;flex-wrap:wrap">
      <span style="font-size:0.75rem;color:#888;margin-right:4px">Modules:</span>
      <button class="module-filter-btn active" data-module="" onclick="toggleModuleFilter(this)" style="background:var(--green);color:var(--bg);border:1px solid var(--green);padding:2px 8px;border-radius:4px;cursor:pointer;font-size:0.75rem">All</button>
      <button class="module-filter-btn" data-module="0x21" onclick="toggleModuleFilter(this)" style="background:var(--accent);color:var(--text);border:1px solid rgba(255,255,255,0.1);padding:2px 8px;border-radius:4px;cursor:pointer;font-size:0.75rem">MOD1</button>
      <button class="module-filter-btn" data-module="0x22" onclick="toggleModuleFilter(this)" style="background:var(--accent);color:var(--text);border:1px solid rgba(255,255,255,0.1);padding:2px 8px;border-radius:4px;cursor:pointer;font-size:0.75rem">MOD2</button>
      <button class="module-filter-btn" data-module="0x23" onclick="toggleModuleFilter(this)" style="background:var(--accent);color:var(--text);border:1px solid rgba(255,255,255,0.1);padding:2px 8px;border-radius:4px;cursor:pointer;font-size:0.75rem">MOD3</button>
    </div>
    <div style="font-size:0.75rem;color:#666;margin-top:4px" id="scrollHint"></div>
  </div>
  <div>
    <div class="card" style="margin-bottom:16px">
      <h2>Send Command</h2>
      <div style="margin-bottom:12px">
        <label style="font-size:0.8rem;color:var(--blue);display:block;margin-bottom:4px">Target Module</label>
        <select id="targetModule" style="width:100%">
          <option value="0x21">MK2_MOD1 — Head / Arm (CAN 0x21)</option>
          <option value="0x22">MK2_MOD2 — Middle / Joint (CAN 0x22)</option>
          <option value="0x23">MK2_MOD3 — Tail / Joint (CAN 0x23)</option>
        </select>
      </div>
      <div style="margin-bottom:4px">
        <label style="font-size:0.8rem;color:var(--blue);display:block;margin-bottom:4px">Command</label>
        <select id="cmdType" onchange="updateForm()" style="width:100%"></select>
      </div>
      <p id="cmdDesc" style="font-size:0.8rem;color:#999;margin:4px 0 8px 0"></p>
      <div class="send-form" id="sendForm">
        <div id="inputGroup" style="display:flex;gap:8px;align-items:center;flex-wrap:wrap">
          <div id="field1" style="display:flex;flex-direction:column;gap:2px">
            <label id="lbl1" style="font-size:0.75rem;color:var(--green)"></label>
            <input id="val1" type="number" step="1" value="0" style="width:140px">
          </div>
          <div id="field2" style="display:flex;flex-direction:column;gap:2px">
            <label id="lbl2" style="font-size:0.75rem;color:var(--green)"></label>
            <input id="val2" type="number" step="1" value="0" style="width:140px">
          </div>
        </div>
        <button onclick="sendCommand()">Send</button>
        <button class="danger" onclick="stopAll()">STOP ALL</button>
      </div>
      <div id="torqueDropdowns" style="display:none;margin-top:8px">
        <div style="display:flex;gap:8px;align-items:center;flex-wrap:wrap">
          <div style="display:flex;flex-direction:column;gap:2px">
            <label style="font-size:0.75rem;color:var(--green)">Motor</label>
            <select id="torqueMotor" style="width:200px"></select>
          </div>
          <div style="display:flex;flex-direction:column;gap:2px">
            <label style="font-size:0.75rem;color:var(--green)">State</label>
            <select id="torqueState" style="width:160px">
              <option value="1">ON (enable torque)</option>
              <option value="0">OFF (disable torque)</option>
            </select>
          </div>
        </div>
      </div>
      <div id="cmdOptions" style="margin-top:8px;display:none">
        <label style="font-size:0.8rem;color:var(--green);cursor:pointer">
          <input type="checkbox" id="optPermanent"> Permanent (persist to flash)
        </label>
      </div>
      <div id="relativeMode" style="margin-top:8px;display:none">
        <label style="font-size:0.8rem;color:var(--green);cursor:pointer">
          <input type="checkbox" id="optRelative"> Relative mode (delta from current)
        </label>
        <button id="btnResetOrigin" onclick="fetch('/reset_positions',{method:'POST'}).then(()=>{const b=document.getElementById('btnResetOrigin');b.textContent='✓ Origin Reset';setTimeout(()=>b.innerHTML='&#8634; Reset Origin',2000)})" title="Set the current position as the new zero reference for relative mode" style="margin-left:8px;font-size:0.75rem;background:var(--accent);color:var(--text);border:1px solid rgba(255,255,255,0.1);padding:2px 8px;border-radius:4px;cursor:pointer">&#8634; Reset Origin</button>
      </div>
      <div style="margin-top:12px;border-top:1px solid rgba(255,255,255,0.1);padding-top:8px">
        <label style="font-size:0.8rem;color:var(--blue)">Burst Mode</label>
        <div style="display:flex;gap:8px;align-items:center;margin-top:4px;flex-wrap:wrap">
          <div style="display:flex;flex-direction:column;gap:2px">
            <label style="font-size:0.75rem;color:var(--green)">Count</label>
            <input id="burstCount" type="number" min="1" max="1000" value="1" style="width:80px">
          </div>
          <div style="display:flex;flex-direction:column;gap:2px">
            <label style="font-size:0.75rem;color:var(--green)">Interval (ms)</label>
            <input id="burstInterval" type="number" min="10" max="5000" value="100" step="10" style="width:100px">
          </div>
          <span id="burstStatus" style="font-size:0.75rem;color:var(--yellow)"></span>
        </div>
      </div>
      <div style="margin-top:12px;border-top:1px solid rgba(255,255,255,0.1);padding-top:8px">
        <label style="font-size:0.8rem;color:var(--blue);cursor:pointer" onclick="document.getElementById('seqPanel').style.display=document.getElementById('seqPanel').style.display==='none'?'':'none'">
          Sequence Builder <span style="font-size:0.7rem">(click to expand)</span>
        </label>
        <div id="seqPanel" style="display:none;margin-top:8px">
          <p style="font-size:0.75rem;color:#888;margin-bottom:6px">Build a sequence of commands with different parameters. Click "Add Current" to add the current command/params to the sequence.</p>
          <div style="display:flex;gap:4px;margin-bottom:8px">
            <button onclick="addToSequence()" style="font-size:0.75rem;background:var(--accent);color:var(--text);border:1px solid rgba(255,255,255,0.1);padding:4px 10px;border-radius:4px;cursor:pointer">+ Add Current</button>
            <button onclick="clearSequence()" style="font-size:0.75rem;background:var(--accent);color:var(--text);border:1px solid rgba(255,255,255,0.1);padding:4px 10px;border-radius:4px;cursor:pointer">Clear</button>
            <button onclick="runSequence()" style="font-size:0.75rem;background:var(--green);color:var(--bg);border:none;padding:4px 10px;border-radius:4px;cursor:pointer;font-weight:bold">Run Sequence</button>
          </div>
          <div style="display:flex;gap:8px;align-items:center;margin-bottom:6px">
            <label style="font-size:0.75rem;color:var(--green)">Delay between (ms)</label>
            <input id="seqDelay" type="number" min="0" max="5000" value="100" step="10" style="width:80px">
            <label style="font-size:0.75rem;color:var(--green);cursor:pointer"><input type="checkbox" id="seqLoop"> Loop</label>
            <label style="font-size:0.75rem;color:var(--green);cursor:pointer"><input type="checkbox" id="seqParallel"> Parallel (no delay)</label>
          </div>
          <div id="seqList" style="font-size:0.75rem;font-family:monospace;max-height:150px;overflow-y:auto"></div>
          <span id="seqStatus" style="font-size:0.75rem;color:var(--yellow)"></span>
        </div>
      </div>
    </div>
    <div class="card">
      <h2 style="cursor:pointer;user-select:none" onclick="toggleStats()">Statistics <span id="statsToggle" style="font-size:0.8rem">▾</span></h2>
      <div id="stats"><table><tbody id="statsBody"></tbody></table></div>
    </div>
    <div class="card" style="margin-top:16px">
      <h2>Latest Values</h2>
      <div id="latestValues"><table><tbody id="latestBody"></tbody></table></div>
    </div>
  </div>
</div>
<script>
let activeFilter = 'all';
let filterText = '';
let filterModules = new Set(); // empty = all modules
let filterCanIds = []; // empty = all CAN IDs
const feed = document.getElementById('feed');
const maxLines = 200;
const stats = {};
const latestValues = {};
let autoScroll = true;
let lastMsgTime = 0;
let paused = false;
let msgBuffer = [];
let dirty = false;
let burstTimer = null;
// Store all rendered messages for retroactive filtering
let allMessages = [];

// Detect if user scrolled up — pause auto-scroll
feed.addEventListener('scroll', () => {
  const atBottom = feed.scrollHeight - feed.scrollTop - feed.clientHeight < 30;
  autoScroll = atBottom;
  document.getElementById('scrollHint').textContent =
    atBottom ? '' : '⏸ Auto-scroll paused — scroll to bottom to resume';
});

// Module-specific command sets
const CMDS_ARM = [
  { group: 'Traction', items: [
    { val: 'traction', label: 'Traction Motors — Set wheel speeds' },
  ]},
  { group: 'Robotic Arm (MOD1)', items: [
    { val: 'arm_1a1b', label: 'Arm J1 (Differential Pitch) — Base shoulder pair' },
    { val: 'arm_j2', label: 'Arm J2 (Elbow Pitch) — Single motor elbow' },
    { val: 'arm_j3', label: 'Arm J3 (Roll) — Forearm rotation' },
    { val: 'arm_j4', label: 'Arm J4 (Wrist Pitch) — Wrist up/down' },
    { val: 'arm_j5', label: 'Arm J5 (Wrist Roll) — Wrist rotation' },
    { val: 'beak_close', label: 'Beak Close — Gripper close' },
    { val: 'beak_open', label: 'Beak Open — Gripper open' },
    { val: 'reset_arm', label: 'Reset Arm — Move to home position' },
    { val: 'reboot_arm', label: 'Reboot Arm — Restart Dynamixel motors' },
    { val: 'set_home', label: 'Set Home — Save current position as home' },
  ]},
  { group: 'System', items: [
    { val: 'torque', label: 'Torque Enable/Disable — Per-motor torque control (bitfield)' },
    { val: 'reboot_traction', label: 'Reboot Traction — Restart DC motors' },
    { val: 'stop_all', label: 'Emergency Stop — Zero all motors' },
  ]},
];

const CMDS_JOINT = [
  { group: 'Traction', items: [
    { val: 'traction', label: 'Traction Motors — Set wheel speeds' },
  ]},
  { group: 'Inter-Module Joint', items: [
    { val: 'joint_1a1b', label: 'Joint Pitch (Differential) — Pitch/yaw pair' },
    { val: 'joint_roll', label: 'Joint Roll — Axial rotation' },
  ]},
  { group: 'System', items: [
    { val: 'torque', label: 'Torque Enable/Disable — Per-motor torque control (bitfield)' },
    { val: 'reboot_traction', label: 'Reboot Traction — Restart DC motors' },
    { val: 'stop_all', label: 'Emergency Stop — Zero all motors' },
  ]},
];

// Command metadata: description, field labels, numeric input count, step size
const CMD_INFO = {
  traction:    { desc: 'Set traction motor speeds. Positive = forward, negative = reverse. Typical range: -200 to 200 RPM.',
                 lbl1: 'Left RPM', lbl2: 'Right RPM', inputs: 2, step: 5 },
  arm_1a1b:   { desc: 'Set arm J1 differential shoulder joint. Theta controls yaw, phi controls pitch.',
                 lbl1: 'Theta / Yaw (rad)', lbl2: 'Phi / Pitch (rad)', inputs: 2, step: 0.05 },
  arm_j2:     { desc: 'Set arm elbow pitch (J2, Dynamixel XM540).',
                 lbl1: 'Angle (rad)', lbl2: '', inputs: 1, step: 0.05 },
  arm_j3:     { desc: 'Set arm forearm roll (J3, Dynamixel XM540).',
                 lbl1: 'Angle (rad)', lbl2: '', inputs: 1, step: 0.05 },
  arm_j4:     { desc: 'Set arm wrist pitch (J4, Dynamixel XL430).',
                 lbl1: 'Angle (rad)', lbl2: '', inputs: 1, step: 0.05 },
  arm_j5:     { desc: 'Set arm wrist roll (J5, Dynamixel XL430).',
                 lbl1: 'Angle (rad)', lbl2: '', inputs: 1, step: 0.05 },
  beak_close: { desc: 'Close the beak/gripper. No parameters needed — sends close command immediately.', lbl1: '', lbl2: '', inputs: 0, step: 1 },
  beak_open:  { desc: 'Open the beak/gripper. No parameters needed — sends open command immediately.', lbl1: '', lbl2: '', inputs: 0, step: 1 },
  reset_arm:  { desc: 'Move all arm joints to their home position. Reads current positions, then slowly returns to zero.', lbl1: '', lbl2: '', inputs: 0, step: 1 },
  reboot_arm: { desc: 'Reboot all arm Dynamixel motors via protocol command. Use when motors are in error state.', lbl1: '', lbl2: '', inputs: 0, step: 1 },
  set_home:   { desc: 'Set current arm position as new home. Check "Permanent" to persist across power cycles.', lbl1: '', lbl2: '', inputs: 0, step: 1, hasOptions: true },
  reboot_traction: { desc: 'Reboot traction DC motors. Sends a reboot command to the traction controller.', lbl1: '', lbl2: '', inputs: 0, step: 1 },
  stop_all:   { desc: 'Emergency stop — sends zero speed to all traction motors on all modules.', lbl1: '', lbl2: '', inputs: 0, step: 1 },
  torque:     { desc: 'Enable or disable torque for a single motor.',
                 lbl1: 'Motor', lbl2: 'State', inputs: 'torque', step: 1 },
  joint_1a1b: { desc: 'Set inter-module joint differential pitch/yaw. Theta controls yaw, phi controls pitch. Range: approx -1.57 to 1.57 rad.',
                 lbl1: 'Theta / Yaw (rad)', lbl2: 'Phi / Pitch (rad)', inputs: 2, step: 0.05 },
  joint_roll: { desc: 'Set inter-module joint axial roll. Range: approx -3.14 to 3.14 rad.',
                 lbl1: 'Angle (rad)', lbl2: '', inputs: 1, step: 0.05 },
};

// Motor options for torque command — per module type
// Each entry: { bit: <bit number>, name: <human readable> }
const TORQUE_MOTORS_ARM = [
  { bit: 0, name: 'Right Traction' },
  { bit: 1, name: 'Left Traction' },
  { bit: 2, name: 'Arm J1a (Shoulder Pitch)' },
  { bit: 3, name: 'Arm J1b (Shoulder Yaw)' },
  { bit: 4, name: 'Arm J2 (Elbow Pitch)' },
  { bit: 5, name: 'Arm J3 (Forearm Roll)' },
  { bit: 6, name: 'Arm J4 (Wrist Pitch)' },
  { bit: 7, name: 'Arm J5 (Wrist Roll)' },
  { bit: 8, name: 'Arm J6 (Beak/Gripper)' },
];
const TORQUE_MOTORS_JOINT = [
  { bit: 0, name: 'Right Traction' },
  { bit: 1, name: 'Left Traction' },
  { bit: 2, name: 'Joint J1-Left (Pitch)' },
  { bit: 3, name: 'Joint J1-Right (Yaw)' },
  { bit: 4, name: 'Joint J2 (Roll)' },
];
const TORQUE_STATES = [
  { val: 1, label: 'ON (enable torque)' },
  { val: 0, label: 'OFF (disable torque)' },
];

// Build command dropdown based on selected module
function rebuildCommandDropdown() {
  const mod = document.getElementById('targetModule').value;
  const isArm = mod === '0x21';  // MK2_MOD1 has arm
  const cmds = isArm ? CMDS_ARM : CMDS_JOINT;
  const sel = document.getElementById('cmdType');
  const prev = sel.value;
  sel.innerHTML = '';
  cmds.forEach(g => {
    const og = document.createElement('optgroup');
    og.label = g.group;
    g.items.forEach(it => {
      const o = document.createElement('option');
      o.value = it.val; o.textContent = it.label;
      og.appendChild(o);
    });
    sel.appendChild(og);
  });
  // Try to preserve previous selection
  if ([...sel.options].some(o => o.value === prev)) sel.value = prev;
  updateForm();
  rebuildTorqueMotors();
}

document.getElementById('targetModule').addEventListener('change', function() {
  rebuildCommandDropdown();
  rebuildTorqueMotors();
});

function updateForm() {
  const cmd = document.getElementById('cmdType').value;
  const info = CMD_INFO[cmd] || { desc: '', lbl1: '', lbl2: '', inputs: 0, step: 1 };
  document.getElementById('cmdDesc').textContent = info.desc;
  const f1 = document.getElementById('field1');
  const f2 = document.getElementById('field2');
  const l1 = document.getElementById('lbl1');
  const l2 = document.getElementById('lbl2');
  const v1 = document.getElementById('val1');
  const v2 = document.getElementById('val2');
  const group = document.getElementById('inputGroup');
  const torqueDropdowns = document.getElementById('torqueDropdowns');

  if (info.inputs === 'torque') {
    // Show torque dropdowns, hide normal inputs
    group.style.display = 'none';
    if (torqueDropdowns) torqueDropdowns.style.display = '';
  } else {
    // Show normal inputs, hide torque dropdowns
    l1.textContent = info.lbl1;
    l2.textContent = info.lbl2;
    v1.step = info.step || 1;
    v2.step = info.step || 1;
    f1.style.display = info.inputs >= 1 ? '' : 'none';
    f2.style.display = info.inputs >= 2 ? '' : 'none';
    group.style.display = info.inputs > 0 ? '' : 'none';
    if (torqueDropdowns) torqueDropdowns.style.display = 'none';
  }
  // Show permanent checkbox for set_home
  document.getElementById('cmdOptions').style.display = info.hasOptions ? '' : 'none';
  // Show relative mode for arm/joint angle commands
  const isAngle = cmd.startsWith('arm_') || cmd.startsWith('joint_');
  document.getElementById('relativeMode').style.display = (isAngle && info.inputs > 0) ? '' : 'none';
  // Show torque quick-preset buttons
  const torquePresets = document.getElementById('torquePresets');
  if (torquePresets) torquePresets.style.display = (cmd === 'torque') ? '' : 'none';
}
rebuildCommandDropdown();  // set initial state

// Filter buttons
document.getElementById('filterBar').addEventListener('click', e => {
  if (e.target.dataset.filter) {
    document.querySelectorAll('.filter-bar button').forEach(b => b.classList.remove('active'));
    e.target.classList.add('active');
    activeFilter = e.target.dataset.filter;
    applyFilters();
  }
});

// Toggle module filter button (multi-select)
function toggleModuleFilter(btn) {
  const mod = btn.dataset.module;
  if (mod === '') {
    // "All" clicked — deselect everything else
    filterModules.clear();
    document.querySelectorAll('.module-filter-btn').forEach(b => {
      b.style.background = 'var(--accent)';
      b.style.color = 'var(--text)';
      b.style.borderColor = 'rgba(255,255,255,0.1)';
    });
    btn.style.background = 'var(--green)';
    btn.style.color = 'var(--bg)';
    btn.style.borderColor = 'var(--green)';
  } else {
    // Toggle specific module
    const allBtn = document.querySelector('.module-filter-btn[data-module=""]');
    if (filterModules.has(mod)) {
      filterModules.delete(mod);
      btn.style.background = 'var(--accent)';
      btn.style.color = 'var(--text)';
      btn.style.borderColor = 'rgba(255,255,255,0.1)';
    } else {
      filterModules.add(mod);
      btn.style.background = 'var(--green)';
      btn.style.color = 'var(--bg)';
      btn.style.borderColor = 'var(--green)';
    }
    // If no modules selected, activate "All"
    if (filterModules.size === 0) {
      allBtn.style.background = 'var(--green)';
      allBtn.style.color = 'var(--bg)';
      allBtn.style.borderColor = 'var(--green)';
    } else {
      allBtn.style.background = 'var(--accent)';
      allBtn.style.color = 'var(--text)';
      allBtn.style.borderColor = 'rgba(255,255,255,0.1)';
    }
  }
  applyFilters();
}

// Combined filter function — applies category + text + modules + CAN IDs
function applyFilters() {
  filterText = (document.getElementById('filterSearch') || {value: ''}).value.toLowerCase();
  // Parse CAN IDs from comma-separated hex input
  const canIdRaw = (document.getElementById('filterCanId') || {value: ''}).value.trim();
  filterCanIds = canIdRaw ? canIdRaw.split(',').map(s => s.trim().toLowerCase().replace(/^0x/, '')).filter(s => s.length > 0) : [];
  renderFilteredFeed();
}

function renderFilteredFeed() {
  feed.innerHTML = '';
  for (const d of allMessages) {
    if (!messageMatchesFilters(d)) continue;
    const div = document.createElement('div');
    div.className = 'msg';
    div.innerHTML = `<span class="src">${d.source}</span> → `
      + `<span class="dst">${d.destination}</span> `
      + `<span class="type">[${d.typeHex}] ${d.msg_name}</span> `
      + `<span class="val">${d.payload_str}</span>`;
    feed.appendChild(div);
  }
  while (feed.children.length > maxLines) feed.removeChild(feed.firstChild);
  if (autoScroll) feed.scrollTop = feed.scrollHeight;
}

function messageMatchesFilters(d) {
  // Category filter
  if (activeFilter !== 'all' && !d.filter_tags.includes(activeFilter)) return false;
  // Text search — matches against msg_name, source, destination, payload_str, typeHex
  if (filterText) {
    const haystack = (d.msg_name + ' ' + d.source + ' ' + d.destination + ' ' + d.payload_str + ' ' + d.typeHex).toLowerCase();
    if (!haystack.includes(filterText)) return false;
  }
  // Module filter — multi-select: message matches if source OR destination is in selected modules
  if (filterModules.size > 0) {
    const srcHex = (d.source_hex || '').toLowerCase();
    const dstHex = (d.destination_hex || '').toLowerCase();
    let modMatch = false;
    for (const mod of filterModules) {
      if (srcHex === mod || dstHex === mod) { modMatch = true; break; }
    }
    if (!modMatch) return false;
  }
  // CAN ID filter — matches against the PF (PDU Format / message type) field only.
  // Entering "22" matches all MOTOR_FEEDBACK messages (PF=0x22), regardless of source/destination.
  // Comma-separated for multiple PF values. Exact 2-hex-digit match.
  if (filterCanIds.length > 0) {
    const pfHex = (d.msg_type || 0).toString(16).toLowerCase().padStart(2, '0');
    if (!filterCanIds.includes(pfHex)) return false;
  }
  return true;
}

function clearFeed() {
  allMessages = [];
  feed.innerHTML = '';
}

// Statistics toggle
function toggleStats() {
  const s = document.getElementById('stats');
  const t = document.getElementById('statsToggle');
  if (s.style.display === 'none') { s.style.display = ''; t.textContent = '▾'; }
  else { s.style.display = 'none'; t.textContent = '▸'; }
}

// Connection status based on actual CAN message flow
let connTimeout = null;
function markConnected() {
  lastMsgTime = Date.now();
  document.getElementById('connDot').className = 'status-dot on';
  document.getElementById('connText').textContent = 'Receiving CAN data';
  clearTimeout(connTimeout);
  connTimeout = setTimeout(() => {
    document.getElementById('connDot').className = 'status-dot off';
    document.getElementById('connText').textContent = 'No CAN data (idle > 5 s)';
  }, 5000);
}

// Poll for new CAN messages
let lastMsgId = 0;
let pollInterval = null;

function startPolling() {
  if (pollInterval) return;
  pollInterval = setInterval(fetchMessages, 200);
}

function stopPolling() {
  if (pollInterval) {
    clearInterval(pollInterval);
    pollInterval = null;
  }
}

async function fetchMessages() {
  try {
    const r = await fetch('/messages?since=' + lastMsgId);
    if (!r.ok) return;
    const data = await r.json();
    if (!data.messages || data.messages.length === 0) return;
    for (const d of data.messages) {
      lastMsgId = Math.max(lastMsgId, d._id || 0);
      markConnected();
      const typeHex = '0x' + d.msg_type.toString(16).toUpperCase().padStart(2, '0');
      const key = d.msg_name + ' [' + typeHex + ']';
      stats[key] = (stats[key] || 0) + 1;
      latestValues[d.msg_name] = d.payload_str;
      msgBuffer.push({...d, typeHex});
      dirty = true;
    }
  } catch(e) {
    // ignore network errors
  }
}

// Start polling when page loads
window.addEventListener('load', startPolling);

function updateStats() {
  const body = document.getElementById('statsBody');
  body.innerHTML = Object.entries(stats).sort((a,b) => b[1]-a[1])
    .map(([k,v]) => `<tr><td>${k}</td><td>${v}</td></tr>`).join('');
}

function updateLatestValues() {
  const body = document.getElementById('latestBody');
  body.innerHTML = Object.entries(latestValues).sort((a,b) => a[0].localeCompare(b[0]))
    .map(([k,v]) => `<tr><td>${k}</td><td>${v || '\u2014'}</td></tr>`).join('');
}

function togglePause() {
  paused = !paused;
  const btn = document.getElementById('pauseBtn');
  btn.textContent = paused ? '\u25b6 Resume' : '\u23f8 Pause';
  if (paused) {
    btn.style.background = 'var(--green)';
    btn.style.color = 'var(--bg)';
    btn.style.borderColor = 'var(--green)';
  } else {
    btn.style.background = 'var(--accent)';
    btn.style.color = 'var(--text)';
    btn.style.borderColor = 'rgba(255,255,255,0.1)';
  }
}

// Batch DOM updates every 100 ms for performance
setInterval(() => {
  if (!dirty && msgBuffer.length === 0) return;
  dirty = false;
  updateStats();
  updateLatestValues();
  if (!paused) {
    for (const d of msgBuffer) {
      allMessages.push(d);
    }
    while (allMessages.length > maxLines) allMessages.shift();
    renderFilteredFeed();
  }
  msgBuffer = [];
}, 100);

function sendOne(cmd, target, v1, v2, opts) {
  fetch('/send', {method:'POST', headers:{'Content-Type':'application/json'},
    body: JSON.stringify({command: cmd, target: target, val1: v1, val2: v2, ...opts})
  }).then(r => r.json()).then(d => { if(d.error) alert(d.error); });
}

// ── Torque bitfield state tracking ──
// Tracks the current bitfield per module so we can toggle individual motors
const torqueState = { '0x21': 0xFF, '0x22': 0xFF, '0x23': 0xFF }; // Start all enabled

// Populate torque motor dropdown based on selected module
function rebuildTorqueMotors() {
  const mod = document.getElementById('targetModule').value;
  const isArm = mod === '0x21';
  const motors = isArm ? TORQUE_MOTORS_ARM : TORQUE_MOTORS_JOINT;
  const sel = document.getElementById('torqueMotor');
  if (!sel) return;
  sel.innerHTML = '';
  motors.forEach(m => {
    const o = document.createElement('option');
    o.value = m.bit; o.textContent = m.name;
    sel.appendChild(o);
  });
}

function sendCommand() {
  const cmd = document.getElementById('cmdType').value;
  const target = parseInt(document.getElementById('targetModule').value);
  const count = parseInt(document.getElementById('burstCount').value) || 1;
  const interval = parseInt(document.getElementById('burstInterval').value) || 100;
  const opts = {};
  if (document.getElementById('optPermanent')) {
    if (document.getElementById('optPermanent').checked) opts.permanent = true;
  }
  if (document.getElementById('optRelative')) {
    if (document.getElementById('optRelative').checked) opts.relative = true;
  }

  let v1, v2;
  if (cmd === 'torque') {
    const motorBit = parseInt(document.getElementById('torqueMotor').value) || 0;
    const state = parseInt(document.getElementById('torqueState').value) || 0;
    const targetHex = '0x' + target.toString(16);
    if (state) {
      torqueState[targetHex] |= (1 << motorBit);
    } else {
      torqueState[targetHex] &= ~(1 << motorBit);
    }
    v1 = torqueState[targetHex];
    v2 = 0;
  } else {
    v1 = parseFloat(document.getElementById('val1').value) || 0;
    v2 = parseFloat(document.getElementById('val2').value) || 0;
  }

  if (count <= 1) { sendOne(cmd, target, v1, v2, opts); return; }

  if (burstTimer) { clearInterval(burstTimer); burstTimer = null; }
  const status = document.getElementById('burstStatus');
  let sent = 0;
  sendOne(cmd, target, v1, v2, opts);
  sent++;
  status.textContent = 'Sending ' + sent + '/' + count + '...';

  burstTimer = setInterval(() => {
    sendOne(cmd, target, v1, v2, opts);
    sent++;
    status.textContent = 'Sending ' + sent + '/' + count + '...';
    if (sent >= count) {
      clearInterval(burstTimer);
      burstTimer = null;
      status.textContent = 'Done \u2014 sent ' + count + ' messages';
      setTimeout(() => status.textContent = '', 5000);
    }
  }, interval);
}

function stopAll() {
  if (burstTimer) { clearInterval(burstTimer); burstTimer = null; }
  if (seqTimer) { clearTimeout(seqTimer); seqTimer = null; seqRunning = false; }
  document.getElementById('burstStatus').textContent = '';
  document.getElementById('seqStatus').textContent = '';
  fetch('/stop', {method:'POST'}).then(r => r.json());
}

// ── Sequence Builder ──
let cmdSequence = [];
let seqTimer = null;
let seqRunning = false;

function addToSequence() {
  const cmd = document.getElementById('cmdType').value;
  const target = parseInt(document.getElementById('targetModule').value);
  const v1 = parseFloat(document.getElementById('val1').value) || 0;
  const v2 = parseFloat(document.getElementById('val2').value) || 0;
  const info = CMD_INFO[cmd] || {};
  cmdSequence.push({cmd, target, v1, v2});
  renderSequence();
}

function clearSequence() { cmdSequence = []; renderSequence(); }

function removeFromSequence(i) { cmdSequence.splice(i, 1); renderSequence(); }

function renderSequence() {
  const el = document.getElementById('seqList');
  if (cmdSequence.length === 0) { el.innerHTML = '<span style="color:#666">No commands queued</span>'; return; }
  el.innerHTML = cmdSequence.map((s, i) => {
    const info = CMD_INFO[s.cmd] || { lbl1: 'v1', lbl2: 'v2', inputs: 0 };
    let params = '';
    if (info.inputs >= 1) params += ` ${info.lbl1 || 'v1'}=${s.v1}`;
    if (info.inputs >= 2) params += ` ${info.lbl2 || 'v2'}=${s.v2}`;
    return `<div style="padding:2px 0;border-bottom:1px solid rgba(255,255,255,0.05);">`
    + `<span style="color:var(--blue)">${i+1}.</span> `
    + `<span style="color:var(--green)">${s.cmd}</span>`
    + `<span style="color:#ccc">${params}</span> → 0x${s.target.toString(16)} `
    + `<a href="#" onclick="removeFromSequence(${i});return false" style="color:var(--red);text-decoration:none">✕</a>`
    + `</div>`;
  }).join('');
}
renderSequence();

function runSequence() {
  if (cmdSequence.length === 0) return;
  const delay = parseInt(document.getElementById('seqDelay').value) || 100;
  const loop = document.getElementById('seqLoop').checked;
  const parallel = document.getElementById('seqParallel').checked;
  const status = document.getElementById('seqStatus');

  if (parallel) {
    cmdSequence.forEach(s => sendOne(s.cmd, s.target, s.v1, s.v2, {}));
    status.textContent = 'Sent ' + cmdSequence.length + ' commands in parallel';
    if (loop) { seqTimer = setTimeout(runSequence, delay); seqRunning = true; }
    return;
  }

  seqRunning = true;
  let idx = 0;
  function next() {
    if (!seqRunning) return;
    if (idx >= cmdSequence.length) {
      if (loop) { idx = 0; } else { status.textContent = 'Sequence complete'; seqRunning = false; return; }
    }
    const s = cmdSequence[idx];
    sendOne(s.cmd, s.target, s.v1, s.v2, {});
    status.textContent = 'Step ' + (idx+1) + '/' + cmdSequence.length;
    idx++;
    seqTimer = setTimeout(next, delay);
  }
  next();
}
</script>
</body></html>"""


# ─── Determine filter tags for a message type ───────────────────────────────

def get_filter_tags(msg_type: int) -> list[str]:
    """Return which named filters this message type belongs to."""
    tags = []
    for name, filter_set in NAMED_FILTERS.items():
        if filter_set is None:
            continue
        if msg_type in filter_set:
            tags.append(name)
    return tags


# ─── Routes ──────────────────────────────────────────────────────────────────

@app.route("/")
def index():
    response = make_response(render_template_string(DASHBOARD_HTML))
    response.headers["Cache-Control"] = "no-store, no-cache, must-revalidate, max-age=0"
    response.headers["Pragma"] = "no-cache"
    response.headers["Expires"] = "0"
    response.headers["X-Version"] = str(time.time())
    return response


@app.route("/status")
def status():
    """Return the last received CAN message for debugging."""
    global _last_recv_msg
    with _last_recv_lock:
        if _last_recv_msg is None:
            return jsonify({"status": "no_messages", "message": "No CAN messages received yet"})
        return jsonify({"status": "ok", "last_msg": _last_recv_msg})

@app.route("/messages")
def messages():
    """Polling endpoint: return messages since given ID."""
    since_id = request.args.get('since', 0, type=int)
    with _msg_buffer_lock:
        # Return messages with _id > since_id
        new_msgs = [m for m in _msg_buffer if m.get('_id', 0) > since_id]
    return jsonify({"messages": new_msgs})

@app.route("/stream")
def stream():
    """Server-Sent Events stream for live CAN messages."""
    q: Queue = Queue(maxsize=100)
    with event_queues_lock:
        event_queues.append(q)

    def generate():
        try:
            while True:
                try:
                    data = q.get(timeout=30)
                    yield f"data: {json.dumps(data)}\n\n"
                except Empty:
                    yield ": keepalive\n\n"
        except GeneratorExit:
            pass
        finally:
            with event_queues_lock:
                if q in event_queues:
                    event_queues.remove(q)

    response = Response(generate(), mimetype="text/event-stream")
    response.headers["Cache-Control"] = "no-cache"
    response.headers["X-Accel-Buffering"] = "no"
    response.headers["Connection"] = "keep-alive"
    return response


@app.route("/send", methods=["POST"])
def send_command():
    """Handle send command from web UI."""
    data = request.get_json(silent=True)
    if not data:
        return jsonify({"error": "Missing or invalid JSON payload"}), 400
    cmd = data.get("command", "")
    dest = int(data.get("target", ModuleAddress.MK2_MOD1))
    v1 = float(data.get("val1", 0))
    v2 = float(data.get("val2", 0))
    permanent = data.get("permanent", False)
    relative = data.get("relative", False)

    # Relative mode: accumulate deltas into absolute positions
    if relative and cmd in _positions:
        _positions[cmd][0] += v1
        _positions[cmd][1] += v2
        v1, v2 = _positions[cmd]
    elif relative:
        _positions[cmd] = [v1, v2]
    else:
        _positions[cmd] = [v1, v2]

    try:
        if cmd == "traction":
            sender.traction(v1, v2, destination=dest)
        elif cmd == "arm_j2":
            sender.arm_pitch_j2(v1)
        elif cmd == "arm_j3":
            sender.arm_roll_j3(v1)
        elif cmd == "arm_j4":
            sender.arm_pitch_j4(v1)
        elif cmd == "arm_j5":
            sender.arm_roll_j5(v1)
        elif cmd == "arm_1a1b":
            sender.arm_pitch_1a1b(v1, v2)
        elif cmd == "beak_close":
            sender.arm_beak(close=True)
        elif cmd == "beak_open":
            sender.arm_beak(close=False)
        elif cmd == "reset_arm":
            sender.reset_arm()
        elif cmd == "reboot_arm":
            sender.reboot_arm()
        elif cmd == "set_home":
            sender.set_home(persist=permanent)
        elif cmd == "reboot_traction":
            sender.reboot_traction(destination=dest)
        elif cmd == "joint_1a1b":
            sender.joint_pitch_1a1b(v1, v2, destination=dest)
        elif cmd == "joint_roll":
            sender.joint_roll(v1, destination=dest)
        elif cmd == "torque":
            # Accept hex (0x...) or decimal bitfield
            bitfield = int(data.get("val1", 0))
            sender.torque_enable(bitfield, destination=dest)
        elif cmd == "stop_all":
            sender.stop_all()
        else:
            return jsonify({"error": f"Unknown command: {cmd}"}), 400

        return jsonify({"ok": True})
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@app.route("/reset_positions", methods=["POST"])
def reset_positions():
    """Reset accumulated relative positions to zero."""
    _positions.clear()
    return jsonify({"ok": True})


@app.route("/stop", methods=["POST"])
def stop_all():
    """Emergency stop all motors."""
    sender.stop_all()
    return jsonify({"ok": True})


# ─── CAN → SSE bridge ───────────────────────────────────────────────────────

def can_to_sse_callback(decoded_id, payload, raw_msg):
    """Push decoded CAN messages to all SSE clients."""
    payload_str = "  ".join(
        f"{k}={v:.4f}" if isinstance(v, float) else f"{k}={v}"
        for k, v in payload.items()
        if not k.startswith("_")
    )

    event = {
        "source": decoded_id.source_name,
        "source_hex": f"0x{decoded_id.source:02X}",
        "destination": decoded_id.destination_name,
        "destination_hex": f"0x{decoded_id.destination:02X}",
        "msg_name": decoded_id.msg_name,
        "msg_type": decoded_id.msg_type,
        "can_id": raw_msg.arbitration_id,
        "payload_str": payload_str,
        "payload": {k: v for k, v in payload.items() if not k.startswith("_")},
        "filter_tags": get_filter_tags(decoded_id.msg_type),
        "timestamp": raw_msg.timestamp or time.time(),
    }

    with event_queues_lock:
        for q in event_queues:
            try:
                q.put_nowait(event)
            except Full:
                pass  # Drop if client is slow
    
    # Track last received message
    global _last_recv_msg
    with _last_recv_lock:
        _last_recv_msg = event
    
    # Store in polling buffer
    global _msg_id_counter
    with _msg_buffer_lock:
        _msg_id_counter += 1
        event_with_id = dict(event)
        event_with_id['_id'] = _msg_id_counter
        _msg_buffer.append(event_with_id)
        # Keep only last 500 messages
        if len(_msg_buffer) > 500:
            _msg_buffer.pop(0)


# ─── Main ────────────────────────────────────────────────────────────────────

def main():
    global bus, sender, monitor

    parser = argparse.ArgumentParser(description="STM32LowLevel CAN Web Dashboard")
    parser.add_argument("--interface", "-i", default="gs_usb")
    parser.add_argument("--channel", "-c", default="0")
    parser.add_argument("--bitrate", "-b", type=int, default=1000000,
                        help="CAN arbitration bitrate in bps (default: 1000000 = 1 Mbit/s)")
    parser.add_argument("--data-bitrate", "-D", type=int, default=2000000,
                        help="CAN FD data-phase bitrate in bps (default: 2000000 = 2 Mbit/s)")
    parser.add_argument("--port", "-p", type=int, default=8080)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument(
        "--demo", action="store_true",
        help="Use virtual CAN bus for GUI preview (no hardware needed)",
    )
    parser.add_argument(
        "--debug", action="store_true",
        help="Enable debug output: print received CAN messages to terminal",
    )
    args = parser.parse_args()

    iface = "virtual" if args.demo else args.interface
    channel = "demo" if args.demo else args.channel

    print(f"Connecting to CAN bus: interface={iface}, "
          f"channel={channel}, bitrate={args.bitrate}, data_bitrate={args.data_bitrate}...")
    if args.demo:
        print("  (demo mode — no real CAN hardware required)")

    try:
        bus = can.Bus(
            interface=iface,
            channel=channel,
            bitrate=args.bitrate,
            data_bitrate=args.data_bitrate,
            fd=True,
        )
    except Exception as e:
        print(f"Failed to connect: {e}")
        sys.exit(1)

    sender = CanSender(bus)
    monitor = CanMonitor(bus)
    monitor.add_callback(can_to_sse_callback)

    # Start CAN monitor in background thread
    # quiet=True suppresses terminal output (use --debug to enable)
    monitor_thread = threading.Thread(
        target=monitor.run,
        kwargs={"quiet": not args.debug},
        daemon=True,
    )
    monitor_thread.start()
    if args.debug:
        print(f"  Debug mode: CAN messages will be printed to terminal", flush=True)

    # Start demo message generator if in demo mode
    if args.demo:
        def demo_generator():
            import random
            import time
            
            print("  Demo mode: Generating test messages...")
            time.sleep(1)  # Wait for monitor to start
            while True:
                try:
                    # Generate random messages for each module
                    for module_addr in [ModuleAddress.MK2_MOD1, ModuleAddress.MK2_MOD2, ModuleAddress.MK2_MOD3]:
                        # Motor feedback (two floats: right_rpm, left_rpm)
                        can_id = encode_can_id(
                            module_addr, ModuleAddress.CENTRAL,
                            MsgType.MOTOR_FEEDBACK
                        )
                        data = encode_payload(MsgType.MOTOR_FEEDBACK, right_rpm=random.uniform(-100, 100), left_rpm=random.uniform(-100, 100))
                        
                        decoded_id = decode_can_id(can_id)
                        payload = decode_payload(MsgType.MOTOR_FEEDBACK, data)
                        raw_msg = can.Message(arbitration_id=can_id, data=data, is_extended_id=True)
                        can_to_sse_callback(decoded_id, payload, raw_msg)
                        
                        if args.debug:
                            print(f"  Demo: {decoded_id.msg_name} from {decoded_id.source_name}")
                        
                        time.sleep(0.2)
                        
                        # Joint yaw feedback (one float)
                        can_id = encode_can_id(
                            module_addr, ModuleAddress.CENTRAL,
                            MsgType.JOINT_YAW_FEEDBACK
                        )
                        data = encode_payload(MsgType.JOINT_YAW_FEEDBACK, angle=random.uniform(-180, 180))
                        
                        decoded_id = decode_can_id(can_id)
                        payload = decode_payload(MsgType.JOINT_YAW_FEEDBACK, data)
                        raw_msg = can.Message(arbitration_id=can_id, data=data, is_extended_id=True)
                        can_to_sse_callback(decoded_id, payload, raw_msg)
                        
                        if args.debug:
                            print(f"  Demo: {decoded_id.msg_name} from {decoded_id.source_name}")
                        
                        time.sleep(0.2)
                        
                        # Battery voltage
                        can_id = encode_can_id(
                            module_addr, ModuleAddress.CENTRAL,
                            MsgType.BATTERY_VOLTAGE
                        )
                        data = encode_payload(MsgType.BATTERY_VOLTAGE, voltage=random.uniform(22.0, 29.0))
                        
                        decoded_id = decode_can_id(can_id)
                        payload = decode_payload(MsgType.BATTERY_VOLTAGE, data)
                        raw_msg = can.Message(arbitration_id=can_id, data=data, is_extended_id=True)
                        can_to_sse_callback(decoded_id, payload, raw_msg)
                        
                        if args.debug:
                            print(f"  Demo: {decoded_id.msg_name} from {decoded_id.source_name}")
                        
                        time.sleep(0.2)
                    
                    time.sleep(0.5)  # Pause between cycles
                except Exception as e:
                    print(f"  Demo generator error: {e}")
                    time.sleep(1)
        
        demo_thread = threading.Thread(target=demo_generator, daemon=True)
        demo_thread.start()

    print(f"Dashboard: http://localhost:{args.port}")
    if args.debug:
        print("  Debug mode: CAN messages will be printed to terminal")
    if args.demo:
        print("  Demo mode: Generating test messages...")
    print("Press Ctrl+C to stop.\n")

    try:
        app.run(host=args.host, port=args.port, threaded=True)
    except KeyboardInterrupt:
        pass
    finally:
        monitor.stop()
        bus.shutdown()


if __name__ == "__main__":
    main()
