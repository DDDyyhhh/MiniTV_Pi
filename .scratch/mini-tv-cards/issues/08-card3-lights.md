# 08 卡片3 米家灯光拟态开关方案

Type: grilling
Status: resolved
Blocked by: 03, 04

## Question

卡片3 = 客厅/房间灯光与蓝牙 Mesh 开关的拟态开关面板。定下:

1. **实体清单**:从 03 确定的接入路径出发,列出要控制的灯/开关的 HA 实体类型(`light.*` / `switch.*`);按房间分组布局。
2. **拟态开关交互规范**:按下 → 加载态(转圈/灰)→ 成功/失败反馈;触感与颜色变化(Apple Fluent 风);「轻按开关」vs「长按/滑动调光」是否需要。
3. **控制与回读一致性**:调 HA 服务(`light.turn_on/off`、`switch.toggle`)后如何回读状态防 UI 漂移(轮询 vs 04 的订阅);弱网/超时下的状态显示策略。
4. **布局**:240x320 一张卡片能放几个开关;房间分区是否需要二级滚动。
5. **依赖**:03 定接入路径,04 定控制通道。

产出:开关面板规格 + 实体映射模板(供 12 实现,含 HA 侧配置清单)。

## Answer

### 结论

卡片3命名为 **Smart Home Card**。首版是一个面向高频灯光操作的 `2×2` Entity Tile 面板:

- 客厅两路:客厅主灯、客厅氛围灯/灯带。
- 卧室两路:卧室主灯、卧室氛围灯/床头灯。
- 每个 Tile 首版只支持**轻触明确开/关**,不做长按、亮度滑动、色温、颜色或二级滚动。
- ESP32 不直接控制米家、BLE Mesh 或网关;只控制配置白名单内的 HA `light.*` / `switch.*` 实体。
- HA Host/Container 关机时,整张卡片显示 Offline Backend State,不可向设备发控制请求。

实际房间设备与 `entity_id` 必须在 HA 安装、米家导入并实机验证后才绑定。不能因为设备在米家 App 可见就假定它在 HA 中是可控制 `light.*`/`switch.*`。

### 1. HA 侧接入与实体映射

#### 接入顺序

1. 对已确认支持的 Yeelight Wi-Fi 灯,先在灯具 App 开启 LAN Control,用 HA Yeelight 集成导入,优先作为首个本地验证负载。
2. 其他 Wi-Fi 米家设备先经小米官方 `xiaomi_home` 自定义集成导入,使用与米家 App 相同的区域。
3. 某个设备未暴露所需可写实体时,才针对该设备试 `xiaomi_miot` 回退;`xiaomi_miio` 仅用于明确支持的传统型号。
4. 蓝牙 Mesh 开关/中枢网关不预设本地控制能力。若 HA 只得到 `event.*`、`button.*` 或只读状态,不把它绑定为 Tile;改控制其实际驱动的灯/继电器,或在 HA 创建稳定的 Template light/switch 包装实体。
5. 每个候选实体都必须测过:HA UI 开/关、物理状态回读、HA 重启、断 WAN(如需本地能力声明)和实际房间对应关系。

#### Entity Mapping 模板

工单 12 完成 HA 导入后,必须建立一个可审查的配置表,ESP32 只接受该表中的实体。示例 entity ID 是命名目标,不是当前已存在的实体:

| Tile | Room Group | UI 别名 | 目标 domain | 推荐稳定 entity_id | 实际模型/协议 | 允许操作 | 验证结果 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 客厅 | 主灯 | `light` 优先,`switch` 可接受 | `light.living_room_main` | 待导入验证 | on/off | 待填 |
| 2 | 客厅 | 氛围灯 | `light` 优先 | `light.living_room_ambient` | 待导入验证 | on/off | 待填 |
| 3 | 卧室 | 主灯 | `light` 优先,`switch` 可接受 | `light.bedroom_main` | 待导入验证 | on/off | 待填 |
| 4 | 卧室 | 床头灯 | `light` 优先 | `light.bedroom_bedside` | 待导入验证 | on/off | 待填 |

每条映射的硬性要求:

- domain 必须是 `light.*` 或 `switch.*`;`event.*`、`button.*`、`sensor.*`、`binary_sensor.*` 不直接绑定。
- entity ID 固定为清晰、稳定、语义化的名称,导入后的随机或设备型号名称应在 HA UI 中改名或用 Template 实体包装。ESP32 使用 UI 别名,不显示 HA `friendly_name`。
- 记录底层设备精确 model、firmware、米家 region、连接协议、集成来源(Yeelight/`xiaomi_home`/`xiaomi_miot`/Matter/HomeKit)和已验证的本地/云状态。
- 同一物理负载只保留一个控制来源,不让 Yeelight、`xiaomi_home`、`xiaomi_miot` 同时控制。

### 2. Entity Tile 视觉与交互

#### 常态

- Tile 遵守深色 Frosted Panel 样式:`#1C1C1E` 底、1px 微亮边框、8-12px 圆角。
- 每个 Tile 显示房间名小标签、UI 别名和明确的开/关状态图标;名称超过一行时使用短别名/截断,不压缩触摸区域。
- `off`:暗色面板、次级文字、低亮图标。
- `on`:保持暗底,以 `#30D158` 开启态和较亮边框/图标表达,不能仅靠颜色区分,同时展示“开”。
- `unavailable` / `unknown`:灰色、禁用态和“不可用”提示;不能显示为关。
- `HA offline`:四个 Tile 统一降灰并禁用,顶部显示“后端离线”。

#### 触摸与 Pending State

1. 用户轻触一个处于 Confirmed State 的 Tile。
2. UI 根据**最后确认**的 `on/off` 计算目标,而不是使用盲 `toggle`:
   - `on` -> 调用 `light.turn_off` / `switch.turn_off`
   - `off` -> 调用 `light.turn_on` / `switch.turn_on`
3. Tile 立即显示目标方向的弱预览,叠加 spinner/pending,并禁用该 Tile。其他 Tile 继续可操作。
4. `ha_io_task` 只发送**一次**服务请求;未知网络结果、超时、TCP 断开都不重发服务,防止不确定状态下多次切灯。
5. 服务请求后立即读取 `GET /api/states/<entity_id>`;若未确认,在 1 秒、3 秒后最多额外读取两次。
6. 读取到目标状态,才写入 Confirmed State,结束 pending,用短暂成功反馈。
7. 三次读取不一致、实体变 `unavailable`、服务失败或 HA offline 时,恢复上一次 Confirmed State 的视觉,显示短暂失败反馈与可理解的“未确认/不可用/后端离线”状态。不得把请求成功或 HTTP 200 当物理灯已改变。
8. pending 最长 5 秒后必须退出。若状态仍未知,Tile 进入 stale/error,用户下次轻触前需先完成一次状态刷新。

### 3. HA / ESP32 REST 契约

ESP32 `ha_io_task` 对映射白名单串行读取,禁止 `/api/states` 全量查询:

```http
GET /api/states/light.living_room_main
Authorization: Bearer <esp32-llat>
```

控制例子:

```http
POST /api/services/light/turn_on
Authorization: Bearer <esp32-llat>
Content-Type: application/json

{"entity_id":"light.living_room_main"}
```

- `light.*` 使用 `light.turn_on` / `light.turn_off`;`switch.*` 使用 `switch.turn_on` / `switch.turn_off`。
- 首版不调用 `toggle`,不发送 brightness、transition、color 或 effect 参数。
- Smart Home Card 可见时:每 **10 秒**依次读取四个映射实体;不可见时:每 **60 秒**只做 HA 可达性探测,不需要持续读取每盏灯。
- 进入卡片、关闭 pending 后、HA 由离线恢复在线时,立即进行一次白名单刷新。
- ESP32 配网页的映射配置上限固定为 4 条,只允许经输入校验的 `light.` / `switch.` entity ID 与短 UI 别名;初版也可将映射编译进固件/配置文件,但不得允许任意服务/domain 输入。

### 4. 布局与手势

- 240x320 竖屏,顶部安全区和标题/HA 状态约 28-36px。
- 剩余主区为 `2×2` 网格:左右 8-12px 边距、列间距 8px、行间距 8px;每 Tile 宽约 100-108px,高约 105-115px,满足最小 44x44px 触摸目标。
- Tile 内部不嵌套卡片,不用二级滚动;首版四条映射固定显示。
- 页面横向滑动优先。Tile 轻触不应触发 `lv_tileview` 切页;横向拖动距离超过手势阈值时,Tile 取消点击。
- 顶部下拉保持 Control Center 的优先级;从 Tile 开始的下滑不触发亮度/调光,因为首版无此手势。

### 5. 后端离线与状态漂移处理

- HA Host 不是常开中枢:Linux 主机关机、HA Container 停止、认证失败、TLS 失败和网络不可达均进入 Offline Backend State,不发服务调用。
- UI 分别呈现 `HA offline`、`entity unavailable`、`entity unknown`、`stale/error` 和已确认 `on/off`,不能一律画成关闭。
- 实体从物理墙壁开关、米家 App 或自动化改变时,由于 ESP32 MVP 没有 WebSocket,卡片最迟在可见时的下一次 10 秒轮询更新;控制后仍以立即+1s+3s 的读回为准。
- HA 恢复或从其他终端改变状态后,以 HA 返回的 state 覆盖本地 Confirmed State,不保留旧的乐观开关视觉。

### 6. 字体与内容

加入 `font_cn_16` 子集的最低文案:

- 房间/开关:客厅 卧室 主灯 氛围灯 床头灯 开 关
- 状态:处理中 已开启 已关闭 未确认 不可用 后端离线
- 配置:待配置

若实际房间名称不同,优先选择短别名(2-4 个汉字),添加到子集后再绑定。

### 7. 工单 12 完成定义

工单 12 只有在以下条件都满足时才能关闭:

- [ ] HA Container 已部署且 ESP32 专用 LLAT、HA endpoint、四个 Entity Mapping 均配置完成。
- [ ] 四个 Tile 分别绑定客厅主灯、客厅氛围灯、卧室主灯、卧室床头灯或实际等价的四个可写 `light.*`/`switch.*` 实体;每个都有准确物理对应记录。
- [ ] 至少一个 Yeelight LAN 灯作为本地链路已验证;其余米家/Mesh 设备按实际型号、region、firmware、集成和实体能力记录,不夸大本地/离线控制能力。
- [ ] 四个实体在 HA UI、ESP32 Tile、物理设备三方的 on/off 状态一致;分别验证从 ESP32、HA UI、米家 App/物理开关改变状态后回读一致。
- [ ] 每个 Tile 验证 on->off、off->on、HA offline、entity unavailable、HA 重启恢复五种状态;未知结果不重复发服务。
- [ ] 触摸 Tile 显示 pending,在 5 秒内进入 confirmed 或错误态;不会重复切换或误触横向滑动。
- [ ] Smart Home Card 在 240x320 真机上四个 Tile 不重叠、不遮挡顶部下拉热区,且每个触摸目标不小于 44x44px。
- [ ] 电脑/HA Host 关机时卡片明确显示后端离线且所有 Tile 禁用;HA 恢复后可刷新状态并恢复控制。

