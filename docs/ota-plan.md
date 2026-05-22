# OTA Update Plan

## Goal

Enable firmware updates over WiFi without a USB cable — intended for use while travelling, with only a phone as infrastructure.

## Build side — GitHub Actions

- Trigger: every push to `main`
- Runner: `pio run -e heltec_wireless_paper`
- Artifact: upload `firmware.bin` as a GitHub release asset under a fixed tag (e.g. `latest`)
- Result: firmware always available at a stable URL:
  `https://github.com/<user>/<repo>/releases/latest/download/firmware.bin`

## Credentials

Hotspot SSID and password are compiled in as build flags in `platformio.ini`, similar to `AP_PASSWORD`:

```ini
-DOTA_SSID='"MyPhone"'
-DOTA_PASSWORD='"mypassword"'
```

These never leave the device and are not stored in the repo.

## Trigger — Option C (menu + boot-hold recovery)

### Normal path: menu item

- New menu item **"Update"** (or "OTA Update")
- Long-press selects it → device:
  1. Connects to the hotspot (`OTA_SSID` / `OTA_PASSWORD`)
  2. Shows "Connecting..." on screen
  3. Fetches `firmware.bin` from the GitHub releases URL via HTTPS
  4. Flashes using Arduino `Update.h`
  5. Shows progress percentage on screen
  6. Reboots on success, or shows error message on failure

### Recovery path: boot-hold

- Hold the BOOT button while powering on (before the normal ISR attaches)
- Detected in `setup()` before `reader_setup()` is called
- Enters the same OTA flow directly, bypassing the normal reader startup
- Useful if a bad flash breaks the menu

## Screen states during update

| State | Display |
|---|---|
| Connecting | `Connecting...` + SSID |
| Downloading | `Updating... XX%` (progress bar) |
| Success | `Done! Rebooting...` |
| Failure | `Update failed:` + error reason |

## Implementation steps

1. Add `OTA_SSID` and `OTA_PASSWORD` build flags to `platformio.ini`
2. Create `src/ota.h` / `src/ota.cpp` with `ota_run()` — handles connect, download, flash, reboot
3. Add **"Update"** menu item in `menu.cpp` → calls `ota_run()`
4. Add boot-hold detection in `main.cpp` `setup()` → calls `ota_run()` if button held at power-on
5. Add GitHub Actions workflow `.github/workflows/build.yml` — builds firmware and publishes release
