#!/usr/bin/env python3
"""Prepare an isolated, short-run on-device HTTP failure probe."""

from __future__ import annotations

import argparse
import ipaddress
import subprocess
import sys
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("checkout", type=Path)
    parser.add_argument("--host", required=True, help="LAN IPv4 of the fixture server")
    args = parser.parse_args()
    address = ipaddress.IPv4Address(args.host)
    subprocess.run([sys.executable, str(Path(__file__).with_name("prepare.py")), str(args.checkout)], check=True)
    source = args.checkout / "Firmware/main/open_meteo_probe.c"
    subprocess.run(["patch", str(source), str(Path(__file__).with_name("fault.patch"))], check=True)
    source.write_text(f'#define PROBE_FAULT_MODE 1\n#define PROBE_FAULT_HOST "{address}"\n' + source.read_text())


if __name__ == "__main__":
    main()
