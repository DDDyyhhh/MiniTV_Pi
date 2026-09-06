#!/usr/bin/env python3
"""Publish PC Confirmed Telemetry and execute fixed Mini TV Launch Actions."""

from __future__ import annotations

import csv
import json
import logging
import math
import os
import re
import signal
import subprocess
import sys
import threading
import time
from collections import OrderedDict
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Final
from urllib.error import HTTPError, URLError
from urllib.parse import urlsplit
from urllib.request import Request, urlopen

import psutil
from websockets.exceptions import WebSocketException
from websockets.sync.client import connect

LOG: Final = logging.getLogger("minitv_pc_agent")
SAMPLE_SECONDS: Final = 5
WEBSOCKET_RETRY_SECONDS: Final = 2
REQUEST_TIMEOUT_SECONDS: Final = 5
MAX_SEEN_REQUESTS: Final = 128
UUID_PATTERN: Final = re.compile(r"^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$")

ALLOWED_ACTIONS: Final[dict[str, tuple[str, ...]]] = {
    "open_vscode": ("code", "--new-window"),
    "open_bilibili": ("google-chrome", "https://www.bilibili.com"),
    "open_douyin": ("google-chrome", "https://www.douyin.com"),
}


@dataclass(frozen=True)
class AgentConfig:
    ha_url: str
    token: str
    display: str
    xauthority: str
    dbus_session_bus_address: str

    @classmethod
    def from_environment(cls) -> "AgentConfig":
        ha_url = os.environ.get("MINITV_HA_URL", "").rstrip("/")
        token = os.environ.get("MINITV_HA_TOKEN", "")
        display = os.environ.get("DISPLAY", "")
        xauthority = os.environ.get("XAUTHORITY", "")
        dbus_session_bus_address = os.environ.get("DBUS_SESSION_BUS_ADDRESS", "")
        parsed = urlsplit(ha_url)
        if parsed.scheme not in {"http", "https"} or not parsed.netloc or parsed.path not in {"", "/"}:
            raise ValueError("MINITV_HA_URL must be an http(s) origin without a path")
        if not token:
            raise ValueError("MINITV_HA_TOKEN is required")
        return cls(ha_url, token, display, xauthority, dbus_session_bus_address)


class HomeAssistant:
    def __init__(self, config: AgentConfig) -> None:
        self._config = config

    def _request(self, method: str, path: str, payload: bytes | None = None) -> dict[str, Any]:
        request = Request(
            f"{self._config.ha_url}{path}",
            data=payload,
            method=method,
            headers={
                "Authorization": f"Bearer {self._config.token}",
                "Content-Type": "application/json",
                "Accept": "application/json",
            },
        )
        try:
            with urlopen(request, timeout=REQUEST_TIMEOUT_SECONDS) as response:
                if not 200 <= response.status < 300:
                    raise RuntimeError(f"HA request returned HTTP {response.status}")
                body = response.read()
        except HTTPError as error:
            raise RuntimeError(f"HA request returned HTTP {error.code}") from error
        except URLError as error:
            raise RuntimeError("HA request is unreachable") from error
        try:
            return json.loads(body) if body else {}
        except json.JSONDecodeError as error:
            raise RuntimeError("HA request returned malformed JSON") from error

    def get_state(self, entity_id: str) -> dict[str, Any]:
        return self._request("GET", f"/api/states/{entity_id}")

    def set_state(self, entity_id: str, state: str, attributes: dict[str, Any]) -> None:
        payload = json.dumps({"state": state, "attributes": attributes}, separators=(",", ":")).encode()
        self._request("POST", f"/api/states/{entity_id}", payload)


def captured_at() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds").replace("+00:00", "Z")


def metric_attributes(name: str, captured: str, availability: str, **extra: Any) -> dict[str, Any]:
    attributes: dict[str, Any] = {
        "friendly_name": name,
        "captured_at": captured,
        "source": "minitv_pc_agent",
        "availability": availability,
    }
    attributes.update(extra)
    return attributes


def finite_or_none(value: float | None) -> float | None:
    return value if value is not None and math.isfinite(value) else None


def cpu_temperature_c() -> float | None:
    try:
        temperatures = psutil.sensors_temperatures(fahrenheit=False)
    except (AttributeError, OSError):
        return None
    for entries in temperatures.values():
        for entry in entries:
            if entry.current is not None:
                return finite_or_none(float(entry.current))
    return None


def gpu_metrics() -> dict[str, float | str | None]:
    command = [
        "nvidia-smi",
        "--query-gpu=name,utilization.gpu,temperature.gpu,memory.used,memory.total,power.draw",
        "--format=csv,noheader,nounits",
    ]
    try:
        output = subprocess.check_output(command, text=True, stderr=subprocess.DEVNULL, timeout=3)
        row = next(csv.reader([output.strip()]))
        if len(row) != 6:
            raise ValueError("unexpected nvidia-smi field count")
        name, utilisation, temperature, memory_used, memory_total, power = (item.strip() for item in row)
        parse = lambda value: None if value in {"", "N/A", "[Not Supported]"} else finite_or_none(float(value))
        return {
            "name": name or None,
            "utilisation": parse(utilisation),
            "temperature": parse(temperature),
            "memory_used": parse(memory_used),
            "memory_total": parse(memory_total),
            "power": parse(power),
        }
    except (FileNotFoundError, subprocess.CalledProcessError, subprocess.TimeoutExpired, StopIteration, ValueError):
        return {"name": None, "utilisation": None, "temperature": None, "memory_used": None, "memory_total": None, "power": None}


def publish_numeric(ha: HomeAssistant, entity_id: str, value: float | None, name: str, captured: str, unit: str, **extra: Any) -> None:
    availability = "available" if value is not None else "unavailable"
    attributes = metric_attributes(name, captured, availability, unit_of_measurement=unit, **extra)
    ha.set_state(entity_id, "unknown" if value is None else f"{value:.2f}", attributes)


def publish_text(ha: HomeAssistant, entity_id: str, value: str | None, name: str, captured: str, **extra: Any) -> None:
    availability = "available" if value else "unavailable"
    ha.set_state(entity_id, value or "unknown", metric_attributes(name, captured, availability, **extra))


def publish_telemetry(ha: HomeAssistant) -> None:
    captured = captured_at()
    memory = psutil.virtual_memory()
    gpu = gpu_metrics()
    cpu_percent = finite_or_none(float(psutil.cpu_percent(interval=None)))
    ram_percent = finite_or_none(float(memory.percent))
    ram_used_gib = finite_or_none(float(memory.used) / (1024**3))
    ram_total_gib = finite_or_none(float(memory.total) / (1024**3))
    cpu_temp = cpu_temperature_c()

    publish_numeric(ha, "sensor.pc_cpu_percent", cpu_percent, "PC CPU utilisation", captured, "%")
    publish_numeric(ha, "sensor.pc_cpu_temp_c", cpu_temp, "PC CPU temperature", captured, "°C", device_class="temperature")
    publish_numeric(ha, "sensor.pc_ram_percent", ram_percent, "PC RAM utilisation", captured, "%")
    publish_numeric(ha, "sensor.pc_ram_used_gib", ram_used_gib, "PC RAM used", captured, "GiB", device_class="data_size", total_gib=ram_total_gib)
    publish_numeric(ha, "sensor.pc_gpu_util_percent", gpu["utilisation"], "PC GPU utilisation", captured, "%")
    publish_numeric(ha, "sensor.pc_gpu_temp_c", gpu["temperature"], "PC GPU temperature", captured, "°C", device_class="temperature")
    publish_numeric(ha, "sensor.pc_gpu_vram_used_mib", gpu["memory_used"], "PC GPU VRAM used", captured, "MiB", device_class="data_size", total_mib=gpu["memory_total"])
    publish_numeric(ha, "sensor.pc_gpu_power_w", gpu["power"], "PC GPU power", captured, "W", device_class="power")
    publish_text(ha, "sensor.pc_gpu_name", gpu["name"], "PC GPU", captured)
    ha.set_state("sensor.pc_telemetry_updated", captured, metric_attributes("PC telemetry updated", captured, "available", device_class="timestamp"))
    ha.set_state("binary_sensor.pc_agent_online", "on", metric_attributes("PC agent online", captured, "available", device_class="connectivity"))


def gui_environment(config: AgentConfig) -> dict[str, str] | None:
    if not config.display.startswith(":") or not config.dbus_session_bus_address.startswith("unix:path="):
        return None
    if not config.xauthority or not Path(config.xauthority).is_file() or not os.access(config.xauthority, os.R_OK):
        return None
    environment = os.environ.copy()
    environment.update(
        {
            "DISPLAY": config.display,
            "XAUTHORITY": config.xauthority,
            "DBUS_SESSION_BUS_ADDRESS": config.dbus_session_bus_address,
        }
    )
    return environment


class ActionExecutor:
    def __init__(self, config: AgentConfig, ha: HomeAssistant) -> None:
        self._config = config
        self._ha = ha
        self._seen: OrderedDict[str, None] = OrderedDict()

    def observe(self, payload: Any) -> None:
        if not isinstance(payload, dict):
            return
        request_id = payload.get("request_id")
        action = payload.get("action_id")
        if not isinstance(request_id, str) or not UUID_PATTERN.fullmatch(request_id):
            return
        if request_id in self._seen:
            return
        self._remember(request_id)
        if action not in ALLOWED_ACTIONS:
            self._publish_result(request_id, str(action), "rejected", "Action is not allowlisted")
            return
        environment = gui_environment(self._config)
        if environment is None:
            self._publish_result(request_id, action, "failed", "Graphical session unavailable")
            return
        try:
            subprocess.Popen(
                ALLOWED_ACTIONS[action],
                env=environment,
                stdin=subprocess.DEVNULL,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                close_fds=True,
                start_new_session=True,
            )
        except OSError:
            self._publish_result(request_id, action, "failed", "Launch failed")
            return
        self._publish_result(request_id, action, "launched", "Launch requested")

    def _remember(self, request_id: str) -> None:
        self._seen[request_id] = None
        if len(self._seen) > MAX_SEEN_REQUESTS:
            self._seen.popitem(last=False)

    def _publish_result(self, request_id: str, action_id: str, state: str, detail: str) -> None:
        updated = captured_at()
        try:
            self._ha.set_state(
                "sensor.pc_ui_action_status",
                state,
                metric_attributes("PC Launch Action status", updated, "available", request_id=request_id, action_id=action_id, updated_at=updated, detail=detail),
            )
        except RuntimeError:
            LOG.warning("Could not publish PC Launch Action result")


def websocket_url(config: AgentConfig) -> str:
    parsed = urlsplit(config.ha_url)
    scheme = "wss" if parsed.scheme == "https" else "ws"
    return f"{scheme}://{parsed.netloc}/api/websocket"


def receive_action_event(message: Any) -> dict[str, Any] | None:
    if not isinstance(message, str):
        return None
    try:
        packet = json.loads(message)
    except json.JSONDecodeError:
        return None
    if not isinstance(packet, dict) or packet.get("type") != "event":
        return None
    event = packet.get("event")
    if not isinstance(event, dict) or event.get("event_type") != "pc_ui_action":
        return None
    data = event.get("data")
    return data if isinstance(data, dict) else None


def action_listener(config: AgentConfig, executor: ActionExecutor, stop_event: threading.Event) -> None:
    while not stop_event.is_set():
        try:
            with connect(websocket_url(config), open_timeout=REQUEST_TIMEOUT_SECONDS, close_timeout=REQUEST_TIMEOUT_SECONDS) as socket:
                greeting = json.loads(socket.recv(timeout=REQUEST_TIMEOUT_SECONDS))
                if greeting.get("type") != "auth_required":
                    raise RuntimeError("HA WebSocket greeting invalid")
                socket.send(json.dumps({"type": "auth", "access_token": config.token}))
                auth = json.loads(socket.recv(timeout=REQUEST_TIMEOUT_SECONDS))
                if auth.get("type") != "auth_ok":
                    raise RuntimeError("HA WebSocket authentication failed")
                socket.send(json.dumps({"id": 1, "type": "subscribe_events", "event_type": "pc_ui_action"}))
                subscription = json.loads(socket.recv(timeout=REQUEST_TIMEOUT_SECONDS))
                if subscription.get("type") != "result" or subscription.get("id") != 1 or not subscription.get("success"):
                    raise RuntimeError("HA WebSocket event subscription failed")
                LOG.info("Subscribed to PC Launch Action events")
                while not stop_event.is_set():
                    try:
                        payload = receive_action_event(socket.recv(timeout=1))
                    except TimeoutError:
                        continue
                    if payload is not None:
                        executor.observe(payload)
        except (OSError, RuntimeError, WebSocketException, json.JSONDecodeError) as error:
            if not stop_event.is_set():
                LOG.warning("PC Launch Action listener unavailable: %s", error)
                stop_event.wait(WEBSOCKET_RETRY_SECONDS)


def mark_offline(ha: HomeAssistant) -> None:
    captured = captured_at()
    ha.set_state("binary_sensor.pc_agent_online", "off", metric_attributes("PC agent online", captured, "unavailable", device_class="connectivity"))


def run(config: AgentConfig) -> int:
    stop_event = threading.Event()

    def stop(_signal: int, _frame: Any) -> None:
        stop_event.set()

    signal.signal(signal.SIGINT, stop)
    signal.signal(signal.SIGTERM, stop)
    ha = HomeAssistant(config)
    psutil.cpu_percent(interval=None)
    executor = ActionExecutor(config, ha)
    listener = threading.Thread(target=action_listener, args=(config, executor, stop_event), name="pc_action_listener")
    listener.start()
    failures = 0
    next_sample = 0.0
    while not stop_event.is_set():
        now = time.monotonic()
        if now >= next_sample:
            try:
                publish_telemetry(ha)
                failures = 0
            except (RuntimeError, OSError, psutil.Error):
                failures += 1
                LOG.warning("PC telemetry sample failed (%d consecutive failures)", failures)
                if failures >= 3:
                    try:
                        mark_offline(ha)
                    except RuntimeError:
                        pass
            next_sample = now + SAMPLE_SECONDS
        stop_event.wait(max(0.0, next_sample - time.monotonic()))
    listener.join(timeout=REQUEST_TIMEOUT_SECONDS + 1)
    try:
        mark_offline(ha)
    except RuntimeError:
        LOG.warning("Could not mark PC agent offline during shutdown")
    return 0


def main() -> int:
    logging.basicConfig(level=os.environ.get("MINITV_PC_AGENT_LOG_LEVEL", "INFO"), format="%(asctime)s %(levelname)s %(message)s")
    try:
        return run(AgentConfig.from_environment())
    except ValueError as error:
        LOG.error("PC agent configuration invalid: %s", error)
        return 2


if __name__ == "__main__":
    sys.exit(main())
