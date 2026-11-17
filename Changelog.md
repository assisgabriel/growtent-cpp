# Changelog


All notable changes to the **Grow Tent OS (`growtent-cpp`)** project will be documented in this file.


The format is based on Keep a Changelog (1.1.0), and this project adheres to Semantic Versioning (2.0.0).


## [Unreleased]


### Added
- **Step-Response Auto-Calibration Spec (`PowerCalibrator.hpp`):** Automated Stage 0.5 commissioning sequence measuring `IdleBaseline` and isolated per-actuator wattages with `5s` inrush settling and `2.0 W` open-load detection.
- **3D Load-Isolating Power Auditor Spec (`PowerAuditor.hpp`):** Separated electrical diagnostics into `PowerFaultCategory`, `LoadKind`, and `suspected_index` with adaptive tolerance (`max(3.0 W, 4% * P_theo)`), `UncalibratedSkip` guard, and proportional load signature matching.
- **Doxygen Documentation Standard:** Added Doxygen comments (`@file`, `@brief`, `@param`, `@return`) across all Milestone 1 headers and optional `docs` target in `CMakeLists.txt`.


### Changed
- **Strong Physical Unit Operators (`Types.hpp`):** Added `operator+`, `operator-`, `operator+=`, and `operator-=` to physical quantities for clean hysteresis/offset math; standardized `PlantId{0}` as unconfigured/invalid slot.
- **Uncalibrated Default Topology (`TentSetupConfig.hpp`):** Initialized all actuator `Watts` ratings to `0.0 W` with `is_power_calibrated = false` and `has_main_light = true` until populated by `PowerCalibrator`.
- Synchronized `docs/SRS_BLUEPRINT.md`, `GEMINI.md`, and `README.md` to Master Specification `v2.3`.


## [0.0.1] - Initial Architecture & Build Bootstrap


### Added
- Modern C++20 `CMakeLists.txt` with Ninja generator, strict compiler warnings (`-Wall -Wextra -Wpedantic -Wconversion`), and automatic GoogleTest `FetchContent` integration.
- Automated `VERSION` + Git short SHA header generator (`core/include/domain/Version.hpp.in` -> `Version.hpp`).
- Initial repository structure, `.gitignore`, `README.md`, `GEMINI.md`, and `docs/SRS_BLUEPRINT.md`.
