# Firmware Agent Notes

## Toolchain and Verification

- This is an ESP-IDF v6.0.2 project targeting `esp32c3` (RISC-V), built with Ninja. Before invoking ESP-IDF tooling outside the dev container, load the environment with `source "$IDF_PATH/export.sh"`; the provided dev container loads `/opt/esp/idf/export.sh` from `~/.bashrc`.
- Build from this directory with `idf.py build`. Use `idf.py -p <serial-port> flash monitor` for a connected board; the configured monitor baud rate is `115200`. `idf.py fullclean` is the reset path when an ESP-IDF configuration or build-generation change leaves stale artifacts.
- `sdkconfig` defines the hardware target and is ignored by Git despite being present locally. Preserve its ESP32-C3/4 MB-flash configuration unless the task specifically changes board configuration.

## Project Shape

- `main/main.c` contains `app_main()` and is the sole application source today. Add every new source file to `main/CMakeLists.txt` in the `idf_component_register(SRCS ...)` call or it will not be compiled.
- Board wiring is authoritative in `docs/hardware/HADRWARE.md`: ST7789 is a 240x320 SPI display; CST816D touch uses I2C SDA GPIO2 and SCL GPIO3 with board-mounted pull-ups; display backlight is active-high GPIO5. Use the ESP-IDF v6 `driver/i2c_master.h` API, not the legacy `driver/i2c.h`; use `esp_lcd` for display work.
- Native USB CDC/JTAG is on GPIO18/GPIO19. Keep GPIO0/GPIO1 available for the CST816D interrupt/reset and GPIO9 for the boot/function key.

## Product Language

- For Mini TV UI, backend, or planning work, read `../CONTEXT.md` first. It defines required domain terms such as Card, Control Center, Pending State, and Confirmed State; preserve those terms in user-facing code and documentation.
