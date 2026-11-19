/**
 * @file PowerAuditor.hpp
 * @brief Non-intrusive electrical load auditor with dynamic tolerance and load signature isolation.
 * @details Compares measured PZEM-004T AC active power against calibrated theoretical power,
 *          detecting stuck relays, open circuits, and hardware interlock violations.
 */

#pragma once

#include "domain/TentSetupConfig.hpp"
#include "domain/Types.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <utility>

namespace grow::services
{

    /**
     * @enum PowerFaultCategory
     * @brief Classification of electrical anomalies detected on the AC bus.
     */
    enum class PowerFaultCategory
    {
        None,                     ///< Measured power matches theoretical power within dynamic tolerance.
        UncalibratedSkip,         ///< Wattage audit paused until PowerCalibrator completes (Interlock still active).
        SafetyInterlockViolation, ///< Immediate critical safety violation: Main light and heater commanded ON together.
        UnderPowerOpenLoad,       ///< Measured power significantly below theoretical (burned load / open circuit).
        OverPowerStuckRelay       ///< Measured power significantly above theoretical (welded / stuck relay contacts).
    };

    /**
     * @enum LoadKind
     * @brief Actuator family identifier used for load signature isolation and step-response auto-calibration.
     */
    enum class LoadKind
    {
        None,         ///< No load suspected or selected.
        IdleBaseline, ///< Controller standby baseline (ESP32, power supplies, relay coils with all AC loads OFF).
        Unknown,      ///< Power divergence does not match a single isolated actuator signature.
        MainLight,    ///< Primary LED grow light panel.
        AuxLight,     ///< Auxiliary spectral LED bar (index 0..1).
        Heater,       ///< Nighttime ceramic heater (index 0..1).
        ExhaustFan,   ///< Primary turbo exhaust fan.
        IntakeFan,    ///< Motorized active intake fan.
        InternalFan,  ///< Internal circulation clip fan (index 0..2).
        PlantPump     ///< Per-plant irrigation water pump (index 0..7).
    };

    /**
     * @struct SystemActuatorSnapshot
     * @brief Immutable snapshot of all commanded relay states at a given control tick.
     */
    struct SystemActuatorSnapshot
    {
        bool main_light_on{false};                                                     ///< Primary LED grow light commanded state.
        std::array<bool, domain::HardwareLimits::kMaxAuxLights> aux_lights_on{};       ///< Auxiliary LED bars commanded states.
        std::array<bool, domain::HardwareLimits::kMaxHeaters> heaters_on{};            ///< Nighttime ceramic heaters commanded states.
        bool exhaust_fan_on{false};                                                    ///< Primary exhaust fan commanded state.
        bool intake_fan_on{false};                                                     ///< Active intake fan commanded state.
        std::array<bool, domain::HardwareLimits::kMaxInternalFans> internal_fans_on{}; ///< Internal clip circulation fans commanded states.
        std::array<bool, domain::HardwareLimits::kMaxPlants> plant_pumps_on{};         ///< Irrigation water pumps commanded states.
    };

    /**
     * @struct PowerAuditReport
     * @brief Diagnostic report produced on every evaluation cycle of the PowerAuditor.
     */
    struct PowerAuditReport
    {
        domain::Watts theoretical_power{};                     ///< Expected power based on active relays and calibrated ratings.
        domain::Watts measured_power{};                        ///< Real AC active power read from PZEM-004T.
        domain::Watts delta_watts{};                           ///< Signed divergence (measured_power - theoretical_power).
        PowerFaultCategory category{PowerFaultCategory::None}; ///< Detected electrical anomaly category.
        LoadKind suspected_load{LoadKind::None};               ///< Isolated actuator family responsible for the fault.
        std::size_t suspected_index{0};                        ///< Zero-based index within multi-unit actuator arrays.
    };

    /**
     * @class PowerAuditor
     * @brief Stateful electrical watchdog with 3-minute fault debounce and load signature matching.
     */
    class PowerAuditor
    {
    public:
        using TimePoint = std::chrono::steady_clock::time_point;

        static constexpr domain::Watts kNoiseFloorWatts{3.0};              ///< Minimum noise floor (detects 5W intake & 10W pumps).
        static constexpr double kRelativeToleranceRatio = 0.04;            ///< 4% proportional AC line voltage fluctuation margin.
        static constexpr auto kDebounceDuration = std::chrono::minutes(3); ///< Continuous divergence duration before alarm.

        /**
         * @brief Computes adaptive power tolerance: `max(3.0 W, 4% of theoretical_power)`.
         * @param theoretical Current expected system power in W.
         * @return Allowed power deviation threshold in W.
         */
        [[nodiscard]] static constexpr domain::Watts computeDynamicTolerance(domain::Watts theoretical) noexcept
        {
            const double proportional = theoretical.value * kRelativeToleranceRatio;
            return domain::Watts{(proportional > kNoiseFloorWatts.value) ? proportional : kNoiseFloorWatts.value};
        }

        /**
         * @brief Sums expected active power across idle baseline and all currently active relays in `cfg`.
         * @param cfg Provisioned and calibrated tent setup configuration.
         * @param state Current ON/OFF command state of all relays.
         * @return Total expected active power in W.
         */
        [[nodiscard]] static constexpr domain::Watts computeTheoreticalPower(
            const domain::TentSetupConfig &cfg,
            const SystemActuatorSnapshot &state) noexcept
        {
            domain::Watts total = cfg.base_idle_power;

            if (cfg.has_main_light && state.main_light_on)
            {
                total += cfg.main_light_power;
            }

            for (std::size_t i = 0; i < cfg.num_aux_lights; ++i)
            {
                if (cfg.aux_lights[i].enabled && state.aux_lights_on[i])
                {
                    total += cfg.aux_lights[i].power_rating;
                }
            }

            for (std::size_t i = 0; i < cfg.num_heaters; ++i)
            {
                if (state.heaters_on[i] && !state.main_light_on)
                {
                    total += cfg.heater_powers[i];
                }
            }

            if (state.exhaust_fan_on)
            {
                total += cfg.exhaust.power_rating;
            }

            if (cfg.intake_mode == domain::IntakeMode::Active && state.intake_fan_on)
            {
                total += cfg.intake_power;
            }

            for (std::size_t i = 0; i < cfg.num_internal_fans; ++i)
            {
                if (state.internal_fans_on[i])
                {
                    total += cfg.internal_fan_powers[i];
                }
            }

            for (std::size_t i = 0; i < cfg.num_plants; ++i)
            {
                if (state.plant_pumps_on[i])
                {
                    total += cfg.plants[i].pump_power_rating;
                }
            }

            return total;
        }

        /**
         * @brief Isolates which specific actuator best explains an observed wattage divergence.
         * @param cfg Calibrated tent setup configuration containing per-load power ratings.
         * @param state Current commanded relay states (checks ON loads for UnderPower, OFF loads for StuckRelay).
         * @param delta_watts Signed divergence (`measured - theoretical`) in W.
         * @return Pair `{LoadKind, index}` of the best matching load, or `{LoadKind::Unknown, 0}`.
         */
        [[nodiscard]] static std::pair<LoadKind, std::size_t> identifySuspectedLoad(
            const domain::TentSetupConfig &cfg,
            const SystemActuatorSnapshot &state,
            double delta_watts) noexcept
        {
            const bool looking_for_on = (delta_watts < 0.0);
            const double abs_delta = std::abs(delta_watts);
            LoadKind best_kind = LoadKind::Unknown;
            std::size_t best_idx = 0;
            double min_error = std::numeric_limits<double>::infinity();
            double best_candidate_watts = 0.0;

            auto calculate_margin = [](double candidate_watts) noexcept
            {
                return std::max(2.5, candidate_watts * 0.25);
            };

            auto check_candidate = [&](LoadKind kind, std::size_t idx, bool is_on, domain::Watts rating)
            {
                if (is_on != looking_for_on || rating.value <= 0.0)
                {
                    return;
                }

                const double candidate_watts = rating.value;
                const double margin = calculate_margin(candidate_watts);
                const double current_error = std::abs(abs_delta - candidate_watts);

                if (current_error <= margin && current_error < min_error)
                {
                    min_error = current_error;
                    best_kind = kind;
                    best_idx = idx;
                    best_candidate_watts = candidate_watts;
                }
            };

            if (cfg.has_main_light)
            {
                check_candidate(LoadKind::MainLight, 0, state.main_light_on, cfg.main_light_power);
            }

            for (std::size_t i = 0; i < cfg.num_aux_lights; ++i)
            {
                if (cfg.aux_lights[i].enabled)
                {
                    check_candidate(LoadKind::AuxLight, i, state.aux_lights_on[i], cfg.aux_lights[i].power_rating);
                }
            }

            for (std::size_t i = 0; i < cfg.num_heaters; ++i)
            {
                check_candidate(LoadKind::Heater, i, state.heaters_on[i], cfg.heater_powers[i]);
            }

            check_candidate(LoadKind::ExhaustFan, 0, state.exhaust_fan_on, cfg.exhaust.power_rating);

            if (cfg.intake_mode == domain::IntakeMode::Active)
            {
                check_candidate(LoadKind::IntakeFan, 0, state.intake_fan_on, cfg.intake_power);
            }

            for (std::size_t i = 0; i < cfg.num_internal_fans; ++i)
            {
                check_candidate(LoadKind::InternalFan, i, state.internal_fans_on[i], cfg.internal_fan_powers[i]);
            }

            for (std::size_t i = 0; i < cfg.num_plants; ++i)
            {
                check_candidate(LoadKind::PlantPump, i, state.plant_pumps_on[i], cfg.plants[i].pump_power_rating);
            }

            if (best_kind != LoadKind::Unknown)
            {
                return {best_kind, best_idx};
            }
            return {LoadKind::Unknown, 0};
        }

        /**
         * @brief Evaluates real PZEM power against theoretical power with interlock check, calibration guard, and 3-minute debounce.
         * @param cfg Tent setup configuration (checks `cfg.is_power_calibrated`).
         * @param state Current commanded relay snapshot.
         * @param pzem_measured Real power measured by PZEM-004T in W.
         * @param now Injected monotonic timestamp for deterministic testing.
         * @return Diagnostic @ref PowerAuditReport.
         */
        [[nodiscard]] PowerAuditReport evaluate(
            const domain::TentSetupConfig &cfg,
            const SystemActuatorSnapshot &state,
            domain::Watts pzem_measured,
            TimePoint now) noexcept
        {
            // Rule 1: Immediate Safety Hardware Interlock (0s delay, active even if uncalibrated)
            if (cfg.has_main_light && state.main_light_on)
            {
                for (std::size_t i = 0; i < cfg.num_heaters; ++i)
                {
                    if (state.heaters_on[i])
                    {
                        const domain::Watts theoretical = computeTheoreticalPower(cfg, state);
                        return PowerAuditReport{
                            .theoretical_power = theoretical,
                            .measured_power = pzem_measured,
                            .delta_watts = pzem_measured - theoretical,
                            .category = PowerFaultCategory::SafetyInterlockViolation,
                            .suspected_load = LoadKind::Heater,
                            .suspected_index = i};
                    }
                }
            }

            // Rule 2: Uncalibrated Guard
            if (!cfg.is_power_calibrated)
            {
                divergence_start_.reset();
                return PowerAuditReport{
                    .theoretical_power = domain::Watts{0.0},
                    .measured_power = pzem_measured,
                    .delta_watts = pzem_measured,
                    .category = PowerFaultCategory::UncalibratedSkip,
                    .suspected_load = LoadKind::None,
                    .suspected_index = 0};
            }

            const domain::Watts theoretical = computeTheoreticalPower(cfg, state);
            const domain::Watts delta = pzem_measured - theoretical;
            const domain::Watts tolerance = computeDynamicTolerance(theoretical);

            // Rule 3 & 4: Debounce and Divergence Classification
            if (std::abs(delta.value) > tolerance.value)
            {
                if (!divergence_start_.has_value())
                {
                    divergence_start_ = now;
                }

                if ((now - *divergence_start_) >= kDebounceDuration)
                {
                    const auto category = (delta.value > 0.0)
                                              ? PowerFaultCategory::OverPowerStuckRelay
                                              : PowerFaultCategory::UnderPowerOpenLoad;
                    const auto [kind, idx] = identifySuspectedLoad(cfg, state, delta.value);

                    return PowerAuditReport{
                        .theoretical_power = theoretical,
                        .measured_power = pzem_measured,
                        .delta_watts = delta,
                        .category = category,
                        .suspected_load = kind,
                        .suspected_index = idx};
                }
            }
            else
            {
                divergence_start_.reset();
            }

            return PowerAuditReport{
                .theoretical_power = theoretical,
                .measured_power = pzem_measured,
                .delta_watts = delta,
                .category = PowerFaultCategory::None,
                .suspected_load = LoadKind::None,
                .suspected_index = 0};
        }

    private:
        std::optional<TimePoint> divergence_start_{std::nullopt}; ///< Latch timestamp for 3-minute anomaly debounce.
    };

} // namespace grow::services
