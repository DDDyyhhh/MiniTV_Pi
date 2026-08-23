# 09 Skeleton Milestone — 真机验收记录

> 构建可在主机验证；下面项目必须在目标 ESP32-C3 真机逐项记录后才可勾选。

## 构建基线

- [x] ESP-IDF v6.0.2；target `esp32c3`；flash `4MB`。
- [x] LVGL 固定为 8.3.11，局部 RGB565 DMA 双缓冲为 240×20 行（19,200 B）。
- [ ] 记录 `idf.py size` 的实际 flash 占用。

## 真机检查清单

- [ ] ST7789 RGB 测试块、方向、顶部热区均正确。
- [ ] GPIO5 PWM 从 10% 到 100% 平滑变化。
- [ ] CST816D 四角/中心点击和左右滑动正确；连续 20 次往返无卡死。
- [ ] 顶部下拉打开 Control Center；打开时背景 Card 不接收触摸。
- [ ] 三张 Card 与页面指示点显示正确。
- [ ] 首次无配置时 SoftAP 和 Provisioning Portal 可用，保存后重启自动 STA 连接。
- [ ] STA 状态显示 SSID、IP、RSSI；失败可重进配网。
- [ ] SNTP 成功后时间状态变为已校时；失败时保持未校时。
- [ ] HA 未配置、在线、认证失败、Offline Backend State 显示清晰。
- [ ] BOOT 短按切换 Control Center；长按 3 秒进入 Provisioning Portal。
- [ ] 空闲 10 分钟、静置 30 分钟无 watchdog/重启。

## 必须采集的数据

将完整串口日志和照片/视频置于 `docs/acceptance/09-evidence/`，记录下列标签：

- [ ] `shell-ready`
- [ ] `wifi-connected`
- [ ] `control-center-10s`
- [ ] `swipe-60s`
- [ ] `ha-offline-5m`
- [ ] `idle-30m`
