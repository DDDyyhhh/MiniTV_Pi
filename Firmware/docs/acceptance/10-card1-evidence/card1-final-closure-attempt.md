# 工单 10 最终联调尝试记录（未闭环）

日期：2026-08-25

## 已确认的 HA 端契约

在宿主机上对 Home Assistant 做了不记录令牌的 REST 验证：

- `GET /api/states/weather.forecast_home`：HTTP 200；实体存在且状态可读。
- `POST /api/services/weather/get_forecasts?return_response`：当前 HA 版本仅接受顶层 `entity_id`；工单早期记录的 `target` body 返回 HTTP 400。
- 有效请求体：`{"type":"hourly","entity_id":"weather.forecast_home"}`。
- 有效 hourly forecast 响应约 9.2 KiB，包含超过 24 个 forecast 点，并含 `temperature` / `precipitation` 字段。

固件已据此更新为顶层 `entity_id` 请求，并使用有界流式解析：只保留前 24 个合法预报对象，不在 ESP32-C3 内存中保存完整 9.2 KiB 响应。

## 发现并修复的启动回归

首次联调固件让天气任务在网络栈初始化前启动，导致：

```text
assert failed: tcpip_send_msg_wait_sem ... (Invalid mbox)
```

根因：Card 1 创建阶段先于 `network_manager_init()` 调用 HTTP 客户端。

修复：天气任务仅在 `IP_EVENT_STA_GOT_IP` 后由网络管理器启动；网络未就绪时不发 HTTP 请求。

## STA / SNTP / Weather 联调结果

当前设备未能完成 STA 认证，实际串口行为：

```text
wifi:state: init -> auth
network: STA disconnected: reason=201 retry=0
network: STA disconnected: reason=201 retry=5
network: Provisioning AP MiniTV-C5D5 started at 192.168.4.1
```

`reason=201` 是 ESP-IDF 的 `WIFI_REASON_NO_AP_FOUND`：ESP32 扫描不到所配置的 2.4 GHz WLAN，而非 HA、LLAT 或天气实体认证失败。因此本次没有获得 `IP_EVENT_STA_GOT_IP`，也没有发生 SNTP 同步或 ESP32 到 HA 的 forecast 请求。不能验证真实温度/降水曲线、极值文案、NVS 天气快照写入或 `Stale Data` 重启恢复。

## 显示稳定性回退

20 MHz 在 SoftAP 场景出现连续 `lvgl` Task WDT；依据既定回退规则，已恢复并锁定 10 MHz。

10 MHz 的 70 秒实测：

```text
ST7789V ready: ... SPI 10000000 Hz
baseline: [shell-ready] free_heap=117324 minimum_free_heap=117280 largest_free_block=106496
baseline: [periodic] free_heap=103588 minimum_free_heap=100428 largest_free_block=90112 fps=26
Task WDT=0; abort=0; lwIP assertion=0
```

## 结论

工单 10 保持 `open`。待 ESP32 成功通过 STA 认证后，重做以下真机验收：

1. `IP_EVENT_STA_GOT_IP` 和 `SNTP synchronized`；
2. `Weather Trend updated: 24 hourly points`；
3. 图表与极值的屏幕观察；
4. 断开 STA / 重启后的 NVS `Stale Data` 恢复。

## 2026-09-05 真机复验

- 主机网卡 `enp34s0` 已改为静态地址 `192.169.0.48/23`，网关 `192.169.0.1`；NetworkManager 配置保持自动连接。主机 MAC 为 `2C:F0:5D:08:0C:A6`，也可在路由器中为该 MAC 配置 DHCP 地址保留。
- Home Assistant 容器状态为 `running`，重启策略为 `unless-stopped`，宿主机 `8123` 端口正常监听。
- 固件已重新编译并烧录，应用大小 `0x157830`，小于 factory 分区 `0x3f0000`。
- 设备重启后读取到 NVS 天气缓存：`Weather cache loaded; waiting for SNTP age validation`。
- STA 获取地址 `192.169.0.57`，HA 探测 `ESP_OK http_status=200`，天气连续成功更新 `24 hourly points`，随后 `SNTP synchronized`。
- 运行约 60 秒的诊断：`free_heap=106996`、`minimum_free_heap=87552`、`largest_free_block=86016`、`fps=50`；未出现 WDT、abort 或 assertion。
- 完整串口证据：`card1-static-ip-cache-restart-20260905.log`。

本次已验证主机重启后不需要手动执行 `docker start`；只要 Docker 服务开机启动，Home Assistant 会按 `unless-stopped` 自动启动。工单 10 的天气数据链路和重启缓存恢复已通过真机验证。
