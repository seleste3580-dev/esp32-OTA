# 🚀 ESP32 WiFi Auto-Connect OTA

A clean Arduino OTA firmware starter for **ESP32-S3** and **ESP32-C6** boards. The sketch connects to your router Wi-Fi on boot, automatically falls back to its own setup access point when the router is unavailable, and starts Arduino OTA so future firmware uploads can happen wirelessly.

## ✨ Features

- 📡 Connects to a hardcoded router Wi-Fi network at startup.
- 🛟 Starts a fallback OTA Wi-Fi access point if router connection times out.
- 🔁 Keeps router Wi-Fi connected and retries after disconnects.
- 🧭 Prints OTA hostname, password, and board IP address to Serial Monitor.
- 📲 Supports wireless firmware uploads with Arduino OTA tooling.
- 🧱 Plain C-style Arduino sketch structure: macros and functions, no classes or lambdas.

## 🧰 Tech Stack

| Layer | Technology |
| --- | --- |
| Microcontroller | ESP32-S3 / ESP32-C6 |
| Framework | Arduino ESP32 Core |
| Language | Arduino C/C++ |
| Connectivity | Wi-Fi station mode + fallback access point mode |
| OTA | `ArduinoOTA` |
| Debugging | USB Serial Monitor at `115200` baud |

## 📁 Project Structure

```text
.
├── README.md
└── WiFiAutoConnectOTA/
    └── WiFiAutoConnectOTA.ino
```

> Arduino expects the `.ino` filename to match its containing folder name, so the sketch lives in `WiFiAutoConnectOTA/WiFiAutoConnectOTA.ino`.

## 🚦 Getting Started

### 1. Install Requirements

- Arduino IDE or another ESP32 Arduino-compatible build/upload tool.
- Arduino ESP32 core with support for your selected ESP32-S3 or ESP32-C6 board.
- USB cable for the first flash.
- Wi-Fi credentials for your router.

### 2. Configure the Sketch

Open `WiFiAutoConnectOTA/WiFiAutoConnectOTA.ino` and update these values:

```cpp
#define WIFI_SSID       "YOUR_ROUTER_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_ROUTER_WIFI_PASSWORD"

#define OTA_HOSTNAME    "esp32-ota-board"
#define OTA_PASSWORD    "12345678"

#define OTA_AP_SSID     "ESP32-OTA-SETUP"
#define OTA_AP_PASSWORD "87654321"
```

Recommended changes before deployment:

- Replace `WIFI_SSID` and `WIFI_PASSWORD` with your router credentials.
- Change `OTA_PASSWORD` to a strong password.
- Change `OTA_AP_PASSWORD` to a strong password of at least 8 characters.
- Optionally customize `OTA_HOSTNAME` to identify the board on your network.

### 3. First Upload Over USB

1. Connect the ESP32-S3 or ESP32-C6 over USB.
2. Select the correct board and port in Arduino IDE.
3. Open the sketch folder.
4. Upload normally over USB/serial.
5. Open Serial Monitor at `115200` baud.

### 4. Upload Updates Over Wi-Fi

After the first USB upload:

1. Keep the board powered on.
2. Make sure your computer is on the same Wi-Fi network as the board, or connect to the fallback AP if router Wi-Fi is unavailable.
3. Select the network OTA port in Arduino IDE.
4. Upload your changed firmware wirelessly.
5. Enter the configured OTA password when prompted.

## 📶 Network Behavior

### Router Mode

On boot, the board tries to connect to `WIFI_SSID` for up to `WIFI_CONNECT_TIMEOUT_MS` milliseconds. If successful, the Serial Monitor prints the board IP address and OTA connection details.

### Fallback AP Mode

If router Wi-Fi is unavailable, the board starts a fallback access point using `OTA_AP_SSID` and `OTA_AP_PASSWORD`. Connect your computer to that AP, then upload over OTA directly to the board.

### Reconnect Handling

When running in router mode, the loop checks for Wi-Fi disconnects and retries after `WIFI_RECONNECT_PAUSE_MS` milliseconds.

## ⚙️ Tunable Constants

| Constant | Default | Purpose |
| --- | ---: | --- |
| `SERIAL_BAUD_RATE` | `115200` | Serial Monitor speed |
| `WIFI_CONNECT_TIMEOUT_MS` | `30000` | Router connection timeout |
| `WIFI_RETRY_DELAY_MS` | `500` | Delay between Wi-Fi connection dots |
| `WIFI_RECONNECT_PAUSE_MS` | `5000` | Delay between reconnect attempts |
| `STATUS_PRINT_INTERVAL_MS` | `10000` | Periodic OTA status logging interval |

## 🔐 Security Notes

- Do not leave the example OTA password in production firmware.
- Do not commit real router credentials to a public repository.
- Use a unique fallback AP SSID and strong AP password.
- Disable fallback AP behavior if the device will be deployed in an untrusted area.

## 🧪 Troubleshooting

| Problem | What to Check |
| --- | --- |
| Board does not appear as an OTA port | Confirm the first USB flash succeeded and your computer is on the same network. |
| OTA password rejected | Confirm `OTA_PASSWORD` matches the password entered by the uploader. |
| Router connection times out | Check `WIFI_SSID`, `WIFI_PASSWORD`, signal strength, and 2.4 GHz compatibility. |
| Fallback AP does not start | Ensure `OTA_AP_PASSWORD` is at least 8 characters. |
| ESP32-C6 build fails | Update to an Arduino ESP32 core version that supports your board. |

