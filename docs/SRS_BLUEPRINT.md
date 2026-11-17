# Software Requirements Specification (SRS) & Architecture Blueprint (v2.3)


**Project:** Grow Tent OS (`growtent-cpp`) — Autonomous Climate, Lighting, Auto-Calibrated Power Auditor & Multi-Plant Irrigation Controller
**Target Standard:** ISO C++20 / C++23 (`-std=c++20`, `-Wall -Wextra -Wpedantic -Wconversion`, Doxygen Documented)
**UI Target:** Qt 6 + QML Commercial Embedded Dashboard (Setup & Auto-Calibration Wizard + Live Operations Dashboard)
**Architecture:** Hexagonal (Ports & Adapters) with 100% Pure Standard C++20 Core (`Zero Heap Allocation`)


---


## 1. Non-Negotiable C++20 & Documentation Standards


| Engineering Rule | Embedded C Habit (Forbidden) | Modern C++20 Standard (Required) |
| :--- | :--- | :--- |
| **API Documentation** | Undocumented headers or ad-hoc comments | Full Doxygen tags (`@file`, `@brief`, `@struct`, `@enum`, `@param`, `@return`, `///<`) |
| **Constants & Limits** | `#define MAX_PLANTS 8` | `static constexpr std::size_t kMaxPlants = 8;` |
| **Physical Units** | `typedef double watts_t;` | Zero-cost `struct` wrappers with `operator<=>`, `+`, `-`, `+=`, `-=` and literals (`23.5_C`, `280.0_W`) |
| **Identifiers** | Raw `int plant_id` | `struct PlantId { std::uint16_t value{0}; }` with `operator<=>` ONLY (`0` = unconfigured; `1..8` = valid) |
| **Memory in `core/`** | `malloc` / `free` / raw `T*` arrays | Stack-allocated `std::array<T, MaxN>` + non-owning `std::span<const T>` views |
| **Sensor Faults** | Magic `-1.0f` return values or output pointers | `[[nodiscard]] std::optional<T>` (`std::nullopt` on invalid/disconnected sensor) |
| **State Machines** | `enum` + giant `switch` + static timers | `std::variant<StateA, StateB, ...>` + `std::visit` |
| **Time & Debounce** | Direct `millis()` calls inside logic | Injected `std::chrono::steady_clock::time_point` + `std::chrono` literals (`3min`, `5s`) |


---


## 2. Stage 0 & Stage 0.5: Configurable Setup (`TentSetupConfig`) & Auto-Calibration (`PowerCalibrator`)


### 2.1 Static Hardware Capacity Bounds (`struct HardwareLimits`)
- `kMaxInternalFans = 3` (On/Off internal circulation clip fans: `0..3`)
- `kMaxAuxLights    = 2` (Auxiliary spectral LED bars: `0..2`)
- `kMaxHeaters      = 2` (Nighttime ceramic heaters: `0..2`)
- `kMaxPlants       = 8` (Independent plant zones: `1..8`, each with `1 SoilMoisture` sensor + `1 WaterPump`)
- `Climate Sensors  = 2` (`1 Internal` Temp+RH sensor pair, `1 External` Temp+RH sensor pair)


### 2.2 Stage 0: User Topology Provisioning (`TentSetupConfig.hpp`)
In the Setup Wizard, the user configures **only physical geometry, topology, and flow rates**—never manual wattage guesses:
1. **Enclosure Geometry (`TentDimensionsCm`):** `width_cm`, `depth_cm`, `height_cm` -> computes `volumeM3() = (W * D * H) / 1,000,000.0` (e.g., `60x60x180 cm` = `0.648 m3`).
2. **Exhaust Airflow (`ExhaustSetup`):** `airflow_m3h` (e.g., `180.0 m3/h`), `target_air_exchanges` (`5.0`), and `safety_margin_ratio` (`0.40` carbon filter loss). Dynamically computes purge duration:
   `t_purge_sec = ceil( ((V_tent_m3 * N_exchanges) / Q_m3h) * 3600.0 * (1.0 + safety_margin_ratio) )`
3. **Actuator Inventory:** `intake_mode` (`Passive` vs `Active`), `has_main_light` (`bool`), `num_aux_lights` (`0..2` with `FollowMainLight_660nm` or `SunsetLag15Min_730nm`), `num_heaters` (`0..2`), `num_internal_fans` (`0..3`), and `num_plants` (`1..8` with `PlantId{1..8}` and `pump_flow_rate` in `L/min`).
4. **Initial Uncalibrated State:** All `Watts` fields initialize to **`0.0_W`** and **`is_power_calibrated = false`**.
5. **Topology Validation (`isValid()`):** Ensures positive volume/airflow, hardware counts within `HardwareLimits`, `1 <= num_plants <= 8`, and `plants[i].id.value >= 1` for all active plants (`0 .. num_plants - 1`).


### 2.3 Stage 0.5: Automated Step-Response Power Calibrator (`PowerCalibrator.hpp`)
Eliminates code edits or manual wattmeter math whenever equipment is installed or swapped (e.g., upgrading a `280W` LED to `480W`):
1. **Queue Construction (`startSession`):** Builds a static step queue (`std::array<CalibrationTarget, 19>`) starting at `LoadKind::IdleBaseline` (all relays `OFF`), followed sequentially by every enabled actuator in `TentSetupConfig` (`include_pumps = false` by default to prevent dry-running empty water pumps).
2. **Isolated Relay Actuation (`currentStepActuatorCommand`):** Emits a `SystemActuatorSnapshot` energizing **at most ONE actuator** at a time (guaranteeing zero cross-load interference and respecting the Light/Heater interlock).
3. **Inrush Settling & Commissioning Check (`recordStepSamples`):**
   - After waiting `kInrushSettleDelay` (`5s`) to bypass LED driver / motor startup transients, averages stabilized PZEM-004T readings (`std::span<const Watts>`).
   - Step 0 records `cfg.base_idle_power = avg_idle`.
   - Subsequent steps compute net power `P_load = avg_step - avg_idle`. If `P_load < 2.0_W` (`kMinDetectableLoad`), aborts with `CalibrationStatus::FailedNoPowerDetected` (identifying unplugged or dead hardware during commissioning).
   - Upon finishing the final step, sets **`cfg.is_power_calibrated = true`** and returns `CalibrationStatus::CompletedOk`.


---


## 3. Core Subsystem Behavioral Specifications


### 3.1 Subsystem A: Dual-Zone Psychrometric Engine (`VpdCalculator.hpp`)
Processes both **Internal** and **External** `ClimateSensorReading` pairs identically:
- **Sanity Guard:** Rejects `NaN`, `RH <= 0.0%`, `RH > 100.0%`, `T < -20.0 C`, or `T > 70.0 C` by returning `std::nullopt`.
- **Magnus-Tetens SVP:** `SVP(T) = 0.61078 * exp((17.27 * T) / (T + 237.3))` [kPa].
- **Air / Leaf VPD:** `VPD = max(0.0, SVP(T_air + dT_leaf) - SVP(T_air) * (RH / 100.0))` [kPa] (`dT_leaf` defaults to `0.0_C`).
- **Dew Point:** `alpha = ((17.27 * T) / (237.7 + T)) + ln(RH / 100.0)`, `T_dew = (237.7 * alpha) / (17.27 - alpha)` [C].


### 3.2 Subsystem B:Adaptive Load-Isolating Power Auditor (`PowerAuditor.hpp`)
- **3D Diagnostic Report (`PowerAuditReport`):** Reports `{theoretical_power, measured_power, delta_watts, PowerFaultCategory, LoadKind suspected_load, std::size_t suspected_index}`.
- **Rule 1 — Immediate Hardware Interlock (`0s` delay, Always Active):** Even if `is_power_calibrated == false`, if `main_light_on == true` and any `heaters_on[i] == true`, immediately emits `PowerFaultCategory::SafetyInterlockViolation` (`LoadKind::Heater`).
- **Rule 2 — Uncalibrated Guard:** If `!cfg.is_power_calibrated`, resets the debounce timer and returns `PowerFaultCategory::UncalibratedSkip`.
- **Rule 3 — Dynamic Proportional Tolerance (`computeDynamicTolerance`):**
  `tolerance = max(3.0_W, theoretical_power * 0.04)`
  Detects small actuator failures (`5W` intake, `10W` water pump, `7.4W` idle drop) during low-load periods while absorbing normal `+/- 4%` AC mains voltage fluctuations during high-wattage LED operation.
- **Rule 4 — 3-Minute Debounce & Load Signature Matching (`identifySuspectedLoad`):**
  If `|delta_watts| > tolerance` continuously for `>= 3 minutes`:
  - `delta < 0` -> `UnderPowerOpenLoad`: searches currently `ON` actuators for the closest `power_rating` to `|delta|`.
  - `delta > 0` -> `OverPowerStuckRelay`: searches currently `OFF` actuators for the closest `power_rating` to `+delta`.
  - Matches candidate within `margin = max(2.5_W, candidate_watts * 0.25)`, returning `{LoadKind, index}` (or `{LoadKind::Unknown, 0}` if multiple simultaneous faults occurred).


### 3.3 Subsystem C: Main & Auxiliary Spectral Lighting (`PhotoperiodScheduler.hpp` — Milestone 2)
- Supports `Auto` vs `Manual` mode, configurable `lights_on_time`, and cycles `Vegetative_18_6`, `Flowering_12_12`, and `Autoflower_20_4` (with full midnight wrap-around).
- **Auxiliary Lights (`0..2`):**
  - `FollowMainLight_660nm` (Deep Red): matches Main Light `[t_on, t_off)`.
  - `SunsetLag15Min_730nm` (Far Red Emerson Effect): active during `[t_off - 15min, t_off + 15min)`.
- **4-Way Lighting Audit:** Uses `0.75 * cfg.main_light_power` as physical proof-of-light threshold to distinguish physical bulb/driver failure, welded relay, and missed software schedule.


### 3.4 Subsystem D: Smart Exhaust FSM (`ExhaustFsm.hpp` — Milestone 2)
- Modeled via `std::variant` + `std::visit`:
  1. `StateNormalContinuousOn` (Default 100% ON).
  2. `StateDryRetentionOff` (Entered when `VPD_in > 1.7_kPa` for `3m` & `T_in < 28.0_C`; holds OFF for `15m` to retain plant transpiration; exits immediately to Continuous ON when `VPD_in <= 1.5_kPa` for `3m`).
  3. `StateDryPeriodicRefreshOn` (Runs ON for dynamic `t_purge_sec` from `ExhaustSetup::calculatePurgeDuration` to renew O2/CO2).
  4. `StateForcedPurgeOn` (Unconditional thermal/humidity override when `T_in >= 28.0_C` for `2m` or `VPD_in < 0.7_kPa` for `5m`; exits at `T_in <= 27.0_C` & `VPD_in >= 0.8_kPa`).
- Compares Internal vs. External psychrometric state to warn when external air cannot improve internal VPD.


### 3.5 Subsystem E: Nighttime Thermal Controller (`ThermalController.hpp` — Milestone 2)
- Hard interlock forces all heaters (`0..2`) OFF whenever Main Light is ON.
- Targets `23.5_C` (`22.0_C` cold-front optimization when `T_ext < 21.0_C`), `+/- 0.5_C` hysteresis, `3m` minimum cycle/cooldown, and optional `30m` pre-dawn heater cutoff before Main Light ignition.


### 3.6 Subsystem F: Multi-Plant Irrigation & Alert Engine (`IrrigationController.hpp` & `AlertManager.hpp` — Milestone 2)
- Independent per-plant (`1..8`) state machine supporting:
  - `Manual` vs `Auto` (`BySoilMoisture < threshold` for `3m` OR `ByTimeInterval` every `K` hours/days).
  - Dosing via `ByVolumeLiters` (`t_total = ceil((Liters / FlowRate_Lmin) * 60s)`) or `ByDurationSeconds`.
  - Delivery via `Continuous` or `Pulsed` (`t_pulse_on` + `t_soak_off` accumulating active pumping time until `t_total`), plus post-soak moisture rise verification to prevent dry-running or flooding on detached soil probes.
- `AlertManager` monitors Critical VPD (`> 1.6_kPa` or `< 0.4_kPa` for `10m`), Foliar Stress (`> 1.7_kPa` for `120m`), Dew Point proximity (`T_in - T_dew <= 2.0_C` for `5m`), and maintains the structured fault history registry.


---


## 4. Master Milestone & Pull Request Roadmap


- **Milestone 1 — Domain Foundation, Setup, Psychrometrics & Auto-Calibrated Power Auditor:**
  - **PR #1 (`feat/m1-strong-types-and-setup`):** `Types.hpp`, `TentSetupConfig.hpp`, initial unit tests in `test_milestone1.cpp`.
  - **PR #2 (`feat/m1-vpd-and-power-auditor`):** `VpdCalculator.hpp`, `PowerAuditor.hpp`, `PowerCalibrator.hpp`, complete Milestone 1 test suite.
- **Milestone 2 — Deterministic State Machines & Controllers:**
  - **PR #3 (`feat/m2-photoperiod-and-exhaust`):** `PhotoperiodScheduler.hpp`, `ExhaustFsm.hpp`, `test_milestone2.cpp`.
  - **PR #4 (`feat/m2-thermal-and-irrigation`):** `ThermalController.hpp`, `IrrigationController.hpp`, `AlertManager.hpp`.
- **Milestone 3 — HAL Ports, Simulator, RingBuffer & Concurrency Engine:**
  - **PR #5 (`feat/m3-hal-and-concurrency`):** `IHardwarePort.hpp`, `MockHardwareDriver.hpp`, `RingBuffer.hpp`, `ControlLoopEngine.hpp` (`std::jthread`).
- **Milestone 4 — Qt 6 + QML Frontend (Setup/Calibration Wizard + Dashboard) & HA Shadow Mode:**
  - **PR #6 (`feat/m4-qt6-dashboard-and-shadow`):** `HaShadowDriver.hpp`, `DashboardBridge.hpp`, `SetupWizardPage.qml`, `MainDashboard.qml`.
- **Milestone 5 — Native ESP32 C++ Firmware & Autonomous Cutover:**
  - **PR #7 (`feat/m5-esp32-native-cutover`):** Direct compilation of `core/` into ESP32 firmware with 2-minute heartbeat fail-safe.
