#!/usr/bin/env python3
"""Capture a single issue #22 probe run; keep unredacted boot logs outside Git."""

from __future__ import annotations

import argparse
import time
from datetime import datetime, timezone
from pathlib import Path

import serial

UNAVAILABLE = b"OM-PROBE: done reason=network_or_time_unavailable"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="/dev/ttyACM0")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--timeout", type=int, default=2100)
    parser.add_argument("--done", default="OM-PROBE: done samples=2")
    args = parser.parse_args()
    done = args.done.encode()
    deadline = time.monotonic() + args.timeout
    with serial.Serial(args.port, 115200, timeout=0.5) as device, args.output.open("wb") as output:
        device.dtr = False
        device.rts = True
        time.sleep(0.1)
        device.rts = False
        time.sleep(0.2)
        device.reset_input_buffer()
        while time.monotonic() < deadline:
            line = device.readline()
            if not line:
                continue
            if b"OM-PROBE:" in line:
                stamp = datetime.now(timezone.utc).isoformat(timespec="seconds")
                output.write(f"HOST_UTC={stamp}\n".encode())
            output.write(line)
            output.flush()
            if done in line:
                print("probe completion marker received")
                return
            if UNAVAILABLE in line:
                raise SystemExit("target network or SNTP unavailable; see redacted log")
    raise SystemExit("probe timed out before second sample")


if __name__ == "__main__":
    main()
