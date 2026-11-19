/**
 * @file PowerCalibrator.hpp
 * @brief Automated step-response electrical load calibrator for Stage 0 commissioning.
 * @details Measures controller idle baseline and sequentially energizes each configured
 *          actuator in isolation to learn exact active power ratings without manual user entry.
 */

#pragma once

#include "domain/TentSetupConfig.hpp"
#include "domain/Types.hpp"
#include "services/PowerAuditor.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <numeric>
#include <optional>
#include <span>

namespace grow::services
{

    /**
     * @enum CalibrationStatus
     * @brief Execution status returned after recording a step sample window.
     */
    enum class CalibrationStatus
    {
        InProgress,           ///< Step recorded successfully; proceed to next target in queue.
        CompletedOk,          ///< All configured loads calibrated; cfg.is_power_calibrated set to true.
        FailedUnstableIdle,   ///< Empty sample buffer or invalid session state.
        FailedNoPowerDetected ///< Energized load drew less than kMinDetectableLoad above idle (burned/open).
    };

    /**
     * @struct CalibrationTarget
     * @brief Identifies a single step in the sequential calibration queue.
     */
    struct CalibrationTarget
    {
        LoadKind kind{LoadKind::None}; ///< Target actuator family (or IdleBaseline for step 0).
        std::size_t index{0};          ///< Zero-based actuator index within its family.
    };

    /**
     * @class PowerCalibrator
     * @brief Zero-heap-allocation sequential step-response calibrator populating TentSetupConfig.
     */
    class PowerCalibrator
    {
    public:
        /// Maximum possible calibration targets: 1 Idle + 1 MainLight + 2 Aux + 2 Heaters + 1 Exhaust + 1 Intake + 3 Fans + 8 Pumps.
        static constexpr std::size_t kMaxCalibrationSteps = 19;
        static constexpr domain::Watts kMinDetectableLoad{2.0};             ///< Minimum net power increase to validate an ON load.
        static constexpr auto kInrushSettleDelay = std::chrono::seconds(5); ///< Settling delay to bypass startup inrush current.

        /**
         * @brief Builds the static calibration step queue from enabled hardware in `cfg`.
         * @param cfg Current tent topology configuration.
         * @param include_pumps If true, includes 1..num_plants water pumps (default false to prevent dry-running).
         */
        void startSession(const domain::TentSetupConfig &cfg, bool include_pumps = false) noexcept
        {
            step_count_ = 0;
            current_step_ = 0;
            measured_idle_ = domain::Watts{0.0};

            steps_[step_count_++] = CalibrationTarget{LoadKind::IdleBaseline, 0};

            if (cfg.has_main_light)
            {
                steps_[step_count_++] = CalibrationTarget{LoadKind::MainLight, 0};
            }

            for (std::size_t i = 0; i < cfg.num_aux_lights; ++i)
            {
                if (cfg.aux_lights[i].enabled)
                {
                    steps_[step_count_++] = CalibrationTarget{LoadKind::AuxLight, i};
                }
            }

            for (std::size_t i = 0; i < cfg.num_heaters; ++i)
            {
                steps_[step_count_++] = CalibrationTarget{LoadKind::Heater, i};
            }

            steps_[step_count_++] = CalibrationTarget{LoadKind::ExhaustFan, 0};

            if (cfg.intake_mode == domain::IntakeMode::Active)
            {
                steps_[step_count_++] = CalibrationTarget{LoadKind::IntakeFan, 0};
            }

            for (std::size_t i = 0; i < cfg.num_internal_fans; ++i)
            {
                steps_[step_count_++] = CalibrationTarget{LoadKind::InternalFan, i};
            }

            if (include_pumps)
            {
                for (std::size_t i = 0; i < cfg.num_plants; ++i)
                {
                    steps_[step_count_++] = CalibrationTarget{LoadKind::PlantPump, i};
                }
            }
        }

        /**
         * @brief Generates the isolated relay command for the active calibration step.
         * @return SystemActuatorSnapshot with at most ONE actuator set to true (all false during IdleBaseline).
         */
        [[nodiscard]] SystemActuatorSnapshot currentStepActuatorCommand() const noexcept
        {
            SystemActuatorSnapshot cmd{};

            if (current_step_ >= step_count_)
            {
                return cmd;
            }

            const auto target = steps_[current_step_];
            switch (target.kind)
            {
            case LoadKind::IdleBaseline:
                break;
            case LoadKind::MainLight:
                cmd.main_light_on = true;
                break;
            case LoadKind::AuxLight:
                cmd.aux_lights_on[target.index] = true;
                break;
            case LoadKind::Heater:
                cmd.heaters_on[target.index] = true;
                break;
            case LoadKind::ExhaustFan:
                cmd.exhaust_fan_on = true;
                break;
            case LoadKind::IntakeFan:
                cmd.intake_fan_on = true;
                break;
            case LoadKind::InternalFan:
                cmd.internal_fans_on[target.index] = true;
                break;
            case LoadKind::PlantPump:
                cmd.plant_pumps_on[target.index] = true;
                break;
            default:
                break;
            }

            return cmd;
        }

        /**
         * @brief Processes stabilized PZEM-004T samples for the current step and updates `cfg_to_update`.
         * @param pzem_samples Non-owning span of stabilized wattage readings collected after kInrushSettleDelay.
         * @param[in,out] cfg_to_update Configuration struct populated with learned wattages and is_power_calibrated.
         * @return Updated CalibrationStatus (InProgress, CompletedOk, or failure code).
         */
        [[nodiscard]] CalibrationStatus recordStepSamples(
            std::span<const domain::Watts> pzem_samples,
            domain::TentSetupConfig &cfg_to_update) noexcept
        {
            if (pzem_samples.empty() || current_step_ >= step_count_)
            {
                return CalibrationStatus::FailedUnstableIdle;
            }

            double sum = 0.0;
            for (const auto &sample : pzem_samples)
            {
                sum += sample.value;
            }
            const domain::Watts avg_watts{sum / static_cast<double>(pzem_samples.size())};

            const auto target = steps_[current_step_];

            if (target.kind == LoadKind::IdleBaseline)
            {
                measured_idle_ = avg_watts;
                cfg_to_update.base_idle_power = avg_watts;
            }
            else
            {
                const domain::Watts net_load{avg_watts.value - measured_idle_.value};
                if (net_load.value < kMinDetectableLoad.value)
                {
                    return CalibrationStatus::FailedNoPowerDetected;
                }

                switch (target.kind)
                {
                case LoadKind::MainLight:
                    cfg_to_update.main_light_power = net_load;
                    break;
                case LoadKind::AuxLight:
                    cfg_to_update.aux_lights[target.index].power_rating = net_load;
                    break;
                case LoadKind::Heater:
                    cfg_to_update.heater_powers[target.index] = net_load;
                    break;
                case LoadKind::ExhaustFan:
                    cfg_to_update.exhaust.power_rating = net_load;
                    break;
                case LoadKind::IntakeFan:
                    cfg_to_update.intake_power = net_load;
                    break;
                case LoadKind::InternalFan:
                    cfg_to_update.internal_fan_powers[target.index] = net_load;
                    break;
                case LoadKind::PlantPump:
                    cfg_to_update.plants[target.index].pump_power_rating = net_load;
                    break;
                default:
                    break;
                }
            }

            ++current_step_;

            if (current_step_ == step_count_)
            {
                cfg_to_update.is_power_calibrated = true;
                return CalibrationStatus::CompletedOk;
            }

            return CalibrationStatus::InProgress;
        }

        /**
         * @brief Returns the target currently awaiting sample recording, or std::nullopt if finished.
         * @return Optional calibration target.
         */
        [[nodiscard]] std::optional<CalibrationTarget> currentTarget() const noexcept
        {
            if (current_step_ >= step_count_)
            {
                return std::nullopt;
            }
            return steps_[current_step_];
        }

    private:
        std::array<CalibrationTarget, kMaxCalibrationSteps> steps_{}; ///< Static queue of targets to calibrate.
        std::size_t step_count_{0};                                   ///< Total number of targets in current session.
        std::size_t current_step_{0};                                 ///< Zero-based index of active target.
        domain::Watts measured_idle_{0.0};                            ///< Learned standby baseline power in W.
    };

} // namespace grow::services
