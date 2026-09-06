# ESP32-C3 桌面小电视 (Mini TV) 硬件上下文规格书

## 1. 系统核心配置
- **主控芯片**: ESP32-C3-12F / ESP32-C3-12E (RISC-V 架构, 4MB Flash)
- **开发框架**: Espressif ESP-IDF v6.0.2 (使用 C 语言与 FreeRTOS)
- **调试方式**: 原生 USB CDC / JTAG (无需外部串口芯片)

## 2. PCB GPIO 引脚映射表 (Pinout Table)

- **显示逻辑方向**: 横屏 `320×240`；ST7789 原生矩阵为 `240×320`，固件启用轴交换与 X 轴镜像；CST816D 坐标同步转换。
- `LCD_SCLK` (SPI 时钟): GPIO 4
- `LCD_MOSI` (SPI 数据): GPIO 6
- `LCD_DC`   (数据/命令): GPIO 7
- `LCD_RST`  (屏幕复位): GPIO 8
- `LCD_CS`   (芯片片选): GPIO 10
- `LCD_BLK`  (背光控制): GPIO 5 (PWM / 高电平点亮)

### 电容触摸接口 (CST816D I2C 触摸屏)
- `TP_SDA`   (I2C 数据): GPIO 2 (PCB 板载 4.7K 上拉)
- `TP_SCL`   (I2C 时钟): GPIO 3 (PCB 板载 4.7K 上拉)
- `TP_INT`   (触摸中断): GPIO 0
- `TP_RST`   (触摸复位): GPIO 1

### 系统交互与按键
- `SW_RST`   (复位按键): CHIP_EN (物理复位)
- `SW_BOOT`  (引导/功能键): GPIO 9 (接 10K 上拉，按下接地)
- `USB_D-`   (原生 USB 数据-): GPIO 18
- `USB_D+`   (原生 USB 数据+): GPIO 19

## 3. 软件编码规范要求 (给 OpenCode 的约束)
1. 代码必须基于 **ESP-IDF v6.0.2** 的最新规范编写（禁止使用旧版 `driver/i2c.h`，统一使用 `driver/i2c_master.h`）。
2. 屏幕驱动优先使用 ESP-IDF 的 **`esp_lcd`** 框架与 DMA 加速。
3. 所有的 GPIO、I2C、SPI 初始化建议封装在独立的头文件 `board_pins.h` 中。
4. 多任务必须基于 **FreeRTOS** (`vTaskDelay` / `xTaskCreate`) 编写。