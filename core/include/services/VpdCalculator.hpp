/**
 * @file VpdCalculator.hpp
 * @brief Stateless Magnus-Tetens psychrometric engine for Internal and External climate sensors.
 */

#pragma once
#include "domain/Types.hpp"
#include <algorithm>
#include <cmath>
#include <optional>

namespace grow::services
{

    /**
     * @struct PsychrometricReport
     * @brief Consolidated psychrometric state for a climate zone.
     */
    struct PsychrometricReport
    {
        domain::Celsius temp{};        ///< Dry-bulb air temperature.
        domain::RelativeHumidity rh{}; ///< Relative humidity.
        domain::Kilopascal vpd{};      ///< Vapor Pressure Deficit.
        domain::Celsius dew_point{};   ///< Calculated dew point temperature.
    };

    /**
     * @class VpdCalculator
     * @brief Pure, deterministic calculation engine for environmental psychrometrics.
     */
    class VpdCalculator
    {
    public:
        /**
         * @brief Calculates Saturation Vapor Pressure (SVP) using the Tetens/Buck equation.
         * @param temp Air or leaf temperature in °C.
         * @return Saturation vapor pressure in kPa.
         */
        [[nodiscard]] static domain::Kilopascal calculateSvpKpa(domain::Celsius temp) noexcept
        {
            const double t = temp.value;
            return domain::Kilopascal{0.61078 * std::exp((17.27 * t) / (t + 237.3))};
        }

        /**
         * @brief Calculates Actual Vapor Pressure (AVP) of ambient air.
         * @param temp Dry-bulb air temperature in °C.
         * @param rh Relative humidity (0.0% to 100.0%).
         * @return Actual vapor pressure in kPa.
         */
        [[nodiscard]] static domain::Kilopascal calculateAvpKpa(domain::Celsius temp, domain::RelativeHumidity rh) noexcept
        {
            return domain::Kilopascal{calculateSvpKpa(temp).value * (rh.value / 100.0)};
        }

        /**
         * @brief Computes Vapor Pressure Deficit (VPD).
         * @param air_temp Dry-bulb air temperature (-20.0 °C to 70.0 °C).
         * @param rh Relative humidity (0.0% < RH <= 100.0%).
         * @param leaf_offset Temperature offset between leaf and air (T_leaf = T_air + leaf_offset).
         * @return VPD in kPa, or std::nullopt if input measurements are out of physical bounds.
         */
        [[nodiscard]] static std::optional<domain::Kilopascal> calculateVpd(
            domain::Celsius air_temp,
            domain::RelativeHumidity rh,
            domain::Celsius leaf_offset = domain::Celsius{0.0}) noexcept
        {
            if (std::isnan(air_temp.value) || std::isnan(rh.value) || std::isnan(leaf_offset.value) ||
                rh.value <= 0.0 || rh.value > 100.0 || air_temp.value < -20.0 || air_temp.value > 70.0)
            {
                return std::nullopt;
            }

            const domain::Celsius leaf_temp = air_temp + leaf_offset;
            const domain::Kilopascal svp_leaf = calculateSvpKpa(leaf_temp);
            const domain::Kilopascal avp_air = calculateAvpKpa(air_temp, rh);

            const double raw_vpd = svp_leaf.value - avp_air.value;
            return domain::Kilopascal{std::max(0.0, raw_vpd)};
        }

        /**
         * @brief Computes Dew Point temperature using the Magnus-Tetens approximation.
         * @param air_temp Dry-bulb air temperature.
         * @param rh Relative humidity.
         * @return Dew point temperature in °C.
         */
        [[nodiscard]] static domain::Celsius calculateDewPoint(domain::Celsius air_temp, domain::RelativeHumidity rh) noexcept
        {
            const double t = air_temp.value;
            const double r = rh.value;
            constexpr double a = 17.27;
            constexpr double b = 237.7;

            const double alpha = ((a * t) / (b + t)) + std::log(r / 100.0);
            const double dew_point_val = (b * alpha) / (a - alpha);

            return domain::Celsius{dew_point_val};
        }

        /**
         * @brief Evaluates raw sensor pair telemetry and generates a full psychrometric report.
         * @param reading Paired temperature and relative humidity reading.
         * @param leaf_offset Leaf-to-air temperature offset.
         * @return PsychrometricReport if reading is physically valid, std::nullopt otherwise.
         */
        [[nodiscard]] static std::optional<PsychrometricReport> evaluateSensorPair(
            const domain::ClimateSensorReading &reading,
            domain::Celsius leaf_offset = domain::Celsius{0.0}) noexcept
        {
            const auto vpd = calculateVpd(reading.temp, reading.rh, leaf_offset);
            if (!vpd.has_value())
            {
                return std::nullopt;
            }

            const domain::Celsius dew_point = calculateDewPoint(reading.temp, reading.rh);

            return PsychrometricReport{
                .temp = reading.temp,
                .rh = reading.rh,
                .vpd = *vpd,
                .dew_point = dew_point};
        }
    };

} // namespace grow::services
