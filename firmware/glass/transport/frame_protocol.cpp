#include "frame_protocol.h"

#include "../config.h"

namespace {
bool write_all(Client &client, const uint8_t *buf, size_t len) {
  size_t offset = 0;
  while (offset < len) {
    const size_t written = client.write(buf + offset, len - offset);
    if (written == 0) {
      return false;
    }
    offset += written;
  }
  return true;
}
}  // namespace

bool frame_protocol_write_frame(Client &client, const JGFrameHeader &header,
                                const uint8_t *payload) {
  if (!client.connected()) {
    return false;
  }
  if (header.payload_len > JG_MAX_PAYLOAD_LEN) {
    return false;
  }

  uint8_t raw[JG_HEADER_SIZE];
  raw[0] = 0x4A;
  raw[1] = 0x47;
  raw[2] = JG_PROTOCOL_VERSION;
  raw[3] = header.type;
  raw[4] = header.flags;
  memcpy(&raw[5], &header.timestamp_ms, sizeof(header.timestamp_ms));
  memcpy(&raw[9], &header.session_id, sizeof(header.session_id));
  memcpy(&raw[11], &header.payload_len, sizeof(header.payload_len));

  if (!write_all(client, raw, sizeof(raw))) {
    return false;
  }
  if (header.payload_len == 0) {
    return true;
  }
  return write_all(client, payload, header.payload_len);
}

void frame_protocol_build_wake_payload(uint8_t out[8]) {
  const uint8_t capture_mode = 1;
  const uint8_t reserved = 0;
  const uint16_t width = 640;
  const uint16_t height = 480;
  const uint16_t fps_x10 = ACTIVE_FPS_X10;

  out[0] = capture_mode;
  out[1] = reserved;
  memcpy(&out[2], &width, sizeof(width));
  memcpy(&out[4], &height, sizeof(height));
  memcpy(&out[6], &fps_x10, sizeof(fps_x10));
}
