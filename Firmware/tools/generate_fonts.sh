#!/usr/bin/env bash
# Regenerate the tracked Skeleton Milestone Chinese subset.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
charset=$(tr '\n' ' ' < "$root/tools/font_charset_skeleton.txt")
npx --yes lv_font_conv --size 16 --bpp 2 --format lvgl \
  --font /usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf \
  -r 0x20-0x7e --symbols "$charset" --lv-font-name font_cn_16 \
  --no-kerning --lv-include lvgl.h -o "$root/main/fonts/font_cn_16.c"
