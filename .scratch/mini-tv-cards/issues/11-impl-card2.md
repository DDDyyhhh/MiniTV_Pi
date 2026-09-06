# 11 卡片2 实现

Type: task
Status: resolved
Blocked by: 07, 09

## Question

按工单 07 的方案,在 09 骨架上实现卡片2,并在被监控的 Linux 电脑上部署采集侧:

- CPU 仪表盘 + 显卡监控(按 07 定的指标与 LVGL 形态,含显卡厂商分支);
- 一键打开抖音/VSCode/B站(按 07 定的链路,含电脑侧部署与 HA 配置);
- 点击反馈与误触防护。

验收:真机逐条过 07 的规格;电脑侧采集部署完成;快捷按钮在真机上实测可用。

## Answer

工单 11 已完成电脑侧部署、HA 桥接、PC Monitor Card 固件实现，以及可执行的构建、烧录和运行烟测。详细证据见 `Firmware/docs/acceptance/11-card2-evidence.md`。

已完成：

- Linux Target PC 上的 Metrics Agent / Command Agent 以 `systemd --user` 服务运行，5 秒发布 CPU/RAM/NVIDIA GPU Confirmed Telemetry；LLAT 私有配置权限为 0600，服务不包含 token。
- HA 三个固定脚本通过 `pc_ui_action` event 桥接 `open_vscode`、`open_bilibili`、`open_douyin`；Command Agent 只接受 allowlist action ID、校验 UUID 并去重，手动三动作均收到匹配 request ID 的 `launched` 回执。
- ESP32-C3 PC Monitor Card 已接入双 gauge、可用性/陈旧数据状态、固定 Launch Actions pending/launched/failed 反馈；`idf.py build`、最新镜像烧录校验和真机联网运行均通过。

当前工单已完成并可关闭：用户补充确认 ESP32 三个快捷按钮均可触发，并提供 240x320 实物照片；验收证据已同步至 `Firmware/docs/acceptance/11-card2-evidence.md`。
GitHub Issue：[#1](https://github.com/DDDyyhhh/MiniTV_Pi/issues/1)，已同步验收评论并关闭。
