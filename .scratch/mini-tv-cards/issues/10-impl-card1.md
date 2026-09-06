# 10 卡片1 实现

Type: task
Status: resolved
Blocked by: 06, 09

## Question

按工单 06 的数据流与呈现规格,在 09 的骨架上实现卡片1:

- 数字翻页时钟(翻转动效、NTP 对时、夜间调暗若有);
- 未来 24 小时气温折线图(LVGL chart 或自绘,按 06 定的数据源与刷新周期);
- 日历 + 节假日倒数(按 06 定的数据源与更新机制);
- 断网降级(最后缓存)与刷新日志。

验收:真机逐条过 06 的呈现/数据规格,记录结果。

## Answer

工单 10 已完成实现与真机验收，状态为 `resolved`。

已实现:

- Time Card 内增加三个纵向子页面：
  1. Flip Clock：24 小时制 `HH:MM`、本地时区（CST-8）、NTP 校时安全态、星期/日期排版、微光拟态时钟底座、分钟变化时的轻量淡入/上移翻页过渡；
  2. Weather Trend：LVGL `lv_chart`，温度与降水双曲线，24 小时点位，温度范围自动缩放，最高/最低标注，圆角线条与中间点插值；
  3. Calendar & Holiday：当前月份小月历、当天标记、内置 2026–2027 中国重要节假日表和下一个节假日倒数。
- 天气数据链路按工单 06 实现：HA `POST /api/services/weather/get_forecasts?return_response`，请求 `type=hourly` 和当前 HA 版本要求的顶层 `entity_id`。响应使用有界流式解析，只保留前 24 个合法 temperature/precipitation 点；已验证 HA 响应约 9.2 KiB，因此不将完整响应保存在 ESP32-C3 内存中。
- 增加天气实体配置字段 `weather_entity`，Provisioning Portal 支持填写 `weather.*` 实体。
- 增加天气缓存：NVS 保存最近有效 24 小时快照；请求失败且缓存未超过 24 小时显示 Stale Data，过期或没有缓存显示天气离线，不生成伪造曲线。
- 配置存储版本从 1 升至 2，并保留旧配置迁移读取能力。
- 保持 10 MHz SPI 稳定发布基线、240×40 DMA 双缓冲和既有三 Card/Control Center 结构。

真机验证:

- 已构建、烧录并通过校验。
- 10 MHz SoftAP 场景完成 70 秒验证：`fps=26`，无 Task WDT、abort 或 lwIP assertion；发布基线锁定为 10 MHz。
- HA `weather.forecast_home` 状态读取为 HTTP 200；`weather.get_forecasts` 使用顶层 `entity_id` 请求返回 HTTP 200，旧 `target` 请求返回 HTTP 400，固件已完成兼容修复。
- 早期 `STA disconnected: reason=201` 已通过后续网络配置和真机复验排除；最新复验获得 IP `192.169.0.57`，HA 探测为 `ESP_OK http_status=200`，天气连续更新 `24 hourly points`，随后 `SNTP synchronized`。
- 重启后成功加载 NVS 天气缓存并完成年龄校验；运行约 60 秒时 `free_heap=106996`、`minimum_free_heap=87552`、`largest_free_block=86016`、`fps=50`，无 WDT、abort 或 assertion。
- 完整证据见 `Firmware/docs/acceptance/10-card1-evidence/card1-final-closure-attempt.md`；天气数据链路和重启缓存恢复已通过真机验证。

GitHub Issue：[#5](https://github.com/DDDyyhhh/MiniTV_Pi/issues/5)，状态：closed。
