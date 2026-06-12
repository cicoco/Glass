# JuneGlass Glass MVP Arduino Design

## Scope

This design covers only the glasses-side MVP described in `docs/glass-requirement-mvp.md`, `docs/glass-implementation-mvp.md`, and `docs/transport-mvp.md`.

In scope:

- Arduino IDE-openable firmware for `Seeed XIAO ESP32S3 Sense`
- WiFi STA + TCP server on port `8765`
- OV2640 JPEG streaming in idle and active profiles
- PDM microphone capture during active sessions
- `IDLE_STREAM` / `SESSION_ACTIVE` / camera switching state handling
- Button and serial-triggered wake / forced session end
- Desktop-side `recv_stream.py` debug receiver for protocol validation

Out of scope:

- Phone app, STT, MiniCPM, TTS
- BLE transport
- On-device wake word
- ESP-IDF migration

## Architecture

The firmware will live under `firmware/glass/` with `glass.ino` as the Arduino entrypoint. `setup()` initializes serial, WiFi, TCP listening, camera idle profile, and runtime singletons. `loop()` remains lightweight and repeatedly services session/input processing plus streaming ticks.

The code is split into three module groups to match the existing docs:

- `app/`: state machine, session lifecycle, input handling
- `capture/`: camera setup/switching and audio setup/reading
- `transport/`: TCP client management, JG framing, outbound send helpers

This keeps hardware control, protocol code, and session policy isolated so later migration to ESP-IDF or BLE can reuse boundaries without refactoring core behavior.

## Runtime Flow

On boot, the device enters `APP_IDLE_STREAM`. In this state it sends `IMAGE` frames only, with idle flags and `session_id = 0`, at roughly 1 fps using QVGA JPEG settings.

When the boot button is pressed or serial `w` is received:

1. allocate a new non-zero `session_id`
2. send `CONTROL_WAKE`
3. switch camera profile from idle to active
4. start PDM audio capture
5. enter `APP_SESSION_ACTIVE`

While active, the firmware interleaves VGA JPEG frames and PCM audio chunks. When the active audio budget ends, timeout triggers, or serial `e` is received, the firmware:

1. marks the last audio chunk when applicable
2. sends `CONTROL_SESSION_END`
3. stops audio
4. switches camera back to idle
5. resumes idle image streaming with `session_id = 0`

## Protocol Contract

All outbound frames use the `JG` binary header defined in `docs/transport-mvp.md`, with little-endian numeric fields and a 14-byte fixed header.

The sender guarantees:

- `CONTROL_WAKE` precedes any active `IMAGE` or `AUDIO`
- idle images always use `session_id = 0`
- all active image/audio frames share the active `session_id`
- `payload_len == 0` is allowed for control frames without body
- sender drops the current frame on send failure instead of blocking capture logic

The WAKE payload follows the implementation doc defaults for active profile metadata.

## Error Handling

Camera capture failure drops only the current frame. Repeated camera failures trigger a camera reinit attempt instead of rebooting the board.

No TCP client is treated as a normal state. The firmware continues running locally and only attempts network writes when a client is connected.

Session termination is idempotent. Duplicate wake requests while active are ignored. Serial status logging exposes WiFi, state, session id, memory, and drop counters for recovery and field debugging.

## Desktop Debug Tool

`firmware/glass/tools/recv_stream.py` acts as a TCP client and protocol validator before a phone app exists. It connects to the glasses IP, reads framed messages, logs headers, counts image/audio/control traffic, and can optionally persist JPEG payloads for inspection.

This tool is part of the MVP acceptance path because the requirements explicitly allow protocol-level validation without the mobile app.
