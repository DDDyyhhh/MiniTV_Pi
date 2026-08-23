# 04 ESP32↔HA 对接规范调研

Type: research
Status: resolved
Blocked by:

## Question

调研 ESP32-C3(ESP-IDF v6.0.2,4MB flash,无 PSRAM)与局域网内 Home Assistant 对接的接口规范:

1. **传输通道**:长期访问令牌 + REST(`GET /api/states`、`POST /api/services/<domain>/<service>`)vs WebSocket API(`/api/websocket`,状态推送);ESP32-C3 上哪个更稳?状态变化需要多实时(卡片2/3 的仪表与开关)?
2. **发现**:HA mDNS (`_home-assistant._tcp`)在 ESP-IDF 上怎么用;还是直接配置 IP(配网页填)。
3. **HTTP vs HTTPS**:局域网内 https 需要 CA 证书(内存成本,ESP32-C3 无 PSRAM);http 直连是否可接受(令牌明文)。
4. **ESP-IDF v6.0.2 可用组件**:`esp_http_client`、`esp_mdns`、`esp_websocket_client` 在 v6.0.2 的可用性与已知坑;轻量 JSON(如 cJSON/jsmn/llhttp)选哪个。
5. **轮询 vs 订阅的取舍**:给出推荐契约——轮询周期、订阅消息格式、控制调用格式、断线重连与超时参数(给 09–12 直接照用)。

产出:接口契约(供 06/07/08 决策与 09–12 实现)。结论写 `research/ha-api.md`,此处回填要点。

## Answer

### 结论

- 首版采用 **REST-only**：专用 `ha_io_task` 串行处理白名单实体的 `GET /api/states/<entity_id>` 与服务调用。禁止全量 `GET /api/states`，UI/LVGL 任务不可直接执行网络 I/O。
- 读写协议:天气每 30 分钟刷新(可配置为 15 分钟);卡片可见时的 PC 传感器和灯/开关每 10 秒读取;灯控发送一次明确的 `turn_on` 或 `turn_off`，紧接读取状态确认。服务调用结果未知时不自动重放。
- WebSocket 不纳入 MVP。仅在 REST 稳定、真机内存测量通过且确实需要低于 10 秒状态延迟后，才增加每个白名单实体一个 `subscribe_trigger`；控制仍保留 REST，断线时回退轮询。
- 首次 SoftAP 配网允许 mDNS 发现 `_home-assistant._tcp.local.` 候选项或手工输入 endpoint；用户测试成功并确认后才持久化。`mdns` 与 `esp_websocket_client` 都需作为受管组件显式锁版本，MVP 只需 `esp_http_client` / `esp_netif` / `nvs_flash` / `esp_netif_sntp`。
- 生产默认 HTTPS 且校验证书与主机名，LLAT 与私有 CA 资料使用 NVS encryption 存储，日志/页面绝不输出 token。短时隔离调试的 HTTP 是显式不安全例外，禁止静默降级。JSON 用显式锁定的 `espressif/cjson`，单实体/服务/天气响应初始上限分别为 2/4/8 KiB，并在真机测量堆峰值后调优。

完整的端点、重试、UI pending/confirmed 状态机、请求示例及一手来源见[研究报告](../research/ha-api.md)。
