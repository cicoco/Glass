# Glass MVP Arduino Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build an Arduino IDE-openable glasses-side MVP firmware and desktop receiver tool that follow the existing JuneGlass docs.

**Architecture:** The firmware uses `glass.ino` as a thin entrypoint and delegates to focused modules for session state, hardware capture, and TCP framing/sending. Runtime behavior follows the documented idle/active state machine and keeps protocol framing isolated from device control logic.

**Tech Stack:** Arduino for ESP32-S3, `esp_camera`, `ESP_I2S`, `WiFi`, Python 3 standard library

---

### Task 1: Create Arduino Project Skeleton

**Files:**
- Create: `firmware/glass/glass.ino`
- Create: `firmware/glass/config.h`
- Create: `firmware/glass/camera_pins.h`
- Create: `firmware/glass/app/app_state.h`
- Create: `firmware/glass/app/session_controller.h`
- Create: `firmware/glass/app/input_handler.h`
- Create: `firmware/glass/capture/camera_capture.h`
- Create: `firmware/glass/capture/audio_capture.h`
- Create: `firmware/glass/transport/frame_protocol.h`
- Create: `firmware/glass/transport/tcp_server.h`

- [ ] Define the Arduino entrypoint and shared config constants from the docs.
- [ ] Define enum/types for state, events, and protocol frame metadata.
- [ ] Define module interfaces so implementation files can stay focused.

### Task 2: Implement Transport and Protocol Framing

**Files:**
- Create: `firmware/glass/transport/frame_protocol.cpp`
- Create: `firmware/glass/transport/tcp_server.cpp`
- Modify: `firmware/glass/transport/frame_protocol.h`
- Modify: `firmware/glass/transport/tcp_server.h`

- [ ] Implement JG header serialization with little-endian fields and control/image/audio type support.
- [ ] Implement WiFi STA connect, TCP listener, single-client replacement, and safe send helpers.
- [ ] Ensure transport helpers expose enough status for serial diagnostics.

### Task 3: Implement Camera and Audio Capture

**Files:**
- Create: `firmware/glass/capture/camera_capture.cpp`
- Create: `firmware/glass/capture/audio_capture.cpp`
- Modify: `firmware/glass/capture/camera_capture.h`
- Modify: `firmware/glass/capture/audio_capture.h`

- [ ] Implement OV2640 init in idle profile and profile switching via deinit/reinit.
- [ ] Implement JPEG capture and failure counting/reinit path.
- [ ] Implement PDM microphone start/read/stop with fixed chunk sizing and last-chunk detection.

### Task 4: Implement Session and Input Logic

**Files:**
- Create: `firmware/glass/app/session_controller.cpp`
- Create: `firmware/glass/app/input_handler.cpp`
- Modify: `firmware/glass/app/app_state.h`
- Modify: `firmware/glass/app/session_controller.h`
- Modify: `firmware/glass/app/input_handler.h`

- [ ] Implement wake, active, timeout, and forced-end state transitions.
- [ ] Implement button debounce and serial commands `w`, `e`, `s`, `i`.
- [ ] Ensure control frame ordering and idle/active session id behavior match the docs.

### Task 5: Wire Main Loop and Add Receiver Tool

**Files:**
- Modify: `firmware/glass/glass.ino`
- Create: `firmware/glass/tools/recv_stream.py`

- [ ] Wire module initialization and periodic tick flow in `glass.ino`.
- [ ] Implement the Python receiver to connect, read 14-byte headers, print frame info, and optionally save JPEG payloads.
- [ ] Keep the firmware Arduino IDE friendly with relative includes and no build-system-only assumptions.

### Task 6: Final Review

**Files:**
- Review: `firmware/glass/**`
- Review: `firmware/glass/tools/recv_stream.py`

- [ ] Check protocol field sizes, state transitions, and config defaults against the docs.
- [ ] Check includes and file layout for Arduino IDE compatibility.
- [ ] Record any verification limitations if hardware compilation or live streaming cannot be exercised here.
