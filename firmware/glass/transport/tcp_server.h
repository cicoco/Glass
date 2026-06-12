#pragma once

#include <Arduino.h>
#include <IPAddress.h>
#include <WiFiClient.h>
#include "frame_protocol.h"

bool tcp_server_begin(Stream &log);
void tcp_server_tick(Stream &log);
bool tcp_server_has_client();
bool tcp_server_send_frame(const JGFrameHeader &header, const uint8_t *payload);
IPAddress tcp_server_local_ip();
int32_t tcp_server_rssi();
uint32_t tcp_server_drop_count();

