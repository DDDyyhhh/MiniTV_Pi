# 09 骨架实现与真机验收

Type: task
Status: resolved

## Question

按工单 05 的验收清单实现固件骨架并真机验收:

- 修正 sdkconfig flash 为 4MB + 分区表;
- `board_pins.h` 接线常量;esp_lcd + ST7789 + DMA 点亮,背光 PWM;
- CST816D 触摸驱动(INT 中断 + I2C 读取)+ LVGL 输入适配(左右滑动/下拉/点击手势);
- LVGL 按工单 01 的版本/字体/视觉规范接入;
- 三卡片滑动壳 + 指示点;下拉控制中心(背光滑条 + Wi-Fi 设置 + 网络状态);
- SoftAP 手机网页配网 + NVS 持久化 + 重连;NTP 对时;
- 记录启动后剩余 heap 基线。

验收:逐条勾完 05 清单并记录真机结果(含照片/现象)。

## Answer

工单 09 已完成并闭环。用户确认以下 Skeleton Milestone 验收项全部达成：

- 三张 Card 使用 `lv_tileview` 横向滑动，页面指示点正常；
- 顶部 45 px 热区可下拉打开 Control Center，Control Center 展开时背景 Card 不接收触摸；
- ST7789 240×320、RGB565、`esp_lcd` DMA 局部双缓冲正常，缓冲为 240×40 行、38,400 B；
- CST816D 点击与左右/顶部手势正常；
- GPIO5 背光 PWM、BOOT 短按/长按行为正常；
- 首次启动 SoftAP + Provisioning Portal 配网、NVS 保存、重启 STA 重连流程正常；
- SNTP 时间状态、SSID/IP/RSSI、HA 未配置/在线/认证失败/Offline Backend State 可显示；
- 已修复 Wi-Fi 启动竞态，避免 STA 配置完成前由 `WIFI_EVENT_STA_START` 提前连接造成 abort/reboot loop；
- 已完成 10 MHz 稳定基线 70 秒横滑压力测试，无 Task WDT 或 abort，周期日志可见 `fps=25`；
- 曾完成 20 MHz 快速启动与简单 Card 横滑校验；但后续 SoftAP 场景复现 `lvgl` Task WDT，故按回退规则弃用 20 MHz；
- 稳定发布 SPI 基线锁定为 10 MHz：已通过 70 秒横滑与 SoftAP 场景稳定性验证；
- 当前 PCB 未连接 ST7789 TE 信号，TE 同步不属于本工单范围。

验收记录已更新至 `Firmware/docs/acceptance/09-skeleton-acceptance.md`。工单 09 状态正式标记为 **resolved**。
GitHub Issue：[#7](https://github.com/DDDyyhhh/MiniTV_Pi/issues/7)，状态：closed。
