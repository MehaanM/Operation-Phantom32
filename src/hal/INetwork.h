#pragma once
#include <stdint.h>

// Connection state for Wi-Fi / network adapters.
enum class NetworkState : uint8_t {
  Disconnected,
  Connecting,
  Connected,
  Error,
};

// INetwork — hardware abstraction for Wi-Fi / Ethernet connectivity.
// Implementations: ESP32 (WiFi.h), native (mock).
class INetwork {
 public:
  virtual ~INetwork() = default;
  virtual bool connect(const char *ssid, const char *password) = 0;
  virtual void disconnect() = 0;
  virtual NetworkState state() const = 0;
  virtual int8_t rssi() const = 0;       // dBm; 0 if disconnected
  virtual uint32_t ipv4() const = 0;     // host-byte-order; 0 if none
  virtual const char *ssid() const = 0;  // current SSID; "" if disconnected
};
