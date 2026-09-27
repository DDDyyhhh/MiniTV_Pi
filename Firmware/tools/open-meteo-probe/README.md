# Open-Meteo device probe (#22)

This is a temporary ESP32-C3 measurement tool. It does not implement the production Ambient Card temperature service. The fixed Beijing coordinates only exercise the agreed current-only request on the target Wi-Fi.

1. Create a private checkout of the firmware source with `managed_components` available. Keep any `main/local_credentials.h` in that private checkout only; it must exist before the first `idf.py build` if Wi-Fi is not already saved in NVS.
2. Run `python3 Firmware/tools/open-meteo-probe/prepare.py <checkout-root>`. This installs `probe.c` into the private checkout and registers it with the temporary build.
3. Source ESP-IDF v6.0.2 and run `idf.py build` from `<checkout-root>/Firmware`. Keep the build identity, image SHA-256, and the exact source snapshot/commit for the evidence record.
4. Use a **persistent, private session directory**, not `/tmp`, for the full-flash backup. Run `bash Firmware/tools/open-meteo-probe/run.sh <checkout-root>/Firmware <session-directory>`. Keep the ESP32 connected for about 35 minutes.
5. After `RESTORE_MATCH`, run `python3 Firmware/tools/open-meteo-probe/summarize.py <session-directory>/serial.raw.log --filtered <redacted-log> --summary <summary-json>`. Publish only the filtered probe lines and validated summary. The raw serial log and full flash backup can contain private device data and must not be committed.

The runner backs up all 4 MiB before flashing, restores all 4 MiB after capture, and compares a full readback byte for byte. If power or the host fails during the run, retain the private backup for recovery. A successful first request is a point-in-time reachability result; only the second request tests the scheduled 30-minute interval.

For bounded failure-path measurement, start `fault_server.py --bind <LAN-IP> --cert <private-cert> --key <private-key>` on a host reachable by the device. Generate a self-signed certificate and keep its key outside the repository. In a second isolated checkout, run `prepare_fault.py <checkout-root> --host <LAN-IP>` before building. Then use a **new** private session directory with `PROBE_CAPTURE_TIMEOUT=180 PROBE_DONE_MARKER='OM-PROBE: done fault_cases=7' bash Firmware/tools/open-meteo-probe/run.sh <checkout-root>/Firmware <new-session-directory>`. After `RESTORE_MATCH`, run `summarize_fault.py` with the same `--filtered` and `--summary` options. The fixture provides HTTP 429/503, an 8-second timeout, truncation, a 1400-byte body, malformed JSON, and an untrusted TLS certificate. It never contacts Open-Meteo for synthetic failures.

`esp_http_client` in ESP-IDF v6 does not propagate a failure return from `HTTP_EVENT_ON_DATA` to `esp_http_client_perform()`. The fault variant explicitly turns its recorded overflow flag into an effective `ESP_ERR_INVALID_SIZE`; this is a requirement for a later production implementation, not a change to the production firmware here.
