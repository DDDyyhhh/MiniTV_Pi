#!/usr/bin/env python3
"""Install the issue #22 probe into an isolated checkout of the pinned base."""

from __future__ import annotations

import argparse
import shutil
from pathlib import Path


def replace_once(path: Path, old: str, new: str) -> None:
    source = path.read_text()
    if source.count(old) != 1:
        raise SystemExit(f"expected exactly one insertion point in {path}")
    path.write_text(source.replace(old, new))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("checkout", type=Path)
    args = parser.parse_args()
    project = args.checkout / "Firmware"
    main_source = project / "main/main.c"
    cmake = project / "main/CMakeLists.txt"
    shutil.copyfile(Path(__file__).with_name("probe.c"), project / "main/open_meteo_probe.c")
    replace_once(
        main_source,
        "static const char *TAG = \"minitv\";",
        'static const char *TAG = "minitv";\nextern void open_meteo_probe_start(void);',
    )
    replace_once(
        main_source,
        "    xTaskCreate(diagnostics_task, \"diagnostics\", 3072, NULL, 2, NULL);",
        '    xTaskCreate(diagnostics_task, "diagnostics", 3072, NULL, 2, NULL);\n    open_meteo_probe_start();',
    )
    replace_once(cmake, '        "pc_monitor.c"', '        "pc_monitor.c"\n        "open_meteo_probe.c"')
    replace_once(cmake, "        esp_http_client\n", "        esp_http_client\n        mbedtls\n")


if __name__ == "__main__":
    main()
