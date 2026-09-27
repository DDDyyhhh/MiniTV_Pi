#!/usr/bin/env python3
"""Read boot health without printing Wi-Fi credentials or backend addresses."""

from __future__ import annotations

import argparse
import time

import serial


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="/dev/ttyACM0")
    parser.add_argument("--seconds", type=int, default=30)
    args = parser.parse_args()
    facts = {
        "serial_output": False,
        "temporary_probe": False,
        "provisioning_ap": False,
        "wifi_got_ip": False,
        "time_synchronized": False,
        "probe_ready_wifi": False,
    }
    with serial.Serial(args.port, 115200, timeout=0.4) as device:
        device.dtr = False
        device.rts = True
        time.sleep(0.1)
        device.rts = False
        time.sleep(0.2)
        device.reset_input_buffer()
        deadline = time.monotonic() + args.seconds
        while time.monotonic() < deadline:
            raw = device.readline()
            if not raw:
                continue
            facts["serial_output"] = True
            line = raw.decode(errors="replace")
            if "OM-PROBE: start" in line:
                facts["temporary_probe"] = True
            if "network: Provisioning AP" in line:
                facts["provisioning_ap"] = True
            if "got ip" in line.lower() or "ip_event_sta_got_ip" in line.lower():
                facts["wifi_got_ip"] = True
            if "SNTP synchronized" in line:
                facts["time_synchronized"] = True
            if "OM-PROBE: ready wifi=1" in line:
                facts["probe_ready_wifi"] = True
    for name, value in facts.items():
        print(f"{name}={int(value)}")


if __name__ == "__main__":
    main()
