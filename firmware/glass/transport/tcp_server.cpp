#include "tcp_server.h"

#include <WiFi.h>

#include "../config.h"

namespace {
WiFiServer g_server(JG_TCP_PORT);
WiFiClient g_client;
uint32_t g_drop_count = 0;

bool connect_wifi(Stream &log) {
  log.println(F("[WIFI] connecting..."));
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    log.print('.');
    if (millis() - start > 30000UL) {
      log.println();
      log.println(F("[WIFI] connect timeout"));
      return false;
    }
  }

  log.println();
  log.print(F("[WIFI] IP: "));
  log.print(WiFi.localIP());
  log.print(':');
  log.println(JG_TCP_PORT);
  return true;
}
}  // namespace

bool tcp_server_begin(Stream &log) {
  if (!connect_wifi(log)) {
    return false;
  }
  g_server.begin();
  g_server.setNoDelay(true);
  log.println(F("[TCP] listening"));
  return true;
}

void tcp_server_tick(Stream &log) {
  WiFiClient pending = g_server.available();
  if (pending) {
    if (g_client && g_client.connected()) {
      log.println(F("[TCP] replacing existing client"));
      g_client.stop();
    }
    g_client = pending;
    g_client.setNoDelay(true);
    log.print(F("[TCP] client connected: "));
    log.println(g_client.remoteIP());
  }

  if (g_client && !g_client.connected()) {
    log.println(F("[TCP] client disconnected"));
    g_client.stop();
  }
}

bool tcp_server_has_client() {
  return g_client && g_client.connected();
}

bool tcp_server_send_frame(const JGFrameHeader &header, const uint8_t *payload) {
  if (!tcp_server_has_client()) {
    return false;
  }
  const bool ok = frame_protocol_write_frame(g_client, header, payload);
  if (!ok) {
    ++g_drop_count;
    g_client.stop();
  }
  return ok;
}

IPAddress tcp_server_local_ip() {
  return WiFi.localIP();
}

int32_t tcp_server_rssi() {
  return WiFi.RSSI();
}

uint32_t tcp_server_drop_count() {
  return g_drop_count;
}
