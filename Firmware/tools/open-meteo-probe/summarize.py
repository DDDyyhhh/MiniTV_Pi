#!/usr/bin/env python3
"""Validate bounded Open-Meteo probe responses from a private serial capture."""

from __future__ import annotations

import argparse
import json
import math
import re
import statistics
import sys
from datetime import datetime, timezone
from pathlib import Path

PREFIX = "OM-PROBE: "
SAMPLE_NAMES = ("current_1", "current_2")


def fields(line: str) -> dict[str, str]:
    return dict(re.findall(r"([a-z_]+)=([^ ]+)", line))


def exact_int(value: object, expected: int | None = None) -> bool:
    return type(value) is int and (expected is None or value == expected)


def validate_sample(result: dict[str, str], body: str, received: datetime) -> list[str]:
    problems: list[str] = []
    if result.get("err") != "ESP_OK" or result.get("status") != "200":
        problems.append("HTTP/TLS request did not complete with 200")
    if result.get("connected") != "1" or result.get("finished") != "1":
        problems.append("connection or response completion missing")
    if result.get("overflow") != "0" or int(result.get("bytes", "1025")) > 1024:
        problems.append("1 KiB response bound exceeded")
    if result.get("content_encoding") != "identity":
        problems.append("response was not identity encoded")
    if len(body.encode("utf-8")) != int(result.get("bytes", "-1")):
        problems.append("captured JSON length differs from device count")
    try:
        data = json.loads(body)
    except json.JSONDecodeError:
        return problems + ["response is not complete JSON"]
    if not isinstance(data, dict):
        return problems + ["root is not an object"]
    units = data.get("current_units")
    current = data.get("current")
    if not exact_int(data.get("utc_offset_seconds"), 28800):
        problems.append("UTC+8 offset invalid")
    if not isinstance(units, dict) or units.get("time") != "unixtime" or units.get("temperature_2m") != "°C":
        problems.append("current units invalid")
    if not isinstance(current, dict):
        return problems + ["current object missing"]
    timestamp = current.get("time")
    if not exact_int(timestamp) or timestamp <= 0:
        problems.append("current.time invalid")
    elif timestamp < received.timestamp() - 7200 or timestamp > received.timestamp() + 1800:
        problems.append("current.time outside -2h/+30m window")
    interval = current.get("interval")
    if not exact_int(interval) or not 0 < interval <= 3600:
        problems.append("current.interval invalid")
    temperature = current.get("temperature_2m")
    if type(temperature) not in (int, float) or not math.isfinite(temperature) or not -100 <= temperature <= 70:
        problems.append("current.temperature_2m invalid")
    return problems


def summarize(raw: Path, filtered: Path) -> dict[str, object]:
    received = datetime.now(timezone.utc)
    results: dict[str, dict[str, str]] = {}
    bodies: dict[str, str] = {}
    times: dict[str, datetime] = {}
    safe_lines: list[str] = []
    periodic: list[dict[str, int]] = []
    for line in raw.read_text(errors="replace").splitlines():
        if line.startswith("HOST_UTC="):
            received = datetime.fromisoformat(line.partition("=")[2])
            safe_lines.append(line)
            continue
        marker = line.find(PREFIX)
        if marker < 0:
            if "baseline: [periodic] " in line:
                values = fields(line)
                keys = ("free_heap", "minimum_free_heap", "largest_free_block", "fps")
                if all(key in values for key in keys):
                    row = {key: int(values[key]) for key in keys}
                    periodic.append(row)
                    safe_lines.append("PERIODIC " + " ".join(f"{key}={row[key]}" for key in keys))
            continue
        safe_lines.append(line[marker:])
        payload = line[marker + len(PREFIX):]
        if payload.startswith("result "):
            result = fields(payload)
            name = result.get("scenario")
            if name:
                results[name] = result
                times[name] = received
        elif payload.startswith("body "):
            match = re.match(r"body scenario=(\S+) json=(.*)", payload)
            if match:
                bodies[match[1]] = match[2]
    filtered.write_text("\n".join(safe_lines) + "\n")
    checks = {
        name: validate_sample(results[name], bodies.get(name, ""), times[name])
        if name in results else ["sample missing"]
        for name in SAMPLE_NAMES
    }
    interval_seconds = (
        int((times["current_2"] - times["current_1"]).total_seconds())
        if all(name in times for name in SAMPLE_NAMES) else None
    )
    fault_checks: dict[str, list[str]] = {}
    bad_parameter = results.get("bad_parameter_400")
    fault_checks["bad_parameter_400"] = (
        [] if bad_parameter and bad_parameter.get("err") == "ESP_OK" and bad_parameter.get("status") == "400"
        and int(bad_parameter.get("bytes", "1025")) <= 1024 else ["expected bounded HTTP 400 response missing"]
    )
    dns_failure = results.get("dns_failure")
    fault_checks["dns_failure"] = (
        [] if dns_failure and dns_failure.get("err") != "ESP_OK" and dns_failure.get("connected") == "0"
        else ["expected pre-connect DNS failure missing"]
    )
    deflate = results.get("deflate_offer")
    fault_checks["deflate_offer"] = (
        [] if deflate and deflate.get("err") == "ESP_OK" and deflate.get("status") == "200"
        and deflate.get("content_encoding") == "deflate" and int(deflate.get("bytes", "1025")) <= 1024
        else ["expected bounded deflate response missing"]
    )
    return {
        "results": results,
        "checks": checks,
        "fault_checks": fault_checks,
        "interval_seconds": interval_seconds,
        "interval_check": [] if interval_seconds is not None and interval_seconds >= 1800 else ["30-minute interval incomplete"],
        "periodic": {
            "count": len(periodic),
            **{key: {"min": min(row[key] for row in periodic),
                     "max": max(row[key] for row in periodic),
                     "median": statistics.median(row[key] for row in periodic)}
               for key in ("free_heap", "minimum_free_heap", "largest_free_block", "fps")}
        } if periodic else {"count": 0},
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("raw", type=Path)
    parser.add_argument("--filtered", required=True, type=Path)
    parser.add_argument("--summary", required=True, type=Path)
    args = parser.parse_args()
    summary = summarize(args.raw, args.filtered)
    args.summary.write_text(json.dumps(summary, indent=2, ensure_ascii=False) + "\n")
    print(json.dumps({"checks": summary["checks"], "fault_checks": summary["fault_checks"],
                      "interval_seconds": summary["interval_seconds"]}, ensure_ascii=False))
    if any(summary[section][name] for section in ("checks", "fault_checks") for name in summary[section]) or summary["interval_check"]:
        sys.exit(1)


if __name__ == "__main__":
    main()
