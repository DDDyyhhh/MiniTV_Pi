#!/usr/bin/env python3
"""Validate controlled ESP32 HTTP failure scenarios from a private capture."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path


def parse(raw: Path, filtered: Path) -> dict[str, object]:
    lines: list[str] = []
    results: dict[str, dict[str, str]] = {}
    bodies: dict[str, str] = {}
    for source in raw.read_text(errors="replace").splitlines():
        if source.startswith("HOST_UTC="):
            lines.append(source)
        elif "OM-PROBE:" in source:
            payload = source[source.index("OM-PROBE:"):]
            lines.append(payload)
            if "OM-PROBE: result " in payload:
                values = dict(re.findall(r"([a-z_]+)=([^ ]+)", payload))
                if "scenario" in values:
                    results[values["scenario"]] = values
            elif "OM-PROBE: body " in payload:
                match = re.search(r"scenario=(\S+) json=(.*)", payload)
                if match:
                    bodies[match[1]] = match[2]
    filtered.write_text("\n".join(lines) + "\n")

    checks: dict[str, list[str]] = {}
    for name, status in (("http_429", "429"), ("http_503", "503")):
        item = results.get(name, {})
        checks[name] = [] if item.get("err") == "ESP_OK" and item.get("status") == status and int(item.get("bytes", "1025")) <= 1024 else [f"HTTP {status} path not observed"]
    timeout = results.get("timeout", {})
    checks["timeout"] = [] if timeout.get("err") not in (None, "ESP_OK") and int(timeout.get("elapsed_ms", "0")) >= 7000 else ["bounded timeout not observed"]
    truncated = results.get("truncated", {})
    checks["truncated"] = [] if truncated.get("err") not in (None, "ESP_OK") and int(truncated.get("bytes", "0")) < int(truncated.get("content_length", "0")) else ["truncation not rejected"]
    oversize = results.get("oversize", {})
    checks["oversize"] = [] if oversize.get("overflow") == "1" and int(oversize.get("bytes", "1025")) <= 1024 and oversize.get("err") != "ESP_OK" else ["oversized body not rejected"]
    malformed = results.get("malformed_json", {})
    invalid_json = False
    try:
        json.loads(bodies.get("malformed_json", ""))
    except json.JSONDecodeError:
        invalid_json = True
    checks["malformed_json"] = [] if malformed.get("err") == "ESP_OK" and malformed.get("status") == "200" and invalid_json else ["malformed JSON path not observed"]
    tls = results.get("untrusted_tls", {})
    checks["untrusted_tls"] = [] if tls.get("err") not in (None, "ESP_OK") and tls.get("status") == "0" and tls.get("connected") == "0" else ["untrusted certificate not rejected"]
    return {"results": results, "checks": checks}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("raw", type=Path)
    parser.add_argument("--filtered", required=True, type=Path)
    parser.add_argument("--summary", required=True, type=Path)
    args = parser.parse_args()
    summary = parse(args.raw, args.filtered)
    args.summary.write_text(json.dumps(summary, indent=2, ensure_ascii=False) + "\n")
    print(json.dumps(summary["checks"], ensure_ascii=False))
    if any(summary["checks"].values()):
        sys.exit(1)


if __name__ == "__main__":
    main()
