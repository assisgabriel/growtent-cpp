/**
 * @file TentSetupConfig.hpp
 * @brief Domain configuration for grow-tent geometry, hardware topology, and learned power ratings.
 * @details Defines the setup types and validation used to provision a tent before control and power
 *          calibration. Electrical load ratings are learned by the PowerCalibrator.
 */

#pragma once
#include "domain/Types.hpp"
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace grow::domain
{

    /**
     * @enum IntakeMode
     * @brief Air intake operating mode configured during enclosure setup.
     */
    enum class IntakeMode
    {
        Passive, ///< Negative-pressure passive mesh vent (no relay, 0.0 W).
        Active   ///< Motorized On/Off intake fan controlled via relay.
    };

    /**
     * @enum AuxLightMode
     * @brief Spectral synchronization schedule for auxiliary LED bars.
     */
    enum class AuxLightMode
    {
        FollowMainLight_660nm, ///< Deep Red (660nm): turns ON and OFF synchronously with the main light.
        SunsetLag15Min_730nm   ///< Far Red (730nm Emerson Effect): ON at [t_off - 15m, t_off + 15m).
    };

    /**
     * @struct TentDimensionsCm
     * @brief Physical internal dimensions of the cultivation enclosure in centimeters.
     */
    struct TentDimensionsCm
    {
        double width_cm{60.0};   ///< Enclosure width in cm.
        double depth_cm{60.0};   ///< Enclosure depth in cm.
        double height_cm{180.0}; ///< Enclosure height in cm.

        /**
         * @brief Computes internal enclosure air volume in cubic meters (m³).
         * @return Volume computed as `(width_cm * depth_cm * height_cm) / 1'000'000.0`.
         */
        [[nodiscard]] constexpr CubicMeters volumeM3() const noexcept
        {
            return CubicMeters{(width_cm * depth_cm * height_cm) / 1'000'000.0};
        }
    };

    /**
     * @struct ExhaustSetup
     * @brief Exhaust fan airflow specification and calibrated electrical power rating.
     */
    struct ExhaustSetup
    {
        CubicMetersPerHour airflow_m3h{180.0}; ///< Nominal/effective exhaust airflow rate (m³/h).
        double target_air_exchanges{5.0};      ///< Target full volume air exchanges per O2 refresh cycle (1h).
        double safety_margin_ratio{0.40};      ///< Pressure-drop safety margin (e.g., 0.40 = +40% for carbon filter).
        Watts power_rating{0.0};               ///< Learned active power in W (populated by PowerCalibrator).

        /**
         * @brief Computes dynamic ON purge duration required to renew tent air volume.
         * @param tent_volume Internal tent volume in m³.
         * @return Duration in seconds, or 60 seconds when airflow or tent volume is non-positive.
         */
        [[nodiscard]] std::chrono::seconds calculatePurgeDuration(CubicMeters tent_volume) const noexcept
        {
            if (airflow_m3h.value <= 0.0 || tent_volume.value <= 0.0)
            {
                return std::chrono::seconds(60);
            }

            double raw_seconds = ((tent_volume.value * target_air_exchanges) / airflow_m3h.value) * 3600.0 * (1.0 + safety_margin_ratio);
            return std::chrono::seconds(static_cast<std::int64_t>(std::ceil(raw_seconds)));
        }
    };

    /**
     * @struct AuxLightConfig
     * @brief Configuration for an individual auxiliary spectral LED bar.
     */
    struct AuxLightConfig
    {
        bool enabled{false};                                    ///< Whether this auxiliary channel is installed.
        AuxLightMode mode{AuxLightMode::FollowMainLight_660nm}; ///< Spectral timing behavior.
        Watts power_rating{0.0};                                ///< Learned active power in W (from PowerCalibrator).
    };

    /**
     * @struct PlantSetupConfig
     * @brief Per-plant irrigation hardware configuration (1..kMaxPlants).
     */
    struct PlantSetupConfig
    {
        PlantId id{0};                       ///< Plant zone ID (0 = inactive slot; valid = 1..8).
        Watts pump_power_rating{0.0};        ///< Learned pump power in W (from PowerCalibrator).
        LitersPerMinute pump_flow_rate{1.5}; ///< Nominal pump hydraulic flow rate in L/min.
    };

    /**
     * @struct TentSetupConfig
     * @brief Master configuration object holding user-selected hardware topology and calibrated wattages.
     */
    struct TentSetupConfig
    {
        TentDimensionsCm dimensions{}; ///< Enclosure physical dimensions.
        ExhaustSetup exhaust{};        ///< Primary exhaust fan configuration.

        IntakeMode intake_mode{IntakeMode::Active}; ///< Passive vs. Active air intake mode.
        Watts intake_power{0.0};                    ///< Learned intake fan power in W (if Active).

        bool has_main_light{true};   ///< Whether a primary grow light panel is installed.
        Watts base_idle_power{0.0};  ///< Learned standby controller power in W (all relays OFF).
        Watts main_light_power{0.0}; ///< Learned primary LED panel power in W.

        bool is_power_calibrated{false}; ///< True only after PowerCalibrator completes step-response learning.

        std::size_t num_aux_lights{0}; ///< Number of active auxiliary LED bars (0..2).
        std::array<AuxLightConfig, HardwareLimits::kMaxAuxLights> aux_lights{};

        std::size_t num_heaters{1}; ///< Number of active ceramic heaters (0..2).
        std::array<Watts, HardwareLimits::kMaxHeaters> heater_powers{};

        std::size_t num_internal_fans{1}; ///< Number of active internal clip fans (0..3).
        std::array<Watts, HardwareLimits::kMaxInternalFans> internal_fan_powers{};

        std::size_t num_plants{1}; ///< Number of active plant zones (1..8).
        std::array<PlantSetupConfig, HardwareLimits::kMaxPlants> plants{
            PlantSetupConfig{.id = PlantId{1}, .pump_power_rating = Watts{0.0}, .pump_flow_rate = LitersPerMinute{1.5}}};

        /**
         * @brief Validates the configured hardware topology and active plant identifiers.
         * @details Requires positive enclosure dimensions, volume, and exhaust airflow; hardware
         *          counts within their fixed capacities; and a unique ID in the range
         *          `1..HardwareLimits::kMaxPlants` for every active plant.
         * @return true when the configuration satisfies these invariants; otherwise, false.
         */
        [[nodiscard]] constexpr bool isValid() const noexcept
        {
            if (!(dimensions.width_cm > 0.0) ||
                !(dimensions.depth_cm > 0.0) ||
                !(dimensions.height_cm > 0.0) ||
                !(dimensions.volumeM3().value > 0.0) ||
                !(exhaust.airflow_m3h.value > 0.0) ||
                num_aux_lights > HardwareLimits::kMaxAuxLights ||
                num_heaters > HardwareLimits::kMaxHeaters ||
                num_internal_fans > HardwareLimits::kMaxInternalFans ||
                num_plants < 1 || num_plants > HardwareLimits::kMaxPlants)
            {
                return false;
            }

            for (std::size_t i = 0; i < num_plants; ++i)
            {
                const auto plant_id = plants[i].id.value;
                if (plant_id == 0U || plant_id > HardwareLimits::kMaxPlants)
                {
                    return false;
                }

                for (std::size_t j = i + 1; j < num_plants; ++j)
                {
                    if (plant_id == plants[j].id.value)
                    {
                        return false;
                    }
                }
            }

            return true;
        }
    };

} // namespace grow::domain
