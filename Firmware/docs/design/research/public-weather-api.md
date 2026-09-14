# 公共温度数据源选择

- Research ticket: [#20](https://github.com/DDDyyhhh/MiniTV_Pi/issues/20)
- 调研日期：2026-09-14
- 范围：ESP32-C3 / ESP-IDF 6.0.2，仅按固定经纬度每 30 分钟获取当前室外温度；不设计或修改生产固件。

## 结论

**推荐：Open-Meteo Forecast API 的免费 public endpoint，但仅限当前项目保持非商业用途。** 它不需要账户或 API key，一次 current-only 请求的 JSON 很小，最适合 ESP32-C3。其代价是：值来自数值天气模型而非现场气象站观测；免费服务无可用性保证；数据为 CC BY 4.0，且 Open-Meteo 要求在展示数据的位置旁提供归属链接；免费 endpoint 只允许非商业用途。[Open-Meteo API 文档](https://open-meteo.com/en/docs) [条款](https://open-meteo.com/en/terms) [许可](https://open-meteo.com/en/licence)

**备用：MET Norway Locationforecast 2.0 `compact`。** 它同样不需要账户或 key，许可与服务条款没有 Open-Meteo 免费 endpoint 的“仅非商业”限制；但每次请求必须携带可联系的唯一 `User-Agent`，必须遵守缓存、gzip 与重定向规则，而且即使 `compact` 也返回整段预报，解压 JSON 约 40 KiB，明显增加 ESP32-C3 的流式解析和内存风险。[Locationforecast 文档](https://api.met.no/weatherapi/locationforecast/2.0/documentation) [服务条款](https://docs.api.met.no/doc/TermsOfService) [许可](https://docs.api.met.no/doc/License)

两个候选的中国大陆可达性均为 **需真机网络验证**；本次桌面网络成功不能替代目标网络、目标设备与固件 CA 配置的验证。

## 决策表

| 候选 | 认证、账户与价格维护 | “当前温度”语义、时间与单位 | 频率、失败与服务保证 | 许可/归属 | ESP32-C3 结论 |
| --- | --- | --- | --- | --- | --- |
| **Open-Meteo Forecast API** | 免费 public endpoint 无账户、无 key；只允许非商业用途。上限 600 次/分、5,000 次/时、10,000 次/日、300,000 次/月；48 次/日远低于上限。商业用途必须切换付费 customer endpoint 和 key。[条款](https://open-meteo.com/en/terms) [价格](https://open-meteo.com/en/pricing) | `current.temperature_2m` 是距地 2 m、单位由 `current_units` 声明的瞬时值；API 聚合数值天气模型并选择适用模型，因此不是气象站实测。`current.time` 是值的有效时刻，`interval` 是计算间隔；Unix 时间总是 GMT+0，`utc_offset_seconds` 表示请求时区偏移。[文档：数据源/参数/返回对象](https://open-meteo.com/en/docs) | 参数错误为 HTTP 400 JSON error。免费 endpoint 无 uptime guarantee，且滥用可无预警封禁；官方材料没有承诺 429 body 或 `Retry-After` 契约，因此 429/超时/非 200 均只按刷新失败处理，不立即重试。[错误文档](https://open-meteo.com/en/docs#errors) [条款](https://open-meteo.com/en/terms) [价格 FAQ](https://open-meteo.com/en/pricing#faq) | API 数据为 CC BY 4.0；须给出适当署名、许可链接和修改说明，并要求在展示 Open-Meteo 数据的位置旁放置 Open-Meteo 链接。[许可](https://open-meteo.com/en/licence) | **推荐（有条件）**：请求/响应最小，维护最低；非商业与归属 UI 是发布门槛。 |
| **MET Norway Locationforecast 2.0** | 无 key/账户；每次请求必须有唯一、可联系的 `User-Agent`（可用应用名和仓库 URL），缺失或禁止 UA 会得 403。条款要求经纬度最多 4 位小数。[文档](https://api.met.no/weatherapi/locationforecast/2.0/documentation#AUTHENTICATION) [条款](https://docs.api.met.no/doc/TermsOfService#identification) | `properties.timeseries[*].data.instant.details.air_temperature` 是指定时刻距地 2 m 的摄氏温度预报；时间序列按 UTC 升序。全球区域来自约 9 km ECMWF 模型、每日更新 4 次；非观测值，建议传海拔以改善地形温度修正。[数据模型](https://docs.api.met.no/doc/locationforecast/datamodel) [JSON 格式](https://docs.api.met.no/doc/ForecastJSON) | 不得在 `Expires` 前重复请求，宜用 `If-Modified-Since`；应用总流量超过 20 次/秒需特别协议。违规会被 throttling 并返回 429，收到后必须立刻降流量。服务明确无交付保证、无 SLA；只支持 HTTPS，并要求客户端支持重定向及 gzip。[条款](https://docs.api.met.no/doc/TermsOfService) | 默认数据使用 NLOD 2.0 和 CC BY 4.0；应署名 “Data from MET Norway” 或等价文字，并建议链接数据源。[许可](https://docs.api.met.no/doc/License) | **备用**：凭据维护低，但响应大、协议义务更多；必须做 gzip/流式解析并缓存。 |
| **OpenWeather Current Weather API** | 需要注册账户、验证邮箱，并在每个请求携带个人 API key；免费计划 60 次/分、1,000,000 次/月，当前天气数据每 2 小时更新。[FAQ](https://openweathermap.org/faq) [价格/限制](https://openweathermap.org/full-price) | `main.temp` 在 `units=metric` 时为摄氏度；`dt` 是数据计算的 UTC Unix 时间，`timezone` 是地点相对 UTC 的秒偏移。数据混合天气模型、卫星、雷达和站点，响应不能证明单一现场观测语义。[Current Weather 文档](https://openweathermap.org/api/current?collection=current_forecast) | 免费计划无 SLA；超过免费配额后官方会通知用户降量或迁移，否则可暂停账户。公开文档未给出本 endpoint 可依赖的 429 body/重试契约。[价格](https://openweathermap.org/full-price) [FAQ](https://openweathermap.org/faq) | 自助计划为 ODbL，要求在展示天气数据的界面提供可见 OpenWeather 归属。[详细价格/许可](https://openweathermap.org/full-price#licenses) | **淘汰**：固定 key、账户/邮箱生命周期和归属维护违背“无长期 Token、低维护”；响应还包含大量不用字段。 |

## 推荐请求契约：Open-Meteo

### 请求

```http
GET /v1/forecast?latitude={LAT_4DP}&longitude={LON_4DP}&current=temperature_2m&temperature_unit=celsius&timeformat=unixtime&timezone=GMT%2B8&forecast_days=1 HTTP/1.1
Host: api.open-meteo.com
Accept: application/json
```

完整模板：

```text
https://api.open-meteo.com/v1/forecast?latitude={LAT_4DP}&longitude={LON_4DP}&current=temperature_2m&temperature_unit=celsius&timeformat=unixtime&timezone=GMT%2B8&forecast_days=1
```

- `LAT_4DP`、`LON_4DP` 由 Provisioning Portal 一次配置，分别限制在 `[-90, 90]`、`[-180, 180]`，序列化为最多 4 位小数；URL 不含凭据。
- 固定 `temperature_unit=celsius`，不依赖服务默认值。
- 固定 `timeformat=unixtime`：官方说明 Unix timestamp 总是 GMT+0；`timezone=GMT%2B8` 使响应显式声明 `utc_offset_seconds=28800`，符合产品固定 UTC+8 决议。[参数文档](https://open-meteo.com/en/docs#api_documentation)
- 仅请求 `current=temperature_2m`；不得加入 hourly/daily/天气图标等未使用字段。

### 成功响应的最小读取字段

```json
{
  "utc_offset_seconds": 28800,
  "current_units": {
    "time": "unixtime",
    "temperature_2m": "°C"
  },
  "current": {
    "time": 1789393500,
    "interval": 900,
    "temperature_2m": 22.5
  }
}
```

只读取：

| JSON path | 用途 | 验证规则 |
| --- | --- | --- |
| `utc_offset_seconds` | 固定时区契约 | 必须为整数 `28800`。 |
| `current_units.time` | 时间单位 | 必须为字符串 `unixtime`。 |
| `current_units.temperature_2m` | 温标 | 必须为字符串 `°C`。 |
| `current.time` | provider valid time | 必须为正的整数 Unix 秒；设备时钟已同步时，不接受比当前 UTC 晚 30 分钟以上或旧 2 小时以上的样本。 |
| `current.interval` | 模型 current 步长 | 必须为正整数且不大于 3600 秒；当前实测为 900 秒，但不要把 900 写死为 API 永久保证。 |
| `current.temperature_2m` | 确认温度 | 必须是有限数；作为本地防御边界，仅接受 `[-100.0, 70.0] °C`，不通过即整次刷新失败。 |

只有 HTTP 200、响应未超本地上限且所有字段验证都通过，才同时替换值、provider valid time 与本地 `confirmed_at`。未知字段必须忽略；错误 JSON、字段缺失、类型错误、截断、TLS/DNS/超时、HTTP 400/429/5xx 都不得覆盖上次 Confirmed 温度。

## 备用请求契约：MET Norway

```http
GET /weatherapi/locationforecast/2.0/compact?lat={LAT_4DP}&lon={LON_4DP} HTTP/1.1
Host: api.met.no
User-Agent: MiniTV-Pi/1.0 github.com/DDDyyhhh/MiniTV_Pi
Accept: application/json
Accept-Encoding: gzip
If-Modified-Since: {PREVIOUS_LAST_MODIFIED_IF_PRESENT}
```

解析第一条满足 `time <= now_utc` 且最接近当前时间的 `properties.timeseries[]`；读取：

- `properties.meta.units.air_temperature` 必须是 `"celsius"`；
- 被选项的 `time` 必须是可解析的 UTC ISO-8601，且不早于当前 UTC 2 小时；
- `data.instant.details.air_temperature` 必须是有限数并通过 `[-100.0, 70.0] °C` 本地边界；
- 保留响应的 `Expires` 与 `Last-Modified`，不得早于 `Expires` 再取，并在下一次请求发送原样 `If-Modified-Since`；收到 304 时延续 Confirmed 值但不伪造新的 provider valid time。[MET 服务条款](https://docs.api.met.no/doc/TermsOfService) [JSON 时间/字段](https://docs.api.met.no/doc/ForecastJSON)

这只是备用适配契约，不等于可以把 40 KiB 响应整体放入堆后再建 DOM。实现前须证明 ESP-IDF 路径能按 MET 条款处理 gzip，并用流式解析或严格有界方案找到首个目标字段。

## 轮询与 Stale Data 映射

1. **无值**：Provisioning Portal 尚无合法经纬度、时间未同步或从未成功确认温度时，隐藏温度；暖灯小屋场景、时间和日期继续独立运行。
2. **成功**：每 30 分钟安排一次刷新；成功后记录温度、provider valid time 和本地单调时钟 `confirmed_at`，显示正常强度。
3. **失败且年龄 `<= 3 h`**：保留上一次 Confirmed 温度，进入视觉弱化的 **Stale Data**；不得用错误体、零值或部分 JSON 覆盖它。
4. **失败且年龄 `> 3 h`**：隐藏温度，场景/时间/日期不受影响。
5. **HTTP 429**：不做紧循环重试；Open-Meteo 等到下一个 30 分钟刷新槽，MET Norway 还必须立刻降流量并遵守 `Expires`。其他网络/HTTP/解析失败采用同一 Stale Data 规则。
6. `confirmed_at` 用本次完整成功接收的本地时刻计算 3 小时窗口；provider valid time 只用于拒绝未来或过旧样本，不能把 304 或失败算作新确认。

## 2026-09-14 匿名 endpoint 实测

样本坐标为北京 `39.9042,116.4074`。这是对第一方 endpoint 的一次无害 GET，仅证明该调研环境当时可访问，不证明中国大陆网络或 ESP32-C3 可访问。

### Open-Meteo

请求为上述推荐契约。服务器返回 HTTP 200、`Content-Type: application/json; charset=utf-8`、chunked + deflate；curl 下载 224 bytes，解压后的 body 为 328 bytes：

```json
{"latitude":39.89455,"longitude":116.35983,"generationtime_ms":0.02586841583251953,"utc_offset_seconds":28800,"timezone":"GMT+0800","timezone_abbreviation":"GMT+8","elevation":47.0,"current_units":{"time":"unixtime","interval":"seconds","temperature_2m":"°C"},"current":{"time":1789393500,"interval":900,"temperature_2m":22.5}}
```

返回网格坐标与请求坐标不完全相同，符合官方说明：响应经纬度是所用预报网格中心，可能距请求点数公里。[返回对象文档](https://open-meteo.com/en/docs#json_return_object)

### MET Norway

携带 `User-Agent: MiniTV-Pi/0.1 github.com/DDDyyhhh/MiniTV_Pi` 请求 `compact?lat=39.9042&lon=116.4074`。服务器返回 HTTP 200、`Content-Type: application/json`、`Content-Encoding: gzip`；传输 `Content-Length` 为 2,773 bytes，解压 body 为 41,530 bytes。目标片段为：

```json
{"properties":{"meta":{"updated_at":"2026-09-14T13:36:25Z","units":{"air_temperature":"celsius"}},"timeseries":[{"time":"2026-09-14T13:00:00Z","data":{"instant":{"details":{"air_temperature":22.9}}}}]}}
```

原响应还有完整预报字段与时间序列；片段仅展示解析路径。响应带 `Expires: Mon, 14 Sep 2026 14:17:06 GMT` 与 `Last-Modified: Mon, 14 Sep 2026 13:46:42 GMT`，验证了备用契约必须保存并服从缓存头。

OpenWeather 因官方要求账户、邮箱验证和 API key，未创建凭据，也未调用 endpoint。[OpenWeather FAQ](https://openweathermap.org/faq)

## ESP-IDF 集成风险与实现前检查

- **TLS/CA**：`esp_http_client` 支持 HTTPS/mbedTLS；服务端校验应使用 ESP x509 certificate bundle（`crt_bundle_attach`）或显式 PEM 根证书，不能关闭证书验证。[ESP-IDF 6.0.2 HTTP Client](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32c3/api-reference/protocols/esp_http_client.html#https-request)
- **阻塞与隔离**：`esp_http_client_perform()` 默认阻塞并会报告连接、读写、超时和不完整数据错误；温度拉取应在独立 worker 中运行，绝不能阻塞 UI/场景/时间任务。[ESP-IDF HTTP Client overview/API](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32c3/api-reference/protocols/esp_http_client.html)
- **响应上限**：推荐源的 current-only body 实测仅 328 bytes，仍须设置严格上限（建议 1 KiB）并拒绝溢出。MET 备用实测解压 41,530 bytes，不得整体复制后再构造 JSON DOM。
- **解析内存**：仓库现有 `card1.c` 已对 HTTP chunk 做字段扫描；本研究不规定复用其脆弱的字符串状态机。实现时选择有界流式 JSON 解析，或对 Open-Meteo 的小 body 用单一固定缓冲区后严格类型解析；避免“接收缓冲 + JSON DOM + 字符串副本”三份峰值。
- **压缩**：Open-Meteo 推荐请求无需主动声明压缩，328-byte identity body 更简单。MET 条款明确要求 gzip 支持；启用备用前必须在真机证明解压峰值、chunk 边界和错误流处理。
- **重定向/缓存**：MET 要求支持 redirect、gzip、`Expires`/`Last-Modified`；ESP-IDF 能进行 HTTP streaming 并读取状态/headers，但保存响应头有额外内存成本，需只保留必需头。[ESP-IDF HTTP Stream/Response Header Access](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32c3/api-reference/protocols/esp_http_client.html#http-stream) [MET 条款](https://docs.api.met.no/doc/TermsOfService)
- **许可 UI**：在实现前确认 320×240 温度区域如何满足 Open-Meteo “展示位置旁的链接”要求；仅在深层设置页写归属不应被假定为合规。若无法满足或项目转为商业用途，不能继续使用免费 endpoint；应改用付费 customer endpoint 或重新选源。
- **坐标隐私**：两家服务会从设备请求看到公网 IP 与坐标；Open-Meteo 条款说明故障排查日志可能保留这些信息 90 天，MET 也说明 API 日志会记录 IP 和可能的坐标。[Open-Meteo 隐私](https://open-meteo.com/en/terms#privacy) [MET 条款：Personal data](https://docs.api.met.no/doc/TermsOfService#personal-data)

## 未解决、必须真机验证

1. 中国大陆目标 Wi-Fi 下 `api.open-meteo.com:443` 与 `api.met.no:443` 的 DNS、TCP、TLS 和连续 24 小时请求成功率：**需真机网络验证**。
2. ESP-IDF 6.0.2 证书包能否验证两个当前证书链，以及设备时间未同步时的 TLS/证书行为。
3. Open-Meteo 1 KiB 上限、分块响应与超时路径；MET gzip 解压和约 40 KiB decoded stream 的峰值堆、最大块与看门狗影响。
4. 归属的屏幕布局/链接表达是否获得许可方认可；非商业假设在每次发行前是否仍成立。
5. 免费服务均不应被当成 SLA：必须保留 3 小时 Stale Data / 超时隐藏设计，并在实现时通过故障注入验证。
