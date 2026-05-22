# ESP32 RSVP Reader

An e-ink book reader running on the **Heltec Wireless Paper v1.2** (ESP32-S3). Books are uploaded as plain text over WiFi and read word-by-word (RSVP mode) or as paginated text (e-reader mode).

## Hardware

- **Board**: Heltec Wireless Paper v1.2
- **Display**: 2.13" e-ink, 250×122 px
- **Input**: single button (BOOT/GPIO0)
- **Battery**: LiPo via JST connector

## Features

- RSVP (Rapid Serial Visual Presentation) reading at 150/200/300 WPM
- Paginated e-reader mode with page history
- Chapter navigation
- WiFi AP for uploading/downloading/deleting book files
- OTA firmware updates over WiFi
- Battery percentage display
- Deep sleep with progress snapshot on screen
- Button lock on wake-up

## Button controls

| Action | Outside menu | Inside menu |
|---|---|---|
| Short press | Start/stop (RSVP) / next page (e-reader) | Move cursor down |
| Double press | Go back / rewind | Move cursor up |
| Long press | Open menu | Select item |

After waking from sleep, the device starts **locked**. Long press to unlock.

## Getting started

### 1. Configure secrets

Copy the template and fill in your credentials:

```bash
cp secrets.ini.example secrets.ini
```

Edit `secrets.ini`:

```ini
[secrets]
build_flags =
    -DAP_PASSWORD='"yourpassword"'
    -DOTA_SSID='"YourHotspotName"'
    -DOTA_PASSWORD='"YourHotspotPassword"'
```

`AP_PASSWORD` must be at least 8 characters (WPA2 requirement).

### 2. Build and flash

```bash
# Build + flash firmware
pio run -e heltec_wireless_paper -t upload

# Flash filesystem (required once, and after partition table changes)
pio run -e heltec_wireless_paper -t uploadfs

# Monitor serial output
pio device monitor
```

### 3. Upload a book

1. Long-press the button to open the menu
2. Navigate to **Settings → WiFi AP** and long-press to turn it on
3. Connect your phone/computer to the `RSVP-Reader` WiFi network
4. Open the IP address shown on screen in a browser
5. Upload a `.txt` file

### 4. Convert an EPUB

Requires `pandoc` and Python 3:

```bash
tools/epub2rsvp.sh input.epub output.txt
```

## OTA firmware updates

Push to `main` triggers a GitHub Actions build that publishes a release with `firmware.bin` and `version.txt`.

To update the device wirelessly:

1. Open the menu → **Settings → Update firmware**
2. The device connects to your phone hotspot (`OTA_SSID`)
3. It shows the available version vs the version currently on the device
4. Long-press to install, short-press to abort

**Recovery**: hold the BOOT button for 3 seconds at power-on to enter OTA mode directly, bypassing the normal reader startup.

## Project structure

```
src/           — firmware source
tools/         — epub2rsvp conversion scripts
data/          — LittleFS filesystem (jszip.js)
docs/          — planning documents
partitions_8mb.csv — partition table (two OTA slots + LittleFS)
```

## Partition layout

| Partition | Size | Purpose |
|---|---|---|
| app0 | 1.75 MB | Active firmware slot |
| app1 | 1.75 MB | OTA firmware slot |
| spiffs | 4.38 MB | LittleFS book storage |
