# 工单 10 配网重提后的串口监视记录（未闭环）

日期：2026-08-25

## 执行范围

用户已通过 Provisioning Portal 重新保存 2.4 GHz Wi-Fi、HA Host `http://192.168.10.55:8123`、`weather.forecast_home` 与 LLAT。本次只执行无凭据串口监视和连通性检查；不读取、不记录或修改 NVS 中的 Wi-Fi 密码和 LLAT。

## 串口结果

串口设备：`/dev/ttyACM0`，115200 baud。连续两段监视（95 秒与 100 秒）均没有 `WIFI_EVENT_STA_DISCONNECTED`、`WIFI_REASON_NO_AP_FOUND` 或 Provisioning AP 回退日志。

在第二段 100 秒窗口中，天气 HTTP 请求反复失败：

```text
E (...) esp-tls: [sock=58] select() timeout
E (...) transport_base: Failed to open a new connection: 32774
E (...) HTTP_CLIENT: Connection failed, sock < 0
I (...) baseline: [periodic] free_heap=102612 minimum_free_heap=87832 largest_free_block=90112 fps=26 buffer_lines=40 buffer_bytes=38400
```

这些天气请求约每 35 秒出现一次。按现有调用链，`weather_task` 仅能由 `card1_start()` 创建，而 `card1_start()` 只在 `IP_EVENT_STA_GOT_IP` 的处理函数中调用。因此这是 STA 已获址并进入天气刷新流程的间接运行时证据；但监视开始时设备已在运行，未捕获到启动阶段的 **字面** `IP_EVENT_STA_GOT_IP` 和 `SNTP synchronized` 日志。

## 对照检查

宿主机在 `192.168.10.55/24` 上可直连 HA：

```text
PING 192.168.10.55: 2 received, 0% packet loss
GET http://192.168.10.55:8123/api/: HTTP 401 Unauthorized
```

HTTP 401 说明 HA Host/端口存在，未携带 LLAT 时拒绝请求符合预期；本记录不使用或暴露 LLAT。ESP32 端的 TCP `select()` timeout 说明 ESP32 所在 WLAN 到 HA Host 的 TCP 路径未通（例如 WLAN 客户端隔离、VLAN/子网 ACL 或防火墙规则），不能归因为 SSID 或密码错误。

## 重启捕获限制

已尝试通过 ESP32-C3 内建 USB-JTAG 执行无擦除复位，以捕获完整启动序列；宿主机拒绝该接口的 libusb 访问：

```text
Error: libusb_open() failed with LIBUSB_ERROR_ACCESS
Error: esp_usb_jtag: could not find or open device!
```

未执行 Flash、NVS 擦除或配置改写。需要物理按下板载复位键，或授予当前宿主机 USB-JTAG 访问权限，才能再次捕获启动阶段的精确 IP/SNTP 日志。

## 验收结论

| 项目 | 结果 |
| --- | --- |
| STA 获址/天气任务启动 | 间接通过；需重启日志取得 `IP_EVENT_STA_GOT_IP` 的直接证据 |
| `SNTP synchronized` | 未捕获，未通过 |
| HA 24 点流式提取 | 未通过：ESP32 到 HA TCP 超时 |
| 平滑折线、极值标注、NVS 快照写入 | 未通过：无成功 forecast 响应 |
| 断网重启后的 `Stale Data` | 未通过：尚无可验证的 NVS 天气快照 |

工单 10 保持 **open**，不可标记为 `resolved`。下一步不是重新提供 Wi-Fi 凭据：先使 ESP32 所在 WLAN 可访问 `192.168.10.55:8123`，然后物理重启设备并重新监视启动日志。
