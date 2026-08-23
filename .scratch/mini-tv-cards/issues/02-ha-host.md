# 02 Home Assistant 安装主机方案

Type: grilling
Status: resolved
Blocked by:

## Question

后端统一走 HA,但用户家里还没装。定下:

1. **装在哪台硬件上**:树莓派 / NAS(Docker)/ 闲置小主机 / 虚拟机?约束:常开、与 ESP32 和小米中枢网关同局域网、能跑米家集成(Xiaomi Miot 需要的能力)、能跑 Linux 电脑性能采集(工单 07 依赖,需能执行命令或跑 Python)。
2. **形态**:HAOS(带 Supervisor/加载项,如 ssh、Advanced SSH & Web Terminal 方便 shell_command)vs Container 版 vs Core;对工单 07 的「一键打开应用」与「Linux 采集」哪个体感最顺?
3. **落地任务**:安装本身是否已有人手?若装好,给出 HA 实例的 IP、长期访问令牌的生成方式、米家与电脑集成的安装清单(这是给工单 11/12 实现的依赖事实)。
4. 若安装责任在本努力内,明确写进 09 或 07 的任务范围。

产出:主机与形态决策 + 安装/初始化清单(含令牌、集成安装步骤),回填为工单 07/08/11/12 的依赖事实。

## Answer

### 结论

Home Assistant 不另配常开小主机,首版安装在用户现有 **Linux 主机** 上;该主机也是卡片2要监控和执行“一键打开应用”的 Linux Target PC。用户明确接受:这台机器开机时才使用 Home Assistant。因此本工单不再按 24 小时家庭中枢承诺,而按“电脑开机时可用的本地控制后端”设计。

这意味着:

- 形态采用 **Home Assistant Container**(Docker/Podman 容器)或等价 HA Core 服务,优先推荐 Docker Compose 的 Home Assistant Container。
- 不采用 HAOS 裸机/专机作为当前决策,因为用户不打算给 HA 独占一台常开设备。HAOS 仍是未来升级选项:若以后需要智能家居 24 小时可用、Add-on 商店、Matter Server 官方路径或更低维护成本,再迁移到 x86 小主机/HAOS。
- ESP32 Mini TV 在 Linux 主机关机或 HA 容器未运行时必须显示 HA 离线/后端不可达,不能伪造灯光、天气或性能数据成功状态。
- 同一局域网已满足:HA 主机、ESP32、小米中枢网关、灯在同一 LAN/VLAN,这保留 Yeelight LAN、mDNS 和 ESP32 REST 访问的可行性。

### 主机与形态

- **HA Host**:现有 Linux 主机。
- **运行形态**:Home Assistant Container via Docker Compose。
- **网络模式**:优先 `network_mode: host`,理由是 mDNS/zeroconf、Yeelight LAN discovery、局域网设备发现和 ESP32 访问最少踩坑。若发行版或网络策略不允许 host network,再用 bridge + 显式端口和 mDNS/发现替代方案,但这不作为首选。
- **可用性边界**:HA Host 不是 always-on hub。电脑关机时:
  - 卡片1:时钟仍可本地运行,NTP/天气/预报无 HA 时显示缓存或离线。
  - 卡片2:性能指标与快捷启动不可用。
  - 卡片3:灯光控制不可用,UI 显示后端离线;物理开关/米家 App 是否可用不由 ESP32 保证。

### 安装/初始化清单

1. 在 Linux 主机安装 Docker Engine 与 Docker Compose plugin。
2. 创建持久目录,例如 `~/homeassistant/config`。
3. 使用 Home Assistant Container 启动,建议 Compose 形态:

```yaml
services:
  homeassistant:
    image: ghcr.io/home-assistant/home-assistant:stable
    container_name: homeassistant
    network_mode: host
    privileged: false
    restart: unless-stopped
    volumes:
      - ./config:/config
      - /etc/localtime:/etc/localtime:ro
```

4. 首次访问 `http://<linux-host-ip>:8123` 建立 HA 用户。
5. 在路由器为 Linux 主机、可固定 IP 的 Wi-Fi 灯和小米中枢网关设置 DHCP 保留;ESP32 配网页后续保存 `ha_endpoint`。
6. 为 ESP32 建一个专用 HA 用户或至少单独生成一个长期访问令牌(LLAT):用户头像/个人资料 -> Security -> Long-Lived Access Tokens。token 只写入 ESP32 NVS,不写日志。
7. 安装/启用基础集成:
   - Yeelight:先在灯具 App 开启 LAN Control,HA 添加 Yeelight 集成,拿到首个稳定 `light.*` 实体。
   - HACS:为小米官方 `xiaomi_home` 自定义集成做准备。
   - `xiaomi_home`:按米家 App 相同区域 OAuth 登录并导入设备;实体能力按工单 03 的验证清单验收。
   - 天气集成:选一个能提供 `weather.*` 且支持 hourly forecast 的集成,具体在工单 06 定。
8. 不依赖 HAOS Add-on。需要 shell/脚本时,在 Linux 主机本机用 systemd user service、Python agent、MQTT 或 HA REST/WebSocket 脚本配合;具体链路在工单 07 定。

### 对卡片2的影响

由于 HA Host 就是 Linux Target PC,卡片2可以走更简单的本机方案:

- 性能采集:优先在 Linux 主机上运行轻量本机采集 agent,读取 CPU/内存/GPU 指标后写入 HA(MQTT 或 REST sensor/template 方式,工单 07 选择)。不要求 HA 容器直接执行宿主机命令。
- 一键打开抖音/VSCode/B站:优先用宿主机本机 agent 接收 HA 命令并在用户桌面会话中启动应用/URL。不要默认使用 HAOS `shell_command` 或 Add-on,因为当前不是 HAOS。
- 需要额外确认的事实推迟到工单 07:Linux 发行版、桌面环境、显卡厂商、浏览器/VSCode 命令、抖音入口(URL 或本地应用)。

### 对卡片3/米家的影响

- 米家接入仍按工单 03:Yeelight LAN 先跑通;其他米家设备用 `xiaomi_home`,必要时 `xiaomi_miot` 回退。
- 因 HA Host 非 24 小时在线,卡片3不能把 ESP32 当成家里长期智能中枢。它是“电脑开机时的桌面控制面板”。
- 如果未来想让灯控在电脑关机时仍可用,必须另开新决策:迁移 HA 到 always-on x86 小主机/树莓派/HAOS。

### 交付给后续工单的依赖事实

- `ha_endpoint`:初版按局域网 IP + 8123 配置,例如 `http://<linux-host-ip>:8123` 用于本地调试;生产/长期使用应按工单 04 迁移到 HTTPS + CA 校验。
- `ha_token`:ESP32 专用 LLAT。
- `ha_host_mode`:Linux host + Home Assistant Container,host network。
- `availability_policy`:HA Host 关机时 UI 显示后端离线,不尝试自动控制灯/快捷/性能数据。
- 工单 07 不依赖 HAOS Add-on,应设计 Linux 宿主机 agent。
- 工单 08 只需要 HA 暴露 `light.*`/`switch.*` 实体;底层是 Yeelight LAN、`xiaomi_home` 云路径还是回退集成,ESP32 不感知。

### 安装责任归属

本努力的固件实现不会直接在当前会话安装/改动用户 Linux 主机。后续执行时,安装 HA Container 与本机 agent 属于工单 11 的电脑侧部署范围;若需要在卡片3前先完成米家联调,工单 12 也可引用本安装清单。工单 09 骨架仍可先以“未配置 HA/HA 离线”模式验收。
