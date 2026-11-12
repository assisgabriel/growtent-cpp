# Software Requirements Specification (SRS) & Architecture Blueprint (v2.2)
**Project:** Grow Tent OS (`growtent-cpp`) — Autonomous Climate, Lighting, Power Auditor & Multi-Plant Irrigation Controller
**Target Standard:** ISO C++20 / C++23 (`-Wall -Wextra -Wpedantic -Wconversion`)
**UI Target:** Qt 6 + QML Commercial Embedded Dashboard (Setup Wizard + Live Operations Dashboard)

---

## 1. C vs. C++20 Reference Matrix (Core Rules)

| Concept | Embedded C Habit (Avoid) | Modern C++20 Pattern (Required) |
| :--- | :--- | :--- |
| Constants & Limits | `#define MAX_PLANTS 8` | `static constexpr std::size_t kMaxPlants = 8;` |
| Physical Units | `typedef double watts_t;` | `struct Watts { double value{}; auto operator<=>(const Watts&) const = default; };` + `280.0_W` |
| Struct Parameters | `void fn(const Config* cfg)` | `void fn(const Config& cfg) noexcept` |
| Sensor Faults | Return `-1.0f` or `int err` | `[[nodiscard]] std::optional<Kilopascal>` (`std::nullopt` on invalid reading) |
| Fixed Collections | `PlantZone p[8];` | `std::array<PlantZone, HardwareLimits::kMaxPlants>` + `std::span<const PlantZone>` |
| State Machines | `enum` + giant `switch` + static timers | `std::variant<StateA, StateB, ...>` + `std::visit` |
| Time & Debounce | `millis()` inside functions | Injected `std::chrono::steady_clock::time_point` + `std::chrono::minutes(3)` |

---

## 2. Stage 0: Configurable Setup & Hardware Limits (`TentSetupConfig`)

### 2.1 Static Hardware Capacity Limits (`struct HardwareLimits`)
- `kMaxInternalFans = 3` (On/Off clip fans: `0..3`)
- `kMaxAuxLights    = 2` (Auxiliary LED bars: `0..2`)
- `kMaxHeaters      = 2` (Ceramic heaters: `0..2`)
- `kMaxPlants       = 8` (Plant zones: `1..8`, each with `1 SoilMoisture` sensor + `1 WaterPump`)
- `Climate Sensors  = 2` (`1 Internal` Temp+RH pair, `1 External` Temp+RH pair)

### 2.2 Enclosure Volume & Dynamic Exhaust Purge Math
- **Enclosure Volume:**
  `V_tent_m3 = (width_cm * depth_cm * height_cm) / 1,000,000.0` (e.g., `60x60x180 cm` = `0.648 m³`).
- **Dynamic Exhaust Purge Duration (`t_purge_sec`):**
  Given Exhaust Airflow `Q_m3h` ($\text{m}^3/\text{h}$), Target Exchanges `N_exchanges` (default `5.0`), and Safety Margin Ratio `M_safety` (default `0.40` for carbon filter loss):
  `t_purge_sec = ceil( ((V_tent_m3 * N_exchanges) / Q_m3h) * 3600.0 * (1.0 + M_safety) )`
- **Intake Configuration:**
  `enum class IntakeMode { Passive, Active }`. If `Passive`, relay is disabled and theoretical power is `0.0_W`. If `Active`, On/Off actuator uses calibrated `intake_power` (e.g., `5.0_W`).

---

## 3. Core Subsystem Functional Rules

### 3.1 Subsystem A: Dual-Zone Psychrometric Engine (`VpdCalculator`)
Evaluates both **Internal** (`T_in`, `RH_in`) and **External** (`T_ext`, `RH_ext`) sensor pairs:
1. **Validation:** Reject `NaN`, `RH <= 0.0%`, `RH > 100.0%`, `T < -20.0 °C`, or `T > 70.0 °C` by returning `std::nullopt`.
2. **Saturation Vapor Pressure (SVP):**
   `SVP(T) = 0.61078 * exp((17.27 * T) / (T + 237.3))` [kPa]
3. **Global Tent VPD:**
   `VPD = max(0.0, SVP(T_air + dT_leaf) - SVP(T_air) * (RH / 100.0))` [kPa] (default `dT_leaf = 0.0_C`).
4. **Dew Point (`Ponto de Orvalho`):**
   `alpha = ((17.27 * T) / (237.7 + T)) + ln(RH / 100.0)`
   `T_dew = (237.7 * alpha) / (17.27 - alpha)` [°C]

### 3.2 Subsystem B: Main & Auxiliary Lighting Controller (`PhotoperiodScheduler`)
1. **Main Light:**
   - Modes: `Auto` vs. `Manual`.
   - Cycles (`enum class PhotoperiodCycle`): `Vegetative_18_6` (18h ON / 6h OFF), `Flowering_12_12` (12h ON / 12h OFF), `Autoflower_20_4` (20h ON / 4h OFF).
   - Configurable `lights_on_time` (`std::chrono::seconds` since midnight). Computes `t_off = (t_on + duration) % 24h` with full midnight wrap-around support.
2. **Auxiliary Lights (`0..2` units, `enum class AuxLightMode`):**
   - `FollowMainLight_660nm` (Deep Red 660nm): Matches Main Light state (`[t_on, t_off)`).
   - `SunsetLag15Min_730nm` (Far Red 730nm / Emerson Effect): Turns ON **15 minutes before** Main Light turns OFF (`t_off - 15min`) and stays ON until **15 minutes after** Main Light turns OFF (`t_off + 15min`), for a 30-minute dusk transition window.

### 3.3 Subsystem C: Smart Exhaust Controller (`ExhaustFsm`) — Default-ON with VPD Cycling
Modeled with `std::variant` and `std::visit`:
1. **`StateNormalContinuousOn` (Default / Stable):** Fan **ON (100%)**.
   - If `VPD_in > 1.7_kPa` for `3 min` AND `T_in < 28.0_C` -> transition to `StateDryRetentionOff`.
   - If `T_in >= 28.0_C` for `2 min` OR `VPD_in < 0.7_kPa` for `5 min` -> transition to `StateForcedPurgeOn`.
2. **`StateDryRetentionOff` (Moisture Retention - OFF Phase):** Fan **OFF**.
   - Thermal Override: If `T_in >= 28.0_C` for `2 min` -> immediately transition to `StateForcedPurgeOn` (ON).
   - Stabilization Exit: If `VPD_in <= 1.5_kPa` for `3 min` -> immediately exit cycle to `StateNormalContinuousOn` (ON).
   - Periodic O2 Renewal: After `15 min` OFF -> transition to `StateDryPeriodicRefreshOn` (ON).
3. **`StateDryPeriodicRefreshOn` (O2 Renewal - Dynamic ON Phase):** Fan **ON** for `t_purge_sec`.
   - When `t_purge_sec` elapses: if `VPD_in > 1.5_kPa` and `T_in < 28.0_C`, return to `StateDryRetentionOff`; otherwise return to `StateNormalContinuousOn`.
4. **`StateForcedPurgeOn` (Thermal / Low-VPD Emergency):** Fan forced **ON**.
   - Returns to `StateNormalContinuousOn` when `T_in <= 27.0_C` AND `VPD_in >= 0.8_kPa`.

### 3.4 Subsystem D: Nighttime Thermal Controller (`ThermalController`)
1. **Hard Interlock:** If `main_light_on == true`, all configured heaters (`0..2`) are forced **OFF** immediately.
2. **Cold-Front Setpoint Optimization:**
   - If `T_ext >= 21.0_C` -> `T_target = 23.5_C`.
   - If `T_ext < 21.0_C` -> `T_target = 22.0_C`.
3. **Hysteresis & Cycle Protection:**
   - Turns ON when `T_in <= T_target - 0.5_C`; turns OFF when `T_in >= T_target + 0.5_C`.
   - Enforces `min_cycle_duration = 3 min` and `cooldown = 3 min`.

### 3.5 Subsystem E: Multi-Plant Irrigation Controller (`IrrigationController`)
Independent state machine per configured plant (`1..8`):
1. **Control Mode (`PumpControlMode`):** `Manual` vs. `Auto`.
2. **Auto Trigger (`AutoTriggerType`):**
   - `BySoilMoisture`: Triggers when `soil_moisture < threshold_min` for `3 min`.
   - `ByTimeInterval`: Triggers periodically every `K` hours/days (e.g., every `72h` / 3 days).
3. **Dosing Calculation (`WaterDosingMode`):**
   - `ByVolumeLiters`: Given target volume `V_target` (`Liters`) and pump flow rate `Q_pump` (`LitersPerMinute`), computes `t_total_sec = ceil((V_target / Q_pump) * 60.0)`.
   - `ByDurationSeconds`: Uses configured `t_total_sec` directly.
4. **Delivery Strategy (`DeliveryStrategy`):**
   - `Continuous`: Runs pump ON continuously for `t_total_sec`, then turns OFF.
   - `Pulsed`: Alternates `t_pulse_on_sec` (ON) and `t_pulse_soak_sec` (OFF), accumulating active ON time across pulses until `accumulated_on_sec == t_total_sec`.

### 3.6 Subsystem F: Electrical Load Diagnostics & Alert Engine (`PowerAuditor` & `AlertManager`)
- **Theoretical Power Sum:**
  `P_theo = P_base + P_MainLight + sum(P_Aux) + (MainLight OFF ? sum(P_Heater) : 0) + P_Exhaust + P_Intake + sum(P_ClipFan) + sum(P_PlantPump)`
- **Fault Rules:**
  1. `InterlockViolationMainLightAndHeaterOn` (`0s` immediate).
  2. Lighting 4-Way Audit (`5 min` debounce, `>= 220W` physical threshold): Physical fail ON, Physical stuck relay OFF, Software timer missed ON/OFF (auto-corrects).
  3. Exhaust Audit (`3 min` debounce, `+/- 35.0_W` tolerance).
  4. Heater / General Audit (`3 min` debounce, `+/- 50.0_W` tolerance).
  5. Climate Alerts: High VPD (`> 1.6_kPa` for `10 min`), Low VPD (`< 0.4_kPa` for `10 min`), Foliar Stress (`> 1.7_kPa` for `120 min`), Dew Point proximity (`T_in - T_dew <= 2.0_C` for `5 min`).

---

## 4. Milestone Implementation & Pull Request Plan

### Milestone 1 (PR #1 & PR #2): Foundation, Configurable Setup, VPD & Power Auditor
- **PR #1 (`feat/m1-strong-types-and-setup`):**
  1. `core/include/domain/Types.hpp`: Strong units (`Celsius`, `RelativeHumidity`, `SoilMoisture`, `Kilopascal`, `Watts`, `CubicMeters`, `CubicMetersPerHour`, `Liters`, `LitersPerMinute`), `PlantId`, literals, `ClimateSensorReading`, `HardwareLimits`.
  2. `core/include/domain/TentSetupConfig.hpp`: `IntakeMode`, `AuxLightMode`, `TentDimensionsCm::volumeM3()`, `ExhaustSetup::calculatePurgeDuration()`, `TentSetupConfig::isValid()`.
  3. Initial unit tests in `tests/test_milestone1.cpp`.
- **PR #2 (`feat/m1-vpd-and-power-auditor`):**
  1. `core/include/services/VpdCalculator.hpp`: `calculateSvpKpa`, `calculateVpd`, `calculateDewPoint`, `evaluateSensorPair`.
  2. `core/include/services/PowerAuditor.hpp`: `PowerFaultType`, `SystemActuatorSnapshot`, `computeTheoreticalPower`, `evaluate` (3-min debounce).
  3. Complete 6-test suite in `tests/test_milestone1.cpp`.

### Milestone 2 (PR #3 & PR #4): Deterministic State Machines & Controllers
- **PR #3 (`feat/m2-photoperiod-and-exhaust`):** `PhotoperiodScheduler.hpp` + `ExhaustFsm.hpp` + tests.
- **PR #4 (`feat/m2-thermal-and-irrigation`):** `ThermalController.hpp` + `IrrigationController.hpp` + `AlertManager.hpp` + tests.

### Milestone 3 (PR #5): HAL, Simulator, RingBuffer & Concurrency Engine
- **PR #5 (`feat/m3-hal-and-concurrency`):** `IHardwarePort.hpp`, `MockHardwareDriver.hpp`, `RingBuffer.hpp`, `ControlLoopEngine.hpp` (`std::jthread`).

### Milestone 4 (PR #6): Qt 6 + QML Setup Wizard, Dashboard & HA Shadow Mode
- **PR #6 (`feat/m4-qt6-dashboard-and-shadow`):** `HaShadowDriver.hpp`, `DashboardBridge.hpp`, `SetupWizardPage.qml`, `MainDashboard.qml`.

### Milestone 5 (PR #7): Native ESP32 Firmware & Production Cutover
- Direct compilation of `core/` on ESP32 with 2-minute autonomous hardware heartbeat fail-safe.
