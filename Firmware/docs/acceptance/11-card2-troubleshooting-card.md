# 技术排坑卡片：ESP32-C3 PC Monitor Card 与 Linux Target PC Agent

## 适用场景

ESP32-C3 通过 Home Assistant（HA）读取 Linux Target PC 的 Confirmed Telemetry，并触发固定的桌面 Launch Actions。链路包含：ESP32 固件、Wi-Fi/HTTP、HA 脚本与事件、Linux 用户会话、Metrics Agent、Command Agent 和 LVGL 触摸界面。

## 一、问题根因

### 1. 网络栈初始化时序错误会直接触发 lwIP 断言

ESP-IDF 的 HTTP 客户端依赖已初始化的 `esp_netif`、默认事件循环和 lwIP TCP/IP mailbox。若业务任务在 `network_manager_init()` 之前启动并发起 HTTP 请求，TCP/IP 栈的同步 mailbox 尚未建立，调用会落入 `tcpip_send_msg_wait_sem` 的无效 mailbox 路径，表现为：

```text
assert failed: tcpip_send_msg_wait_sem ... (Invalid mbox)
```

本质不是天气、HA 或令牌问题，而是“网络业务先于网络栈就绪”违反了初始化前置条件。

### 2. 通过 Wi-Fi 获得 IP 之前不能把 HA 请求当作可用

STA 关联、DHCP 获取地址和应用层 HA 可达性是三个不同阶段。没有 `IP_EVENT_STA_GOT_IP` 时，设备没有可用的本地 IP 路由；此时启动天气、PC 遥测或动作请求只会制造超时/失败，不能证明后端契约有问题。

### 3. GUI 启动依赖真实用户会话，不是普通后台进程环境

`systemd --user` 服务可能运行，但 `DISPLAY`、`XAUTHORITY` 或用户 D-Bus 会话缺失时，`code`/Chrome 无法可靠连接当前 X11 图形会话。以 root、HA Container 或错误用户启动 GUI 会把“进程已创建”误判为“桌面动作成功”。

### 4. HA 服务调用成功不等于桌面动作成功

`POST /api/services/script/turn_on` 的成功只表示 HA 接受并执行了脚本；脚本发出 event 也只表示事件已发布。真正的结果发生在 Linux Command Agent：它还必须通过 allowlist、会话检查，并成功调用固定程序。因而必须使用 request ID 将请求、事件和结果关联起来。

### 5. 遥测值、可用性和新鲜度是不同维度

CPU/GPU 数值缺失、Agent 离线、HA 不可达和已有样本超过 30 秒不是同一种状态。把失败值写成 `0` 会伪装成低负载；保留旧值却不标 age 会伪装成实时数据。

### 6. ESP32-C3 的显示性能受 SPI、DMA 和网络负载共同影响

LVGL 绘制、ST7789 SPI 传输和 SoftAP/HTTP 负载共享有限的 MCU 资源。提高 SPI 频率不必然提高端到端体验；在本硬件和工作负载下，20 MHz 会触发 LVGL Task WDT，而 10 MHz 配合局部 DMA 双缓冲可稳定运行。问题是系统级时序/资源竞争，不是单个控件绘制错误。

### 7. 横向 Card 手势与按钮点击共享同一触摸输入流

PC Monitor Card 的快捷按钮若允许在 Pending State 期间继续接收点击，或让按钮/子对象吞掉横向拖动，用户一次滑动可能产生误触发或重复 Launch Action。必须由容器手势优先级、点击阈值和动作状态共同约束。

## 二、最终生效的解决方案

### 1. 固定初始化与启动门槛

主流程先初始化显示、触摸、UI 和网络管理器；业务网络任务只在 `IP_EVENT_STA_GOT_IP` 中启动：

```c
static void ip_event_handler(void *argument, esp_event_base_t event_base,
                             int32_t event_id, void *event_data)
{
    if (event_id != IP_EVENT_STA_GOT_IP) {
        return;
    }

    time_service_start();
    backend_probe_request_now();
    card1_start();
    pc_monitor_start();
    ui_runtime_request_refresh();
}
```

`main.c` 中先调用：

```c
ESP_ERROR_CHECK(network_manager_init());
```

再根据已保存配置调用：

```c
ESP_ERROR_CHECK(network_manager_connect(&config));
```

业务任务不在 UI 创建阶段执行 HTTP。

### 2. 固定 HA REST 白名单与串行读取

ESP32 只读取明确列出的实体，例如：

- `binary_sensor.pc_agent_online`
- `sensor.pc_telemetry_updated`
- `sensor.pc_cpu_percent`
- `sensor.pc_cpu_temp_c`
- `sensor.pc_ram_percent`
- `sensor.pc_ram_used_gib`
- `sensor.pc_gpu_util_percent`
- `sensor.pc_gpu_temp_c`
- `sensor.pc_gpu_vram_used_mib`
- `sensor.pc_gpu_name`
- `sensor.pc_ui_action_status`

PC Monitor Card 可见时每 10 秒刷新；不可见时只保留低频在线/时间戳刷新。HTTP 响应使用固定上限缓冲区，解析失败、`unknown`、`unavailable` 或越界数值都标记为不可用，不转换成零值。

### 3. Linux Agent 使用固定采集和固定程序映射

最终运行依赖：

```text
psutil==5.9.0
websockets==15.0.1
```

GPU 使用固定格式的回退命令：

```bash
nvidia-smi \
  --query-gpu=name,utilization.gpu,temperature.gpu,memory.used,memory.total,power.draw \
  --format=csv,noheader,nounits
```

Agent 每 5 秒发布一次遥测；数值不可用时发布 `unknown`，并携带 `captured_at`、`source`、`availability` 和单位/总量属性。连续三次采样失败后将 `binary_sensor.pc_agent_online` 标记为 `off`。

桌面动作只允许以下三项固定映射：

```python
ALLOWED_ACTIONS = {
    "open_vscode": ("code", "--new-window"),
    "open_bilibili": ("google-chrome", "https://www.bilibili.com"),
    "open_douyin": ("google-chrome", "https://www.douyin.com"),
}
```

执行时使用参数数组和 `subprocess.Popen(..., shell=False)` 的默认行为，不接受 shell 字符串、任意 URL、路径或参数。

### 4. 使用 `systemd --user` 绑定图形会话

最终服务配置：

```ini
[Unit]
Description=Mini TV PC Metrics and Command Agent
After=graphical-session.target
PartOf=graphical-session.target

[Service]
Type=simple
EnvironmentFile=%h/.config/minitv-pc-agent/environment
Environment=DISPLAY=:1
Environment=XAUTHORITY=%t/gdm/Xauthority
Environment=DBUS_SESSION_BUS_ADDRESS=unix:path=%t/bus
ExecStart=%h/.local/share/minitv-pc-agent/venv/bin/python %h/.local/share/minitv-pc-agent/minitv_pc_agent.py
Restart=on-failure
RestartSec=5

[Install]
WantedBy=graphical-session.target
```

LLAT 只放在：

```text
~/.config/minitv-pc-agent/environment
```

目录权限为 `0700`，文件权限为 `0600`。服务文件、代码和日志不包含 token。

Command Agent 在执行前检查：

- `DISPLAY` 以 `:` 开头；
- D-Bus 地址以 `unix:path=` 开头；
- `XAUTHORITY` 文件存在且可读。

任一条件不满足，返回 `failed / Graphical session unavailable`，不尝试猜测其他用户或会话。

### 5. HA 使用固定脚本做事件桥接

三个脚本只发布固定 event，不运行宿主机 shell：

```yaml
pc_open_vscode:
  mode: single
  sequence:
    - event: pc_ui_action
      event_data:
        action_id: open_vscode
        request_id: "{{ request_id }}"

pc_open_bilibili:
  mode: single
  sequence:
    - event: pc_ui_action
      event_data:
        action_id: open_bilibili
        request_id: "{{ request_id }}"

pc_open_douyin:
  mode: single
  sequence:
    - event: pc_ui_action
      event_data:
        action_id: open_douyin
        request_id: "{{ request_id }}"
```

完整配置以 `host-agent/home-assistant-scripts.yaml` 为准。Command Agent 连接 HA WebSocket `/api/websocket`，只订阅 `pc_ui_action`，校验 UUID、检查 allowlist，并用有界去重缓存避免重复开窗。

### 6. Launch Action 采用“发一次、按 ID 确认、不重放”

ESP32 点击后：

1. 生成 UUID request ID；
2. 进入 Pending State，短暂禁用快捷按钮；
3. 通过 `script.turn_on` 调用对应固定脚本，只发送一次；
4. 轮询 `sensor.pc_ui_action_status`；
5. 只有 `request_id` 完全匹配且状态为 `launched` 才进入成功态；
6. `failed`、`rejected`、超时或 ID 不匹配均进入失败态，不自动重发。

HA 服务 HTTP 成功不作为最终成功条件。

### 7. UI 与 LVGL 采用单任务刷新和局部 DMA

网络任务只更新受 mutex 保护的状态快照，再调用 `ui_runtime_request_refresh()`；LVGL 对象只由 UI task 操作。显示配置固定为：

```c
#define LCD_SPI_CLOCK_HZ (10 * 1000 * 1000)
#define BOARD_LCD_DMA_LINES 40
```

320×240 横屏渲染使用 ST7789 轴交换和 X 镜像；触摸坐标同步转换。PC Monitor Card 使用两个紧凑 gauge、底部三个快捷触摸目标，按钮宽高为 `94×48`，横向 Card 切换由 `lv_tileview` 处理。

## 三、最终验证命令与结果

### 固件构建

```bash
source /home/dyh/.espressif/v6.0.2/esp-idf/export.sh
idf.py build
```

最终结果：

```text
Project build complete.
Firmware.bin binary size 0x15a370 bytes.
Smallest app partition is 0x3f0000 bytes.
```

### 固件烧录

```bash
idf.py -p /dev/ttyACM0 flash
```

最终结果包含：

```text
Hash of data verified.
Hard resetting via RTS pin...
Done
```

### Agent 与 HA 检查

```bash
/home/dyh/.local/share/minitv-pc-agent/venv/bin/python -m py_compile host-agent/minitv_pc_agent.py
bash -n host-agent/provision-pc-agent.sh
systemctl --user is-enabled minitv-pc-agent.service
systemctl --user is-active minitv-pc-agent.service
docker inspect --format '{{.State.Running}}' homeassistant
```

最终结果：代理 `enabled`、`active`，HA Container 运行中；`psutil`、`websockets` 可导入，`nvidia-smi` 返回有效 NVIDIA 指标。

### 真实链路结果

- Agent 每 5 秒更新时间戳。
- 三个固定 Launch Actions 均收到匹配 request ID 的 `launched` 回执。
- 非法 action 返回 `rejected / Action is not allowlisted`。
- 无图形会话返回 `failed / Graphical session unavailable`。
- 停止 Agent 后 online 状态为 `off` / `unavailable`；恢复后回到 `on` / `available`。
- ESP32 真机日志确认 Wi-Fi 获取 IP、HA `http_status=200`、SNTP 同步、FPS=50；连续运行未出现 WDT、abort、assertion 或 flush 错误。

## 四、避坑指南

### 架构与时序

1. 把 `IP_EVENT_STA_GOT_IP` 作为所有网络业务的硬门槛；不要以 `esp_wifi_start()`、STA 已关联或配置已加载代替“可发 TCP 请求”。
2. 初始化阶段只创建对象和任务；网络任务第一次执行也要检查配置和网络可用性。
3. HTTP、JSON 和重试放在独立 FreeRTOS 任务，不放进 LVGL 回调、触摸回调、Wi-Fi 事件回调或绘制回调。
4. 对每条跨任务状态建立明确快照和 mutex；外部任务通知 UI，不直接修改 LVGL 对象。

### 数据契约

5. 先固定实体白名单、单位、时间戳和可用性语义，再写 UI；不要先画界面再猜 HA 状态。
6. `unknown`、`unavailable`、HA 不可达、Agent offline 和 Stale Data 必须有独立状态；缺失数据不能写成 0。
7. 所有控制请求都带 request ID；服务受理、事件发布、执行成功和 UI 成功是四个不同阶段。
8. 未匹配 request ID 的旧回执必须忽略；网络超时不自动重放非幂等 Launch Action。

### 主机与安全

9. GUI 动作必须运行在真实登录用户的 `systemd --user` 图形会话；显式检查 `DISPLAY`、`XAUTHORITY` 和 D-Bus。
10. 只使用固定参数数组；禁止 `shell=True`、任意 URL、任意路径、任意命令和把 HA 输入直接拼成 shell。
11. LLAT 使用专用 HA 用户和私有 `0600` 配置文件；日志、验收记录和服务单元不得出现 token。
12. Agent、HA 和 ESP32 使用各自职责边界：Agent 发布展示状态，HA 脚本转发固定事件，ESP32 只读白名单实体并确认结果。

### 嵌入式性能与触摸

13. ESP32-C3 无 PSRAM 时优先局部 DMA 双缓冲，避免双全屏 framebuffer；先测最低 heap 和最大连续块，再调整 buffer。
14. SPI 频率必须在真实网络负载、SoftAP 和连续滑动下验收；不要只用静态画面测试高频率。
15. 每个快捷目标至少满足约 `44×44` 触摸面积；Pending State 时禁用重复点击。
16. 保持横向 tile 手势优先级；按钮只处理明确点击，横向拖动超过阈值时取消点击。
17. 对真实屏幕做布局验收，不以编译成功或模拟器截图代替 240×320 真机观察。

### 验证顺序

18. 先验证依赖和单个边界：`nvidia-smi`、HA `/api/`、WebSocket 鉴权、X11 会话，再验证完整链路。
19. 先做手动固定脚本回执，再做 ESP32 触摸；每一步都记录 request ID 和最终状态。
20. 必须分别停止 Agent、停止 HA、结束图形会话，确认 UI 显示的是明确失败/离线状态，而不是保留旧数据冒充成功。
21. 固件每次改动后都执行“构建 → 烧录校验 → 启动日志 → 稳定运行 → 真机操作”闭环；不要把旧镜像的串口日志当成新代码证据。
