#include <gtest/gtest.h>
#include "domain/Types.hpp"
#include "domain/TentSetupConfig.hpp"
#include "services/VpdCalculator.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>

template <typename Quantity>
constexpr bool hasExpectedQuantityArithmetic() noexcept
{
    Quantity accumulated{2.0};
    accumulated += Quantity{3.0};
    accumulated -= Quantity{1.0};

    return (Quantity{2.0} + Quantity{3.0}).value == 5.0 &&
           (Quantity{3.0} - Quantity{2.0}).value == 1.0 &&
           accumulated.value == 4.0 &&
           Quantity{3.0} > Quantity{2.0};
}

static_assert(hasExpectedQuantityArithmetic<grow::domain::Celsius>());
static_assert(hasExpectedQuantityArithmetic<grow::domain::Watts>());
static_assert(hasExpectedQuantityArithmetic<grow::domain::RelativeHumidity>());
static_assert(hasExpectedQuantityArithmetic<grow::domain::SoilMoisture>());
static_assert(hasExpectedQuantityArithmetic<grow::domain::Kilopascal>());
static_assert(hasExpectedQuantityArithmetic<grow::domain::CubicMeters>());
static_assert(hasExpectedQuantityArithmetic<grow::domain::CubicMetersPerHour>());
static_assert(hasExpectedQuantityArithmetic<grow::domain::Liters>());
static_assert(hasExpectedQuantityArithmetic<grow::domain::LitersPerMinute>());
static_assert(grow::domain::PlantId{1} < grow::domain::PlantId{2});
static_assert(grow::domain::PlantId{0} != grow::domain::PlantId{1});

TEST(DomainTypesTest, UserDefinedLiteralsPreserveQuantityValues)
{
    using namespace grow::domain::literals;

    EXPECT_DOUBLE_EQ((23.5_C).value, 23.5);
    EXPECT_DOUBLE_EQ((280.0_W).value, 280.0);
    EXPECT_DOUBLE_EQ((60.0_RH).value, 60.0);
    EXPECT_DOUBLE_EQ((45.0_Soil).value, 45.0);
    EXPECT_DOUBLE_EQ((1.2_kPa).value, 1.2);
    EXPECT_DOUBLE_EQ((0.648_m3).value, 0.648);
    EXPECT_DOUBLE_EQ((180.0_m3h).value, 180.0);
    EXPECT_DOUBLE_EQ((2.5_L).value, 2.5);
    EXPECT_DOUBLE_EQ((1.5_Lmin).value, 1.5);
}

TEST(TentSetupConfigTest, CalculatesEnclosureVolume)
{
    using namespace grow::domain;

    const TentDimensionsCm dimensions{
        .width_cm = 60.0,
        .depth_cm = 60.0,
        .height_cm = 180.0
    };

    EXPECT_NEAR(dimensions.volumeM3().value, 0.648, 0.000001);
}

TEST(TentSetupConfigTest, CalculatesPurgeDurationAndUsesFallbackForInvalidInputs)
{
    using namespace grow::domain;

    ExhaustSetup exhaust{};
    EXPECT_EQ(exhaust.calculatePurgeDuration(CubicMeters{0.648}), std::chrono::seconds{91});
    EXPECT_EQ(exhaust.calculatePurgeDuration(CubicMeters{0.0}), std::chrono::seconds{60});

    exhaust.airflow_m3h = CubicMetersPerHour{0.0};
    EXPECT_EQ(exhaust.calculatePurgeDuration(CubicMeters{0.648}), std::chrono::seconds{60});
}

TEST(TentSetupConfigTest, StartsUncalibratedWithZeroPowerRatings)
{
    using namespace grow::domain;

    const TentSetupConfig config{};
    EXPECT_FALSE(config.is_power_calibrated);
    EXPECT_DOUBLE_EQ(config.exhaust.power_rating.value, 0.0);
    EXPECT_DOUBLE_EQ(config.intake_power.value, 0.0);
    EXPECT_DOUBLE_EQ(config.base_idle_power.value, 0.0);
    EXPECT_DOUBLE_EQ(config.main_light_power.value, 0.0);

    for (const auto &light : config.aux_lights)
    {
        EXPECT_DOUBLE_EQ(light.power_rating.value, 0.0);
    }
    for (const auto &heater_power : config.heater_powers)
    {
        EXPECT_DOUBLE_EQ(heater_power.value, 0.0);
    }
    for (const auto &fan_power : config.internal_fan_powers)
    {
        EXPECT_DOUBLE_EQ(fan_power.value, 0.0);
    }
    for (const auto &plant : config.plants)
    {
        EXPECT_DOUBLE_EQ(plant.pump_power_rating.value, 0.0);
    }
}

TEST(TentSetupConfigTest, AcceptsUniquePlantIdsWithinRange)
{
    using namespace grow::domain;

    TentSetupConfig config{};
    config.num_plants = 2;
    config.plants[0].id = PlantId{1};
    config.plants[1].id = PlantId{HardwareLimits::kMaxPlants};

    EXPECT_TRUE(config.isValid());
}

TEST(TentSetupConfigTest, RejectsDuplicatePlantIds)
{
    using namespace grow::domain;

    TentSetupConfig config{};
    config.num_plants = 2;
    config.plants[0].id = PlantId{1};
    config.plants[1].id = PlantId{1};

    EXPECT_FALSE(config.isValid());
}

TEST(TentSetupConfigTest, RejectsPlantIdsOutsideValidRange)
{
    using namespace grow::domain;

    TentSetupConfig config{};
    config.plants[0].id = PlantId{0};
    EXPECT_FALSE(config.isValid());

    config.plants[0].id = PlantId{static_cast<std::uint16_t>(HardwareLimits::kMaxPlants + 1)};
    EXPECT_FALSE(config.isValid());

    config.num_plants = 2;
    config.plants[0].id = PlantId{1};
    config.plants[1].id = PlantId{static_cast<std::uint16_t>(HardwareLimits::kMaxPlants + 1)};
    EXPECT_FALSE(config.isValid());
}

TEST(TentSetupConfigTest, AcceptsMaximumHardwareCounts)
{
    using namespace grow::domain;

    TentSetupConfig config{};
    config.num_aux_lights = HardwareLimits::kMaxAuxLights;
    config.num_heaters = HardwareLimits::kMaxHeaters;
    config.num_internal_fans = HardwareLimits::kMaxInternalFans;
    config.num_plants = HardwareLimits::kMaxPlants;
    for (std::size_t i = 0; i < config.num_plants; ++i)
    {
        config.plants[i].id = PlantId{static_cast<std::uint16_t>(i + 1)};
    }

    EXPECT_TRUE(config.isValid());
}

TEST(TentSetupConfigTest, RejectsCountsOutsideHardwareLimits)
{
    using namespace grow::domain;

    TentSetupConfig config{};
    config.num_aux_lights = HardwareLimits::kMaxAuxLights + 1;
    EXPECT_FALSE(config.isValid());

    config = TentSetupConfig{};
    config.num_heaters = HardwareLimits::kMaxHeaters + 1;
    EXPECT_FALSE(config.isValid());

    config = TentSetupConfig{};
    config.num_internal_fans = HardwareLimits::kMaxInternalFans + 1;
    EXPECT_FALSE(config.isValid());

    config = TentSetupConfig{};
    config.num_plants = 0;
    EXPECT_FALSE(config.isValid());

    config.num_plants = HardwareLimits::kMaxPlants + 1;
    EXPECT_FALSE(config.isValid());
}

TEST(TentSetupConfigTest, RejectsNonPositiveAirflow)
{
    using namespace grow::domain;

    TentSetupConfig config{};
    config.exhaust.airflow_m3h = CubicMetersPerHour{0.0};

    EXPECT_FALSE(config.isValid());
}

TEST(TentSetupConfigTest, RejectsNonPositiveIndividualDimensions)
{
    using namespace grow::domain;

    TentSetupConfig config{};
    config.dimensions.width_cm = -60.0;
    config.dimensions.depth_cm = -60.0;

    EXPECT_FALSE(config.isValid());
}

TEST(VpdCalculatorTest, CalculatesSvpCorrectlyAtKnownTemperatures)
{
    using namespace grow::domain;
    using namespace grow::services;

    // A 0.0 °C, e^0 = 1.0 -> SVP deve ser exatamente 0.61078 kPa
    const auto svp_zero = VpdCalculator::calculateSvpKpa(Celsius{0.0});
    EXPECT_NEAR(svp_zero.value, 0.61078, 0.0001);

    // A 20.0 °C -> SVP é ~2.338 kPa
    const auto svp_20 = VpdCalculator::calculateSvpKpa(Celsius{20.0});
    EXPECT_NEAR(svp_20.value, 2.3382, 0.01);

    // A 25.0 °C -> SVP é ~3.167 kPa
    const auto svp_25 = VpdCalculator::calculateSvpKpa(Celsius{25.0});
    EXPECT_NEAR(svp_25.value, 3.1677, 0.01);
}

TEST(VpdCalculatorTest, CalculatesActualVaporPressure)
{
    using namespace grow::domain;
    using namespace grow::services;

    const auto avp = VpdCalculator::calculateAvpKpa(Celsius{25.0}, RelativeHumidity{60.0});
    EXPECT_NEAR(avp.value, 1.9006, 0.01);
}

TEST(VpdCalculatorTest, RejectsOutOfRangeAndInvalidReadings)
{
    using namespace grow::domain;
    using namespace grow::services;

    // NaN
    EXPECT_FALSE(VpdCalculator::calculateVpd(Celsius{std::numeric_limits<double>::quiet_NaN()}, RelativeHumidity{50.0}).has_value());
    EXPECT_FALSE(VpdCalculator::calculateVpd(Celsius{25.0}, RelativeHumidity{std::numeric_limits<double>::quiet_NaN()}).has_value());
    EXPECT_FALSE(VpdCalculator::calculateVpd(
                     Celsius{25.0},
                     RelativeHumidity{50.0},
                     Celsius{std::numeric_limits<double>::quiet_NaN()})
                     .has_value());

    // RH <= 0 ou > 100
    EXPECT_FALSE(VpdCalculator::calculateVpd(Celsius{25.0}, RelativeHumidity{0.0}).has_value());
    EXPECT_FALSE(VpdCalculator::calculateVpd(Celsius{25.0}, RelativeHumidity{-5.0}).has_value());
    EXPECT_FALSE(VpdCalculator::calculateVpd(Celsius{25.0}, RelativeHumidity{100.1}).has_value());

    // Temp < -20 ou > 70
    EXPECT_FALSE(VpdCalculator::calculateVpd(Celsius{-20.1}, RelativeHumidity{50.0}).has_value());
    EXPECT_FALSE(VpdCalculator::calculateVpd(Celsius{70.1}, RelativeHumidity{50.0}).has_value());

    // Limites exatos válidos
    EXPECT_TRUE(VpdCalculator::calculateVpd(Celsius{-20.0}, RelativeHumidity{0.1}).has_value());
    EXPECT_TRUE(VpdCalculator::calculateVpd(Celsius{70.0}, RelativeHumidity{100.0}).has_value());
}

TEST(VpdCalculatorTest, CalculatesVpdStandardConditionsAndLeafOffset)
{
    using namespace grow::domain;
    using namespace grow::services;

    // 25 °C, 60% RH sem offset foliar (SVP ~3.167 kPa, AVP ~1.900 kPa -> VPD ~1.267 kPa)
    const auto vpd_zero_offset = VpdCalculator::calculateVpd(Celsius{25.0}, RelativeHumidity{60.0});
    ASSERT_TRUE(vpd_zero_offset.has_value());
    EXPECT_NEAR(vpd_zero_offset->value, 1.267, 0.01);

    // Folha 2 °C mais fria (offset = -2.0 °C -> T_leaf = 23 °C)
    // SVP(23 °C) ~2.809 kPa, AVP(25 °C, 60%) ~1.900 kPa -> VPD ~0.908 kPa
    const auto vpd_cooler_leaf = VpdCalculator::calculateVpd(Celsius{25.0}, RelativeHumidity{60.0}, Celsius{-2.0});
    ASSERT_TRUE(vpd_cooler_leaf.has_value());
    EXPECT_NEAR(vpd_cooler_leaf->value, 0.908, 0.01);

    // Condensação (folha muito fria faz SVP_leaf < AVP_air -> clamp em 0.0)
    const auto vpd_clamped = VpdCalculator::calculateVpd(Celsius{25.0}, RelativeHumidity{95.0}, Celsius{-5.0});
    ASSERT_TRUE(vpd_clamped.has_value());
    EXPECT_DOUBLE_EQ(vpd_clamped->value, 0.0);
}

TEST(VpdCalculatorTest, CalculatesDewPointAccurately)
{
    using namespace grow::domain;
    using namespace grow::services;

    // 25.0 °C, 50.0% RH -> Ponto de orvalho ~13.84 °C
    const auto dp1 = VpdCalculator::calculateDewPoint(Celsius{25.0}, RelativeHumidity{50.0});
    EXPECT_NEAR(dp1.value, 13.84, 0.05);

    // 20.0 °C, 80.0% RH -> Ponto de orvalho ~16.44 °C
    const auto dp2 = VpdCalculator::calculateDewPoint(Celsius{20.0}, RelativeHumidity{80.0});
    EXPECT_NEAR(dp2.value, 16.44, 0.05);
}

TEST(VpdCalculatorTest, EvaluatesSensorPairTelemetry)
{
    using namespace grow::domain;
    using namespace grow::services;

    const ClimateSensorReading valid_reading{
        .temp = Celsius{24.0},
        .rh = RelativeHumidity{55.0}
    };

    const auto report = VpdCalculator::evaluateSensorPair(valid_reading);
    ASSERT_TRUE(report.has_value());
    EXPECT_DOUBLE_EQ(report->temp.value, 24.0);
    EXPECT_DOUBLE_EQ(report->rh.value, 55.0);
    EXPECT_GT(report->vpd.value, 0.0);
    EXPECT_LT(report->dew_point.value, report->temp.value);

    // Telemetria inválida
    const ClimateSensorReading invalid_reading{
        .temp = Celsius{24.0},
        .rh = RelativeHumidity{105.0} // RH inválido
    };
    EXPECT_FALSE(VpdCalculator::evaluateSensorPair(invalid_reading).has_value());
}
