/*
  WiFiAutoConnectOTA.ino

  ESP32-S3 / ESP32-C6 Arduino OTA sketch.

  What this does:
  1. On power-up, the board connects to the hardcoded router Wi-Fi below.
  2. If that router is not available, the board starts its own fallback Wi-Fi
     access point using OTA_AP_SSID and OTA_AP_PASSWORD below.
  3. After Wi-Fi is up, Arduino OTA starts so you can upload changed firmware
     wirelessly from Arduino IDE or another ESP32 OTA-capable uploader.

  Important:
  - The first upload must be over USB/serial. After this sketch is on the board,
    future uploads can be done over Wi-Fi/OTA.
  - Arduino .ino files are built by the Arduino core as C++, but this sketch is
    written in plain C style: macros, functions, no classes, and no lambdas.
  - For ESP32-C6, use an Arduino-ESP32 core version that supports your board.
*/

#if !defined(ESP32)
  #error "Select an ESP32-S3 or ESP32-C6 board in Arduino before compiling."
#endif

#include <WiFi.h>
#include <ArduinoOTA.h>

/* ===== EDIT THESE HARD-CODED VALUES ===== */
#define WIFI_SSID       "YOUR_ROUTER_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_ROUTER_WIFI_PASSWORD"

#define OTA_HOSTNAME    "esp32-ota-board"
#define OTA_PASSWORD    "12345678"

#define OTA_AP_SSID     "ESP32-OTA-SETUP"
#define OTA_AP_PASSWORD "87654321"
/* ======================================== */

#define SERIAL_BAUD_RATE          115200U
#define WIFI_CONNECT_TIMEOUT_MS   30000UL
#define WIFI_RETRY_DELAY_MS       500UL
#define WIFI_RECONNECT_PAUSE_MS   5000UL
#define STATUS_PRINT_INTERVAL_MS  10000UL

static unsigned long last_status_print_ms = 0UL;
static unsigned long last_reconnect_attempt_ms = 0UL;
static int fallback_ap_active = 0;

static void print_network_ready_message(void)
{
  if (fallback_ap_active) {
    Serial.print("Fallback OTA Wi-Fi AP SSID: ");
    Serial.println(OTA_AP_SSID);
    Serial.print("Fallback OTA Wi-Fi AP password: ");
    Serial.println(OTA_AP_PASSWORD);
    Serial.print("Board AP IP address: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.print("Connected to router Wi-Fi SSID: ");
    Serial.println(WIFI_SSID);
    Serial.print("Board IP address: ");
    Serial.println(WiFi.localIP());
  }

  Serial.print("OTA hostname: ");
  Serial.println(OTA_HOSTNAME);
  Serial.print("OTA password: ");
  Serial.println(OTA_PASSWORD);
}

static int connect_to_router_wifi(void)
{
  unsigned long start_ms;

  fallback_ap_active = 0;
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to router Wi-Fi SSID: ");
  Serial.println(WIFI_SSID);

  start_ms = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if ((millis() - start_ms) >= WIFI_CONNECT_TIMEOUT_MS) {
      Serial.println();
      Serial.println("Router Wi-Fi connection timed out.");
      return 0;
    }

    Serial.print('.');
    delay(WIFI_RETRY_DELAY_MS);
  }

  Serial.println();
  return 1;
}

static int start_fallback_ota_ap(void)
{
  fallback_ap_active = 1;
  WiFi.disconnect(true);
  delay(250U);
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);

  Serial.print("Starting fallback OTA Wi-Fi AP: ");
  Serial.println(OTA_AP_SSID);

  if (!WiFi.softAP(OTA_AP_SSID, OTA_AP_PASSWORD)) {
    Serial.println("Failed to start fallback OTA Wi-Fi AP.");
    return 0;
  }

  return 1;
}

static void ota_on_start(void)
{
  Serial.println("OTA update started.");
}

static void ota_on_end(void)
{
  Serial.println();
  Serial.println("OTA update finished. Rebooting...");
}

static void ota_on_progress(unsigned int progress, unsigned int total)
{
  unsigned int percent = 0U;

  if (total > 0U) {
    percent = (progress * 100U) / total;
  }

  Serial.print("OTA progress: ");
  Serial.print(percent);
  Serial.println('%');
}

static void ota_on_error(ota_error_t error)
{
  Serial.print("OTA error: ");
  Serial.println((unsigned int)error);

  if (error == OTA_AUTH_ERROR) {
    Serial.println("Authentication failed. Check OTA_PASSWORD.");
  } else if (error == OTA_BEGIN_ERROR) {
    Serial.println("Begin failed.");
  } else if (error == OTA_CONNECT_ERROR) {
    Serial.println("Connect failed.");
  } else if (error == OTA_RECEIVE_ERROR) {
    Serial.println("Receive failed.");
  } else if (error == OTA_END_ERROR) {
    Serial.println("End failed.");
  }
}

static void setup_ota(void)
{
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart(ota_on_start);
  ArduinoOTA.onEnd(ota_on_end);
  ArduinoOTA.onProgress(ota_on_progress);
  ArduinoOTA.onError(ota_on_error);
  ArduinoOTA.begin();

  Serial.println("OTA service is ready.");
}

static void start_network_for_ota(void)
{
  if (!connect_to_router_wifi()) {
    Serial.println("Using fallback AP mode for OTA upload.");

    while (!start_fallback_ota_ap()) {
      Serial.println("Retrying fallback OTA AP...");
      delay(WIFI_RECONNECT_PAUSE_MS);
    }
  }

  print_network_ready_message();
}

static void keep_router_wifi_connected(void)
{
  if (fallback_ap_active) {
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  if ((millis() - last_reconnect_attempt_ms) < WIFI_RECONNECT_PAUSE_MS) {
    return;
  }

  last_reconnect_attempt_ms = millis();
  Serial.println("Router Wi-Fi disconnected. Reconnecting...");

  if (connect_to_router_wifi()) {
    print_network_ready_message();
  }
}

static void print_periodic_status(void)
{
  if ((millis() - last_status_print_ms) < STATUS_PRINT_INTERVAL_MS) {
    return;
  }

  last_status_print_ms = millis();

  if (fallback_ap_active) {
    Serial.print("OTA ready on fallback AP IP: ");
    Serial.println(WiFi.softAPIP());
  } else if (WiFi.status() == WL_CONNECTED) {
    Serial.print("OTA ready on router IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Router Wi-Fi is not connected.");
  }
}

void setup(void)
{
  Serial.begin(SERIAL_BAUD_RATE);
  delay(1000U);
  Serial.println();
  Serial.println("Booting ESP32-S3/C6 WiFiAutoConnectOTA...");

  start_network_for_ota();
  setup_ota();
}

void loop(void)
{
  keep_router_wifi_connected();
  ArduinoOTA.handle();
  print_periodic_status();
  delay(10U);
}
