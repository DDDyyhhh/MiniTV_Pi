# 07 卡片2 电脑性能副屏方案

Type: grilling
Status: resolved
Blocked by: 02, 04

## Question

卡片2 = Linux 电脑 CPU/GPU 监控仪表盘 + 一键打开抖音/VSCode/B站。定下:

1. **Linux 性能采集**(被监控电脑是 Linux,显卡厂商未知):NVIDIA(`nvidia-smi`)/ AMD(sysfs + `amdgpu_top`)/ Intel(`intel_gpu_top`)/ 核显 四路分支怎么探;采集程序放哪(HA 侧执行 vs 电脑侧常驻小脚本)?
2. **数据进 HA 的方式**:Glances 集成 vs 自写 Python 脚本写 HA 的 REST API vs MQTT;要哪些指标(CPU 占用/温度/内存/GPU 占用/显存/频率?)与刷新周期。
3. **一键打开抖音/VSCode/B站**:HA `shell_command`(需 SSH add-on 或 HAOS 加载项)vs 电脑侧常驻代理(收 HA 命令后在本机启动应用);鉴权与误触防护;点击后的 UI 反馈(成功/失败)。
4. **仪表盘呈现**:CPU 仪表盘 + 显卡监控各自的 LVGL 形态(环形 gauge / 折线 / 数字卡);从 04 契约拉数的订阅/轮询方式。
5. **依赖**:02 决定 HA 装在哪,决定 shell_command 与采集部署的可行性;04 决定数据通道。

产出:采集方案 + 快捷控制链路 + 仪表盘规格(供 11 实现,含电脑侧部署清单)。

## Answer

### 结论

卡片2命名为 **PC Monitor Card**。被监控电脑与 HA Host 是同一台已核验的 Linux 主机:

- Ubuntu 22.04.5 LTS,GNOME,X11 桌面会话,用户 `dyh`。
- NVIDIA GeForce GTX 1660 SUPER,驱动 580.173.02,`nvidia-smi` 可用。
- `code`、`google-chrome`、`firefox` 均在 PATH。
- 图形会话环境已确认:`DISPLAY=:1`,`XAUTHORITY=/run/user/1000/gdm/Xauthority`,`DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus`。

因此首版不做 AMD/Intel GPU 分支,不依赖 HAOS Add-on/容器 shell,而是由当前图形用户的 **Python systemd --user agent** 在宿主机采集指标并执行受限 Launch Action。HA Container 只作 ESP32 与 agent 间的认证/状态桥接。

### 1. Metrics Agent

#### 技术选择

- 使用 Python 3 + `psutil` 采集 CPU、内存、磁盘和网络基本指标。
- 使用 NVIDIA NVML(Python binding 优先)采集 GPU 指标;若 NVML binding 不可用,回退为固定格式的 `nvidia-smi --query-gpu=... --format=csv,noheader,nounits`。
- 不使用 Glances:它能快速展示指标,但这里仍需实现图形桌面快捷启动通道;一套本机 agent 可以更小、更可控,避免额外 Glances API/容器网络边界。
- 不使用 MQTT 首版:项目当前 HA-ESP32 已定 REST-only,为单台电脑额外引入 broker、discovery 和 retained 状态管理没有收益。

#### 最小指标集

Metrics Agent 每 **5 秒**采样并发布以下 Confirmed Telemetry。卡片不可见时 ESP32 仍可按 10-60 秒拉 HA 数据,agent 保持 5 秒采样即可,不需要 ESP32 直接连 agent。

| HA 实体 | 值/单位 | 来源 | 卡片用途 |
| --- | --- | --- | --- |
| `sensor.pc_cpu_percent` | 0-100 `%` | `psutil.cpu_percent(interval=None)` | CPU 环形仪表 |
| `sensor.pc_cpu_temp_c` | 摄氏度或 `unknown` | `psutil.sensors_temperatures()` 可用时 | CPU 详情小字 |
| `sensor.pc_ram_percent` | 0-100 `%` | `psutil.virtual_memory()` | CPU 区内存小字 |
| `sensor.pc_ram_used_gib` | GiB | `psutil.virtual_memory()` | CPU 区内存小字 |
| `sensor.pc_gpu_util_percent` | 0-100 `%` | NVML / `nvidia-smi` | GPU 环形仪表 |
| `sensor.pc_gpu_temp_c` | 摄氏度 | NVML / `nvidia-smi` | GPU 详情小字 |
| `sensor.pc_gpu_vram_used_mib` | MiB | NVML / `nvidia-smi` | GPU 详情小字 |
| `sensor.pc_gpu_vram_total_mib` | MiB | NVML / `nvidia-smi` | GPU 详情小字 |
| `sensor.pc_gpu_power_w` | W 或 `unknown` | NVML / `nvidia-smi` | GPU 详情小字 |
| `binary_sensor.pc_agent_online` | `on` / `off` | agent heartbeat | 卡片可用性 |
| `sensor.pc_telemetry_updated` | ISO 时间 | agent heartbeat | 陈旧数据判断 |

- 值采集失败必须发布 `unknown` 和诊断属性,不得输出 0 伪装成低占用。
- 单个指标 `attributes` 至少包含 `unit_of_measurement`、`device_class`(适用时)、`friendly_name`、`captured_at`、`source`、`availability`。
- agent 启动后先采样一次再开始 5 秒周期;连续 3 次采样失败时将 `binary_sensor.pc_agent_online` 标记为 `off`。

#### 发布到 HA 的方式

- Metrics Agent 使用专用的“PC agent”HA 用户的 LLAT,通过 HA REST API `POST /api/states/<entity_id>` 上报**展示型** sensor 状态与 attributes。
- 这条 API 仅设置 HA 表示状态,不是物理控制通道,适合本机遥测。LLAT 只存在于 agent 的用户私密配置文件中,权限最小化,不得输出到日志。
- Agent 访问 `http://127.0.0.1:8123` 或 HA Host 的本机 endpoint;后续生产切换 HTTPS 后同样遵守 CA 校验。ESP32 仍只经 HA REST 读取单项 `sensor.*`,绝不连 agent。
- 由 `systemd --user` 启动,绑定 `graphical-session.target` 并随用户图形会话启动/停止;服务设置 `Restart=on-failure` 和合理退避。该选择确保 GUI 环境变量属于真实登录用户,不由 root 或 HA Container 猜测/伪造。

### 2. Launch Action 链路

#### 安全模型

- ESP32 和 HA 均不可传递 shell 字符串、任意 URL、文件路径或进程参数给 Linux。
- Command Agent 只接受三个固定 action ID:
  - `open_vscode`
  - `open_bilibili`
  - `open_douyin`
- 实现使用 `subprocess` 参数数组,不使用 `shell=True`;每个 action 映射到编译/配置内的固定可执行程序与固定 URL。
- 所有 action 使用 request ID 去重,避免 ESP32 重试或网络抖动造成重复开窗。

#### 选择的用户体验

- `open_vscode`:执行 `code --new-window`。用户选择“空白 VSCode”,不固定打开特定项目目录。
- `open_bilibili`:执行 `google-chrome https://www.bilibili.com`。
- `open_douyin`:执行 `google-chrome https://www.douyin.com`。
- 点击即启动,无需二次确认;UI 将该按钮置为 pending 并短暂禁用 **3 秒**。接到 agent 回执后显示 launched/failed;3 秒并不是成功判定,只是本地去抖窗口。
- `launched` 仅表示 agent 已成功请求启动该进程,不承诺浏览器页面已加载或窗口已前置。

#### HA 桥接契约

1. ESP32 通过通用的 `script.turn_on` 服务调用固定脚本实体,例如:

```http
POST /api/services/script/turn_on
Authorization: Bearer <esp32-llat>
Content-Type: application/json

{
  "entity_id": "script.pc_open_vscode",
  "variables": {"request_id": "<uuid>"}
}
```

同理使用 `script.pc_open_bilibili`、`script.pc_open_douyin` 实体。每个 HA script 固定发出一个 `pc_ui_action` event,数据只包含其固定的 allowlisted `action_id` 与服务传入的 `request_id`;HA script 不运行宿主 shell。

2. Command Agent 用 agent LLAT 连 HA WebSocket `/api/websocket`,认证后只订阅 `pc_ui_action`。尽管 ESP32 MVP 是 REST-only,这条 **宿主机 agent → HA** WebSocket 是独立链路,只为低延迟接收固定 event,不改变 ESP32 固件的 REST-only 决策。

3. Agent 校验 `action_id` 在 allowlist、`request_id` 格式与去重缓存后执行固定映射。执行结果通过 REST 更新:

- `sensor.pc_ui_action_status` 的状态=`launched`/`failed`/`rejected`
- attributes=`request_id`, `action_id`, `updated_at`, `detail`(不含 token/环境变量)

4. ESP32 的 `ha_io_task` 在服务调用后读取 `sensor.pc_ui_action_status`,以匹配的 `request_id` 为准。超时、request_id 不匹配或状态失败都显示失败,不重发 Launch Action。

### 3. PC Monitor Card 呈现

240x320 竖屏上采用两个紧凑性能区和一个快捷区,不引入复杂实时曲线:

1. **顶部状态栏(约 24-30px)**
   - 标题“电脑状态”、agent 在线/离线点、`pc_telemetry_updated` 相对时间。
   - agent 离线或样本超过 30 秒时,数值降灰并显示“电脑离线/数据陈旧”。

2. **双仪表区(约 130-145px)**
   - 左:CPU 环形 gauge,中心显示 CPU `%`,底部显示温度与 RAM `used/total`。
   - 右:GPU 环形 gauge,中心显示 GPU `%`,底部显示 GTX 1660 SUPER、温度和 VRAM `used/total`。
   - 环形 meter 不使用连续动画追逐每一个 5 秒样本;数值变化用 150-250ms 缓动,避免频繁重绘影响 tile 滑动。
   - CPU/GPU 温度缺失时显示 `--`,而不是 `0°C`。

3. **快捷区(约 90-105px)**
   - 三个等宽触摸目标:VSCode、B站、抖音;每个目标至少 44x44px。
   - 常态使用图标 + 短标签;pending 显示 spinner/减亮;launched 用短暂成功色;failed/rejected 用短暂错误色并恢复可点状态。
   - 不显示 agent 返回的未过滤错误文本。

### 4. ESP32 读取契约

- ESP32 `ha_io_task` 只读取白名单 `sensor.*` 与 `binary_sensor.pc_agent_online`,不拉取全量 HA states。
- 卡片可见时:每 **10 秒**依次读取 online、CPU、RAM、GPU 利用率、GPU 温度、显存和 action status。总请求保持串行,优先 action confirmation。
- 卡片不可见时:每 **60 秒**只读取 `binary_sensor.pc_agent_online` 与 `sensor.pc_telemetry_updated`。
- agent offline、HA offline、`unknown`、样本超过 30 秒都属于不同可见状态;UI 不把它们压成 0%。
- ESP32 操作成功的判定只能来自匹配 request ID 的 `pc_ui_action_status=launched`;HA script 服务返回成功仅代表 event 已受理。

### 5. 工单 11 电脑侧部署与真机验收

工单 11 必须完成以下部署/验收:

- [ ] 安装 agent 运行所需 Python 包:至少 `psutil`,NVIDIA binding 或 `nvidia-smi` 回退路径需要的依赖。
- [ ] 创建 agent 专用 HA 用户/LLAT、私有配置文件和 `systemd --user` service;token 权限和日志脱敏通过检查。
- [ ] Agent 在 GNOME/X11 图形会话登录后自动运行;登出/无图形会话时 action 明确失败,不尝试在错误用户/会话启动 GUI。
- [ ] HA 内创建三个固定 script 和 `pc_ui_action` event 桥接;agent 只处理允许的 action ID。
- [ ] 手动和 ESP32 触摸各执行一次 `open_vscode`、`open_bilibili`、`open_douyin`,Chrome/VSCode 启动且 `pc_ui_action_status` request ID 匹配。
- [ ] Agent 每 5 秒更新 NVIDIA GPU 和基础 CPU/RAM 指标;ESP32 在卡片可见时每 10 秒正确显示。
- [ ] 停止 agent、停止 HA Container、关闭图形会话分别验证:卡片显示 agent 离线、HA 离线或 action 失败,不显示旧数据为实时成功。
- [ ] 卡片2在 240x320 真机无文字/仪表/快捷区重叠,左右 tile 滑动不被快捷按钮误触打断。

