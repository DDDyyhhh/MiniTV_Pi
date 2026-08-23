# 03 米家接入 HA 路径调研

Type: research
Status: resolved
Blocked by:

## Question

用户的米家设备组合 = **Wi-Fi 直连灯 + 蓝牙 Mesh 开关 + 小米中枢网关**。调研出把这三类设备接进 Home Assistant 的最优路径:

1. **官方 Xiaomi Home 集成**(HA 自带,2024+ 新版,走米家云账号)vs **HACS 的 Xiaomi Miot Auto**:各自能力边界、安装门槛、稳定性口碑(2026 年现状)。
2. **本地 vs 云端**:Wi-Fi 直连灯(如 Yeelight/米家 Wi-Fi 灯)是否可局域网直控?蓝牙 Mesh 设备是否必须经中枢网关再经云端,还是中枢提供本地 API?
3. **延迟与断网表现**:ESP32 → HA → 灯 的典型链路延迟;米家云掉线时三类设备是否仍可控。
4. **给本努力的推荐**:对「客厅/房间灯光开关面板」这一用例,选哪条路径?对 ESP32 侧接口(工单 04)有什么依赖?

产出:路径决策(供工单 08 用)+ 关键事实清单。结论写 `research/mihome-path.md`,此处回填要点。

## Answer

### 结论

- 后端主机从零部署选 **Home Assistant OS**。它是 HA 面向一般部署的推荐形态，也为后续 Matter/加载项留出受支持路径。
- 对已确认可在 Yeelight App 开启 **LAN Control** 的 Wi-Fi 灯，优先使用 HA 内建 Yeelight 集成，作为卡片3首个可验证的纯局域网负载。ESP32 仅调用由 HA 暴露出的实体，不直连灯、不做 BLE 网关。
- 其他米家 Wi-Fi 设备先用小米官方维护的 `xiaomi_home` 自定义集成导入；它不是 HA Core 内建组件。仅当官方集成不能提供所需实体时，按设备逐个以 HACS `xiaomi_miot` 回退；`xiaomi_miio` 只用于官方明确支持的传统型号。
- 蓝牙 Mesh 开关与“小米中枢网关”不得预设存在 HA 直连或离线本地控制。实际能力取决于精确型号、米家区域、固件与是否符合官方 Central Hub 的本地 MQTT 条件。先导入并验收实际可写 `light.*` / `switch.*` 实体；只有 `event.*` / `button.*` 的无线开关不作为可直接切换的卡片负载。
- ESP32 的稳定业务契约只识别 `light.*`、`switch.*` 及它们的 `on`/`off`、`unavailable`/`unknown` 状态；控制使用 `light.turn_on/off` 或 `switch.turn_on/off`，服务成功后等待 HA 状态回读确认。需要时用 HA Template light/switch 包装不合适的底层实体。

详细比较、型号验证清单、断 WAN 验收步骤和来源见[研究报告](../research/mihome-path.md)。
