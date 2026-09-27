# UI local-refresh baseline

Source: [redacted serial log](ui-perf-baseline-20260923T105726Z.log). Measurement tool: [temporary probe patch](../../../tools/ui-perf-baseline/probe.patch), [capture script](../../../tools/ui-perf-baseline/capture.py), [summarizer](../../../tools/ui-perf-baseline/summarize.py), and [device procedure](../../../tools/ui-perf-baseline/run.sh).

| Scenario | Samples | Transfers | Pixels | Mean submit-to-complete ms | Max submit-to-complete ms | Max LVGL refresh call ms | Max schedule jitter ms | Overlaps |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| full_screen_once | 1 | 6 | 76800 | 20.74 | 20.80 | 120.24 | 0.00 | 0 |
| static_idle_10s | 0 | 0 | 0 | 0.00 | 0.00 | 0.00 | N/A | 0 |
| clock_64x24_1hz | 10 | 10 | 34440 | 5.81 | 5.81 | 4.23 | 3.43 | 0 |
| date_80x12_1hz | 10 | 10 | 29400 | 5.00 | 5.00 | 3.77 | 5.37 | 0 |
| temperature_attribution_96x16_1hz | 10 | 10 | 38760 | 6.50 | 6.50 | 4.01 | 5.51 | 0 |
| motion_16x16_2hz | 40 | 40 | 27040 | 1.37 | 1.38 | 2.21 | 7.10 | 0 |
| motion_16x16_2hz_wifi_scan | 20 | 20 | 13520 | 1.38 | 1.57 | 2.04 | 6.70 | 0 |

- Minimum free heap observed: `79768` bytes.
- Smallest largest-free-block observed: `69632` bytes.
- Every accepted row completed its required samples with zero failures and zero overlapping transfers.
- Submit-to-complete starts immediately before `esp_lcd_panel_draw_bitmap()` and ends in the transfer-complete callback. It includes driver queueing and is not pure SPI wire time. The full-screen total sums six submissions and is not a separately timed frame duration.
- Static idle has no scheduled samples. Its 10-second delay ended 5.45 ms from the requested interval; that is a window timing error, not sample jitter.
- This table reports measurement only. Acceptance limits belong in the downstream motion decision.

## Provenance and restoration receipt

- Hardware: one ESP32-C3 with 320×240 landscape ST7789 at the verified 10 MHz SPI rate and 40-line DMA buffers; see the [hardware specification](../../hardware/HADRWARE.md). No hardware or clock change was made.
- Probe base commit: `0e78f071d726444e37d623ec993e0bfae3a01935`.
- Probe patch SHA-256: `26a798cd84435ca366e16bbd96802a2dc816cb47977a0cde7f5e875c7e8d230d`.
- ESP-IDF: `ESP-IDF v6.0.2`; esptool: `esptool v5.3.1`.
- Capture: one physical unit over native USB serial/JTAG; unit identifier redacted; UTC artifact stamp `20260923T105726Z`.
- Build identity: app version `0e78f07-dirty`, defined by the base commit and probe patch above.
- Capture command: `capture.py --port <redacted> --output <temporary>/serial.raw.log --timeout 165`.
- Raw serial log SHA-256: `8ba4d42c4034e3131627b8318aaf6fb42bc2e07e505d756dffbb4292a552612d`; raw log was retained only in the temporary session until validation.
- Original flash backup: 4,194,304 bytes, SHA-256 `5228f9ac2e5d56d9b6a4c5e2bfcd607bbf65208f25d485eddac4b5e3dc034b78`.
- Restoration receipt: all 4,194,304 bytes were read back and compared byte-for-byte with the backup before reset; recorded result `MATCH`. The raw backup and readback were not retained in the repository, so this receipt cannot be independently replayed.
- On 2026-09-27, the pinned source plus the recorded patch was rebuilt with ESP-IDF v6.0.2. The build succeeded; `Firmware.bin` was `0x15c840` bytes, below the `0x3f0000` app partition. The archived serial log was accepted by the bundled summarizer; changing the 40-sample motion row to 39 samples and one failure made it reject the log. The device was not connected for a new capture that day.

## Measured facts

- One 320×240 invalidation transferred 76,800 pixels in six 40-line DMA submissions: 124.43 ms summed submit-to-complete durations, 20.80 ms maximum individual duration, and 120.24 ms for the measured LVGL refresh call including UI lock acquisition.
- A genuinely unchanged probe screen produced zero transfers and zero pixels during the 10-second static-idle window.
- Ten representative text changes each produced one transfer per change. Clock (`64×24`) peaked at 5.81 ms transfer / 4.23 ms refresh call; date (`80×12`) at 5.00 / 3.77 ms; temperature and attribution region (`96×16`) at 6.50 / 4.01 ms. Maximum scheduling jitter was 5.51 ms.
- Forty changes to a 16×16 object at 2 Hz produced 40 transfers and 27,040 transferred pixels (676 pixels per change after LVGL invalidation expansion). Maxima were 1.38 ms transfer, 2.21 ms refresh call, and 7.10 ms scheduling jitter.
- Twenty animation samples were admitted while the probe's Wi-Fi scan flag was active. Four scans completed. The samples produced 20 transfers and 13,520 pixels, with maxima of 1.57 ms submit-to-complete, 2.04 ms refresh call, and 6.70 ms scheduling jitter. Every scenario completed with zero failed samples and zero overlapping transfers. The flag check occurs before each update; this log does not prove that every transfer completed before its scan ended or quantify radio airtime overlap.
- Minimum free heap remained 79,768 bytes and the smallest largest-free block remained 69,632 bytes. Free heap at the end of the Wi-Fi scan scenario was 84,012 bytes versus 86,732 bytes before the measured scenarios.

## Derived estimates

- The 2 Hz candidate interval is 500 ms. Its worst observed submit-to-complete duration, LVGL refresh call, and scheduling jitter were each below 8 ms in this run. These separate maxima cannot be summed into an end-to-end frame latency, and the sample does not establish a sustained frame-rate limit.

## Unknowns

- Sustained multi-hour behavior, the final Ambient Card’s actual invalidation geometry, multiple simultaneous effects, visual quality, touch latency, and network traffic other than repeated Wi-Fi scans were not measured. The scan run is a limited network stress sample, not a bound for HTTP or reconnection traffic.
- This probe does not diagnose the user-reported horizontal-swipe lag; that interaction is outside the single-card direction.

## Decision input

- Keep the Ambient Card background static after its initial draw and update time, date, and temperature only when their displayed values change.
- For issue #18, compare a fully static scene with one small effect at no more than 2 Hz first. Measure the selected production scene before accepting additional or overlapping effects. Do not use periodic full-screen invalidation as an animation strategy.
