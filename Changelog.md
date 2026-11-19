# Changelog


All notable changes to the **Grow Tent OS (`growtent-cpp`)** project will be documented in this file.


The format is based on Keep a Changelog (1.1.0), and this project adheres to Semantic Versioning (2.0.0).


## [Unreleased]


### Added
- Planned Milestone 2 controllers: `PhotoperiodScheduler.hpp`, `ExhaustFsm.hpp`, `ThermalController.hpp`, `IrrigationController.hpp`, and `AlertManager.hpp`.


## [0.1.0] - 2026-11-19


### Added
- **Strong Physical Units (`Types.hpp`):** Zero-overhead C++20 domain types (`Celsius`, `RelativeHumidity`, `SoilMoisture`, `Kilopascal`, `Watts`, `CubicMeters`, `CubicMetersPerHour`, `Liters`, `LitersPerMinute`) with dimensional arithmetic, `<=>` comparisons, and user-defined literals (`_C`, `_RH`, `_pct`, `_kPa`, `_W`, `_m3`, `_m3h`, `_L`, `_Lmin`).
- **Enclosure Topology & Hardware Validation (`TentSetupConfig.hpp`):** Setup provisioning model with `HardwareLimits`, volume calculation, air purge duration calculation, duplicate/out-of-range plant ID rejection, and default uncalibrated state.
- **Dual-Zone Psychrometric Engine (`VpdCalculator.hpp`):** Pure Magnus-Tetens calculations for Saturation Vapor Pressure (SVP), Actual Vapor Pressure (AVP), Vapor Pressure Deficit (VPD) with leaf temperature offset (`dT_leaf`), Dew Point temperature, and `std::nullopt` telemetry anomaly guards.
- **Step-Response Auto-Calibrator (`PowerCalibrator.hpp`):** Automated Stage 0.5 commissioning sequence measuring `IdleBaseline` and isolated per-actuator wattages with `5s` inrush settling and `2.0 W` open-load detection, setting `cfg.is_power_calibrated = true`.
- **3D Load-Isolating Power Auditor (`PowerAuditor.hpp`):** Non-intrusive electrical diagnostics outputting `PowerAuditReport` with `{PowerFaultCategory, LoadKind, suspected_index}`. Features Rule 1 immediate safety interlock (0s delay for light/heater), Rule 2 `UncalibratedSkip` guard, Rule 3 dynamic proportional tolerance (`max(3.0 W, 4% * P_theo)`), and Rule 4 continuous 3-minute debounce with proportional load signature isolation (`UnderPowerOpenLoad`, `OverPowerStuckRelay`).
- **Milestone 1 Test Suite (`test_milestone1.cpp`):** 25 automated unit tests in GoogleTest covering all domain types, configuration validation, psychrometrics, auto-calibration sequence, and electrical fault isolation with 100% pass rate.
- **Doxygen Documentation Standard:** Comprehensive Doxygen comments (`@file`, `@brief`, `@param`, `@return`, `@details`) across all Milestone 1 headers with optional `docs` target in `CMakeLists.txt`.


### Changed
- Synchronized `docs/SRS_BLUEPRINT.md`, `GEMINI.md`, and `README.md` to Master Specification `v2.3`.


## [0.0.1] - Initial Architecture & Build Bootstrap


### Added
- Modern C++20 `CMakeLists.txt` with Ninja generator, strict compiler warnings (`-Wall -Wextra -Wpedantic -Wconversion`), and automatic GoogleTest `FetchContent` integration.
- Automated `VERSION` + Git short SHA header generator (`core/include/domain/Version.hpp.in` -> `Version.hpp`).
- Initial repository structure, `.gitignore`, `README.md`, `GEMINI.md`, and `docs/SRS_BLUEPRINT.md`.
