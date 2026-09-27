#!/usr/bin/env python3
"""Reset an ESP32-C3 and capture serial output until the perf probe finishes."""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

import serial

DONE_MARKER = b"PERF-PROBE: done scenarios=7"


def reset_target(connection: serial.Serial) -> None:
    connection.dtr = False
    connection.rts = True
    time.sleep(0.1)
    connection.rts = False
    time.sleep(0.2)
    connection.reset_input_buffer()


def capture(port: str, output: Path, timeout_seconds: float) -> bool:
    deadline = time.monotonic() + timeout_seconds
    found_done = False
    with serial.Serial(port, 115200, timeout=0.25) as connection, output.open("wb") as log:
        reset_target(connection)
        while time.monotonic() < deadline:
            line = connection.readline()
            if not line:
                continue
            log.write(line)
            log.flush()
            sys.stdout.buffer.write(line)
            sys.stdout.buffer.flush()
            if DONE_MARKER in line:
                found_done = True
                break
    return found_done


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--timeout", type=float, default=105.0)
    args = parser.parse_args()
    if not capture(args.port, args.output, args.timeout):
        raise SystemExit("probe completion marker not received before timeout")


if __name__ == "__main__":
    main()
