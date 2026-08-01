//! ESP-IDF embedded Rust version of the ESP32-S3 / ESP32-C6 Wi-Fi auto-connect
//! OTA workflow. It connects to a router first, starts a fallback AP when the
//! router is unavailable, and exposes an HTTP OTA upload endpoint at `/update`.
//!
//! Build note:
//! - Use this file as `src/main.rs` in an `esp-idf-template` Rust project.
//! - Configure an OTA-capable partition table with at least two OTA app slots.

use anyhow::{bail, Context, Result};
use embedded_svc::http::Method;
use embedded_svc::io::{Read, Write};
use embedded_svc::wifi::{AccessPointConfiguration, AuthMethod, ClientConfiguration, Configuration};
use esp_idf_svc::eventloop::EspSystemEventLoop;
use esp_idf_svc::hal::modem::Modem;
use esp_idf_svc::http::server::{Configuration as HttpConfiguration, EspHttpServer};
use esp_idf_svc::nvs::EspDefaultNvsPartition;
use esp_idf_svc::ota::EspOta;
use esp_idf_svc::wifi::{BlockingWifi, EspWifi};
use log::{info, warn};
use std::thread;
use std::time::Duration;

const WIFI_SSID: &str = "YOUR_ROUTER_WIFI_SSID";
const WIFI_PASSWORD: &str = "YOUR_ROUTER_WIFI_PASSWORD";

const OTA_HOSTNAME: &str = "esp32-ota-board";
const OTA_PASSWORD: &str = "12345678";

const OTA_AP_SSID: &str = "ESP32-OTA-SETUP";
const OTA_AP_PASSWORD: &str = "87654321";

const WIFI_CONNECT_TIMEOUT_MS: u64 = 30_000;
const STATUS_PRINT_INTERVAL_MS: u64 = 10_000;
const OTA_UPLOAD_BUFFER_SIZE: usize = 4096;

fn connect_to_router_wifi(wifi: &mut BlockingWifi<EspWifi<'static>>) -> Result<bool> {
    let client_config = ClientConfiguration {
        ssid: WIFI_SSID.try_into().context("router SSID is too long")?,
        password: WIFI_PASSWORD.try_into().context("router password is too long")?,
        ..Default::default()
    };

    wifi.set_configuration(&Configuration::Client(client_config))?;
    wifi.start()?;

    info!("Connecting to router Wi-Fi SSID: {WIFI_SSID}");
    match wifi.connect() {
        Ok(()) => match wifi.wait_netif_up() {
            Ok(()) => {
                info!("Connected to router Wi-Fi SSID: {WIFI_SSID}");
                Ok(true)
            }
            Err(error) => {
                warn!("Router network interface did not come up: {error:?}");
                Ok(false)
            }
        },
        Err(error) => {
            warn!("Router Wi-Fi connection failed: {error:?}");
            Ok(false)
        }
    }
}

fn start_fallback_ota_ap(wifi: &mut BlockingWifi<EspWifi<'static>>) -> Result<()> {
    let ap_config = AccessPointConfiguration {
        ssid: OTA_AP_SSID.try_into().context("fallback AP SSID is too long")?,
        password: OTA_AP_PASSWORD.try_into().context("fallback AP password is too long")?,
        auth_method: AuthMethod::WPA2Personal,
        max_connections: 4,
        ..Default::default()
    };

    wifi.stop()?;
    wifi.set_configuration(&Configuration::AccessPoint(ap_config))?;
    wifi.start()?;

    info!("Fallback OTA Wi-Fi AP SSID: {OTA_AP_SSID}");
    info!("Fallback OTA Wi-Fi AP password: {OTA_AP_PASSWORD}");
    Ok(())
}

fn start_ota_http_server() -> Result<EspHttpServer<'static>> {
    let mut server = EspHttpServer::new(&HttpConfiguration::default())?;

    server.fn_handler("/update", Method::Post, |mut request| {
        let password_header = request.header("X-OTA-Password").unwrap_or_default();
        if password_header != OTA_PASSWORD {
            request.into_status_response(401)?.write_all(b"Invalid OTA password")?;
            return Ok(());
        }

        let mut ota = EspOta::new()?;
        let mut update = ota.initiate_update()?;
        let mut buffer = [0_u8; OTA_UPLOAD_BUFFER_SIZE];

        loop {
            let read = request.read(&mut buffer)?;
            if read == 0 {
                break;
            }

            update.write(&buffer[..read])?;
        }

        update.complete()?;
        request.into_ok_response()?.write_all(b"OTA update accepted. Rebooting...")?;
        thread::sleep(Duration::from_secs(1));
        esp_idf_svc::hal::reset::restart();
        Ok(())
    })?;

    info!("HTTP OTA endpoint ready: POST /update with X-OTA-Password header");
    Ok(server)
}

fn print_periodic_status() {
    loop {
        info!("OTA hostname: {OTA_HOSTNAME}");
        info!("OTA password header value: {OTA_PASSWORD}");
        info!("OTA upload command: curl -X POST --data-binary @firmware.bin -H 'X-OTA-Password: {OTA_PASSWORD}' http://BOARD_IP/update");
        thread::sleep(Duration::from_millis(STATUS_PRINT_INTERVAL_MS));
    }
}

fn main() -> Result<()> {
    esp_idf_svc::sys::link_patches();
    esp_idf_svc::log::EspLogger::initialize_default();

    let peripherals = esp_idf_svc::hal::peripherals::Peripherals::take().context("peripherals already taken")?;
    let modem: Modem = peripherals.modem;
    let system_event_loop = EspSystemEventLoop::take()?;
    let nvs = EspDefaultNvsPartition::take()?;
    let wifi = EspWifi::new(modem, system_event_loop.clone(), Some(nvs))?;
    let mut wifi = BlockingWifi::wrap(wifi, system_event_loop)?;

    info!("Booting ESP32-S3/C6 embedded Rust WiFiAutoConnectOTA...");

    if !connect_to_router_wifi(&mut wifi)? {
        warn!("Router Wi-Fi timed out after {WIFI_CONNECT_TIMEOUT_MS} ms. Using fallback AP mode for OTA upload.");
        start_fallback_ota_ap(&mut wifi)?;
    }

    let _server = start_ota_http_server()?;
    print_periodic_status();
    bail!("status loop exited unexpectedly")
}
