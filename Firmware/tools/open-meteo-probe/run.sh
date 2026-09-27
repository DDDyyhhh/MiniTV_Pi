#!/usr/bin/env bash
set -euo pipefail
umask 077

# This runner uses a prebuilt isolated checkout. It always restores the exact
# original 4 MiB flash, including NVS, and verifies the readback before reset.
if [[ $# -ne 2 ]]; then
  echo "usage: run.sh <isolated-Firmware-directory> <private-session-directory>" >&2
  exit 2
fi
PROBE_FIRMWARE="$1"
PROBE_SESSION="$2"
PROBE_PORT="/dev/ttyACM0"
PROBE_BACKUP="$PROBE_SESSION/original-flash-4mb.bin"
PROBE_READBACK="$PROBE_SESSION/restored-flash-4mb.bin"
PROBE_RAW="$PROBE_SESSION/serial.raw.log"
PROBE_CAPTURE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)/capture.py"
PROBE_IDF_EXPORT="${IDF_EXPORT:-${IDF_PATH:-/home/dyh/.espressif/v6.0.2/esp-idf}/export.sh}"
PROBE_CAPTURE_TIMEOUT="${PROBE_CAPTURE_TIMEOUT:-2100}"
PROBE_DONE_MARKER="${PROBE_DONE_MARKER:-OM-PROBE: done samples=2}"
PROBE_RESTORE=0
PROBE_RESULT=0
mkdir -p "$PROBE_SESSION"
[[ ! -e "$PROBE_BACKUP" ]] || { echo "backup path already exists; use a fresh session directory" >&2; exit 2; }
[[ -c "$PROBE_PORT" && -r "$PROBE_PORT" && -w "$PROBE_PORT" ]]
[[ -f "$PROBE_FIRMWARE/build/Firmware.bin" ]]
source "$PROBE_IDF_EXPORT" >/dev/null 2>&1

restore_original() {
  [[ -f "$PROBE_BACKUP" && "$(stat -c '%s' "$PROBE_BACKUP")" -eq 4194304 ]] || return 1
  python -m esptool --chip esp32c3 --port "$PROBE_PORT" --after no-reset write-flash 0 "$PROBE_BACKUP"
  python -m esptool --chip esp32c3 --port "$PROBE_PORT" --after no-reset read-flash 0 0x400000 "$PROBE_READBACK"
  cmp "$PROBE_BACKUP" "$PROBE_READBACK"
  python -m esptool --chip esp32c3 --port "$PROBE_PORT" run
  PROBE_RESTORE=0
  echo "RESTORE_MATCH sha256=$(sha256sum "$PROBE_BACKUP" | cut -d ' ' -f 1)"
}

cleanup() {
  local result=$?
  trap - EXIT
  if [[ "$PROBE_RESTORE" -eq 1 ]]; then
    if ! restore_original; then
      echo "RESTORE_FAILED: keep $PROBE_BACKUP and $PROBE_READBACK; device needs recovery" >&2
      exit 1
    fi
  fi
  exit "$result"
}
trap cleanup EXIT

python -m esptool --chip esp32c3 --port "$PROBE_PORT" read-flash 0 0x400000 "$PROBE_BACKUP"
[[ "$(stat -c '%s' "$PROBE_BACKUP")" -eq 4194304 ]]
echo "BACKUP_OK sha256=$(sha256sum "$PROBE_BACKUP" | cut -d ' ' -f 1)"
PROBE_RESTORE=1
if (cd "$PROBE_FIRMWARE" && idf.py -p "$PROBE_PORT" flash); then
  if python "$PROBE_CAPTURE" --port "$PROBE_PORT" --output "$PROBE_RAW" --timeout "$PROBE_CAPTURE_TIMEOUT" --done "$PROBE_DONE_MARKER"; then
    echo "CAPTURE_OK sha256=$(sha256sum "$PROBE_RAW" | cut -d ' ' -f 1)"
  else
    echo "CAPTURE_INCOMPLETE: $PROBE_RAW" >&2
    PROBE_RESULT=1
  fi
else
  echo "FLASH_FAILED: original flash will be restored" >&2
  PROBE_RESULT=1
fi
restore_original
exit "$PROBE_RESULT"
