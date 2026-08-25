# 09 Skeleton Milestone — 真机验收记录

> 工单 09 已按用户真机验收结果闭环。业务 Card 仍保持占位/空数据安全态；本工单只验收 Skeleton Milestone 的硬件、UI 壳、触摸、Control Center、配网、时间和后端状态基础。

## 构建基线

- [x] ESP-IDF v6.0.2；target `esp32c3`；flash `4MB`。
- [x] LVGL 固定为 8.3.11，局部 RGB565 DMA 双缓冲为 240×40 行（38,400 B）。
- [x] 构建日志记录应用镜像：`Firmware.bin` 为 `0x14e5b0` bytes；最小应用分区剩余 `0x2a1a50` bytes（67%）。
- [x] SPI 稳定发布基线锁定为 **10 MHz**。20 MHz 快速启动/简单横滑虽曾通过，但在 SoftAP 场景复现 `lvgl` Task WDT，已按回退规则弃用。10 MHz 已通过 70 秒横滑与 SoftAP 场景稳定性验证。
- [x] 当前 PCB 未提供 ST7789 TE 信号；不纳入本工单实现。

## 真机检查清单

- [x] ST7789 RGB 显示、方向、顶部热区正确。
- [x] GPIO5 PWM 背光可调。
- [x] CST816D 四角/中心点击和左右滑动正确；连续往返无卡死。
- [x] 顶部下拉打开 Control Center；打开时背景 Card 不接收触摸。
- [x] 三张 Card 与页面指示点显示正确。
- [x] 首次无配置时 SoftAP 和 Provisioning Portal 可用，保存后重启自动 STA 连接。
- [x] STA 状态显示 SSID、IP、RSSI；失败可重新进入配网。
- [x] SNTP 成功后时间状态变为已校时；失败时保持未校时。
- [x] HA 未配置、在线、认证失败、Offline Backend State 显示清晰。
- [x] BOOT 短按切换 Control Center；长按 3 秒进入 Provisioning Portal。
- [x] 空闲和横滑压力期间无 watchdog/重启；10 MHz 基线已完成 70 秒压力验证。

## 关键串口证据

10 MHz 稳定基线启动：

```text
ST7789V ready: 240x320, BGR RGB565, SPI 10000000 Hz, black clear complete
LVGL 8.3.11 registered: DMA double buffer 40 lines, 38400 bytes
baseline: [shell-ready] ... buffer_lines=40 buffer_bytes=38400
baseline: [periodic] ... fps=26 buffer_lines=40 buffer_bytes=38400
Task WDT=0, abort=0, lwIP assertion=0
```

20 MHz 仅通过过快速校验；在 SoftAP 场景复现 `lvgl` Task WDT，故不作为发布配置。

## 结论

工单 09 的 Skeleton Milestone 已完成并闭环：三张 `lv_tileview` Card、45 px 顶部 Control Center 手势、240×40 行 DMA 双缓冲、Wi-Fi/SoftAP Provisioning Portal、SNTP 和后端状态基础均达到验收标准。工单状态：**resolved**。
