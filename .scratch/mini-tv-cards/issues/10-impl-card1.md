# 10 卡片1 实现

Type: task
Status: open
Blocked by: 06, 09

## Question

按工单 06 的数据流与呈现规格,在 09 的骨架上实现卡片1:

- 数字翻页时钟(翻转动效、NTP 对时、夜间调暗若有);
- 未来 24 小时气温折线图(LVGL chart 或自绘,按 06 定的数据源与刷新周期);
- 日历 + 节假日倒数(按 06 定的数据源与更新机制);
- 断网降级(最后缓存)与刷新日志。

验收:真机逐条过 06 的呈现/数据规格,记录结果。

## Answer

工单 10 已启动并完成第一版完整实现，当前状态保持 `open`，等待 ESP32 成功加入 2.4 GHz STA 后完成 HA weather entity 的真机数据链路验收。

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
- Card 1 在 20 MHz 的短期/横滑验证曾通过，但 SoftAP 场景后续复现 `lvgl` Task WDT；已按回退规则锁定 10 MHz。
- 10 MHz SoftAP 场景的 70 秒验证：`fps=26`，无 Task WDT、abort 或 lwIP assertion。
- HA 端验证：`weather.forecast_home` 当前状态读取为 HTTP 200；服务 metadata 表明 `weather.get_forecasts` 仅声明 `type` 字段，顶层 `entity_id` 请求返回 HTTP 200，旧 `target` 请求返回 HTTP 400。固件已完成兼容修复。
- 早期 STA 联调曾记录 `STA disconnected: reason=201`（`WIFI_REASON_NO_AP_FOUND`）并回退 Provisioning Portal；该历史记录保留在 `Firmware/docs/acceptance/10-card1-evidence/sta-no-ap-found-10mhz.log`。
- 用户重新提交配置后的最新监视未出现 STA 断开或 Provisioning Portal 回退，且天气任务反复发起连接（该任务仅会在 `IP_EVENT_STA_GOT_IP` 后启动）；不过 ESP32 到 `192.168.10.55:8123` 的 TCP 连接持续超时。尚未捕获启动阶段的精确 IP/SNTP 日志，也未取得 forecast 响应，故 24 点曲线、极值、NVS 快照与 `Stale Data` 重启恢复仍未完成现场验收；详情见 `Firmware/docs/acceptance/10-card1-evidence/2026-08-25-post-provisioning-monitor.md`。
