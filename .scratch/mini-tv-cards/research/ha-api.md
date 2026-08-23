# ESP32 与 Home Assistant 对接规范

## 结论摘要

本项目首版应采用 **REST-only**：由独立的 `ha_io_task` 顺序执行白名单实体的 `GET /api/states/<entity_id>` 和控制用的 `POST /api/services/<domain>/<service>`。不要调用全量 `GET /api/states`，也不要让 LVGL/UI 任务直接进行网络 I/O。Home Assistant 的全量状态接口返回全部实体的状态对象；单实体接口仅返回所请求实体，实体不存在时返回 `404`。[HA REST API](https://developers.home-assistant.io/docs/api/rest/)

该方案适合 ESP32-C3（4 MB Flash、无 PSRAM、约 300 KB 可用 RAM）：请求、响应、JSON 解析和阻塞均被限制在一个网络任务和固定上限内。本文的 2/4/8 KiB 等数值是**本项目初始建议**，不是 Espressif 或 HA 声称的堆占用；真机上线前必须以 `heap_caps_get_minimum_free_size()` 与 `heap_caps_get_largest_free_block()` 测量峰值余量和碎片。[ESP-IDF Heap Memory Debug](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32c3/api-reference/system/heap_debug.html)

| 数据/动作 | MVP 通道 | 建议频率或时机 | 说明 |
| --- | --- | --- | --- |
| 当前天气、24 小时预报 | REST | 30 分钟，可配为 15 分钟 | 当前值读取 `weather.*`；预报调用 `weather.get_forecasts?return_response`。 |
| Linux CPU/GPU/内存传感器 | REST 单实体读取 | 卡片可见时 10 秒；不可见时 60 秒 | 只轮询配置的 `sensor.*`。 |
| 灯/开关状态 | REST 单实体读取 | 卡片进入、控制后、可见时 10 秒 | 物理开关变化最多延迟一个轮询周期。 |
| 灯/开关控制 | REST 服务调用 + 单实体确认读取 | 用户操作 | 使用明确的 `turn_on`/`turn_off`，不把 `toggle` 用作可重试操作。 |
| 实时推送（后续） | WebSocket `subscribe_trigger` | 仅白名单灯/开关和必要仪表 | REST MVP 通过并有实时性需求后再加入。 |

每个 REST 请求带 `Authorization: Bearer <LLAT>`。LLAT 在用户个人资料的 Security 页面创建。HA WebSocket 首帧为 `auth_required`，客户端发送含 `access_token` 的 `auth`，成功为 `auth_ok`；失败为 `auth_invalid` 且会话结束。[HA REST API](https://developers.home-assistant.io/docs/api/rest/) [HA Authentication API](https://developers.home-assistant.io/docs/auth_api/#long-lived-access-token) [HA WebSocket API](https://developers.home-assistant.io/docs/api/websocket/)

## REST 与 WebSocket 取舍

### REST 的适用性与限制

HA REST API 与前端共用服务端。相关端点如下：

* `GET /api/states`：返回全部实体数组，**本项目禁用**。
* `GET /api/states/<entity_id>`：返回一个实体的 `entity_id`、`state`、`last_changed`、`attributes`；不存在为 `404`。
* `POST /api/services/<domain>/<service>`：JSON 服务数据通常含 `entity_id`；完成后返回此次执行期间改变的状态，可能包含其他并发改变的实体。

以上语义来自 [Home Assistant REST API](https://developers.home-assistant.io/docs/api/rest/)。服务调用返回的状态数组只能作辅助诊断，不能假设只包含目标实体，也不能作为最终确认；控制后必须再读取 `GET /api/states/<entity_id>`。

`esp_http_client_perform()` 默认同步阻塞；`is_async = true` 的异步模式目前只支持 HTTPS，遇到 `EAGAIN`、`EWOULDBLOCK` 或 `EINPROGRESS` 时须重复调用直到完成或失败。同一 client handle 不可并发 `perform()`；顺序请求可以复用 handle 和持久连接。[ESP-IDF v6.0.2 esp_http_client](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32c3/api-reference/protocols/esp_http_client.html)

实现一个唯一拥有 HTTP handle 的 `ha_io_task`：UI、触摸和定时器仅向 FreeRTOS 队列投递“读实体”或“控制实体”命令；网络任务完成后向 UI 投递已归一化的状态快照。不要在 LVGL 回调、Wi-Fi/IP 事件回调或 WebSocket 事件回调内执行阻塞 HTTP。FreeRTOS 中阻塞任务虽不占 CPU，但栈仍保留；ESP-IDF 的 `xTaskCreate()` 会动态分配 TCB 与栈，且栈深度单位是**字节**，须按网络调用、回调和局部变量的最深路径实测配置。[ESP-IDF v6.0.2 FreeRTOS](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32c3/api-reference/system/freertos_idf.html)

### WebSocket 的价值与成本

HA WebSocket 端点为 `ws(s)://<host>[:port]/api/websocket`。认证后，`subscribe_events` 可订阅 `state_changed`；事件带受影响实体、旧状态、新状态、时间和 context。它会收到所有匹配变化，因此小屏终端不应订阅全局 `state_changed` 后再在本地筛选。[HA WebSocket API](https://developers.home-assistant.io/docs/api/websocket/#subscribe-to-events)

`subscribe_trigger` 接受 HA 自动化 trigger 定义。单实体 state trigger 的事件在 `event.variables.trigger` 中提供 `entity_id`、`from_state`、`to_state` 等，可把订阅限定到白名单实体。[HA WebSocket API](https://developers.home-assistant.io/docs/api/websocket/#subscribe-to-trigger) 推荐后续每个需要实时更新的灯/开关建立一个 state trigger；不要假定未验证的“多实体数组 trigger”格式。

```json
// 示例：认证成功后；id 在该 WebSocket 会话内唯一
{
  "id": 21,
  "type": "subscribe_trigger",
  "trigger": {
    "platform": "state",
    "entity_id": "light.living_room"
  }
}
```

Espressif 受管 `esp_websocket_client` 有自己的任务、任务优先级/栈和 `buffer_size`；意外断线默认自动重连，默认重连延迟 10 秒。接收 payload 大于 buffer 时会拆成多个 data 事件，应用须依据总 payload 长度和 offset 重组或拒绝超限消息。`stop`、`close`、`destroy` 不能在事件处理函数内调用。[Espressif esp_websocket_client API](https://docs.espressif.com/projects/esp-protocols/esp_websocket_client/docs/latest/index.html) [组件头文件](https://github.com/espressif/esp-protocols/blob/master/components/esp_websocket_client/include/esp_websocket_client.h)

因此 WebSocket 不是“免费实时性”：它增加常驻任务、TLS/连接状态、认证重连、分片组包、订阅 ID 路由和状态版本处理。对于本项目，REST 每 10 秒刷新少量可见开关已可用，天气本来低频。**仅在以下条件全部满足后加入 WebSocket：**

1. REST MVP 已稳定，白名单实体数、响应上限和网络任务栈有真机测量。
2. 确实需要在 10 秒内反映墙壁开关/自动化变化，轮询延迟不可接受。
3. 能使用每实体 `subscribe_trigger`，而非全局 `state_changed`。
4. 可在断线、重新认证和过大/分片事件后保留最后确认状态，并以 REST 白名单刷新纠偏。
5. WSS 证书校验、任务峰值内存和连续 24 小时重连测试通过。

后续 WebSocket 仅作**状态推送**；控制仍统一走 REST 服务调用和 REST 单实体确认。订阅断开或认证失败时退回 REST 周期读取，不阻塞 UI。

## 服务发现与配置

HA Core 源码将自身服务类型定义为 `_home-assistant._tcp.local.`。本地服务记录以位置名作为实例名、实例 UUID 的 `.local.` 主机名、`hass.http.server_port` 端口和广播地址；TXT 属性包括 `location_name`、`uuid`、`version`、`external_url`、`internal_url` 和兼容用途的 `base_url`。[HA Core 常量](https://github.com/home-assistant/core/blob/dev/homeassistant/components/zeroconf/const.py) [HA Core 广播实现](https://github.com/home-assistant/core/blob/dev/homeassistant/components/zeroconf/__init__.py)

在 ESP-IDF v6.0.2 主仓中 mDNS 已移出框架；官方 v6.0.2 页面要求通过受管组件添加 `espressif/mdns`。当前组件 manifest 标示 mDNS 1.11.3、依赖 ESP-IDF `>=5.0`；项目须在依赖文件中**锁定实际验证版本**，不可无约束升级。其 `mdns_query_ptr(service_type, proto, timeout, max_results, &results)` 可查询 PTR 服务，结果使用 `mdns_query_results_free()` 释放。[ESP-IDF v6.0.2 mDNS 说明](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32c3/api-reference/protocols/mdns.html) [mDNS manifest](https://github.com/espressif/esp-protocols/blob/master/components/mdns/idf_component.yml) [mDNS API](https://docs.espressif.com/projects/esp-protocols/mdns/docs/latest/en/index.html#mdns-service-query)

推荐 SoftAP 手机配网页首次流程：

1. STA 获得 IP 后，执行一次、最多 3 秒的 PTR 查询：`mdns_query_ptr("_home-assistant", "_tcp", 3000, 4, &results)`；只展示有可达 IPv4 地址和端口的最多四项结果。
2. 将发现记录展示为候选：位置名、IP/主机、端口、HA 宣告的 `internal_url`。mDNS 不为本项目提供端点认证，**不得**静默信任 TXT URL 或自动覆盖已有配置。
3. 用户选择候选或手工输入 endpoint 与 LLAT；HTTPS 情形提供 CA PEM 上传/粘贴或选择固件证书 bundle 的方式。随后执行 API 认证与证书校验测试。
4. 仅在测试成功、用户确认后持久化规范化 endpoint。启动优先使用该配置；配置失效、连续连接失败或用户“重新发现”时才再次 mDNS，不让临时广播改写已保存 endpoint。

`esp_netif` 是 IDF 对 lwIP 的线程安全网络接口抽象，负责接口、DHCP、DNS 和事件。应用等待 STA 获得 IP 后再发现/访问 HA；配网页 SoftAP 与 STA 并存时明确让 HA 管理流量走 STA 默认路由，避免绑定到 AP 地址。[ESP-IDF v6.0.2 esp_netif](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32c3/api-reference/network/esp_netif.html)

持久化的 `ha_endpoint` 是不含路径、查询、凭据的绝对 origin，例如 `https://ha.lan:8123` 或 `https://192.168.1.20:8123`；请求代码只追加固定 `/api/...` 路径。配网页拒绝非 `http`/`https`、含 `@`、路径、查询、片段和非法端口的值，并校验实体 ID 不含 `/` 或 URL 控制字符。`host:port` 不替代 `https://` scheme：协议必须显式且可审计。

## HTTP、HTTPS 与令牌安全

HA 的 HTTP server 在 Settings > System > Network 配置。官方 HTTP 集成支持以绝对路径配置证书和私钥；反向代理场景须显式启用并限制受信任 `X-Forwarded-For` 代理 IP/CIDR。官方当前文档指出 HAOS 自 2026.8 默认端口为 80，Container 默认仍为 8123；设备不能硬编码 8123，mDNS SRV 端口和已保存 endpoint 才是权威。[HA HTTP integration](https://www.home-assistant.io/integrations/http/)

**生产默认：LAN 内也使用 HTTPS，并验证服务器证书与主机名。** LLAT 是 Bearer 凭据；任何取得它的网络观察者或中间人都可按其权限调用 API。ESP-IDF TLS 客户端须选择 CA buffer、全局 CA store、证书 bundle 或 PSK 等一种服务器验证方式；`skip_common_name` 会关闭主机名验证和 SNI。为私有 CA/自签发链配置 HA 时，配网页导入 CA（或受控的叶证书锚），请求使用 `cert_pem`/`cert_der` 或受控证书 bundle，且 endpoint 主机名须与证书匹配。[ESP-IDF v6.0.2 ESP-TLS](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32c3/api-reference/protocols/esp_tls.html) [ESP-IDF v6.0.2 esp_http_client](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32c3/api-reference/protocols/esp_http_client.html)

不要为“局域网方便”关闭 CA 或 Common Name 验证。ESP-TLS 文档有测试用的跳过服务器证书验证配置；它移除服务器身份验证，不能进入量产固件。反向代理不改变此规则：设备连接的是代理公开的 HTTPS endpoint，必须信任并校验该证书。HA 同样建议不要直接暴露实例到公网，应使用受保护的远程访问方式。[ESP-IDF v6.0.2 ESP-TLS](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32c3/api-reference/protocols/esp_tls.html) [HA securing guide](https://www.home-assistant.io/docs/configuration/securing/)

**开发例外（不安全）：** 只有隔离测试 LAN、短时调试和可随时撤销的测试 LLAT 可使用 `http://`。它会以明文传递 LLAT 与控制数据，易被窃听、重放或篡改；配网页应以风险确认和编译期开关限制它，生产设置中默认拒绝且不自动回退 HTTP。无论开发还是生产，均不推荐禁用 HTTPS 证书验证。

令牌及配置规则：

* 在 NVS 独立命名空间保存 `endpoint`、`token`、可选 `ca_pem`、白名单和配置版本；token 用 blob 或字符串读取时核对长度，不输出到串口、日志、错误页、HTTP 响应或截图。
* 配网页保存时遮罩 token；清除配置须擦除该 namespace 并重启未配对状态。使用专用 HA 用户建立 LLAT；丢失设备或转让前从 HA 个人资料撤销并清除设备配置。
* 启用 NVS encryption。IDF v6.0.2 使用 XTS-AES：可配合 Flash Encryption 与 `nvs_keys` 分区，或用 HMAC eFuse 派生密钥；后者会消耗 eFuse key block。**只启用通用 Flash Encryption 不是 NVS encryption 的替代品**，NVS 分区不应仅以硬件 Flash Encryption 的 `encrypted` 标志处理。[ESP-IDF v6.0.2 NVS encryption](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32c3/api-reference/storage/nvs_encryption.html) [NVS flash](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32c3/api-reference/storage/nvs_flash.html)
* NVS 加密保护静态闪存机密性与修改检测，但不防止擦除，也不能挽回 HTTP、调试日志或已被攻陷 token 的泄露；它不替代传输安全。
* 首次 HTTPS 请求前初始化时间。IDF 推荐线程安全包装 `esp_netif_sntp_init()` 与 `esp_netif_sntp_sync_wait()`；时间未同步时显示“等待校时”，不得把证书时间错误降级为不校验。[ESP-IDF v6.0.2 system time / SNTP](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32c3/api-reference/system/system_time.html)

## ESP-IDF 组件与 JSON 选型

### 组件基线

| 组件 | ESP-IDF v6.0.2 状态与用途 | 本项目处理 |
| --- | --- | --- |
| `esp_http_client` | v6.0.2 主仓组件；支持 HTTP/HTTPS、`timeout_ms`、`buffer_size`、`buffer_size_tx`，默认同步 `perform()`。 | MVP 必选；任务独占 handle，显式设置超时和收/发缓冲。 |
| `esp_netif` | v6.0.2 主仓组件；STA/AP、IP、DHCP、DNS、SNTP 网络抽象。 | 必选；IP 事件后开始 HA 工作。 |
| `nvs_flash` | v6.0.2 主仓组件；保存配置；可配 NVS encryption。 | 必选；LLAT 加密存储。 |
| `esp_netif_sntp` / `esp_sntp` | v6.0.2 公开 API；前者为推荐线程安全应用封装，后者是底层 SNTP 控制 API。 | 用 `esp_netif_sntp` 初始化与等待同步。 |
| `mdns` | 不在 v6.0.2 主仓；官方要求受管 `espressif/mdns`。manifest 为 1.11.3、IDF `>=5.0`。 | 可选，仅首次发现/用户重新发现；锁版本后 C3 真机验证。 |
| `esp_websocket_client` | 不在 v6.0.2 主仓；受管 `espressif/esp_websocket_client`。manifest 为 1.8.0、IDF `>=5.0`。 | MVP 不加入；满足前述准入条件才加。 |

v6.0.2 主仓 components 可直接确认 `esp_http_client`、`esp_netif`、`nvs_flash`；`mdns` 与 WebSocket 客户端不在该列表中。[ESP-IDF v6.0.2 components tree](https://github.com/espressif/esp-idf/tree/v6.0.2/components) 受管组件版本和 IDF 下限见 [mDNS manifest](https://github.com/espressif/esp-protocols/blob/master/components/mdns/idf_component.yml) 与 [WebSocket manifest](https://github.com/espressif/esp-protocols/blob/master/components/esp_websocket_client/idf_component.yml)。实际项目应锁定解析后的依赖，避免无意升级。

当前固件 [`sdkconfig`](/home/dyh/EPS32-C3_Mini_TV/Firmware/sdkconfig) 已启用 `CONFIG_ESP_HTTP_CLIENT_ENABLE_HTTPS=y`。这仅表示 HTTP client 的 HTTPS transport 已编译，**不表示**已配置或验证 HA 的 CA/主机名。官方 `esp_http_client_config_t` 有 HTTP 接收 `buffer_size`、发送 `buffer_size_tx` 字段；没有据此可断言的“TLS 固定缓冲大小”，故不把 HTTP 缓冲数值误称 TLS RAM 需求。[v6.0.2 esp_http_client header](https://github.com/espressif/esp-idf/blob/v6.0.2/components/esp_http_client/include/esp_http_client.h) [v6.0.2 HTTP client Kconfig](https://github.com/espressif/esp-idf/blob/v6.0.2/components/esp_http_client/Kconfig)

### JSON 选择与边界

“cJSON 已原生在 ESP-IDF v6.0.2 主仓中”不成立：该 release 的主仓 components 列表没有 `cjson` 项。Espressif 维护的受管组件 `espressif/cjson` 版本 1.7.19~2 标示 IDF `>=5.0`；若现有依赖树未引入它，应显式加入并锁定版本。[ESP-IDF v6.0.2 components tree](https://github.com/espressif/esp-idf/tree/v6.0.2/components) [cJSON manifest](https://github.com/espressif/idf-extra-components/blob/master/cjson/idf_component.yml)

MVP 推荐 cJSON：仅解析少量结构稳定的 HA 响应，集成成本低；但它构建树状对象，解析峰值不等于输入字节数。不要解析 `/api/states`，不要保留 JSON DOM，不要把完整 `attributes` 原样复制到 UI 模型。每次只解析一个封顶响应，提取 `entity_id`、`state`、`last_changed` 与白名单属性后立即 `cJSON_Delete()`。缺字段、类型不符或超限字符串视为刷新失败，保留最后确认值并标为 stale。

初始内存策略：普通单实体响应累积上限 2 KiB，服务响应 4 KiB，天气预报响应 8 KiB；HTTP data 回调在 `Content-Length` 已知且超限时提前失败，未知/分块传输时累计计数，超限立即中止。每项任务同时仅保留一个响应缓冲和一个 cJSON DOM；天气解析后只保留最多 24 个 `{epoch, temperature, condition}` 采样点。真实 HA 属性或预报超出 8 KiB 时，应在 HA 侧用 template/helper 缩减数据或改用更小单用途传感器，不能无限扩容。

只有真机测量表明 cJSON 峰值不可接受时，才评估只提取固定键的 token/流式解析器。这是复杂度交换，不是 MVP 前置依赖；无论解析器为何，响应、字段、数组和嵌套深度上限必须保留。

## 推荐接口契约

### 配置模型与实体白名单

SoftAP 配网页提交后写入下列逻辑配置；名称为契约建议，可映射为 NVS key/JSON/blob，但明文 token 不得回传：

```text
ha_endpoint = "https://ha.lan:8123"       # 绝对 origin；无 /api 路径
ha_token = "<long-lived access token>"    # 加密 NVS；仅写入路径可见
ha_ca_pem = "<optional private CA PEM>"   # HTTPS 私有 CA 时需要
ha_tls_mode = "verify-ca-and-host"        # 生产唯一允许值
ha_weather_entity = "weather.home"
ha_weather_forecast_type = "hourly"
ha_pc_entities = ["sensor.pc_cpu", "sensor.pc_gpu", "sensor.pc_memory"]
ha_light_entities = ["light.living_room", "light.bedroom"]
ha_switch_entities = ["switch.desk_power"]
```

白名单由用户在配网页明确添加，设编译期最大数（例如天气 1、PC 8、灯/开关合计 12），并按域校验：天气仅 `weather.`，PC 指标仅允许的 `sensor.`，控制仅 `light.` 或 `switch.`。上方实体名为**示例**，不是预置真实 HA 实体。`404` 标记配置项失效，不无限重试。

`ha_io_task` 维护仅含 UI 所需字段的缓存：`entity_id`、标准化值/单位、`confirmed_at`、`last_attempt_at`、`quality`（`confirmed`/`pending`/`stale`/`unavailable`/`error`）和本地 generation。UI 仅读取复制快照。网络任务不得持有 LVGL 对象；LVGL 不得持有 HTTP/cJSON 指针。

### REST 读取与天气

单实体读取示例：

```http
GET /api/states/sensor.pc_cpu HTTP/1.1
Host: ha.lan:8123
Authorization: Bearer <LLAT>
Accept: application/json
```

成功时仅抽取目标状态和白名单属性。`unknown`、`unavailable` 不是解析错误：UI 显示不可用和最后成功时间，不能把它们当数值 0。

当前天气走 `GET /api/states/<weather_entity>`。24 小时曲线使用返回数据的天气服务，必须带 `?return_response`；HA 文档说明 `weather.get_forecasts` 使用 `type: hourly` 时按 weather entity 键返回含 `forecast` 数组的数据，而 REST 服务 API 要求有返回数据的服务使用 `return_response`。[HA weather integration](https://www.home-assistant.io/integrations/weather/) [HA REST service API](https://developers.home-assistant.io/docs/api/rest/#post-apiservicesdomainservice)

```http
POST /api/services/weather/get_forecasts?return_response HTTP/1.1
Authorization: Bearer <LLAT>
Content-Type: application/json

{"type":"hourly","target":{"entity_id":"weather.home"}}
```

这是官方 action 的 JSON 化**示例**：实现先在目标 HA 版本测试 service data，再只抽取对应实体 `forecast` 前 24 个合法温度/时间条目。若天气集成不支持 hourly forecast 或响应超限，显示当前天气和上次有效曲线，不伪造预测。天气事务至多一次当前状态请求加一次预报请求，每 30 分钟执行；手动刷新最多每 5 分钟一次。

### 控制请求、确认与 UI 状态机

按钮从本地**最后确认**状态计算目标，再调用幂等的显式服务；不要把盲目 `toggle` 作为网络重试目标：

```http
POST /api/services/light/turn_on HTTP/1.1
Authorization: Bearer <LLAT>
Content-Type: application/json

{"entity_id":"light.living_room"}
```

```http
POST /api/services/light/turn_off HTTP/1.1
Authorization: Bearer <LLAT>
Content-Type: application/json

{"entity_id":"light.living_room"}
```

`light.turn_on`/`light.turn_off` 是 light 集成动作；`turn_on` 还可设亮度、颜色、色温、effect、transition，首版只发 `entity_id`。开关同理映射 `switch.turn_on`/`switch.turn_off`；若后续采用 `switch.toggle`，须复用下列“不自动重试”规则。[HA light actions](https://www.home-assistant.io/integrations/light/) [HA switch actions](https://www.home-assistant.io/integrations/switch/)

每次触摸的 UI 状态机：

1. 已确认 `on/off` 后立即显示目标视觉值和 spinner/pending；同实体禁用再次触摸，但不记为 confirmed。
2. `ha_io_task` 发一次服务请求。HTTP 成功仅表明 HA 接受/执行调用，不以 `changed_states` 数组作为最终模型。
3. 紧接 `GET /api/states/<entity_id>`；若状态与目标一致，写入 confirmed。若不同、超时或解析失败，在 1 秒、3 秒至多再读两次，仍不重发服务。
4. 服务明确失败、三次读取不一致或连接断开时，恢复最后 confirmed 视觉值，显示短暂错误，缓存置 `stale/error`。初始状态未知时不发 toggle 类动作，先刷新状态。

### 超时、重试与调度

下表是项目建议初值，须在同一 LAN 的 HA、反向代理和 Wi-Fi 环境真机验证后微调：

| 场景 | 策略 |
| --- | --- |
| 普通 REST 请求 | `timeout_ms = 5000`；单任务串行；请求总工作预算 7 秒。官方 timeout 为 0 时默认 5 秒，项目显式设值避免配置漂移。 |
| 幂等读取失败 | 1 秒、2 秒、4 秒指数退避，最多 3 次；仍失败则下周期退避 30 秒、60 秒、120 秒、最大 5 分钟。IP 恢复事件可触发一次读取。 |
| 服务调用 | 每次触摸只发一次；超时、断链或未知结果**不**自动重放；随后按 UI 状态机读取确认。 |
| 认证/配置错误 | `401` 停止自动重试并提示重新配对；`404` 标记实体配置错；TLS 验证错误不回退 HTTP。 |
| 轮询排序 | 控制确认 > 可见灯/开关 > 可见 PC 指标 > 天气 > 不可见卡片指标；每次只发一个请求。 |

正式启用 WebSocket 后，由已验证 endpoint 将 `https` 改为 `wss`、`http` 改为 `ws` 并追加固定 `/api/websocket`，不得接受服务器下发的任意 URL。认证成功后建立每实体 trigger；网络重连后重新认证、重建全部订阅，再依次 REST 刷新白名单以消除离线窗口差异。仅当事件 `entity_id` 精确命中白名单、payload 未超限且字段验证通过时才更新缓存；否则丢弃并只记录不含 token 的诊断计数。

## 信息来源

以下均为 Home Assistant、Espressif 官方文档/仓库或官方组件 manifest；访问日期为 2026-08-15。

* [Home Assistant REST API](https://developers.home-assistant.io/docs/api/rest/)：Bearer 认证、states、services、`return_response`、状态码。
* [Home Assistant WebSocket API](https://developers.home-assistant.io/docs/api/websocket/)：认证、`subscribe_events`、`subscribe_trigger`、result/event 格式。
* [Home Assistant Authentication API](https://developers.home-assistant.io/docs/auth_api/#long-lived-access-token)：LLAT 建立位置与用途。
* [Home Assistant Weather integration](https://www.home-assistant.io/integrations/weather/)：`weather.get_forecasts` 与 hourly forecast。
* [Home Assistant HTTP integration](https://www.home-assistant.io/integrations/http/)：HTTP server、端口、TLS 证书、反向代理。
* [Home Assistant securing guide](https://www.home-assistant.io/docs/configuration/securing/)：公网暴露与受保护远程访问建议。
* [HA Core Zeroconf constants](https://github.com/home-assistant/core/blob/dev/homeassistant/components/zeroconf/const.py) 与 [广播实现](https://github.com/home-assistant/core/blob/dev/homeassistant/components/zeroconf/__init__.py)：服务名、SRV/TXT 广播字段。
* [ESP-IDF v6.0.2 esp_http_client](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32c3/api-reference/protocols/esp_http_client.html)、[公开头文件](https://github.com/espressif/esp-idf/blob/v6.0.2/components/esp_http_client/include/esp_http_client.h)：同步/异步、超时、HTTP 缓冲、连接复用。
* [ESP-IDF v6.0.2 esp_netif](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32c3/api-reference/network/esp_netif.html)、[SNTP/system time](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32c3/api-reference/system/system_time.html)：网络接口与推荐 SNTP 封装。
* [ESP-IDF v6.0.2 ESP-TLS](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32c3/api-reference/protocols/esp_tls.html)：服务器 CA/主机名验证与不安全测试选项。
* [ESP-IDF v6.0.2 NVS encryption](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32c3/api-reference/storage/nvs_encryption.html)、[NVS flash](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32c3/api-reference/storage/nvs_flash.html)：NVS 加密、HMAC/Flash Encryption 约束。
* [ESP-IDF v6.0.2 FreeRTOS](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32c3/api-reference/system/freertos_idf.html)、[Heap Memory Debug](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32c3/api-reference/system/heap_debug.html)：任务栈与堆峰值/碎片测量。
* [ESP-IDF v6.0.2 components tree](https://github.com/espressif/esp-idf/tree/v6.0.2/components)：主仓组件归属。
* [Espressif mDNS manifest](https://github.com/espressif/esp-protocols/blob/master/components/mdns/idf_component.yml)、[mDNS API](https://docs.espressif.com/projects/esp-protocols/mdns/docs/latest/en/index.html#mdns-service-query)：受管组件版本下限与查询 API。
* [Espressif WebSocket manifest](https://github.com/espressif/esp-protocols/blob/master/components/esp_websocket_client/idf_component.yml)、[WebSocket API](https://docs.espressif.com/projects/esp-protocols/esp_websocket_client/docs/latest/index.html)：受管组件、事件、重连、分片、任务配置。
* [Espressif cJSON manifest](https://github.com/espressif/idf-extra-components/blob/master/cjson/idf_component.yml)：cJSON 受管组件版本与 IDF 下限。
