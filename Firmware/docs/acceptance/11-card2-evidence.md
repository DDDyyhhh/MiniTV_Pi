# 工单 11 PC Monitor Card 验收记录

日期：2026-09-06

## 验收范围

本记录覆盖工单 07 的 PC Monitor Card 方案：Linux Target PC 的 Confirmed Telemetry、固定 Launch Actions、点击反馈、Offline Backend State，以及 ESP32-C3 真机固件路径。

## 电脑侧部署

- [x] Metrics Agent 与 Command Agent 已部署为 `systemd --user` 服务 `minitv-pc-agent.service`。
- [x] Python 运行环境可导入 `psutil==5.9.0` 与 `websockets==15.0.1`；`nvidia-smi` 回退命令可用，实测 GPU 为 NVIDIA GeForce GTX 1660 SUPER。
- [x] 服务状态为 `enabled` / `active`，服务使用 `graphical-session.target`，并注入 `DISPLAY=:1`、`XAUTHORITY=/run/user/1000/gdm/Xauthority`、`DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus`。
- [x] LLAT 位于 `~/.config/minitv-pc-agent/environment`，权限为 `0600`；service 文件不包含 token；代理日志中未发现 token、环境变量或任意用户输入回显。
- [x] HA 中已加载三个固定脚本：`script.pc_open_vscode`、`script.pc_open_bilibili`、`script.pc_open_douyin`，均只发出 `pc_ui_action` event，不执行宿主机 shell。

## 代理行为验证

- [x] `sensor.pc_telemetry_updated` 连续采样时间戳为 `10:54:07`、`10:54:12`、`10:54:17` UTC，间隔 5 秒；`binary_sensor.pc_agent_online=on`。
- [x] HA 实体读取到 CPU、CPU 温度、RAM、GPU 名称、GPU 利用率、GPU 温度、VRAM 等有效值，并带 `captured_at`、`source=minitv_pc_agent`、`availability` 和单位/总量属性。
- [x] 手动触发三个固定脚本各一次，HA `sensor.pc_ui_action_status` 分别返回 `launched`，并匹配 request ID：`open_vscode`=`ea082de1-d23c-4ef0-b7ad-223eb898bc10`、`open_bilibili`=`7759798f-0a68-4d21-b399-84d1764d3115`、`open_douyin`=`64f8408b-f4e6-4e53-91e6-c2cc221bb1b4`。
- [x] 代理 allowlist 拒绝非法 `run_shell`，回执为 `rejected / Action is not allowlisted`。
- [x] 无 `DISPLAY`、`XAUTHORITY` 或 D-Bus 会话时，固定 Launch Action 回执为 `failed / Graphical session unavailable`。

## ESP32-C3 固件验证

- [x] `idf.py build` 成功；`Firmware.bin` 大小为 `0x15a370`，小于 factory 分区 `0x3f0000`。
- [x] 最新镜像已通过 `/dev/ttyACM0` 烧录并校验（`Hash of data verified`）。
- [x] 烧录后真机启动日志确认 ESP-IDF v6.0.2、ESP32-C3、ST7789 `320x240`、LVGL 8.3.11、DMA 双缓冲 40 行 / 51200 bytes、Wi-Fi 获取 `192.169.0.96`、HA probe `ESP_OK http_status=200`、SNTP synchronized。
- [x] 真机连续运行约 4 分钟，FPS=50；最低 heap=60052、最大连续块=61440；未出现 WDT、abort、assertion 或 flush 错误。
- [x] 固件实现 PC Monitor Card 可见时 10 秒刷新、不可见时 60 秒刷新；双 gauge、GPU 名称/温度/VRAM、按钮 pending/launched/failed 状态和 agent/telemetry 可用性路径均已接入。

## Offline / failure 验证

- [x] 停止 `minitv-pc-agent.service` 后 HA `binary_sensor.pc_agent_online=off` 且 `availability=unavailable`；重启服务后恢复 `on` / `available`。
- [x] 停止并恢复 Home Assistant Container：停止期间 `127.0.0.1:8123` 不可达，恢复后容器运行且 agent/HA 状态恢复。
- [x] 固件对后端不可用、agent offline、telemetry stale、action failed 使用明确状态，不将缺失值伪装成 0 或成功。

## 现场补充证据

- [x] 用户于 2026-09-06 提供 ESP32 实物照片，照片显示 PC Monitor Card 已在 240x320 屏幕上运行：顶部 `PC agent online`，CPU/GPU 双仪表，底部 `VS Code`、`Bilibili`、`Douyin` 三个快捷按钮和卡片指示点均可见；照片中未见文字、仪表或快捷区相互重叠。
- [x] 用户确认 ESP32 的三个快捷按钮均可触发。该确认补齐了三类 ESP32 触摸 Launch Action 的现场可用性验收项。
- [x] 三个快捷按钮触摸后的实际效果与既有 HA/agent 手动回执一致：动作限定为 `open_vscode`、`open_bilibili`、`open_douyin`，成功状态以匹配 request ID 的 `launched` 为准。

## 结论

工单 11 的实现与验收项已完成：电脑侧部署、HA 桥接、Metrics/Command Agent、安全边界、固定动作手动与 ESP32 触摸验证、PC Monitor Card 240x320 布局以及固件构建/烧录/运行烟测均有证据支持。可关闭工单 11。
