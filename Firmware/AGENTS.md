# Repository Guidelines

## Project Overview
Mini TV (`MiniTV_Pi/Firmware`) is an ESP-IDF v6.0.2 embedded firmware application targeting the ESP32-C3 (RISC-V architecture, 4MB Flash). It implements a desktop mini-display terminal featuring a 240×320 ST7789 SPI color LCD, CST816D I2C capacitive touch, native USB CDC/JTAG console, Wi-Fi SoftAP/STA connectivity, and Home Assistant control backend integration. The interface follows the "Fluent Card Style" visual language presenting three full-screen horizontal Cards (Time Card, PC Monitor Card, Smart Home Card) beneath a pull-down Control Center overlay.

## Architecture & Data Flow
- **Hardware Drivers (`board_*`)**:
  - `board_display`: ST7789 display controller via ESP-IDF `esp_lcd` SPI panel driver. Operating clock is fixed at 10 MHz (`LCD_SPI_CLOCK_HZ`) with DMA double buffering (40 lines / 38,400 bytes per buffer) for verified stability.
  - `board_touch`: CST816D touch controller over I2C Master (`driver/i2c_master.h` on SDA GPIO2 / SCL GPIO3).
  - `board_backlight`: Active-high PWM backlight on GPIO5 via LEDC timer.
  - `board_button`: Hardware BOOT/Function key on GPIO9 (short press toggles Control Center; 3s long press enters Provisioning Portal).
- **UI Subsystem (`ui_*`)**:
  - Built on LVGL 8.3.11 managed by `ui_runtime`.
  - Dedicated FreeRTOS task (`ui_task`, stack 6144, priority 6) running `lv_timer_handler()` every 10 ms with `esp_timer` 5 ms tick callback.
  - Cross-task LVGL thread safety is strictly enforced via `ui_runtime_lock(timeout_ms)` and `ui_runtime_unlock()`. Non-UI tasks request updates asynchronously using `ui_runtime_request_refresh()` or `ui_runtime_request_control_center_toggle()`.
  - `ui_shell` manages the root `lv_tileview` containing three Cards plus the swipe-down Control Center overlay (45 px top hotzone).
- **Core Services & State**:
  - `app_model`: Thread-safe centralized application state (Wi-Fi state, Home Assistant backend probe status, SNTP sync status, brightness, RSSI) protected by a FreeRTOS mutex.
  - `config_store`: Non-Volatile Storage (NVS) persistence under namespace `minitv_cfg` (Wi-Fi credentials, Home Assistant endpoint/token, brightness, entities).
  - `network_manager`: State machine managing Wi-Fi station connection, reconnect backoff, and fallback to SoftAP provisioning portal (`provisioning_portal`).
  - `backend_probe`: Asynchronous HTTP probing for Home Assistant connectivity and authentication verification.
  - `time_service`: SNTP synchronization service updating local system clock and holiday/time state.

```
[Touch CST816D / Button] ──> [ui_runtime (LVGL Task)] ──> [ui_shell / ui_control_center / Cards]
                                     │                                      │
[Network / Backend / SNTP] ──> [app_model (Mutex)] <─────────────────────────┘
                                     │
                             [config_store (NVS)]
```

## Key Directories
- `main/`: Core application and driver source code.
  - `main.c`: Application entry point (`app_main()`).
  - `board_*.c/.h`: Hardware abstraction layer (display, touch, backlight, button, pin definitions).
  - `ui_*.c/.h`: UI runtime, LVGL event loop, shell container, control center overlay, and custom fonts.
  - `card1.c/.h`: Card 1 implementation (Time Card: flip clock, weather trend, holiday countdown).
  - `network_manager.*`, `provisioning_portal.*`: Wi-Fi STA management and SoftAP HTTP configuration portal.
  - `config_store.*`, `app_model.*`: NVS storage and thread-safe runtime state.
  - `backend_probe.*`, `time_service.*`, `diagnostics.*`: HA polling, SNTP sync, and heap/fps diagnostics.
- `docs/hardware/`: Hardware context and pin mapping specification (`HADRWARE.md`).
- `docs/acceptance/`: Formal hardware and milestone acceptance test logs.
- `docs/design/mockups/`: Card and UI mockup references.

## Development Commands

### Environment Setup
Before running build tools outside the ESP-IDF dev container:
```bash
source "$IDF_PATH/export.sh"
```

### Build & Clean
```bash
# Full firmware build
idf.py build

# Clean build artifacts
idf.py clean

# Full clean (resets CMake cache and stale configurations)
idf.py fullclean
```

### Flash & Monitor
```bash
# Flash to target board and open serial monitor (native USB CDC/JTAG)
idf.py -p /dev/ttyACM0 flash monitor

# Serial monitor baud rate
# Configured default baud rate: 115200 (USB CDC console)
```

## Code Conventions & Common Patterns

### Build System Requirements
- Every new `.c` source file **MUST** be explicitly listed in `main/CMakeLists.txt` under `idf_component_register(SRCS ...)`. CMake will not auto-glob C files.
- Header dependencies are managed via `INCLUDE_DIRS "."` and `PRIV_REQUIRES` / `REQUIRES`.

### ESP-IDF v6 APIs & Hardware Rules
- **I2C**: Always use the ESP-IDF v6 `driver/i2c_master.h` driver API. **NEVER** include the legacy `driver/i2c.h`.
- **Display**: Use `esp_lcd` framework (`esp_lcd_panel_io_spi`, `esp_lcd_panel_st7789`).
- **Display SPI Clock**: Maintain the verified **10 MHz** clock (`LCD_SPI_CLOCK_HZ = 10000000`). 20 MHz triggers LVGL Task Watchdog Timer timeouts during SoftAP network load.
- **Pin Definitions**: Define all GPIO pins in `main/board_pins.h`. Never hardcode raw GPIO numbers in functional modules.
- **Error Handling**: Check ESP-IDF calls with `ESP_ERROR_CHECK()` during boot initialization, or return `esp_err_t` using `ESP_RETURN_ON_ERROR()`.

### FreeRTOS & Thread Safety
- **LVGL UI Access**:
  - From external FreeRTOS tasks: **MUST** acquire `ui_runtime_lock(pdMS_TO_TICKS(timeout_ms))` before touching LVGL objects, then release with `ui_runtime_unlock()`.
  - Prefer event notifications: use `ui_runtime_request_refresh()` to signal `ui_task` to refresh safely without external locking.
- **Model State**:
  - `app_model_*` setters and getters handle internal mutex locking automatically. Safe to call from any FreeRTOS task.
- **Task Delays**: Use `vTaskDelay(pdMS_TO_TICKS(ms))` instead of busy-waits.

### Domain Terminology (Product Language)
Follow the ubiquitous language defined in root `CONTEXT.md`:
- Use **Card** (Time Card, PC Monitor Card, Smart Home Card) — *not* page, screen, or tile.
- Use **Control Center** — *not* menu, settings drawer, or notification shade.
- Use **Fluent Card Style** and **Frosted Panel** — *not* glassmorphism or neumorphism.
- Use **Pending State** and **Confirmed State** for entity transitions — *not* optimistic success.
- Use **Offline Backend State** — *not* error or disconnected mode.

## Important Files
- `main/main.c`: Application lifecycle entry point (`app_main`). Coordinates NVS, board peripherals, UI runtime, and network initialization.
- `main/board_pins.h`: Single source of truth for board GPIO allocations:
  - ST7789 SPI: SCLK GPIO4, MOSI GPIO6, DC GPIO7, RST GPIO8, CS GPIO10, BLK GPIO5 (PWM).
  - CST816D I2C: SDA GPIO2, SCL GPIO3 (board pull-ups), INT GPIO0, RST GPIO1.
  - Controls: BOOT Key GPIO9, USB CDC GPIO18/19.
- `main/ui_runtime.c/.h`: LVGL task execution, buffer allocation, tick generation, and UI mutex synchronization.
- `main/app_model.c/.h`: Runtime state storage for Wi-Fi, HA backend, brightness, and time sync.
- `main/config_store.c/.h`: NVS configuration serialization and credential persistence.
- `main/card1.c/.h`: Time Card implementation (flip clock, weather forecast line, holiday display).
- `main/idf_component.yml`: Managed component dependencies (pins LVGL to `8.3.11`).
- `sdkconfig.defaults`: Baseline ESP-IDF settings (target `esp32c3`, 4MB flash, custom `partitions.csv`, USB CDC console, LVGL 16-bit color swap).
- `partitions.csv`: Partition table layout (`nvs` at `0x9000`, `nvs_keys` at `0xF000`, `factory` app at `0x10000` / 3904 KB).
- `docs/hardware/HADRWARE.md`: Authoritative PCB hardware specification and pinout reference.

## Runtime/Tooling Preferences
- **Build Toolchain**: CMake 3.22+ and Ninja driven by ESP-IDF `idf.py` (ESP-IDF v6.0.2).
- **Target Architecture**: `esp32c3` (RISC-V 32-bit RV32IMC).
- **Language**: C99 / C11.
- **Local Credentials**: For development without entering Wi-Fi credentials via portal on every flash, copy `main/local_credentials.h.example` to `main/local_credentials.h` (Git-ignored).

## Testing & QA
- **Unit & Verification Strategy**:
  - Embedded FreeRTOS target: automated test suite runs via physical device flashing or hardware-in-the-loop tests.
  - Runtime diagnostics: `diagnostics.c` runs a periodic 60-second FreeRTOS task logging heap metrics (`minimum_free_heap`, `largest_free_block`), FPS (`ui_runtime_fps`), and Task Watchdog status.
  - Acceptance Checklists: Verified against formal milestone records in `docs/acceptance/` (e.g. `09-skeleton-acceptance.md`).
- **Pre-Commit Verification**:
  - Always verify that the project builds cleanly without warnings:
    ```bash
    idf.py build
    ```
  - Verify image size and partition boundaries: `Firmware.bin` must fit within the factory partition (`0x3F0000` bytes).
