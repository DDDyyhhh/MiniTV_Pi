# 09 骨架实现与真机验收

Type: task
Status: open
Blocked by: 05

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

(待解决后填写)
