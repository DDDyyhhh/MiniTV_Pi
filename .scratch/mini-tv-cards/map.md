# Mini TV 三卡片固件 — 地图

```yaml
labels: [wayfinder:map]
effort: mini-tv-cards
```

## Destination

ESP32-C3 Mini TV 真机运行完整固件:左右滑动三张 Apple Fluent 卡片风页面(① 数字翻页时钟 + 未来 24 小时气温折线图 + 日历与节假日倒数;② Linux 电脑 CPU/GPU 监控仪表盘 + 一键打开抖音/VSCode/B站;③ 米家灯光拟态开关面板),下拉调出控制中心(背光 PWM 滑条 + Wi-Fi 设置/手机网页配网)。数据与控制统一经 Home Assistant(从零安装,含米家接入),每张卡片在真机上逐张验收。

## Notes

- **本努力携带实现**:目的地是真机全实现,故 09–12 为执行型 `task` 工单——这是对 wayfinder「只规划不实现」默认的显式覆盖。规划工单(01–08)仍按「决定而非交付」推进。
- **硬件权威事实**(见 `Firmware/docs/hardware/HADRWARE.md`,规划与实现工单一律以它为准,不靠猜测):
  - MCU:ESP32-C3-12F,4MB flash(⚠️ sdkconfig 现误设 `CONFIG_ESPTOOLPY_FLASHSIZE_2MB`,需改为 4MB);无 PSRAM,LVGL 用局部缓冲,内存预算 ~300KB。
  - 屏:ST7789 240x320 SPI,SCLK=GPIO4 / MOSI=GPIO6 / DC=GPIO7 / RST=GPIO8 / CS=GPIO10 / BLK=GPIO5(PWM 可调背光);须用 `esp_lcd` + DMA。
  - 触摸:CST816D 电容触摸,I2C SDA=GPIO2 / SCL=GPIO3(板载 4.7K 上拉),INT=GPIO0 / RST=GPIO1;须用新版 `driver/i2c_master.h` API。
  - 输入:BOOT 键 GPIO9(低有效)作为辅助输入。
  - 框架:ESP-IDF v6.0.2(riscv32-esp-elf 15.2.0),C + FreeRTOS;接线常量放 `board_pins.h`;USB-C 原生 CDC/JTAG 调试。
- **用户已定决策**(不必再问):
  - 后端统一 Home Assistant(尚未安装,从零开始);米家设备经 HA 接入。
  - 被监控电脑是 **Linux**(显卡厂商未知,NVIDIA/AMD/Intel 需在工单 07 分支处理)。
  - 配网 = 手机网页(SoftAP 门户),不做屏上软键盘。
  - 米家设备组合 = Wi-Fi 直连灯 + 蓝牙 Mesh 开关 + 小米中枢网关。
  - 节奏 = 先骨架后卡片,逐卡真机验收。
- 各 grilling / task 工单开会时调用 `/grilling` 与 `/domain-modeling`;research 工单由 `/research` 子代理解决。
- 术语:卡片(三张主页面)、控制中心(下拉面板)、拟态开关(skeuomorphic 开关)、一键快捷(卡片2 的三个按钮)。

## Decisions so far

- [米家接入 HA 路径调研](issues/03-mihome-path.md) — Yeelight LAN Control 是首个确定的本地链路;其余米家/Mesh 必须以具体型号、区域、固件和 HA 实体实机验证,ESP32 只对接 `light.*`/`switch.*`;HAOS 建议已由 02 按用户现有 Linux 主机约束覆盖。
- [ESP32↔HA 对接规范调研](issues/04-ha-api.md) — MVP 采用 REST-only 的白名单单实体读取与显式服务调用;专用 `ha_io_task`、HTTPS+LLAT、NVS encryption 和确认读取;WebSocket 仅在真机评估后按实体追加。
- [UI 技术栈与视觉系统定案](issues/01-ui-stack.md) — 采用 LVGL 8.3.11 + `esp_lcd` ST7789 局部 DMA 双缓冲 + `lv_tileview`;视觉为深色 Frosted Panel 风格(无实时 blur);中文 300-500 字子集以 C 数组静态编译,50 KiB 内为目标。
- [Home Assistant 安装主机方案](issues/02-ha-host.md) — 首版 HA 装在现有 Linux 主机上的 Home Assistant Container(host network),不是 24 小时常开中枢;电脑关机时 ESP32 显示 HA 离线,卡片2用同一 Linux 主机的本机 agent 做性能采集和快捷启动。
- [骨架里程碑验收标准](issues/05-skeleton-acceptance.md) — 工单 09 的完成定义锁定为真机点亮 ST7789、CST816D 触摸/手势、三卡片壳、首版 Control Center、SoftAP 配网、NTP、HA 离线态、4MB flash 修正和 heap/FPS/稳定性基线;控制中心首版不做二级页。
- [卡片1 数据源与呈现](issues/06-card1-data.md) — Time Card 采用本地 SNTP 时间 + HA `weather.get_forecasts` 24h 天气 + ESP32 内置 2026-2027 中国节假日表;天气缓存小于 24h 显示 Stale Data,节假日离线可用,折线首版自绘轻量实现。
- [卡片2 电脑性能副屏方案](issues/07-card2-pc.md) — Ubuntu GNOME/X11 + GTX 1660 SUPER 使用 `systemd --user` Python agent:5 秒采集 psutil/NVIDIA 指标到 HA,固定 allowlist action 经 HA event 打开 Chrome 的抖音/B站或空白 VSCode;ESP32 10 秒 REST 读取并按 request ID 确认启动结果。
- [卡片3 米家灯光拟态开关方案](issues/08-card3-lights.md) — Smart Home Card 首版是客厅两路+卧室两路的 2×2 Entity Tile,仅轻触明确开/关;HA 实体必须逐个验证并映射,服务后立即/1s/3s 回读确认,HA 离线时禁用全部控制。

## Not yet specified

- OTA 是否纳入本努力(建议默认不纳入,除非工单 01/05 提出强理由)。

## Out of scope

- git 仓库初始化 / CI / 代码托管(仓库当前非 git,本努力不改)。
- 屏上软键盘配网(已选手机网页配网)。
- ESP32 直接充当 BLE 网关直连米家设备(米家统一经 HA)。
- Arduino/PlatformIO 迁移(固件锁定 ESP-IDF)。
