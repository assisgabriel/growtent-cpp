# 🌱 Grow Tent OS (`growtent-cpp`)


**Autonomous Indoor Cultivation Climate, Spectral Lighting, Self-Calibrating Power Auditor & Multi-Plant Irrigation Controller**
*Built with Modern ISO C++20/23, Hexagonal Architecture, Zero-Heap Core & Qt 6 / QML*


---


## 📖 Overview


**Grow Tent OS (`growtent-cpp`)** is an industrial-grade environmental, lighting, electrical-diagnostics, and multi-plant irrigation control system designed for indoor cultivation enclosures.


Evolving from a live **Home Assistant + ESPHome** setup, this project implements a deterministic, hardware-agnostic **C++20 Core Engine** paired with a **Qt 6 / QML Embedded & Desktop Dashboard**—capable of running in pure simulation, read-only **Shadow Mode** alongside Home Assistant, or natively on **Raspberry Pi / ESP32** in production.


### ✨ Key Engineering Highlights
- **Zero-Heap Embedded Core:** Uses compile-time capacity bounds (`std::array` + `std::span`) with zero `malloc`/`new` inside `core/`.
- **Compile-Time Dimensional Safety:** Physical units (`Celsius`, `RelativeHumidity`, `SoilMoisture`, `Kilopascal`, `Watts`, `CubicMeters`, `Liters`, etc.) are zero-cost strong types with C++20 `operator<=>`, offset/accumulation operators, and user-defined literals (`23.5_C`, `1.2_kPa`, `280.0_W`).
- **Plug-and-Play Auto-Calibration (`PowerCalibrator`):** Users never hardcode actuator wattages. During Stage 0 setup (or after swapping an LED panel/fan), an automated step-response routine measures standby idle power and energizes each actuator in isolation (with `5s` inrush settling) to learn exact wattage signatures.
- **3D Non-Intrusive Load Monitoring (`PowerAuditor`):** Using a single main-bus PZEM-004T wattmeter, isolates stuck relays (`OverPowerStuckRelay`) and burned/unplugged actuators (`UnderPowerOpenLoad`) down to the specific `{LoadKind, index}` via adaptive tolerance (`max(3.0 W, 4% * P_theo)`).
- **Psychrometric & Spectral Precision:** Dual-zone (Internal & External) Magnus-Tetens VPD/Dew Point engine, Default-ON exhaust with dynamic volume-based O2 refresh cycles, nighttime thermal interlock with cold-front protection, `660nm`/`730nm` auxiliary lighting schedules, and 8-zone pulsed/continuous crop-steering irrigation.


---


## 🏗️ Hexagonal Repository Structure


```text
growtent-cpp/
├── CMakeLists.txt                        # C++20 build, Git SHA Version generator, GTest & Doxygen
├── VERSION                               # Single-source-of-truth Semantic Version (e.g., 0.1.0)
├── CHANGELOG.md                          # Release history following Keep a Changelog & SemVer
├── GEMINI.md                             # Socratic C++20 Mentor & PR Review rules
├── docs/
│   └── SRS_BLUEPRINT.md                  # Master Software Requirements Specification (v2.3)
├── core/                                 # 100% Pure C++20 Domain & Control Logic (Zero OS/Qt deps)
│   └── include/
│       ├── domain/
│       │   ├── Version.hpp.in            # CMake template -> build/generated/domain/Version.hpp
│       │   ├── Types.hpp                 # Strong physical units, literals, PlantId & HardwareLimits
│       │   └── TentSetupConfig.hpp       # Stage 0 topology & learned wattage profile
│       ├── services/
│       │   ├── VpdCalculator.hpp         # Dual-zone Magnus-Tetens SVP, VPD & Dew Point
│       │   ├── PowerAuditor.hpp          # Adaptive 3D load-signature fault isolator
│       │   ├── PowerCalibrator.hpp       # Step-response automatic wattage calibrator
│       │   └── AlertManager.hpp          # VPD, Dew Point & hardware fault registry (M2)
│       └── controllers/
│           ├── PhotoperiodScheduler.hpp  # 18/6, 12/12, 20/4 + 660nm/730nm aux schedules (M2)
│           ├── ExhaustFsm.hpp            # std::variant Default-ON + VPD retention FSM (M2)
│           ├── ThermalController.hpp     # Nighttime heater hysteresis + light interlock (M2)
│           └── IrrigationController.hpp  # 1..8 plant Manual/Auto, Liters/Time, Pulsed/Cont (M2)
├── hal/                                  # Hardware Abstraction Layer (Milestone 3 & 4)
├── app/                                  # RingBuffer & std::jthread ControlLoopEngine (Milestone 3)
├── ui/                                   # Qt 6 + QML Setup Wizard & Live Dashboard (Milestone 4)
└── tests/                                # Automated GoogleTest Suite
    └── test_milestone1.cpp
```


---


## 🛠️ Prerequisites & Build Instructions


### Toolchain Requirements
- **Compiler:** GCC 13+ / MinGW-w64 15+ (`g++` with full C++20 support), Clang 16+, or MSVC 2022
- **Build System:** CMake `3.20+` and Ninja `1.10+`
- **UI Framework (Milestone 4):** Qt `6.7+` (`Qt Quick`, `Qt Graphs`, `Qt WebSockets`)
- **Optional:** Doxygen (for generating HTML API docs via `--target docs`)


### Configure, Compile & Run Unit Tests
```powershell
# 1. Configure Ninja build and fetch GoogleTest
cmake -B build -G Ninja -DCMAKE_CXX_COMPILER=g++


# 2. Compile with strict C++20 warnings (-Wall -Wextra -Wpedantic -Wconversion)
cmake --build build


# 3. Execute the unit test suite
ctest --test-dir build --output-on-failure
# Or run directly with colorful output:
.\build\unit_tests.exe
```


---


## 🏷️ Versioning & Release Traceability


Firmware and desktop builds embed compile-time Semantic Versioning + Git commit traceability automatically:
1. **`VERSION`:** Holds the current `MAJOR.MINOR.PATCH` string (`0.1.0`).
2. **`Version.hpp.in` -> `Version.hpp`:** CMake injects `@PROJECT_VERSION@` and `git rev-parse --short HEAD` into `grow::domain::SystemVersion::kFullBuildTag` (e.g., `"v0.1.0+a1b2c3d"`) as zero-overhead `constexpr std::string_view` constants.
3. **`CHANGELOG.md` & Git Tags:** Every milestone merge into `main` updates `CHANGELOG.md` and creates an annotated Git tag (`v0.1.0`, `v0.2.0`, ..., `v1.0.0`).


---


## 🗺️ Development Roadmap (Milestones & Pull Requests)


| Milestone | Pull Requests | Status | Description |
| :--- | :--- | :---: | :--- |
| **M0: Bootstrap** | `main` (`v0.0.1`) | ✅ Done | CMake + Ninja + GTest pipeline, SemVer generator & SRS `v2.3` |
| **M1: Domain Core** | **PR #1** (`feat/m1-strong-types-and-setup`)<br>**PR #2** (`feat/m1-vpd-and-power-auditor`) | ✅ Done (`v0.1.0`) | Strong units, `TentSetupConfig`, `VpdCalculator`, 3D `PowerAuditor` & `PowerCalibrator` |
| **M2: Controllers** | **PR #3** (`feat/m2-photoperiod-and-exhaust`)<br>**PR #4** (`feat/m2-thermal-and-irrigation`) | 🚧 Next / In Progress | `std::variant` FSMs for Lighting (`660nm`/`730nm`), Exhaust, Heater & 8-Plant Irrigation |
| **M3: Concurrency** | **PR #5** (`feat/m3-hal-and-concurrency`) | ⏳ Planned | `IHardwarePort`, `MockHardwareDriver`, `RingBuffer` & `std::jthread` engine |
| **M4: Qt 6 & Shadow** | **PR #6** (`feat/m4-qt6-dashboard-and-shadow`) | ⏳ Planned | Qt 6 Setup/Auto-Calibration Wizard, Live Graphs & Home Assistant WebSocket Shadow Mode |
| **M5: ESP32 Native** | **PR #7** (`feat/m5-esp32-native-cutover`) | ⏳ Planned | Production ESP32 C++ firmware sharing `core/` with 2-min autonomous watchdog |
