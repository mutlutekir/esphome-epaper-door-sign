# Door Message Board

An ESPHome + LVGL powered e-paper door sign built on the **Elecrow CrowPanel 4.2" E-Paper** display (ESP32-S3), letting you update a message for visitors/couriers directly from Home Assistant — no physical access to the device needed.

Great for messages like *"Please leave the package by the door"*, *"Back in 10 minutes"*, or a custom welcome note — editable remotely, and it holds the image with zero power draw thanks to e-paper.

## Features

- 📝 Editable door message via a Home Assistant text field — updates the display instantly
- 🔠 Three selectable font sizes (Small / Medium / Large) via a Home Assistant dropdown
- 🖼️ Custom bottom illustration (swap out the PNG for your own artwork)
- 🎨 Built with LVGL for clean, flicker-free layout control
- 🔋 E-paper display — retains the image with no power once drawn
- 🧹 Automatic full refresh every 8 partial updates to prevent ghosting
- 📶 Wi-Fi with fallback AP + captive portal if it can't connect
- 🏠 Native Home Assistant integration via ESPHome API (encrypted)

## Hardware

| Component | Details |
|---|---|
| Board | Espressif ESP32-S3-DevKitC-1-N8R8 (8MB Flash, 8MB PSRAM) |
| Display | Elecrow CrowPanel 4.2" E-Paper (v1.2 / UC8276C) |
| PSRAM | Octal, 80MHz |

### Wiring (E-Paper, SPI)

| Pin | GPIO |
|---|---|
| CLK | GPIO12 |
| MOSI | GPIO11 |
| CS | GPIO45 |
| DC | GPIO46 |
| RESET | GPIO47 |
| BUSY | GPIO48 |
| Power switch | GPIO7 |

## Dependencies

This config uses a community external component for the e-paper display driver:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/ESPBoards/esphome-lvgl-crowpanel-epaper-5.79-4.2
      ref: main
    components:
      - crowpanel_epaper
```

## Installation

1. Install [ESPHome](https://esphome.io/) (via Home Assistant add-on or CLI).
2. Create a `secrets.yaml` with your Wi-Fi credentials:
   ```yaml
   wifi_ssid: "your-ssid"
   wifi_password: "your-password"
   ```
3. Place your bottom illustration image in the same folder as the YAML, named `en_alt_cicek_kedi.png` (or update the `image:` path in the config to your own filename).
4. Flash the config to your board:
   ```bash
   esphome run door-message-board.yaml
   ```
5. Add the device to Home Assistant — it will be auto-discovered via the ESPHome integration.

> ⚠️ Replace the `api.encryption.key` and the Wi-Fi AP fallback password in the YAML with your own values before flashing.

## Configurable Entities (exposed to Home Assistant)

- `Door Message` — text (up to 100 characters)
- `Message Font Size` — select (Small / Medium / Large)
- Clear Screen (button) — Manually trigger a full e-ink screen refresh to clear any potential ghosting.
- Customizable png photo.

## License

MIT
