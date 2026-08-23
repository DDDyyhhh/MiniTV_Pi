# 米家接入 Home Assistant 路径

## 结论摘要

- 先修正一个前提：截至本次核验，`xiaomi_home` 不是随 Home Assistant Core 发布的内建集成，而是小米官方维护、安装到 `custom_components/xiaomi_home` 的自定义集成；可由 HACS 或手工安装。HA Core 中不存在对应组件目录。[小米官方仓库 README](https://github.com/XiaoMi/ha_xiaomi_home/blob/main/README.md) [Core 路径核验](https://api.github.com/repos/home-assistant/core/contents/homeassistant/components/xiaomi_home)
- ESP32 只应调用 HA 的 `light.*` / `switch.*` 实体和服务，绝不作为 BLE 网关或米家云客户端。对已确认是 Yeelight 且可开启 LAN Control 的 Wi-Fi 灯，优先使用 HA 内建 Yeelight 集成，这是本项目最明确的本地直连路径。其余米家设备先由小米官方 `xiaomi_home` 导入；其默认命令链路仍经过米家云。若官方映射缺少必需实体，再以 HACS `xiaomi_miot` 为单设备回退；`xiaomi_miio` 只用于官方支持清单中明确命中的传统 miIO/网关型号。
- 蓝牙 Mesh 开关不能假定可被 HA 直接控制，也不能从“有小米 Hub”推论有本地 API。小米官方集成的同 LAN IP 控制明确不覆盖 BLE Mesh/ZigBee；满足特定“中国大陆 Central Hub Gateway + 指定固件”条件时，`xiaomi_home` 才有经中枢本地 MQTT 的完整本地路径，否则依赖米家云，或改走该具体型号实际暴露的 Matter/HomeKit 入口。[小米官方 README](https://github.com/XiaoMi/ha_xiaomi_home/blob/main/README.md)
- 从零部署应选 Home Assistant OS。它是 HA 官方对多数用户的推荐方式，也是使用官方 Matter Server 的受支持路径；`xiaomi_home` README 要求 Core `>= 2024.4.4`、HA OS `>= 13.0`。Container 能运行 HACS，但不能把“HACS 可安装”误读为具备同等 Matter 或网关本地化支持。[HA 安装方式](https://www.home-assistant.io/installation/) [Matter 集成](https://www.home-assistant.io/integrations/matter/)
- 不以论坛口碑宣称某集成“绝对稳定”。维护状态只根据一手仓库的 release/提交证据判断；真实可用性取决于精确型号、米家区域、固件、网关型号及 HA 最终生成的实体，必须实机验收。

## 集成选型对比

| 项目 | `xiaomi_home`（小米官方自定义集成） | `xiaomi_miot` / Xiaomi Miot Auto（HACS） | `xiaomi_miio`（HA Core 内建） |
| --- | --- | --- | --- |
| 定位 | 依据 MIoT-Spec-V2，把服务、属性、事件和动作转换为 HA 实体；小米官方支持。 | 第三方 `miot` 协议适配器，追求较广的 Xiaomi/Mijia 设备覆盖。 | 官方 Core 的传统 miIO、网关及指定型号适配器；不是通用 MIoT 自动发现方案。 |
| 安装 | HACS、仓库安装脚本或手工复制 `custom_components/xiaomi_home`；不是 Core 内建。 | HACS 的 `Xiaomi Miot` 或手工 `custom_components` 安装。 | Settings > Devices & services 配置，无 HACS。 |
| 账号与凭据 | 必须走小米 OAuth 2.0，选择正确米家云区域：中国大陆、欧洲、印度、俄罗斯、新加坡、美国。密码不保存在 HA，但 token、证书和设备资料会以未加密形式保存在 HA 配置中。 | 可用小米账号/区域走云端，也可对已确认支持 MIoT LAN 的 IP 设备填写 `host + token`；提供自动、本地、云端三种模式。 | 可用小米账号和区域发现设备/取得 host、token，亦可手工填写 32 字符 token 与 host；随后对支持型号走 LAN。 |
| 覆盖与设备类型 | README 称覆盖多数智能设备类别，但排除 Bluetooth、红外和虚拟设备类别。可写 bool 通常映射为 `switch`，可写范围数值映射为 `number` 等；不保证每型号都有期望的灯/开关实体。 | README 明示面向 Wi-Fi、BLE、ZigBee；清单涵盖灯、插座、风扇、空调、锁、传感器等。BLE/ZigBee 推荐云端模式；轮询不能实时监听某些事件型设备。 | 官方文档列出特定 Xiaomi/Aqara 网关及部分 Zigbee 子设备、小米 Philips 灯、插座、净化器、风扇、扫地机等；许多已识别子设备仍未实现。适合精确型号命中的情况。 |
| 控制路径 | 默认：云 MQTT 订阅属性/事件，云 HTTP 下发命令。完整本地：合格中枢的 MQTT；受限 LAN：同网段 IP 设备。 | 云端模式经米家云；本地模式仅适合支持 MIoT LAN 的设备，部分要求 HA 与设备同子网/VLAN。 | `local_polling`；云账号主要用于发现/取得资料。支持型号经 host/token 在 LAN 控制；某些网关子设备先从云取得列表。 |
| 当前维护证据 | 官方仓库未归档，最新稳定版 `v0.4.7` 发布于 2026-01-07，最后推送为 2026-01-28。 | 第三方仓库未归档，最新稳定版 `v1.1.4` 发布于 2026-03-10，最后推送为 2026-07-31；属第三方自定义集成，生产升级应先测试。 | Core 源码当前仍有配置流、诊断与平台模块；该目录最近一次源码提交为 2026-08-14。它仍在维护，不能仅因“legacy”称谓认定废弃。 |

`xiaomi_home` 的 manifest 标为 `cloud_polling`，但 README 说明云端状态/事件通过 MQTT 推送；`xiaomi_miot` 同样标为 `cloud_polling`；`xiaomi_miio` 标为 `local_polling`。IoT class 是集成分类，不保证每个型号具有相同传输路径或更新频率。[xiaomi_home manifest](https://api.github.com/repos/XiaoMi/ha_xiaomi_home/contents/custom_components/xiaomi_home/manifest.json) [xiaomi_miot README](https://github.com/al-one/hass-xiaomi-miot) [xiaomi_miot manifest](https://api.github.com/repos/al-one/hass-xiaomi-miot/contents/custom_components/xiaomi_miot/manifest.json) [xiaomi_miio 文档](https://www.home-assistant.io/integrations/xiaomi_miio/) [xiaomi_miio manifest](https://api.github.com/repos/home-assistant/core/contents/homeassistant/components/xiaomi_miio/manifest.json) [官方 release](https://api.github.com/repos/XiaoMi/ha_xiaomi_home/releases/latest) [Miot Auto release](https://api.github.com/repos/al-one/hass-xiaomi-miot/releases/latest) [Core 提交](https://api.github.com/repos/home-assistant/core/commits?path=homeassistant/components/xiaomi_miio&per_page=1)

选型重点不是“哪个声称支持最多设备”，而是该实际型号能否暴露可控制、可回读的 HA 实体。对 Yeelight 已有明确 LAN 能力时，原生 Yeelight 优先；对必须由米家中枢转发的 Mesh 设备，`xiaomi_home` 官方云路径优先，`xiaomi_miot` 是找回缺失 MIoT 映射的回退，`xiaomi_miio` 不应作为通用补救手段。[Yeelight 集成](https://www.home-assistant.io/integrations/yeelight/)

## 本地与云端能力

以下“本地”指 HA 到设备或桥接器的控制链路不经过互联网；不等于设备自身物理按键或网关内自动化在断网后一定可用或一定失效。未提供准确型号，不能替用户对后者作保证。

| 设备类别 | 已证实的可行路径 | 不能默认成立的事情 | 本项目应如何处理 |
| --- | --- | --- | --- |
| Wi-Fi 直连 Yeelight 灯 | 在 Yeelight App 完成初始配网后开启 **LAN Control**，HA Yeelight 集成可自动发现或按 IP 添加，IoT class 为 Local Push。ESP32 不直连灯，已建立的 LAN 控制不依赖米家云执行。 | 并非每盏“米家 Wi-Fi 灯”都支持 Yeelight LAN Control；型号、固件和是否启用该开关决定结果。 | 优先导入，给灯保留 DHCP 地址；用 HA `light.turn_on/off` 验证。常规模式每分钟约 60 请求，ESP32 必须按键去抖、请求未完成时禁连点，不能将拖动动画变成高频服务调用。 |
| 其他 Wi-Fi MIoT 灯 | `xiaomi_home` 的云 HTTP 命令 + 云 MQTT 状态；或仅当型号支持时，`xiaomi_miot` 的 host/token LAN。`xiaomi_home` LAN 功能仅是 HA 同 LAN IP 设备的部分本地能力。 | 不能从“Wi-Fi”推论必然 LAN 控制；小米官方 README 明确提醒 LAN 功能可能有异常且不建议常规启用。 | 先以 `xiaomi_home` 云路径取得实体；只有断 WAN 验证成功后，才标记为“本地已验证”。 |
| 蓝牙 Mesh 开关/灯 | 不由 ESP32 BLE 直连，其无线侧应由现有小米网关/中枢承担。`xiaomi_miot` README 把 BLE/ZigBee 建议放在云端模式；若小米官方 Central Hub 条件满足，HA 可订阅/发布中枢本地 MQTT。 | 不能把普通“小米智能网关/Hub”名称等同于 `xiaomi_home` 所说 Central Hub Gateway，也不能假定 HA 可直连 BLE。小米官方 LAN IP 控制明确不能操作 BLE Mesh/ZigBee；官方 README 又将 Bluetooth 类别列为不支持。因此不能承诺这类设备在 HA 中一定可控。 | 以导入后实际出现的实体为准。若只得到 `event.*`、`button.*` 或传感器而没有可写 `switch.*` / `light.*`，它不是卡片的可切换负载；应控制该开关实际驱动的灯/继电器实体，或用 HA 自动化/模板包装后再给 ESP32。 |
| 通过小米中枢的设备 | `xiaomi_home` 完整本地订阅/控制的官方前提：Xiaomi Central Hub Gateway 固件 `3.3.0_0023+`，或带兼容内置中枢能力且软件 `0.8.9+` 的设备；HA 与中枢同 LAN，经中枢 MQTT 发布命令、订阅状态。 | README 同时说明该 Central Hub Gateway 仅中国大陆可得。用户已有 Hub 是否属于此产品线、帐号区域能否启用、固件是否达到门槛均未知；不能凭名称假定普通 Hub 有离线本地 API。 | 在米家 App 的设备信息页记录精确 model、firmware、region，确认是否存在中央中枢本地能力。未明确符合时按云路径设计，不把离线控制写入验收承诺。 |
| 明示 Matter 或 HomeKit 的灯/桥 | Matter 设备或实际暴露 Matter bridge 的网关，可由 HA 经 Wi-Fi/Ethernet/Thread 本地控制，无需厂商云；HomeKit IP 配件可由 HomeKit Device 在同 LAN 配对后本地控制。 | “支持 Bluetooth Mesh/ZigBee”的 Hub 不等于支持 Matter 或 HomeKit。Matter/Works with HomeKit 标志、QR/配对码及桥接后实际 endpoint 必须由具体型号确认；桥接器可少于原生集成能力。 | 这是优于米家云的可选本地化分支。先找产品标识、米家 App 内 Matter 配对码或 HomeKit 配对码；测试成功后只保留一个控制来源，不要让多个集成同时控制同一物理负载。 |

官方 Xiaomi Global 的 Smart Home Hub 2 页面只确认 Bluetooth、Bluetooth Mesh、ZigBee（含 ZigBee 3.0）支持，未证明 Matter、HomeKit 或 `xiaomi_home` Central Hub MQTT 能力，故不能外推。[Xiaomi Smart Home Hub 2 产品页](https://www.mi.com/global/product/xiaomi-smart-home-hub-2/) [小米官方本地控制说明](https://github.com/XiaoMi/ha_xiaomi_home/blob/main/README.md) [Miot Auto README](https://github.com/al-one/hass-xiaomi-miot) [Matter 集成](https://www.home-assistant.io/integrations/matter/) [HomeKit Device 集成](https://www.home-assistant.io/integrations/homekit_controller/)

## 延迟与断网行为

**未找到可用于这三类实际设备组合的官方端到端毫秒数据，以下不提供伪精确 ms 数字。** 相对排序是工程预期，必须在用户的 Wi-Fi、HA 主机与网关上实测。

| 路径 | 延迟/抖动的工程预期 | 互联网断开后的预期 |
| --- | --- | --- |
| ESP32 -> HA（同 LAN）-> Yeelight LAN | 通常最低且最稳定：后半段是同 LAN IP 控制，不含云往返。Wi-Fi 丢包、灯具重连仍会造成延迟或失败。 | 在 HA、ESP32、AP 和灯仍互通、LAN Control 已启用时，预期可继续控制；必须以断 WAN 测试确认。 |
| ESP32 -> HA -> Matter / HomeKit IP | 同为局域网链路，通常低抖动；Thread 还受边界路由器与 mesh 路由质量影响。 | Matter 官方明确可在无互联网、无厂商云时本地控制。HomeKit Device 是本 LAN 配对/通信路径，仍依赖 HA 与配件网络可达。 |
| ESP32 -> HA -> 兼容中枢 MQTT -> Mesh 设备 | 全程本地时通常快于云路径；中枢到 Mesh 的无线转发、设备休眠和重试会增加不确定性。路径是否存在先取决于中枢型号/固件。 | 只有完整本地中枢已实测验证时才预期仍可控制；普通 Hub 或条件不满足时不作承诺。 |
| ESP32 -> HA -> 米家云 HTTP -> 设备，云 MQTT 回报 | 命令依赖 WAN、认证服务与云端转发，平均时间和抖动均应高于纯 LAN，且随区域服务和外网变化；服务 HTTP 被接受不代表物理负载已改变。 | HA 不能到米家云时，云命令和云状态回报不可靠/不可用；设备物理按键或网关内自动化是否还能执行，属具体型号行为，须单独测试。 |

Yeelight 官方给出的运行约束是：常规模式约 60 请求/分钟；Music mode 可绕过限制，却可能在连接断开后仍报告可用、延迟后续命令，首个命令可能失败。因此本面板不建议开启 Music mode，也不应把一条服务 HTTP 成功响应当作最终 UI 状态。服务调用后应等待 HA 状态更新；超时显示“状态未确认”，而非强制显示开/关。[Yeelight 集成](https://www.home-assistant.io/integrations/yeelight/) [HA REST API](https://developers.home-assistant.io/docs/api/rest/)

断网验收要拆开做：

1. 保持 LAN、断 HA 主机 WAN，分别操作 Yeelight LAN 灯、云导入设备和 Mesh 设备，记录控制与状态回读。
2. 保持 WAN、断 ESP32 与 HA 的 Wi-Fi，确认 ESP32 显示控制不可达而非伪造成功。
3. 恢复网络后确认 HA 实体从 `unavailable` / `unknown` 恢复，物理状态与 HA 状态一致；有差异时以下一次 HA 实际状态更新为准。

## 推荐落地路径

**阶段 0：部署基础。** 在与 AP、灯和小米网关同一可达 LAN 的常开主机安装 HA OS；为 HA、网关和可固定 IP 的 Wi-Fi 灯设置 DHCP 保留。安装当前稳定 HA，至少满足 `xiaomi_home` README 的 Core `>= 2024.4.4` 和 HA OS `>= 13.0`。Container 对熟悉 Docker 的用户可运行 HACS，且 Yeelight/`xiaomi_miio` 的 LAN 可达性主要取决于容器网络；但它没有 HA OS 的 Apps，官方 Matter Server 对 Container 的独立 Docker 部署标为不受支持。因为从零开始且可能使用 Matter，HA OS 风险更低。[HA 安装方式](https://www.home-assistant.io/installation/) [小米官方安装说明](https://github.com/XiaoMi/ha_xiaomi_home/blob/main/README.md) [HACS 安装](https://www.hacs.xyz/docs/use/download/download/) [Matter 集成](https://www.home-assistant.io/integrations/matter/)

**阶段 1：先得到一个确定的本地灯。** 对确认是 Yeelight 的 Wi-Fi 灯，在原 App 更新到支持固件、开启 LAN Control，再添加 HA Yeelight 集成。记录 HA 生成的 `light.*`、IP、型号和实体能力；反复验证 `light.turn_on`、`light.turn_off`、亮度和 HA 状态回读。这是 ESP32 卡片的首个联调目标，不应等待网关/Mesh 方案全部完成。[Yeelight 集成](https://www.home-assistant.io/integrations/yeelight/)

**阶段 2：官方米家云导入其余设备。** 安装 HACS 后从小米官方仓库添加 `xiaomi_home`，按 OAuth 登录与米家 App **相同区域**导入家庭/设备。保护 HA 配置备份，因为该集成会保存未加密 token/证书。先不要开启受限 LAN control；先确认目标设备是否在云路径正确生成实体并可受控。`xiaomi_home` 是首选，因为由生态方维护、用官方 OAuth/云接口，但不能保证蓝牙类或每项 MIoT 属性均映射为所需实体。

**阶段 3：按证据启用本地化分支。**

1. Wi-Fi 灯包装或设置若明确有 Matter/HomeKit，则分别试 HA Matter 或 HomeKit Device；成功后只保留一个控制来源。
2. 小米 Hub 的精确 model、区域和 firmware 若满足 `xiaomi_home` Central Hub 条件，单独测试中枢 MQTT 本地路径；成功后才把它列为断 WAN 可控。
3. 官方 `xiaomi_home` 漏掉必需 MIoT 实体时，安装 `xiaomi_miot` 作**单设备/单类设备回退**。BLE/ZigBee 先选 cloud mode；仅对已证实支持 MIoT LAN 的 IP 设备使用 token/host 本地模式。不要让两个集成同时控制同一物理设备。
4. 仅在设备型号处于 `xiaomi_miio` 官方清单时才尝试它，尤其传统网关/指定 Xiaomi Philips 灯等；不要猜测性扫描 Mesh 开关。

**阶段 4：冻结 ESP32 清单前的验证。** 每个候选负载都记录并实测：

- 米家 App 的地区、设备/中枢精确 model、firmware、连接协议（Wi-Fi/BLE Mesh/ZigBee）和物理房间。
- HA 的 `device_id`、最终 `entity_id`、domain、当前 `state`、实体页开/关是否可执行、亮度/颜色能力。
- 物理对应关系：HA 操作的是正确灯/继电器；物理按键改变后 HA 是否回读。
- 断 WAN、HA 重启、AP 重启后状态恢复和控制结果；分别标记“本地已证实”“只在云在线时可用”“不纳入卡片开关”。
- 为 ESP32 使用的实体在 HA UI 中改为稳定、语义化 ID，例如 `light.living_room_main`、`switch.bedroom_relay`。不要依赖首次导入名称；实体 ID 可以被用户改动。

## ESP32 可见实体契约

ESP32 面向 HA 的稳定业务接口，不感知 Xiaomi、Yeelight、BLE、Matter 或 HomeKit 的底层差异。物理“开关”不代表必然存在 `switch.*`：无线按钮可能只产生 `event.*` / `button.*`，只读传感器也不能被卡片切换。卡片配置只纳入下列可写实体，或 HA 自动化/模板明确包装出的等价实体。

| 类型 | 必需状态 | UI 应读取的属性 | 调用 |
| --- | --- | --- | --- |
| `light.<name>` | `on` / `off`；亦处理 `unavailable`、`unknown`。 | `friendly_name`；可调光时 `brightness`（HA 标度 1--255）；当前 `color_mode`；能力集合 `supported_color_modes`；当前模式对应的 `color_temp_kelvin`、`hs_color`、`rgb_color`、`xy_color` 等；可选 `min_color_temp_kelvin`、`max_color_temp_kelvin`、`effect`、`effect_list`。属性缺失即该功能不可用。 | `light.turn_on`、`light.turn_off`、可选 `light.toggle`。调光发 `brightness`；颜色只发该灯 `supported_color_modes` 接受的字段。 |
| `switch.<name>` | `on` / `off`；亦处理 `unavailable`、`unknown`。 | `friendly_name`、可选 `device_class`（如 `outlet`/`switch`）。不期待亮度/颜色字段。 | `switch.turn_on`、`switch.turn_off`、`switch.toggle`。 |
| 可选房间组 `light.<room>` / `switch.<room>` | 默认任一成员 `on` 即为 `on`；全部成员不可用时为 `unavailable`。 | 同上，但组状态不等于每成员相同；单路状态仍须单独读取。 | 同 domain 服务，前提是该组实际支持相应服务。 |

所有 REST 请求带 `Authorization: Bearer <长期访问令牌>` 与 JSON `Content-Type`。读取单实体为 `GET /api/states/<entity_id>`；物理控制必须是 `POST /api/services/<domain>/<service>`，例如：

```json
POST /api/services/light/turn_on
{
  "entity_id": "light.living_room_main",
  "brightness": 128
}
```

```json
POST /api/services/switch/turn_off
{
  "entity_id": "switch.bedroom_relay"
}
```

`POST /api/states/<entity_id>` 只改 HA 的状态表示，**不会**操作真实设备，ESP32 不得使用它作控制。服务成功后以返回的 changed states 及后续状态同步重绘；状态未确认时保留加载态，失败/超时显示不可达或未确认。availability 不是统一可依赖属性：客户端先用 state 是否为 `unavailable` / `unknown` 判断，再读取集成特有属性。`last_changed`、`last_updated` 可做陈旧状态诊断，不应用来伪造在线状态。[HA REST API](https://developers.home-assistant.io/docs/api/rest/) [Light domain](https://www.home-assistant.io/integrations/light/) [Switch domain](https://www.home-assistant.io/integrations/switch/) [Light entity model](https://developers.home-assistant.io/docs/core/entity/light/) [组集成](https://www.home-assistant.io/integrations/group/)

若某集成只能给出不稳定或不合适实体类型，可在 HA 用 Template light/switch 包装，并设置 `unique_id`、初始 `default_entity_id` 和 `availability`；ESP32 只引用包装后的契约。此做法不能绕过底层云/网关可达性。[Template 集成](https://www.home-assistant.io/integrations/template/)

## 信息来源

所有下列 URL 均在编写本报告时实际抓取核验；优先采用官方 HA 文档、HA Core 源码或相应项目的一手仓库。

1. [Xiaomi 官方 ha_xiaomi_home README](https://github.com/XiaoMi/ha_xiaomi_home/blob/main/README.md)：安装、OAuth、区域、云 MQTT/HTTP、中枢 MQTT、LAN 限制及设备类别。
2. [xiaomi_home manifest](https://api.github.com/repos/XiaoMi/ha_xiaomi_home/contents/custom_components/xiaomi_home/manifest.json)、[release](https://api.github.com/repos/XiaoMi/ha_xiaomi_home/releases/latest)、[仓库元数据](https://api.github.com/repos/XiaoMi/ha_xiaomi_home)：版本、IoT class 和维护状态。
3. [Xiaomi Miot Auto README](https://github.com/al-one/hass-xiaomi-miot)、[manifest](https://api.github.com/repos/al-one/hass-xiaomi-miot/contents/custom_components/xiaomi_miot/manifest.json)、[release](https://api.github.com/repos/al-one/hass-xiaomi-miot/releases/latest)、[仓库元数据](https://api.github.com/repos/al-one/hass-xiaomi-miot)：HACS、云/本地模式、BLE/ZigBee 范围和维护状态。
4. [HA Xiaomi Miio 文档](https://www.home-assistant.io/integrations/xiaomi_miio/)、[manifest](https://api.github.com/repos/home-assistant/core/contents/homeassistant/components/xiaomi_miio/manifest.json)、[最近源码提交](https://api.github.com/repos/home-assistant/core/commits?path=homeassistant/components/xiaomi_miio&per_page=1)：支持型号范围、本地 token 路径和当前维护证据。
5. [HA Yeelight 文档](https://www.home-assistant.io/integrations/yeelight/)：LAN Control、Local Push、发现、请求限额、Music mode 限制。
6. [HA Matter 文档](https://www.home-assistant.io/integrations/matter/) 与 [HomeKit Device 文档](https://www.home-assistant.io/integrations/homekit_controller/)：本地控制、配对、网络与 HA OS/Container 约束。
7. [HA 安装方式](https://www.home-assistant.io/installation/) 与 [HACS 安装说明](https://www.hacs.xyz/docs/use/download/download/)：HA OS 推荐、Container 的 Apps 限制与 HACS 安装边界。
8. [HA REST API](https://developers.home-assistant.io/docs/api/rest/)、[Light domain](https://www.home-assistant.io/integrations/light/)、[Switch domain](https://www.home-assistant.io/integrations/switch/)、[Light entity model](https://developers.home-assistant.io/docs/core/entity/light/)：ESP32 状态/服务契约。
9. [Xiaomi Smart Home Hub 2 官方产品页](https://www.mi.com/global/product/xiaomi-smart-home-hub-2/)：该具体产品页可确认的无线协议范围；不作为 Matter/HomeKit 或本地 MQTT 的证明。
