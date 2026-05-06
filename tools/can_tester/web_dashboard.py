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

from flask import Flask, Response, request, jsonify, render_template_string

import can

from .protocol import ModuleAddress, MsgType, decode_can_id, MSG_NAMES
from .codec import decode_payload, format_decoded
from .sender import CanSender
from .monitor import CanMonitor, NAMED_FILTERS

app = Flask(__name__)

# Global state (initialized in main)
bus: can.BusABC = None  # type: ignore
sender: CanSender = None  # type: ignore
monitor: CanMonitor = None  # type: ignore
event_queues: list[Queue] = []
event_queues_lock = threading.Lock()

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
      <button data-filter="imu">IMU</button>
      <button data-filter="feedback">Feedback</button>
      <button id="pauseBtn" onclick="togglePause()" style="margin-left:12px">⏸ Pause</button>
    </div>
    <div id="feed"></div>
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
        <button onclick="sendCmd()">Send</button>
        <button class="danger" onclick="stopAll()">STOP ALL</button>
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
  joint_1a1b: { desc: 'Set inter-module joint differential pitch/yaw. Theta controls yaw, phi controls pitch. Range: approx -1.57 to 1.57 rad.',
                 lbl1: 'Theta / Yaw (rad)', lbl2: 'Phi / Pitch (rad)', inputs: 2, step: 0.05 },
  joint_roll: { desc: 'Set inter-module joint axial roll. Range: approx -3.14 to 3.14 rad.',
                 lbl1: 'Angle (rad)', lbl2: '', inputs: 1, step: 0.05 },
};

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
}

document.getElementById('targetModule').addEventListener('change', rebuildCommandDropdown);

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
  l1.textContent = info.lbl1;
  l2.textContent = info.lbl2;
  v1.step = info.step || 1;
  v2.step = info.step || 1;
  f1.style.display = info.inputs >= 1 ? '' : 'none';
  f2.style.display = info.inputs >= 2 ? '' : 'none';
  group.style.display = info.inputs > 0 ? '' : 'none';
  // Show permanent checkbox for set_home
  document.getElementById('cmdOptions').style.display = info.hasOptions ? '' : 'none';
  // Show relative mode for arm/joint angle commands
  const isAngle = cmd.startsWith('arm_') || cmd.startsWith('joint_');
  document.getElementById('relativeMode').style.display = (isAngle && info.inputs > 0) ? '' : 'none';
}
rebuildCommandDropdown();  // set initial state

// Filter buttons
document.getElementById('filterBar').addEventListener('click', e => {
  if (e.target.dataset.filter) {
    document.querySelectorAll('.filter-bar button').forEach(b => b.classList.remove('active'));
    e.target.classList.add('active');
    activeFilter = e.target.dataset.filter;
  }
});

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

// SSE
function connect() {
  const es = new EventSource('/stream');
  es.onopen = () => {
    // SSE channel open — but don't show "connected" until we get CAN data
  };
  es.onmessage = e => {
    const d = JSON.parse(e.data);
    markConnected();
    const typeHex = '0x' + d.msg_type.toString(16).toUpperCase().padStart(2, '0');
    const key = d.msg_name + ' [' + typeHex + ']';
    stats[key] = (stats[key] || 0) + 1;
    latestValues[d.msg_name] = d.payload_str;
    msgBuffer.push({...d, typeHex});
    dirty = true;
  };
  es.onerror = () => {
    document.getElementById('connDot').className = 'status-dot off';
    document.getElementById('connText').textContent = 'SSE disconnected — reconnecting...';
    es.close();
    setTimeout(connect, 2000);
  };
}
connect();

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
  btn.classList.toggle('active', paused);
}

// Batch DOM updates every 100 ms for performance
setInterval(() => {
  if (!dirty && msgBuffer.length === 0) return;
  dirty = false;
  updateStats();
  updateLatestValues();
  if (!paused) {
    const frag = document.createDocumentFragment();
    for (const d of msgBuffer) {
      if (activeFilter !== 'all' && !d.filter_tags.includes(activeFilter)) continue;
      const div = document.createElement('div');
      div.className = 'msg';
      div.innerHTML = `<span class="src">${d.source}</span> \u2192 `
        + `<span class="dst">${d.destination}</span> `
        + `<span class="type">[${d.typeHex}] ${d.msg_name}</span> `
        + `<span class="val">${d.payload_str}</span>`;
      frag.appendChild(div);
    }
    feed.appendChild(frag);
    while (feed.children.length > maxLines) feed.removeChild(feed.firstChild);
    if (autoScroll) feed.scrollTop = feed.scrollHeight;
  }
  msgBuffer = [];
}, 100);

function sendOne(cmd, target, v1, v2, opts) {
  fetch('/send', {method:'POST', headers:{'Content-Type':'application/json'},
    body: JSON.stringify({command: cmd, target: target, val1: v1, val2: v2, ...opts})
  }).then(r => r.json()).then(d => { if(d.error) alert(d.error); });
}

function sendCmd() {
  const cmd = document.getElementById('cmdType').value;
  const target = parseInt(document.getElementById('targetModule').value);
  const v1 = parseFloat(document.getElementById('val1').value) || 0;
  const v2 = parseFloat(document.getElementById('val2').value) || 0;
  const count = parseInt(document.getElementById('burstCount').value) || 1;
  const interval = parseInt(document.getElementById('burstInterval').value) || 100;
  const opts = {};
  if (document.getElementById('optPermanent').checked) opts.permanent = true;
  if (document.getElementById('optRelative').checked) opts.relative = true;

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
    return render_template_string(DASHBOARD_HTML)


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

    return Response(generate(), mimetype="text/event-stream")


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
        "destination": decoded_id.destination_name,
        "msg_name": decoded_id.msg_name,
        "msg_type": decoded_id.msg_type,
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
    monitor_thread = threading.Thread(
        target=monitor.run,
        kwargs={"quiet": True},
        daemon=True,
    )
    monitor_thread.start()

    print(f"Dashboard: http://localhost:{args.port}")
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
