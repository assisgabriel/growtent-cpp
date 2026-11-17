#pragma once
#include <compare>
#include <cstddef>
#include <cstdint>

/**
 * @file Types.hpp
 * @brief Zero-cost strong physical units, identifiers, and hardware capacity limits.
 *
 * This header defines domain-specific strong types and compile-time constants for the
 * Grow Tent OS core. Strong types prevent accidental unit mismatches (such as adding
 * temperature to relative humidity) at compile time while incurring zero runtime or
 * memory overhead compared to raw arithmetic types.
 */

namespace grow::domain
{

    // ============================================================================
    // 1. Zero-Cost Strong Physical Units
    // ============================================================================

    /**
     * @struct Celsius
     * @brief Temperature in degrees Celsius (°C).
     */
    struct Celsius
    {
        double value{}; ///< Temperature value in degrees Celsius.

        constexpr auto operator<=>(const Celsius &) const = default;

        [[nodiscard]] constexpr Celsius operator+(Celsius other) const noexcept
        {
            return Celsius{value + other.value};
        }

        [[nodiscard]] constexpr Celsius operator-(Celsius other) const noexcept
        {
            return Celsius{value - other.value};
        }

        constexpr Celsius &operator+=(Celsius other) noexcept
        {
            value += other.value;
            return *this;
        }

        constexpr Celsius &operator-=(Celsius other) noexcept
        {
            value -= other.value;
            return *this;
        }
    };

    /**
     * @struct Watts
     * @brief Active electrical power consumption in Watts (W).
     */
    struct Watts
    {
        double value{}; ///< Power consumption value in Watts.

        constexpr auto operator<=>(const Watts &) const = default;

        [[nodiscard]] constexpr Watts operator+(Watts other) const noexcept
        {
            return Watts{value + other.value};
        }

        [[nodiscard]] constexpr Watts operator-(Watts other) const noexcept
        {
            return Watts{value - other.value};
        }

        constexpr Watts &operator+=(Watts other) noexcept
        {
            value += other.value;
            return *this;
        }

        constexpr Watts &operator-=(Watts other) noexcept
        {
            value -= other.value;
            return *this;
        }
    };

    /**
     * @struct RelativeHumidity
     * @brief Ambient relative humidity percentage (0.0% to 100.0% RH).
     */
    struct RelativeHumidity
    {
        double value{}; ///< Relative humidity percentage.

        constexpr auto operator<=>(const RelativeHumidity &) const = default;

        [[nodiscard]] constexpr RelativeHumidity operator+(RelativeHumidity other) const noexcept
        {
            return RelativeHumidity{value + other.value};
        }

        [[nodiscard]] constexpr RelativeHumidity operator-(RelativeHumidity other) const noexcept
        {
            return RelativeHumidity{value - other.value};
        }

        constexpr RelativeHumidity &operator+=(RelativeHumidity other) noexcept
        {
            value += other.value;
            return *this;
        }

        constexpr RelativeHumidity &operator-=(RelativeHumidity other) noexcept
        {
            value -= other.value;
            return *this;
        }
    };

    /**
     * @struct SoilMoisture
     * @brief Volumetric substrate/soil water content percentage (0.0% to 100.0%).
     */
    struct SoilMoisture
    {
        double value{}; ///< Soil moisture percentage.

        constexpr auto operator<=>(const SoilMoisture &) const = default;

        [[nodiscard]] constexpr SoilMoisture operator+(SoilMoisture other) const noexcept
        {
            return SoilMoisture{value + other.value};
        }

        [[nodiscard]] constexpr SoilMoisture operator-(SoilMoisture other) const noexcept
        {
            return SoilMoisture{value - other.value};
        }

        constexpr SoilMoisture &operator+=(SoilMoisture other) noexcept
        {
            value += other.value;
            return *this;
        }

        constexpr SoilMoisture &operator-=(SoilMoisture other) noexcept
        {
            value -= other.value;
            return *this;
        }
    };

    /**
     * @struct Kilopascal
     * @brief Atmospheric pressure or Vapor Pressure Deficit (VPD) in kilopascals (kPa).
     */
    struct Kilopascal
    {
        double value{}; ///< Pressure / VPD value in kilopascals.

        constexpr auto operator<=>(const Kilopascal &) const = default;

        [[nodiscard]] constexpr Kilopascal operator+(Kilopascal other) const noexcept
        {
            return Kilopascal{value + other.value};
        }

        [[nodiscard]] constexpr Kilopascal operator-(Kilopascal other) const noexcept
        {
            return Kilopascal{value - other.value};
        }

        constexpr Kilopascal &operator+=(Kilopascal other) noexcept
        {
            value += other.value;
            return *this;
        }

        constexpr Kilopascal &operator-=(Kilopascal other) noexcept
        {
            value -= other.value;
            return *this;
        }
    };

    /**
     * @struct CubicMeters
     * @brief Physical enclosure volume in cubic meters (m³).
     */
    struct CubicMeters
    {
        double value{}; ///< Volume value in cubic meters.

        constexpr auto operator<=>(const CubicMeters &) const = default;

        [[nodiscard]] constexpr CubicMeters operator+(CubicMeters other) const noexcept
        {
            return CubicMeters{value + other.value};
        }

        [[nodiscard]] constexpr CubicMeters operator-(CubicMeters other) const noexcept
        {
            return CubicMeters{value - other.value};
        }

        constexpr CubicMeters &operator+=(CubicMeters other) noexcept
        {
            value += other.value;
            return *this;
        }

        constexpr CubicMeters &operator-=(CubicMeters other) noexcept
        {
            value -= other.value;
            return *this;
        }
    };

    /**
     * @struct CubicMetersPerHour
     * @brief Volumetric airflow rate in cubic meters per hour (m³/h).
     */
    struct CubicMetersPerHour
    {
        double value{}; ///< Airflow rate value in cubic meters per hour.

        constexpr auto operator<=>(const CubicMetersPerHour &) const = default;

        [[nodiscard]] constexpr CubicMetersPerHour operator+(CubicMetersPerHour other) const noexcept
        {
            return CubicMetersPerHour{value + other.value};
        }

        [[nodiscard]] constexpr CubicMetersPerHour operator-(CubicMetersPerHour other) const noexcept
        {
            return CubicMetersPerHour{value - other.value};
        }

        constexpr CubicMetersPerHour &operator+=(CubicMetersPerHour other) noexcept
        {
            value += other.value;
            return *this;
        }

        constexpr CubicMetersPerHour &operator-=(CubicMetersPerHour other) noexcept
        {
            value -= other.value;
            return *this;
        }
    };

    /**
     * @struct Liters
     * @brief Liquid volume in liters (L).
     */
    struct Liters
    {
        double value{}; ///< Volume value in liters.

        constexpr auto operator<=>(const Liters &) const = default;

        [[nodiscard]] constexpr Liters operator+(Liters other) const noexcept
        {
            return Liters{value + other.value};
        }

        [[nodiscard]] constexpr Liters operator-(Liters other) const noexcept
        {
            return Liters{value - other.value};
        }

        constexpr Liters &operator+=(Liters other) noexcept
        {
            value += other.value;
            return *this;
        }

        constexpr Liters &operator-=(Liters other) noexcept
        {
            value -= other.value;
            return *this;
        }
    };

    /**
     * @struct LitersPerMinute
     * @brief Volumetric liquid flow rate in liters per minute (L/min).
     */
    struct LitersPerMinute
    {
        double value{}; ///< Flow rate value in liters per minute.

        constexpr auto operator<=>(const LitersPerMinute &) const = default;

        [[nodiscard]] constexpr LitersPerMinute operator+(LitersPerMinute other) const noexcept
        {
            return LitersPerMinute{value + other.value};
        }

        [[nodiscard]] constexpr LitersPerMinute operator-(LitersPerMinute other) const noexcept
        {
            return LitersPerMinute{value - other.value};
        }

        constexpr LitersPerMinute &operator+=(LitersPerMinute other) noexcept
        {
            value += other.value;
            return *this;
        }

        constexpr LitersPerMinute &operator-=(LitersPerMinute other) noexcept
        {
            value -= other.value;
            return *this;
        }
    };

    // ============================================================================
    // 2. Strong Plant Identifier
    // ============================================================================

    /**
     * @struct PlantId
     * @brief Strongly-typed unique identifier for a plant cultivation zone.
     *
     * Represents plant zone indices (1..HardwareLimits::kMaxPlants). Unlike physical units,
     * PlantId intentionally omits arithmetic operators (+, -) because mathematical operations
     * between plant IDs are semantically meaningless.
     */
    struct PlantId
    {
        std::uint16_t value{}; ///< Identifier numeric value.

        constexpr auto operator<=>(const PlantId &) const = default;
    };

    // ============================================================================
    // 3. User-Defined Literals
    // ============================================================================

    /**
     * @namespace grow::domain::literals
     * @brief User-defined literals for expressive, compile-time physical unit construction.
     *
     * Enables idiomatic syntax such as:
     * @code
     * using namespace grow::domain::literals;
     * constexpr auto ambient_temp = 24.5_C;
     * constexpr auto light_power  = 280.0_W;
     * constexpr auto target_rh    = 60.0_RH;
     * @endcode
     */
    namespace literals
    {

        /**
         * @brief Literal operator for temperature in degrees Celsius (°C).
         * @param v Raw floating-point value.
         * @return Celsius instance.
         */
        constexpr Celsius operator""_C(long double v) noexcept
        {
            return Celsius{static_cast<double>(v)};
        }

        /**
         * @brief Literal operator for active electrical power in Watts (W).
         * @param v Raw floating-point value.
         * @return Watts instance.
         */
        constexpr Watts operator""_W(long double v) noexcept
        {
            return Watts{static_cast<double>(v)};
        }

        /**
         * @brief Literal operator for relative humidity percentage (% RH).
         * @param v Raw floating-point value.
         * @return RelativeHumidity instance.
         */
        constexpr RelativeHumidity operator""_RH(long double v) noexcept
        {
            return RelativeHumidity{static_cast<double>(v)};
        }

        /**
         * @brief Literal operator for soil moisture percentage (%).
         * @param v Raw floating-point value.
         * @return SoilMoisture instance.
         */
        constexpr SoilMoisture operator""_Soil(long double v) noexcept
        {
            return SoilMoisture{static_cast<double>(v)};
        }

        /**
         * @brief Literal operator for pressure or VPD in kilopascals (kPa).
         * @param v Raw floating-point value.
         * @return Kilopascal instance.
         */
        constexpr Kilopascal operator""_kPa(long double v) noexcept
        {
            return Kilopascal{static_cast<double>(v)};
        }

        /**
         * @brief Literal operator for enclosure volume in cubic meters (m³).
         * @param v Raw floating-point value.
         * @return CubicMeters instance.
         */
        constexpr CubicMeters operator""_m3(long double v) noexcept
        {
            return CubicMeters{static_cast<double>(v)};
        }

        /**
         * @brief Literal operator for volumetric airflow rate in cubic meters per hour (m³/h).
         * @param v Raw floating-point value.
         * @return CubicMetersPerHour instance.
         */
        constexpr CubicMetersPerHour operator""_m3h(long double v) noexcept
        {
            return CubicMetersPerHour{static_cast<double>(v)};
        }

        /**
         * @brief Literal operator for liquid volume in liters (L).
         * @param v Raw floating-point value.
         * @return Liters instance.
         */
        constexpr Liters operator""_L(long double v) noexcept
        {
            return Liters{static_cast<double>(v)};
        }

        /**
         * @brief Literal operator for volumetric liquid flow rate in liters per minute (L/min).
         * @param v Raw floating-point value.
         * @return LitersPerMinute instance.
         */
        constexpr LitersPerMinute operator""_Lmin(long double v) noexcept
        {
            return LitersPerMinute{static_cast<double>(v)};
        }

    } // namespace literals

    // ============================================================================
    // 4. Climate Sensor Telemetry & Static Hardware Limits
    // ============================================================================

    /**
     * @struct ClimateSensorReading
     * @brief Paired temperature and relative humidity measurement from a thermo-hygrometer.
     *
     * Used for both internal (canopy) and external (lung room / ambient) sensor telemetry.
     */
    struct ClimateSensorReading
    {
        Celsius temp{};        ///< Temperature measurement.
        RelativeHumidity rh{}; ///< Relative humidity measurement.
    };

    /**
     * @struct HardwareLimits
     * @brief Static compile-time hardware capacity limits.
     *
     * Enforces the zero-heap allocation constraint across the domain core by sizing
     * fixed-capacity collections (e.g., std::array) at compile time.
     */
    struct HardwareLimits
    {
        static constexpr std::size_t kMaxInternalFans = 3; ///< Maximum number of internal circulation/clip fans (0..3).
        static constexpr std::size_t kMaxAuxLights = 2;    ///< Maximum number of auxiliary spectral LED bars (0..2).
        static constexpr std::size_t kMaxHeaters = 2;      ///< Maximum number of ceramic heating elements (0..2).
        static constexpr std::size_t kMaxPlants = 8;       ///< Maximum number of independent plant irrigation zones (1..8).
    };

} // namespace grow::domain
