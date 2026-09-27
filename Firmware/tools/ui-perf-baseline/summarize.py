#!/usr/bin/env python3
"""Summarize redacted [PERF-PROBE] serial output into issue-ready Markdown."""

from __future__ import annotations

import argparse
import re
from pathlib import Path

RESULT = re.compile(r"result\s+(?P<fields>.+)$")
PAIR = re.compile(r"(?P<key>[a-z0-9_]+)=(?P<value>[^\s]+)")
EXPECTED = (
    "full_screen_once",
    "static_idle_10s",
    "clock_64x24_1hz",
    "date_80x12_1hz",
    "temperature_attribution_96x16_1hz",
    "motion_16x16_2hz",
    "motion_16x16_2hz_wifi_scan",
)
EXPECTED_SAMPLES = {
    "full_screen_once": 1,
    "static_idle_10s": 0,
    "clock_64x24_1hz": 10,
    "date_80x12_1hz": 10,
    "temperature_attribution_96x16_1hz": 10,
    "motion_16x16_2hz": 40,
    "motion_16x16_2hz_wifi_scan": 20,
}
INTEGER_FIELDS = {
    "samples",
    "failed_samples",
    "transfers",
    "pixels",
    "transfer_total_us",
    "transfer_max_us",
    "render_total_us",
    "render_max_us",
    "schedule_jitter_max_us",
    "overlaps",
    "free_heap",
    "minimum_free_heap",
    "largest_free_block",
}


def parse_results(text: str) -> dict[str, dict[str, int | str]]:
    if "PERF-PROBE: done scenarios=7" not in text:
        raise ValueError("probe completion marker is missing")
    if "PERF-PROBE: failed " in text:
        raise ValueError("probe reported a failed measurement")

    network_lines = [
        line for line in text.splitlines()
        if "PERF-PROBE" in line and "network_activity=wifi_scan" in line
    ]
    if len(network_lines) != 1 or "start_result=ESP_OK" not in network_lines[0]:
        raise ValueError("exactly one successful Wi-Fi scan activity result is required")
    network_fields = {
        pair.group("key"): pair.group("value")
        for pair in PAIR.finditer(network_lines[0])
    }
    scans_started = int(network_fields.get("scans_started", "0"))
    scan_done_events = int(network_fields.get("scan_done_events", "-1"))
    active_samples = int(network_fields.get("active_samples", "0"))
    if scans_started < 1 or scan_done_events != scans_started or active_samples != 20:
        raise ValueError("Wi-Fi activity did not cover all 20 required animation samples")

    results: dict[str, dict[str, int | str]] = {}
    for line in text.splitlines():
        if "PERF-PROBE" not in line or "result scenario=" not in line:
            continue
        match = RESULT.search(line)
        if match is None:
            raise ValueError(f"malformed result line: {line}")
        fields: dict[str, int | str] = {}
        for pair in PAIR.finditer(match.group("fields")):
            key, value = pair.group("key"), pair.group("value")
            fields[key] = int(value) if key in INTEGER_FIELDS else value
        scenario = fields.get("scenario")
        if not isinstance(scenario, str) or scenario not in EXPECTED:
            raise ValueError(f"unexpected scenario result: {scenario}")
        if scenario in results:
            raise ValueError(f"duplicate scenario result: {scenario}")
        missing_fields = sorted(INTEGER_FIELDS - fields.keys())
        if missing_fields:
            raise ValueError(f"{scenario} missing field(s): {', '.join(missing_fields)}")
        if fields["samples"] != EXPECTED_SAMPLES[scenario] or fields["failed_samples"] != 0:
            raise ValueError(f"{scenario} did not complete the required samples")
        if fields["overlaps"] != 0:
            raise ValueError(f"{scenario} has overlapping transfers")
        if scenario == "static_idle_10s":
            if fields["transfers"] != 0 or fields["pixels"] != 0:
                raise ValueError("static idle produced display traffic")
        elif fields["transfers"] == 0 or fields["pixels"] == 0:
            raise ValueError(f"{scenario} has no measured display traffic")
        results[scenario] = fields
    missing = [name for name in EXPECTED if name not in results]
    if missing:
        raise ValueError(f"missing scenario result(s): {', '.join(missing)}")
    return results


def milliseconds(value: int) -> str:
    return f"{value / 1000:.2f}"


def render_markdown(results: dict[str, dict[str, int | str]], source: Path) -> str:
    rows = []
    for name in EXPECTED:
        values = results[name]
        transfers = int(values["transfers"])
        transfer_total = int(values["transfer_total_us"])
        mean_transfer = transfer_total // transfers if transfers else 0
        rows.append(
            "| {name} | {samples} | {transfers} | {pixels} | {mean} | {maximum} | {render} | {jitter} | {overlaps} |".format(
                name=name,
                samples=values["samples"],
                transfers=transfers,
                pixels=values["pixels"],
                mean=milliseconds(mean_transfer),
                maximum=milliseconds(int(values["transfer_max_us"])),
                render=milliseconds(int(values["render_max_us"])),
                jitter=milliseconds(int(values["schedule_jitter_max_us"])),
                overlaps=values["overlaps"],
            )
        )
    minimum_heap = min(int(results[name]["minimum_free_heap"]) for name in EXPECTED)
    largest_block = min(int(results[name]["largest_free_block"]) for name in EXPECTED)
    return "\n".join(
        [
            "# UI local-refresh baseline",
            "",
            f"Source: `{source.name}`",
            "",
            "| Scenario | Samples | Transfers | Pixels | Mean transfer ms | Max transfer ms | Max render call ms | Max schedule jitter ms | Overlaps |",
            "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |",
            *rows,
            "",
            f"- Minimum free heap observed: `{minimum_heap}` bytes.",
            f"- Smallest largest-free-block observed: `{largest_block}` bytes.",
            "- Every accepted row completed its required samples with zero failures and zero overlapping transfers.",
            "- This table reports measurement only. Acceptance limits belong in the downstream motion decision.",
        ]
    ) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    results = parse_results(args.log.read_text(encoding="utf-8", errors="replace"))
    args.output.write_text(render_markdown(results, args.log), encoding="utf-8")
    print(args.output)


if __name__ == "__main__":
    main()
