#include "input_handler.h"

#include "../config.h"

namespace {
bool g_last_button_level = HIGH;
uint32_t g_last_button_edge_ms = 0;

void reset_flags(InputCommandFlags *flags) {
  flags->wake_requested = false;
  flags->force_end_requested = false;
  flags->print_status_requested = false;
  flags->print_network_requested = false;
}
}  // namespace

void input_handler_begin() {
  pinMode(WAKE_BTN_PIN, INPUT_PULLUP);
}

void input_handler_poll(InputCommandFlags *flags) {
  if (flags == nullptr) {
    return;
  }
  reset_flags(flags);

  const bool level = digitalRead(WAKE_BTN_PIN);
  const uint32_t now = millis();
  if (g_last_button_level == HIGH && level == LOW &&
      (now - g_last_button_edge_ms) >= DEBOUNCE_MS) {
    flags->wake_requested = true;
    g_last_button_edge_ms = now;
  }
  g_last_button_level = level;

  while (Serial.available() > 0) {
    const int ch = Serial.read();
    switch (ch) {
      case 'w':
      case 'W':
        flags->wake_requested = true;
        break;
      case 'e':
      case 'E':
        flags->force_end_requested = true;
        break;
      case 's':
      case 'S':
        flags->print_status_requested = true;
        break;
      case 'i':
      case 'I':
        flags->print_network_requested = true;
        break;
      default:
        break;
    }
  }
}
