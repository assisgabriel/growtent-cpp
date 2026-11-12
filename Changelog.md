# Changelog

All notable changes to the **Grow Tent OS (`growtent-cpp`)** project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Initial C++20 CMake project structure, Ninja build configuration, and GoogleTest integration.
- Master Software Requirements Specification (`docs/SRS_BLUEPRINT.md` v2.2) and Socratic Mentor instructions (`GEMINI.md`).
- Automated CMake + Git SHA compile-time version header generator (`core/include/domain/Version.hpp.in`).

## [0.1.0] - Planned (Milestone 1)
- Zero-cost strong physical unit types (`Types.hpp`) with C++20 `operator<=>` and user-defined literals.
- Stage 0 Configurable Tent Setup (`TentSetupConfig.hpp`) with static hardware limits and dynamic exhaust purge calculation.
- Dual-zone Magnus-Tetens Psychrometric Calculator (`VpdCalculator.hpp`) and 1-to-N Electrical Power Auditor (`PowerAuditor.hpp`).
