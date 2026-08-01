/*
  wifi_auto_connect_ota.c

  ESP-IDF embedded C version of the ESP32-S3 / ESP32-C6 Wi-Fi auto-connect
  OTA workflow. It connects to a router first, starts a fallback AP when the
  router is unavailable, and exposes an HTTP OTA upload endpoint at /update.

  Build note:
  - Add this file to an ESP-IDF project main component.
  - Enable Wi-Fi, NVS, esp_http_server, and app_update components.
  - Set partition table to include at least two OTA app partitions.
*/

#include <stdio.h>
#include <string.h>

#include "esp_app_format.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#define WIFI_SSID       "YOUR_ROUTER_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_ROUTER_WIFI_PASSWORD"

#define OTA_HOSTNAME    "esp32-ota-board"
#define OTA_PASSWORD    "12345678"

#define OTA_AP_SSID     "ESP32-OTA-SETUP"
#define OTA_AP_PASSWORD "87654321"

#define WIFI_CONNECT_TIMEOUT_MS   30000U
#define WIFI_RECONNECT_PAUSE_MS   5000U
#define STATUS_PRINT_INTERVAL_MS  10000U
#define OTA_UPLOAD_BUFFER_SIZE    4096U

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAILED_BIT    BIT1

static const char *TAG = "wifi_auto_ota_c";
static EventGroupHandle_t wifi_event_group;
static int fallback_ap_active = 0;
static int reconnecting = 0;
static httpd_handle_t ota_http_server = NULL;

static esp_err_t ota_update_handler(httpd_req_t *req)
{
  char password_header[64];
  char buffer[OTA_UPLOAD_BUFFER_SIZE];
  int received;
  esp_ota_handle_t ota_handle = 0;
  const esp_partition_t *update_partition;
  esp_err_t result;

  if (httpd_req_get_hdr_value_str(req, "X-OTA-Password", password_header, sizeof(password_header)) != ESP_OK ||
      strcmp(password_header, OTA_PASSWORD) != 0) {
    httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Invalid OTA password");
    return ESP_FAIL;
  }

  update_partition = esp_ota_get_next_update_partition(NULL);
  if (update_partition == NULL) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition");
    return ESP_FAIL;
  }

  result = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
  if (result != ESP_OK) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
    return result;
  }

  while (req->remaining > 0) {
    received = httpd_req_recv(req, buffer, sizeof(buffer));
    if (received <= 0) {
      esp_ota_abort(ota_handle);
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA receive failed");
      return ESP_FAIL;
    }

    result = esp_ota_write(ota_handle, buffer, (size_t)received);
    if (result != ESP_OK) {
      esp_ota_abort(ota_handle);
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA write failed");
      return result;
    }
  }

  result = esp_ota_end(ota_handle);
  if (result != ESP_OK) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA end failed");
    return result;
  }

  result = esp_ota_set_boot_partition(update_partition);
  if (result != ESP_OK) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA boot partition failed");
    return result;
  }

  httpd_resp_sendstr(req, "OTA update accepted. Rebooting...");
  vTaskDelay(pdMS_TO_TICKS(1000U));
  esp_restart();
  return ESP_OK;
}

static void start_ota_http_server(void)
{
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  httpd_uri_t update_uri = {
    .uri = "/update",
    .method = HTTP_POST,
    .handler = ota_update_handler,
    .user_ctx = NULL,
  };

  if (ota_http_server != NULL) {
    return;
  }

  if (httpd_start(&ota_http_server, &config) == ESP_OK) {
    httpd_register_uri_handler(ota_http_server, &update_uri);
    ESP_LOGI(TAG, "HTTP OTA endpoint ready: POST /update with X-OTA-Password header");
  }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED && !fallback_ap_active && !reconnecting) {
    reconnecting = 1;
    ESP_LOGW(TAG, "Router Wi-Fi disconnected. Reconnecting...");
    vTaskDelay(pdMS_TO_TICKS(WIFI_RECONNECT_PAUSE_MS));
    esp_wifi_connect();
    reconnecting = 0;
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "Connected to router Wi-Fi SSID: %s", WIFI_SSID);
    ESP_LOGI(TAG, "Board IP address: " IPSTR, IP2STR(&event->ip_info.ip));
    xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

static int connect_to_router_wifi(void)
{
  wifi_config_t wifi_config = {0};
  EventBits_t bits;

  fallback_ap_active = 0;
  strncpy((char *)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid));
  strncpy((char *)wifi_config.sta.password, WIFI_PASSWORD, sizeof(wifi_config.sta.password));

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_ERROR_CHECK(esp_wifi_connect());

  ESP_LOGI(TAG, "Connecting to router Wi-Fi SSID: %s", WIFI_SSID);
  bits = xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));

  if ((bits & WIFI_CONNECTED_BIT) == 0) {
    ESP_LOGW(TAG, "Router Wi-Fi connection timed out.");
    return 0;
  }

  return 1;
}

static void start_fallback_ota_ap(void)
{
  wifi_config_t ap_config = {0};

  fallback_ap_active = 1;
  ESP_ERROR_CHECK(esp_wifi_stop());
  strncpy((char *)ap_config.ap.ssid, OTA_AP_SSID, sizeof(ap_config.ap.ssid));
  strncpy((char *)ap_config.ap.password, OTA_AP_PASSWORD, sizeof(ap_config.ap.password));
  ap_config.ap.ssid_len = strlen(OTA_AP_SSID);
  ap_config.ap.max_connection = 4;
  ap_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(TAG, "Fallback OTA Wi-Fi AP SSID: %s", OTA_AP_SSID);
  ESP_LOGI(TAG, "Fallback OTA Wi-Fi AP password: %s", OTA_AP_PASSWORD);
}

static void status_task(void *arg)
{
  while (1) {
    ESP_LOGI(TAG, "OTA hostname: %s", OTA_HOSTNAME);
    ESP_LOGI(TAG, "OTA password header value: %s", OTA_PASSWORD);
    ESP_LOGI(TAG, "OTA upload command: curl -X POST --data-binary @firmware.bin -H 'X-OTA-Password: %s' http://BOARD_IP/update", OTA_PASSWORD);
    vTaskDelay(pdMS_TO_TICKS(STATUS_PRINT_INTERVAL_MS));
  }
}

void app_main(void)
{
  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();
  esp_netif_create_default_wifi_ap();

  wifi_event_group = xEventGroupCreate();
  wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&init_config));
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL));

  ESP_LOGI(TAG, "Booting ESP32-S3/C6 embedded C WiFiAutoConnectOTA...");

  if (!connect_to_router_wifi()) {
    ESP_LOGI(TAG, "Using fallback AP mode for OTA upload.");
    start_fallback_ota_ap();
  }

  start_ota_http_server();
  xTaskCreate(status_task, "ota_status", 4096, NULL, 5, NULL);
}
