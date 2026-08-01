# 🚀 ESP32 WiFi Auto-Connect OTA

A multi-language OTA firmware starter for **ESP32-S3** and **ESP32-C6** boards. Pick the implementation that matches your toolchain: Arduino, pure ESP-IDF embedded C, or embedded Rust.

The firmware connects to your router Wi-Fi on boot, falls back to its own setup access point when the router is unavailable, and exposes an OTA update path so future firmware can be updated wirelessly after the first USB flash.

## ✨ Features

- 📡 Router Wi-Fi auto-connect at startup.
- 🛟 Fallback OTA Wi-Fi access point when the router is unavailable.
- 🔁 Router reconnect/status logic for easier field debugging.
- 🧭 Serial/log output for hostname, OTA password, update endpoint, and network state.
- 📲 Wireless firmware updates after the first USB/serial upload.
- 🧑‍💻 Three implementation choices: Arduino sketch, embedded C, and embedded Rust.

## 🧰 Tech Stack

| Option | Language | Framework | OTA Style | Best For |
| --- | --- | --- | --- | --- |
| Arduino | Arduino C/C++ | Arduino ESP32 Core | `ArduinoOTA` network upload | Fast Arduino IDE workflow |
| Embedded C | C | ESP-IDF | HTTP `POST /update` upload | Production ESP-IDF projects |
| Embedded Rust | Rust | `esp-idf-svc` / ESP-IDF | HTTP `POST /update` upload | Rust-first ESP32 projects |

## 📁 Project Structure

```text
.
├── README.md
├── WiFiAutoConnectOTA/
│   └── WiFiAutoConnectOTA.ino
├── embedded-c/
│   └── wifi_auto_connect_ota.c
└── embedded-rust/
    └── src/
        └── main.rs
```

> Arduino expects the `.ino` filename to match its containing folder name, so the Arduino sketch lives in `WiFiAutoConnectOTA/WiFiAutoConnectOTA.ino`.

## 🧭 Which Version Should I Use?

### Use the Arduino version if...

- You use Arduino IDE or Arduino CLI.
- You want `ArduinoOTA` upload discovery in Arduino tooling.
- You want the quickest path from USB flash to wireless updates.

### Use the embedded C version if...

- You are building a native ESP-IDF firmware project.
- You want a pure C implementation without Arduino wrappers.
- You prefer an HTTP OTA endpoint that accepts a compiled firmware binary.

### Use the embedded Rust version if...

- You are using an `esp-idf-template` Rust project.
- You want Rust application code backed by ESP-IDF services.
- You prefer an HTTP OTA endpoint from Rust.

## 🚦 Getting Started

### 1. Install Requirements

Choose one stack:

- **Arduino:** Arduino IDE/CLI and an Arduino ESP32 core that supports your ESP32-S3 or ESP32-C6 board.
- **Embedded C:** ESP-IDF with Wi-Fi, NVS, HTTP server, and OTA/app update components enabled.
- **Embedded Rust:** Rust ESP-IDF tooling, such as `esp-idf-template`, plus an OTA-capable partition table.

All stacks need:

- USB cable for the first flash.
- Wi-Fi credentials for your router.
- An OTA partition layout if using ESP-IDF C or Rust OTA binaries.

### 2. Configure Credentials

Each implementation has the same configuration values near the top of the file:

```c
#define WIFI_SSID       "YOUR_ROUTER_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_ROUTER_WIFI_PASSWORD"

#define OTA_HOSTNAME    "esp32-ota-board"
#define OTA_PASSWORD    "12345678"

#define OTA_AP_SSID     "ESP32-OTA-SETUP"
#define OTA_AP_PASSWORD "87654321"
```

In Rust, the same values are declared as constants:

```rust
const WIFI_SSID: &str = "YOUR_ROUTER_WIFI_SSID";
const WIFI_PASSWORD: &str = "YOUR_ROUTER_WIFI_PASSWORD";
const OTA_HOSTNAME: &str = "esp32-ota-board";
const OTA_PASSWORD: &str = "12345678";
const OTA_AP_SSID: &str = "ESP32-OTA-SETUP";
const OTA_AP_PASSWORD: &str = "87654321";
```

Recommended changes before deployment:

- Replace router SSID/password placeholders.
- Change `OTA_PASSWORD` to a strong password.
- Change `OTA_AP_PASSWORD` to a strong password of at least 8 characters.
- Customize `OTA_HOSTNAME` for each board.
- Do not commit real Wi-Fi or OTA credentials to a public repository.

## 📲 Upload Workflows

### Arduino OTA Workflow

1. Open `WiFiAutoConnectOTA/WiFiAutoConnectOTA.ino` in Arduino IDE.
2. Select your ESP32-S3 or ESP32-C6 board and USB serial port.
3. Upload once over USB.
4. Open Serial Monitor at `115200` baud and note the IP/hostname.
5. For future uploads, select the network OTA port and enter `OTA_PASSWORD` when prompted.

### Embedded C HTTP OTA Workflow

1. Copy `embedded-c/wifi_auto_connect_ota.c` into an ESP-IDF project main component.
2. Build and flash once over USB with your ESP-IDF tooling.
3. Build later firmware updates as a `.bin` image.
4. Upload the binary to the board:

```bash
curl -X POST \
  --data-binary @firmware.bin \
  -H 'X-OTA-Password: 12345678' \
  http://BOARD_IP/update
```

### Embedded Rust HTTP OTA Workflow

1. Use `embedded-rust/src/main.rs` as the main file in an `esp-idf-template` Rust project.
2. Build and flash once over USB.
3. Build later firmware updates as a `.bin` image.
4. Upload the binary with the same HTTP OTA command:

```bash
curl -X POST \
  --data-binary @firmware.bin \
  -H 'X-OTA-Password: 12345678' \
  http://BOARD_IP/update
```

## 📶 Network Behavior

### Router Mode

On boot, the board tries to connect to `WIFI_SSID` for up to the configured connection timeout. If successful, logs show the router SSID and board address.

### Fallback AP Mode

If router Wi-Fi is unavailable, the board starts a fallback access point using `OTA_AP_SSID` and `OTA_AP_PASSWORD`. Connect your computer to that AP, then upload over OTA directly to the board IP.

### OTA Authentication

- Arduino uses `ArduinoOTA.setPassword(OTA_PASSWORD)`.
- Embedded C and Rust expect the HTTP header `X-OTA-Password` to match `OTA_PASSWORD`.

## ⚙️ Tunable Constants

| Constant | Default | Purpose |
| --- | ---: | --- |
| `SERIAL_BAUD_RATE` | `115200` | Arduino Serial Monitor speed |
| `WIFI_CONNECT_TIMEOUT_MS` | `30000` | Router connection timeout |
| `WIFI_RETRY_DELAY_MS` | `500` | Arduino delay between Wi-Fi connection dots |
| `WIFI_RECONNECT_PAUSE_MS` | `5000` | Delay between reconnect attempts |
| `STATUS_PRINT_INTERVAL_MS` | `10000` | Periodic OTA status logging interval |
| `OTA_UPLOAD_BUFFER_SIZE` | `4096` | HTTP OTA receive buffer in C/Rust examples |

## 🔐 Security Notes

- Do not leave the example OTA password in production firmware.
- Do not commit real router credentials to a public repository.
- Use a unique fallback AP SSID and strong AP password.
- Disable fallback AP behavior if the device will be deployed in an untrusted area.
- Prefer HTTPS, signed firmware, or secure boot/flash encryption for production devices.

## 🧪 Troubleshooting

| Problem | What to Check |
| --- | --- |
| Board does not appear as an Arduino OTA port | Confirm the Arduino sketch was flashed once over USB and your computer is on the same network. |
| HTTP OTA returns `401` | Confirm the `X-OTA-Password` header matches `OTA_PASSWORD`. |
| Router connection times out | Check `WIFI_SSID`, `WIFI_PASSWORD`, signal strength, and 2.4 GHz compatibility. |
| Fallback AP does not start | Ensure `OTA_AP_PASSWORD` is at least 8 characters. |
| ESP32-C6 build fails | Update to a framework/toolchain version that supports your exact board. |
| OTA boot fails | Confirm the partition table has at least two OTA app partitions. |

## 📜 License

Add your preferred license before publishing or distributing this firmware.
