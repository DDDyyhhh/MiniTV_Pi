# 工单 #22：Open-Meteo 目标设备实机取证

状态：**取证完成**（2026-09-27）。临时探针未接入生产固件。

## 设备与可追溯性

- 目标设备是 ESP32-C3 小电视、4 MiB Flash、ST7789 320×240 横屏，配置资产见[硬件规格](../hardware/HADRWARE.md)。固件为 ESP-IDF v6.0.2、10 MHz SPI、40 行 DMA 缓冲。设备标识、Wi-Fi 凭据和原 Flash 均不入库。
- 使用日常 Wi-Fi；启动日志出现 `SNTP synchronized`，探针开始及第二次请求前均报告 Wi-Fi 已连接、时间已同步。HTTPS 使用 `esp_http_client` 和 ESP-IDF 完整 x509 CA bundle，未关闭证书校验。
- 请求遵循 [#20 的固定契约](https://github.com/DDDyyhhh/MiniTV_Pi/issues/20)：`api.open-meteo.com/v1/forecast`、样本坐标 `39.9042,116.4074`、仅 `current=temperature_2m`、摄氏度、Unix 时间、`GMT+8`、`forecast_days=1`。北京坐标只用于网络取证，不代表用户位置。
- 公共服务临时分支 `codex/issue-22-open-meteo-probe`，提交 `8669a26fcb0f1a3fe50dab96986021769d5dcb33`；故障临时分支 `codex/issue-22-open-meteo-fault-probe`，提交 `8b0ad78dbc5d4ff747b607bb1358beee5ce7076b`。二者基于当前固件源码的隔离快照编译。
- [公共服务探针](../../tools/open-meteo-probe/probe.c)镜像 SHA-256 `6ba78fd0ab893b89cb9a89aeb5c5da02c6e3b6130567b6640ebd885fd5164bc1`、大小 `0x16f890`；[故障变体补丁](../../tools/open-meteo-probe/fault.patch)生成的镜像 SHA-256 `8a66500e4c97600a8489e2a955fc7e13a03b3b3129071664b813583188a62fab`、大小 `0x16f780`。两次构建无警告，镜像均小于 `0x3f0000` app 分区。

## 公共服务实测

| 场景 | 设备结果 | 延迟 | 响应与 UI |
| --- | --- | ---: | --- |
| 第一次 current-only GET | `ESP_OK`、HTTP 200，HTTPS 及证书校验成功 | 3069 ms | 329 字节，chunked + identity，无溢出；FPS 51→51 |
| 错误纬度 | `ESP_OK`、HTTP 400 | 3015 ms | 98 字节，identity；FPS 51→50 |
| `.invalid` 域名 | `ESP_ERR_HTTP_CONNECT`，未建立连接 | 88 ms | 无响应体；FPS 50→50 |
| 声明接受 deflate | `ESP_OK`、HTTP 200 | 2831 ms | 225 压缩字节，chunked + deflate、2 次 data event；FPS 50→50 |
| 1800 秒等待后的第二次 current-only GET | `ESP_OK`、HTTP 200，HTTPS 及证书校验成功 | 3375 ms | 328 字节，chunked + identity，无溢出；FPS 50→51 |

两次成功请求的主机接收时间相隔 **1809 秒**，符合真实 30 分钟刷新节奏。对捕获的完整 identity JSON 做主机端严格校验：两次均含 `utc_offset_seconds=28800`、`current_units.time=unixtime`、`current_units.temperature_2m=°C`、正整数 `current.time`、`current.interval=900` 和有限温度（24.4°C、23.7°C）；时间戳均在采集窗口。响应均小于 1 KiB。压缩响应只记录头和字节数，未当作 JSON 解析；生产端应请求 identity，并拒绝意外压缩或显式解压。

探针 HTTPS 请求在独立任务中约耗 3 秒。第一次请求调用前/后的空闲堆为 70,036/41,248 字节，系统累计最低空闲堆由 54,732 降至 20,120 字节；第二次为 69,364/41,056 字节，累计最低 20,108 字节。30 条每分钟周期日志的空闲堆为 65,360–69,568 字节，中位数 69,452；FPS 为 50–51，中位数 50。采集期未见 task watchdog、崩溃或棕断。累计最低堆是全系统启动以来的高水位，并非天气任务的精确增量；FPS 计数不能代替人工触摸延迟测试。

## 受控失败路径实测

短时局域网服务提供故障响应，避免对公共服务制造 429/5xx。故障探针仍使用 8 秒请求超时、1 KiB 缓冲和证书包校验。七项均由目标 ESP32 发起：

| 场景 | 设备结果 | 实现判断 |
| --- | --- | --- |
| HTTP 429 / 503 | 分别收到状态 429 / 503，响应小于 1 KiB | HTTP 传输成功仍须按状态拒绝 |
| 服务器无响应 | `ESP_ERR_HTTP_EAGAIN`，8035 ms | 8 秒超时生效 |
| 响应中途截断 | `ESP_ERR_HTTP_INCOMPLETE_DATA`，声明 200 字节，实收 11 字节 | 不得确认不完整 JSON |
| 1400 字节响应 | 检出溢出，缓冲只保留 888 字节；有效结果为 `ESP_ERR_INVALID_SIZE` | ESP-IDF v6 忽略 `HTTP_EVENT_ON_DATA` 的失败返回值，调用者必须显式检查溢出标志 |
| HTTP 200 但 JSON 无效 | 完整收到 11 字节，主机 JSON 校验拒绝 | 生产端需做语法和字段校验 |
| 自签证书 HTTPS | `ESP_ERR_HTTP_CONNECT`，未建立经验证连接 | CA bundle 未绕过 |

这些是局域网模拟，不代表 Open-Meteo 的实际限流策略、长期可用率或公网故障发生率。

## 实现决策与边界

当前网络、设备和证书配置下，Open-Meteo current-only 请求可按 30 分钟节奏取得有界温度响应。后续生产模块必须验证 HTTP 状态、响应完整性、编码、1 KiB 上限、JSON 字段及时间戳；不能把 `esp_http_client_perform()==ESP_OK` 单独视为有效温度。

失败时不得覆盖 Confirmed 温度；成功时间在 3 小时内时显示弱化的 Stale Data，超过 3 小时隐藏温度；场景、时间和日期不受影响。本探针未实现或验收该 UI 策略，也未测触摸延迟。一次 30 分钟间隔的连续样本不能证明长期 SLA。

脱敏证据：[公共服务日志](artifacts/open-meteo-20260927.log)、[公共服务摘要](artifacts/open-meteo-20260927.json)、[故障日志](artifacts/open-meteo-fault-20260927.log)、[故障摘要](artifacts/open-meteo-fault-20260927.json)。原始串口日志 SHA-256 分别为 `73fb4f9a299bf829120fa9d8581f784842a53254ce4a848f205e6549ad425197`、`f0fdff71aca6b3f5470880c70d444903e443be8bc804c21d8abc7a4a794ae262`。两次原 Flash 恢复后均完成 4 MiB 全片读回逐字节比对，备份 SHA-256 分别为 `c45024ac3a47a7de47a614394ba40e1dcf0b9697e3647a6d9ec4453d9d32996b`、`82060907baa5f7cba1c76c0fb66a4584eba987af8ab6275c9cdf2e8605986c47`；最后一次重启确认运行原固件且 SNTP 再次同步。原始串口和 Flash 备份只在私有会话留到恢复核验完成。
