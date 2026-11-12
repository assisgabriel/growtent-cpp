# 🌱 Grow Tent OS (`growtent-cpp`)

**Autonomous Indoor Cultivation Climate, Spectral Lighting, Electrical Power Auditor & Multi-Plant Irrigation Controller**
*Built with Modern ISO C++20/23, Hexagonal Architecture, Zero-Heap Domain Core, and Qt 6 / QML.*

---

## 📖 Overview

**Grow Tent OS (`growtent-cpp`)** is an industrial-grade, fault-tolerant environmental and irrigation control system for indoor cultivation enclosures. Originally prototyped and validated in production on **Home Assistant + ESPHome**, this project re-architects the entire control, psychrometric, electrical auditing, and crop-steering engine into **Modern ISO C++20/23** with a commercial embedded **Qt 6 + QML** dashboard.

Unlike hardcoded microcontroller scripts, `growtent-cpp` operates as a **configurable operating system**: users provision their tent geometry, exhaust airflow, passive/active intake, supplemental lighting spectra, heaters, circulation fans, and $1 \dots 8$ independent plant irrigation zones through a **Setup Wizard**, while the core engine enforces deterministic hardware interlocks and real-time electrical power auditing.

---

## ✨ Key Engineering & Agronomic Features

### 1. Stage 0: Configurable Hardware Provisioning (`TentSetupConfig`)
* **Enclosure Geometry:** Configurable Width $\times$ Depth $\times$ Height ($\text{cm}$) with compile-time volume calculation ($V_{\text{tent}}$ in $\text{m}^3$).
* **Static Compile-Time Capacity Limits (`Zero Heap Allocation`):**
  * **Climate Sensors (`2 pairs`):** $1$ Internal ($\text{Temp} + \text{RH}$) and $1$ External ($\text{Temp} + \text{RH}$) sensor pair.
  * **Main Light (`1 unit`):** Primary LED Quantum Board ($18/6$, $12/12$, or $20/4$ photoperiods).
  * **Auxiliary Spectral Lights (`0..2 units`):** Configurable per bar for **660nm Deep Red** (synchronous with main light) or **730nm Far Red / Emerson Effect** (turns ON $15\text{ min}$ before lights-off and remains ON for $15\text{ min}$ after lights-off).
  * **Nighttime Ceramic Heaters (`0..2 units`):** Hardware-interlocked against main lighting with cold-front optimization.
  * **Internal Circulation / Clip Fans (`0..3 units`):** Independent ON/OFF canopy boundary-layer disruption.
  * **Air Intake (`0..1 unit`):** Selectable between `Passive` ($0\text{ W}$, no relay) and `Active` (ON/OFF relay with calibrated wattage).
  * **Plant Zones & Water Pumps (`1..8 units`):** Each plant zone owns $1$ Capacitive Soil Moisture sensor (`SoilMoisture`) and $1$ dedicated Water Pump (`WaterPump`) with calibrated flow rate ($\text{L/min}$) and power ($\text{W}$).

### 2. Dual-Zone Psychrometric Engine (`VpdCalculator`)
Computes real-time psychrometric telemetry for both **Internal** and **External** sensors using the Magnus-Tetens equations:
* **Saturation Vapor Pressure ($\text{SVP}$):**
  $$\text{SVP}(T) = 0.61078 \times \exp\left(\frac{17.27 \times T}{T + 237.3}\right) \quad [\text{kPa}]$$
* **Global Air / Canopy Vapor Pressure Deficit ($\text{VPD}$):**
  $$\text{VPD} = \max\left(0.0,\ \text{SVP}(T_{\text{air}} + \Delta T_{\text{leaf}}) - \text{SVP}(T_{\text{air}}) \times \frac{\text{RH}}{100.0}\right) \quad [\text{kPa}]$$
* **Dew Point ($T_{\text{dew}}$):**
  $$\alpha(T, \text{RH}) = \frac{17.27 \times T}{237.7 + T} + \ln\left(\frac{\text{RH}}{100.0}\right), \quad T_{\text{dew}} = \frac{237.7 \times \alpha(T, \text{RH})}{17.27 - \alpha(T, \text{RH})} \quad [^\circ\text{C}]$$

### 3. Smart Exhaust FSM (`ExhaustFsm`) — Default-ON with Dynamic Purge Math
* **Default-ON Operation:** Runs $100\%$ continuously when climate is within target bounds.
* **High-VPD Moisture Retention Cycle:** If $\text{VPD}_{\text{in}} > 1.7\text{ kPa}$ for $3\text{ min}$ (and $T_{\text{in}} < 28.0^\circ\text{C}$), turns OFF for $15\text{ min}$ to trap plant transpiration, then purges $\text{O}_2/\text{CO}_2$ for a dynamically calculated duration $t_{\text{purge}}$ based on tent volume $V_{\text{tent}}$, exhaust airflow $Q_{\text{m}^3/\text{h}}$, target air exchanges ($N_{\text{exchanges}} = 5$), and carbon-filter safety margin ($M_{\text{safety}}$):
  $$t_{\text{purge (s)}} = \left\lceil \left( \frac{V_{\text{tent}} \times N_{\text{exchanges}}}{Q_{\text{m}^3/\text{h}}} \times 3600 \right) \times (1.0 + M_{\text{safety}}) \right\rceil$$
* **Hysteresis & Thermal Override:** Immediately exits cycling back to $100\%$ ON when $\text{VPD}_{\text{in}} \le 1.5\text{ kPa}$ (for $3\text{ min}$) or unconditionally if $T_{\text{in}} \ge 28.0^\circ\text{C}$ (for $2\text{ min}$).

### 4. Nighttime Thermal Controller (`ThermalController`)
* **Hard Safety Interlock:** Strictly forbids any heater ($0..2$) from activating while the Main Light is ON.
* **Cold-Front Setpoint Optimization:** Targets $23.5^\circ\text{C}$ during normal nights ($T_{\text{ext}} \ge 21.0^\circ\text{C}$) and automatically drops target to $22.0^\circ\text{C}$ when $T_{\text{ext}} < 21.0^\circ\text{C}$ to prevent $100\%$ heater duty-cycle saturation.
* **Relay Protection:** $\pm 0.5^\circ\text{C}$ hysteresis band with $3\text{ min}$ minimum cycle/cooldown protection.

### 5. Multi-Mode Crop-Steering Irrigation (`IrrigationController`)
Each of the $1 \dots 8$ plants runs an independent state machine supporting:
* **Control Mode:** `Manual` vs. `Auto`.
* **Auto Triggers:** `BySoilMoisture` ($\text{moisture} < \text{threshold}$ for $3\text{ min}$) or `ByTimeInterval` (e.g., every $72\text{h}$ / $3\text{ days}$).
* **Dosing Calculation:** `ByVolumeLiters` ($t_{\text{total}} = \lceil \frac{V_{\text{L}}}{Q_{\text{L/min}}} \times 60 \rceil$) or `ByDurationSeconds`.
* **Delivery Strategy:** `Continuous` (single uninterrupted run) or `Pulsed` (multi-pulse ON + soak OFF intervals accumulating exact active pumping time until $t_{\text{total}}$ is reached).

### 6. Active PZEM-004T Electrical Auditor & Alert Engine (`PowerAuditor` & `AlertManager`)
Continuously cross-references real PZEM-004T AC power measurements against the theoretical power sum of all active relays:
* **Immediate Interlock Fault (`0s`):** Main Light and Heater active simultaneously.
* **4-Way Lighting Auditor (`5 min`, $\ge 220\text{ W}$):** Distinguishes between *Burned Bulb/Driver*, *Physically Welded Relay* (`Relé Colado`), and *Missed Software Timer* (with automatic self-healing).
* **Exhaust Auditor (`3 min`, $\pm 35.0\text{ W}$) & Heater/General Auditor (`3 min`, $\pm 50.0\text{ W}$).**
* **Agronomic Alerts:** Critical High VPD ($> 1.6\text{ kPa}$ for $10\text{ min}$), Critical Low VPD / Bud Rot Risk ($< 0.4\text{ kPa}$ for $10\text{ min}$), Foliar Stress ($> 1.7\text{ kPa}$ for $120\text{ min}$), and Dew Point Condensation Proximity ($T_{\text{in}} - T_{\text{dew}} \le 2.0^\circ\text{C}$ for $5\text{ min}$).

---

## 🏗️ System Architecture (Hexagonal / Ports & Adapters)

The codebase is strictly partitioned so that `core/` has **zero external dependencies** (only the C++20 Standard Library `std::`), enabling the exact same domain code to compile on Windows/Linux Desktop and bare-metal ESP32:

```text
growtent-cpp/
├── CMakeLists.txt                        # Modern Target-Based CMake (C++20, GoogleTest, Qt6)
├── GEMINI.md                             # Socratic C++20 Mentor & Code Review Rules
├── README.md                             # Project Documentation
├── docs/
│   └── SRS_BLUEPRINT.md                  # Full Software Requirements Spec & C-to-C++20 Guide
├── core/                                 # 100% Pure C++20 Domain & Control Logic (Zero Heap / Zero OS deps)
│   └── include/
│       ├── domain/
│       │   ├── Types.hpp                 # Strong physical units, <=> operator, literals, HardwareLimits
│       │   └── TentSetupConfig.hpp       # Stage 0 configurable setup, volume & purge math
│       ├── services/
│       │   ├── VpdCalculator.hpp         # Magnus-Tetens SVP, VPD & Dew Point (Internal & External)
│       │   ├── PowerAuditor.hpp          # Dynamic theoretical power sum & 3-min fault debounce
│       │   └── AlertManager.hpp          # Environmental & hardware fault alert registry
│       └── controllers/
│           ├── PhotoperiodScheduler.hpp  # 18/6, 12/12, 20/4 + 660nm/730nm Aux Light scheduler
│           ├── ExhaustFsm.hpp            # std::variant / std::visit Default-ON + VPD retention FSM
│           ├── ThermalController.hpp     # Nighttime heater hysteresis + Main Light interlock
│           └── IrrigationController.hpp  # 1..8 Plant FSMs (Manual/Auto, Liters/Time, Continuous/Pulsed)
├── hal/                                  # Hardware Abstraction Layer (Ports & Adapters)
│   └── include/
│       ├── IHardwarePort.hpp             # Pure virtual interface / C++20 Concept for Sensors & Relays
│       ├── MockHardwareDriver.hpp        # Synthetic climate generator & fault injector
│       └── HaShadowDriver.hpp            # QtWebSockets read-only live Home Assistant Digital Twin
├── app/                                  # Concurrency, Storage & Orchestration
│   └── include/
│       ├── RingBuffer.hpp                # Fixed-capacity template circular buffer for 24h plots
│       └── ControlLoopEngine.hpp         # std::jthread deterministic tick loop + std::scoped_lock
├── ui/                                   # Qt 6 + QML Commercial Embedded Presentation Layer
│   ├── DashboardBridge.hpp               # QObject Q_PROPERTY bridge exposing Core to QML
│   ├── SetupWizardPage.qml               # Interactive Stage 0 Tent Provisioning UI
│   └── MainDashboard.qml                 # Industrial dark-mode dashboard + Qt Graphs real-time plots
└── tests/                                # Automated GoogleTest Suite
    ├── test_milestone1.cpp               # Types, TentSetupConfig, VpdCalculator & PowerAuditor tests
    ├── test_milestone2.cpp               # Photoperiod, ExhaustFsm, Thermal & Irrigation tests
    └── test_milestone3.cpp               # RingBuffer & ControlLoopEngine concurrency tests
```

---

## 🧠 Modern C++20/23 Paradigms Practiced

| Embedded C Pattern Replaced | Modern C++20/23 Paradigm Used | Where Applied |
| :--- | :--- | :--- |
| `#define` constants & magic numbers | `static constexpr` & User-Defined Literals (`280.0_W`, `23.5_C`) | `Types.hpp`, `TentSetupConfig.hpp` |
| `typedef double` unit aliases | Zero-cost strong unit `struct` wrappers + `operator<=>` | `Types.hpp` |
| `-1.0f` error floats & output pointers | `[[nodiscard]] std::optional<T>` & `std::nullopt` | `VpdCalculator.hpp`, `IHardwarePort.hpp` |
| Raw C arrays & `malloc`/`free` | `std::array<T, MaxN>` + `std::span<const T>` | `TentSetupConfig.hpp`, `PowerAuditor.hpp` |
| Giant `switch(enum)` state machines | `std::variant<States...>` + `std::visit` | `ExhaustFsm.hpp`, `IrrigationController.hpp` |
| Global `millis()` tick math | Deterministic injected `std::chrono::steady_clock::time_point` | All Services, Controllers & Unit Tests |
| `pthread` + manual mutex unlock | `std::jthread`, `std::stop_token` & RAII `std::scoped_lock` | `ControlLoopEngine.hpp` |

---

## 🛠️ Prerequisites & Build Instructions

### Toolchain Requirements
* **Compiler:** GCC 13+ / MinGW-w64 15.1+ (or Clang 16+ / MSVC 2022) with full **C++20** support
* **Build System:** CMake `3.20+` and `Ninja` `1.10+`
* **UI Framework (Milestone 4):** Qt `6.8+` / `6.12+` (`Qt Quick`, `Qt Graphs`, `Qt WebSockets`)
* **Testing Framework:** `GoogleTest v1.14.0` (automatically fetched by CMake `FetchContent`)

### Quick Start: Configure, Build & Run Unit Tests

```powershell
# 1. Configure CMake with Ninja and g++
cmake -B build -G Ninja -DCMAKE_CXX_COMPILER=g++

# 2. Compile with strict compiler warnings (-Wall -Wextra -Wpedantic -Wconversion)
cmake --build build

# 3. Run the automated unit test suite via CTest (or directly)
ctest --test-dir build --output-on-failure
.\build\unit_tests.exe
```

---

## 🗺️ Development Roadmap & Pull Request Plan

| Milestone | Pull Request / Branch | Deliverables | Status |
| :--- | :--- | :--- | :--- |
| **Setup** | `main` | Project skeleton, `CMakeLists.txt`, `.gitignore`, `README.md`, `docs/SRS_BLUEPRINT.md` | ✅ Ready |
| **Milestone 1** | **PR #1:** `feat/m1-strong-types-and-setup` | `Types.hpp` + `TentSetupConfig.hpp` + Setup & Volume unit tests | 🚧 In Progress |
| **Milestone 1** | **PR #2:** `feat/m1-vpd-and-power-auditor` | `VpdCalculator.hpp` + `PowerAuditor.hpp` + Complete Milestone 1 test suite | ⏳ Planned |
| **Milestone 2** | **PR #3:** `feat/m2-photoperiod-and-exhaust` | `PhotoperiodScheduler.hpp` (660nm/730nm) + `ExhaustFsm.hpp` (`std::variant`) | ⏳ Planned |
| **Milestone 2** | **PR #4:** `feat/m2-thermal-and-irrigation` | `ThermalController.hpp` + `IrrigationController.hpp` + `AlertManager.hpp` | ⏳ Planned |
| **Milestone 3** | **PR #5:** `feat/m3-hal-and-concurrency` | `IHardwarePort.hpp`, `MockHardwareDriver.hpp`, `RingBuffer.hpp`, `ControlLoopEngine.hpp` | ⏳ Planned |
| **Milestone 4** | **PR #6:** `feat/m4-qt6-dashboard-and-shadow` | Qt 6 Setup Wizard & Live Dashboard + `HaShadowDriver.hpp` (Live HA Read-Only Twin) | ⏳ Planned |
| **Milestone 5** | **PR #7:** `feat/m5-esp32-native-firmware` | Native ESP32 C++ firmware integration & 2-minute local heartbeat fail-safe | ⏳ Planned |

---

## 👤 Author

**Assis Greselle**
*Associate Software Engineer — Embedded & Platform Systems*
