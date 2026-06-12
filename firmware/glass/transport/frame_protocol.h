#pragma once

#include <Arduino.h>
#include <Client.h>

constexpr uint8_t JG_PROTOCOL_VERSION = 0x01;
// The transport doc text says "14-byte header", but the declared field layout
// and struct format add up to 15 bytes. The implementation follows the field
// layout so both firmware and debug receiver stay aligned.
constexpr size_t JG_HEADER_SIZE = 15;
constexpr uint32_t JG_MAX_PAYLOAD_LEN = 512UL * 1024UL;

enum JGFrameType : uint8_t {
  JG_FRAME_IMAGE = 0x01,
  JG_FRAME_AUDIO = 0x02,
  JG_FRAME_CONTROL_WAKE = 0x10,
  JG_FRAME_CONTROL_SESSION_END = 0x11,
};

enum JGFrameFlags : uint8_t {
  JG_FLAG_STREAM_IDLE = 1 << 0,
  JG_FLAG_AUDIO_LAST = 1 << 1,
};

struct JGFrameHeader {
  uint8_t type;
  uint8_t flags;
  uint32_t timestamp_ms;
  uint16_t session_id;
  uint32_t payload_len;
};

bool frame_protocol_write_frame(Client &client, const JGFrameHeader &header,
                                const uint8_t *payload);
void frame_protocol_build_wake_payload(uint8_t out[8]);
