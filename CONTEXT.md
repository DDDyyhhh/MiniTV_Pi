# ESP32-C3 Mini TV

This context defines the user-facing vocabulary for the Mini TV firmware effort, so planning tickets and implementation code refer to the same concepts.

## Language

**Card**:
One of the three full-screen horizontal pages in the Mini TV interface: time/weather/calendar, PC monitor, or smart-home controls.
_Avoid_: Page, screen, tile

**Control Center**:
The top-pulled overlay used for global controls such as backlight brightness and Wi-Fi status/settings.
_Avoid_: Menu, settings drawer, notification shade

**Fluent Card Style**:
The project's dark, Apple-inspired visual language: calm layered panels, subtle borders, restrained translucency, and fluid motion adapted to the 240x320 display.
_Avoid_: Glassmorphism, realtime blur, neumorphism

**Frosted Panel**:
A translucent-looking panel that suggests frosted glass through flat colors, opacity, and borders rather than realtime Gaussian blur.
_Avoid_: Blur layer, glass shader

**Entity Tile**:
A touch target on the smart-home card that represents one Home Assistant controllable entity such as a light or switch.
_Avoid_: Button, device card, switch widget

**Pending State**:
The temporary UI state after the user sends a control action and before Home Assistant confirms the resulting entity state.
_Avoid_: Loading, optimistic success

**HA Host**:
The Linux computer that runs Home Assistant for this project while it is powered on and reachable on the local network.
_Avoid_: Always-on hub, cloud server

**Control Backend**:
Home Assistant plus its local helper services that expose weather, PC metrics, app launch actions, and smart-home entities to the Mini TV.
_Avoid_: API server, Mi Home bridge

**Linux Target PC**:
The same Linux computer whose CPU/GPU state and app-launch actions are shown on the PC monitor card.
_Avoid_: Desktop, workstation, HA server

**Skeleton Milestone**:
The first firmware milestone where the Mini TV can boot, display the card shell, accept touch gestures, provision Wi-Fi, keep time, and show backend availability before any full card feature is implemented.
_Avoid_: MVP, demo firmware, driver test

**Provisioning Portal**:
The phone-accessible setup page exposed by the Mini TV when Wi-Fi or Home Assistant connection details need to be configured.
_Avoid_: Captive app, settings website

**Offline Backend State**:
The visible state used when the Mini TV is working locally but the Control Backend cannot be reached.
_Avoid_: Error, disconnected mode

**Time Card**:
The first Card, combining the flip clock, weather trend, calendar, and holiday countdown.
_Avoid_: Clock page, weather page

**Flip Clock**:
The large time display whose digits change with a short flip-like animation.
_Avoid_: Digital clock, clock widget

**Weather Trend**:
The compact 24-hour temperature line shown on the Time Card.
_Avoid_: Weather graph, forecast chart

**Holiday Countdown**:
The Time Card element that shows the next configured holiday and days remaining based on the local holiday table.
_Avoid_: Festival API, calendar reminder

**Stale Data**:
Previously confirmed data that remains displayable after refresh fails, with an explicit age or offline marker.
_Avoid_: Cached success, last value

**PC Monitor Card**:
The second Card, showing the Linux Target PC's confirmed performance telemetry and fixed desktop launch actions.
_Avoid_: Dashboard, PC page

**Metrics Agent**:
The user-session service on the Linux Target PC that collects local system telemetry and publishes it to the Control Backend.
_Avoid_: HA integration, polling script

**Command Agent**:
The user-session service on the Linux Target PC that accepts only fixed launch actions from the Control Backend and starts them in the logged-in graphical session.
_Avoid_: Remote shell, command runner

**Launch Action**:
One named, allowlisted desktop action exposed to the PC Monitor Card, such as opening VSCode, Bilibili, or Douyin.
_Avoid_: Shell command, user-provided URL

**Confirmed Telemetry**:
The most recently published PC metric sample with its capture time and availability state.
_Avoid_: Live value, current value

**Smart Home Card**:
The third Card, providing direct local-network control of a small, configured set of Home Assistant lights or switches.
_Avoid_: Mi Home page, home dashboard

**Room Group**:
A named, user-facing area that owns one or more configured Entity Tiles, such as the living room or bedroom.
_Avoid_: Area, zone, floor

**Entity Mapping**:
The explicit configuration that binds a named Entity Tile to one writable Home Assistant `light.*` or `switch.*` entity.
_Avoid_: Automatic discovery, raw device name

**Confirmed State**:
The last Home Assistant state read after a control action, used as the source of truth for an Entity Tile's displayed on/off state.
_Avoid_: Optimistic state, requested state
