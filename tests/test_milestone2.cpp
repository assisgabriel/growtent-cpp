#include <gtest/gtest.h>
#include "controllers/PhotoperiodScheduler.hpp"
#include "domain/TentSetupConfig.hpp"
#include "domain/Types.hpp"
#include <chrono>

using namespace grow::controllers;
using namespace grow::domain;
using namespace std::chrono_literals;

// ============================================================================
// 1. Compile-Time constexpr Guarantees
// ============================================================================

static_assert(PhotoperiodScheduler::kMinutesPerDay == 1440);
static_assert(PhotoperiodScheduler::kFarRedLeadMinutes == 15);
static_assert(PhotoperiodScheduler::kFarRedLagMinutes == 15);

static_assert(PhotoperiodScheduler::normalizeDayMinutes(0) == 0);
static_assert(PhotoperiodScheduler::normalizeDayMinutes(1439) == 1439);
static_assert(PhotoperiodScheduler::normalizeDayMinutes(1440) == 0);
static_assert(PhotoperiodScheduler::normalizeDayMinutes(1500) == 60);
static_assert(PhotoperiodScheduler::normalizeDayMinutes(-15) == 1425);
static_assert(PhotoperiodScheduler::normalizeDayMinutes(-1440) == 0);
static_assert(PhotoperiodScheduler::normalizeDayMinutes(-1450) == 1430);

// Circular window compile-time checks
static_assert(!PhotoperiodScheduler::isWithinCircularWindow(360, 360, 360));      // start == end is empty
static_assert(PhotoperiodScheduler::isWithinCircularWindow(360, 360, 1080));      // boundary start
static_assert(PhotoperiodScheduler::isWithinCircularWindow(720, 360, 1080));      // midday
static_assert(!PhotoperiodScheduler::isWithinCircularWindow(1080, 360, 1080));    // boundary end [start, end)
static_assert(PhotoperiodScheduler::isWithinCircularWindow(1300, 1200, 480));     // midnight wrap before 24:00
static_assert(PhotoperiodScheduler::isWithinCircularWindow(100, 1200, 480));      // midnight wrap after 00:00
static_assert(!PhotoperiodScheduler::isWithinCircularWindow(600, 1200, 480));     // outside wrap window

// ============================================================================
// 2. Unit Tests: Normalization & Circular Window Arithmetic
// ============================================================================

TEST(PhotoperiodSchedulerTest, NormalizeDayMinutesHandlesExtremeOffsets)
{
    EXPECT_EQ(PhotoperiodScheduler::normalizeDayMinutes(0), 0);
    EXPECT_EQ(PhotoperiodScheduler::normalizeDayMinutes(720), 720);
    EXPECT_EQ(PhotoperiodScheduler::normalizeDayMinutes(1439), 1439);
    EXPECT_EQ(PhotoperiodScheduler::normalizeDayMinutes(1440), 0);
    EXPECT_EQ(PhotoperiodScheduler::normalizeDayMinutes(2880), 0);
    EXPECT_EQ(PhotoperiodScheduler::normalizeDayMinutes(1445), 5);

    EXPECT_EQ(PhotoperiodScheduler::normalizeDayMinutes(-1), 1439);
    EXPECT_EQ(PhotoperiodScheduler::normalizeDayMinutes(-15), 1425);
    EXPECT_EQ(PhotoperiodScheduler::normalizeDayMinutes(-1440), 0);
    EXPECT_EQ(PhotoperiodScheduler::normalizeDayMinutes(-1441), 1439);
    EXPECT_EQ(PhotoperiodScheduler::normalizeDayMinutes(-2880), 0);
}

TEST(PhotoperiodSchedulerTest, CircularWindowStandardDaytime)
{
    constexpr int start = 360;  // 06:00
    constexpr int end = 1080;  // 18:00

    EXPECT_FALSE(PhotoperiodScheduler::isWithinCircularWindow(359, start, end));
    EXPECT_TRUE(PhotoperiodScheduler::isWithinCircularWindow(360, start, end));
    EXPECT_TRUE(PhotoperiodScheduler::isWithinCircularWindow(720, start, end));
    EXPECT_TRUE(PhotoperiodScheduler::isWithinCircularWindow(1079, start, end));
    EXPECT_FALSE(PhotoperiodScheduler::isWithinCircularWindow(1080, start, end));
    EXPECT_FALSE(PhotoperiodScheduler::isWithinCircularWindow(1200, start, end));
    EXPECT_FALSE(PhotoperiodScheduler::isWithinCircularWindow(0, start, end));
}

TEST(PhotoperiodSchedulerTest, CircularWindowMidnightWrapAround)
{
    constexpr int start = 1200; // 20:00
    constexpr int end = 480;    // 08:00 next day

    EXPECT_FALSE(PhotoperiodScheduler::isWithinCircularWindow(1199, start, end));
    EXPECT_TRUE(PhotoperiodScheduler::isWithinCircularWindow(1200, start, end));
    EXPECT_TRUE(PhotoperiodScheduler::isWithinCircularWindow(1439, start, end));
    EXPECT_TRUE(PhotoperiodScheduler::isWithinCircularWindow(0, start, end));
    EXPECT_TRUE(PhotoperiodScheduler::isWithinCircularWindow(300, start, end));
    EXPECT_TRUE(PhotoperiodScheduler::isWithinCircularWindow(479, start, end));
    EXPECT_FALSE(PhotoperiodScheduler::isWithinCircularWindow(480, start, end));
    EXPECT_FALSE(PhotoperiodScheduler::isWithinCircularWindow(720, start, end));
}

TEST(PhotoperiodSchedulerTest, CircularWindowFarRedNegativeOffsets)
{
    // If lights_off is 00:05 (5 min), Far-Red is [5 - 15, 5 + 15) = [-10, 20) -> [1430, 20)
    constexpr int start_raw = -10;
    constexpr int end_raw = 20;

    EXPECT_FALSE(PhotoperiodScheduler::isWithinCircularWindow(1429, start_raw, end_raw));
    EXPECT_TRUE(PhotoperiodScheduler::isWithinCircularWindow(1430, start_raw, end_raw));
    EXPECT_TRUE(PhotoperiodScheduler::isWithinCircularWindow(1439, start_raw, end_raw));
    EXPECT_TRUE(PhotoperiodScheduler::isWithinCircularWindow(0, start_raw, end_raw));
    EXPECT_TRUE(PhotoperiodScheduler::isWithinCircularWindow(19, start_raw, end_raw));
    EXPECT_FALSE(PhotoperiodScheduler::isWithinCircularWindow(20, start_raw, end_raw));
    EXPECT_FALSE(PhotoperiodScheduler::isWithinCircularWindow(720, start_raw, end_raw));
}

// ============================================================================
// 3. Main Grow Light Schedule Evaluation
// ============================================================================

TEST(PhotoperiodSchedulerTest, RespectsHasMainLightFlag)
{
    TentSetupConfig setup{};
    setup.has_main_light = false;

    PhotoperiodConfig schedule{};
    schedule.preset = PhotoperiodPreset::Flower_12_12;
    schedule.lights_on_time = 360min;
    schedule.lights_off_time = 1080min;

    // Even during the day, main light must remain off if hardware has no main light
    const auto out = PhotoperiodScheduler::evaluate(setup, schedule, 720min);
    EXPECT_FALSE(out.main_light_on);
    EXPECT_EQ(out.minutes_until_main_light_on.count(), 0);
}

TEST(PhotoperiodSchedulerTest, EvaluatesStandard12_12DaySchedule)
{
    TentSetupConfig setup{};
    setup.has_main_light = true;

    PhotoperiodConfig schedule{};
    schedule.preset = PhotoperiodPreset::Flower_12_12;
    schedule.lights_on_time = 360min;  // 06:00
    schedule.lights_off_time = 1080min; // 18:00

    // 05:00 (Darkness, 60 minutes until sunrise)
    {
        const auto out = PhotoperiodScheduler::evaluate(setup, schedule, 300min);
        EXPECT_FALSE(out.main_light_on);
        EXPECT_EQ(out.minutes_until_main_light_on.count(), 60);
    }

    // 05:35 (Darkness, 25 minutes until sunrise - thermal pre-ramp trigger)
    {
        const auto out = PhotoperiodScheduler::evaluate(setup, schedule, 335min);
        EXPECT_FALSE(out.main_light_on);
        EXPECT_EQ(out.minutes_until_main_light_on.count(), 25);
    }

    // 06:00 (Sunrise)
    {
        const auto out = PhotoperiodScheduler::evaluate(setup, schedule, 360min);
        EXPECT_TRUE(out.main_light_on);
        EXPECT_EQ(out.minutes_until_main_light_on.count(), 0);
    }

    // 12:00 (Midday)
    {
        const auto out = PhotoperiodScheduler::evaluate(setup, schedule, 720min);
        EXPECT_TRUE(out.main_light_on);
        EXPECT_EQ(out.minutes_until_main_light_on.count(), 0);
    }

    // 17:59 (Last minute of daylight)
    {
        const auto out = PhotoperiodScheduler::evaluate(setup, schedule, 1079min);
        EXPECT_TRUE(out.main_light_on);
        EXPECT_EQ(out.minutes_until_main_light_on.count(), 0);
    }

    // 18:00 (Sunset)
    {
        const auto out = PhotoperiodScheduler::evaluate(setup, schedule, 1080min);
        EXPECT_FALSE(out.main_light_on);
        EXPECT_EQ(out.minutes_until_main_light_on.count(), 720); // 12 hours until next 06:00
    }
}

TEST(PhotoperiodSchedulerTest, EvaluatesInvertedNighttimeSchedule)
{
    TentSetupConfig setup{};
    setup.has_main_light = true;

    PhotoperiodConfig schedule{};
    schedule.preset = PhotoperiodPreset::Custom;
    schedule.lights_on_time = 1200min; // 20:00
    schedule.lights_off_time = 480min;  // 08:00 next day

    // 19:35 (Darkness, 25 min until sunrise)
    {
        const auto out = PhotoperiodScheduler::evaluate(setup, schedule, 1175min);
        EXPECT_FALSE(out.main_light_on);
        EXPECT_EQ(out.minutes_until_main_light_on.count(), 25);
    }

    // 20:00 (Sunrise)
    {
        const auto out = PhotoperiodScheduler::evaluate(setup, schedule, 1200min);
        EXPECT_TRUE(out.main_light_on);
        EXPECT_EQ(out.minutes_until_main_light_on.count(), 0);
    }

    // 23:59 (Late night)
    {
        const auto out = PhotoperiodScheduler::evaluate(setup, schedule, 1439min);
        EXPECT_TRUE(out.main_light_on);
        EXPECT_EQ(out.minutes_until_main_light_on.count(), 0);
    }

    // 00:00 (Midnight wrap)
    {
        const auto out = PhotoperiodScheduler::evaluate(setup, schedule, 0min);
        EXPECT_TRUE(out.main_light_on);
        EXPECT_EQ(out.minutes_until_main_light_on.count(), 0);
    }

    // 07:59 (Morning light)
    {
        const auto out = PhotoperiodScheduler::evaluate(setup, schedule, 479min);
        EXPECT_TRUE(out.main_light_on);
        EXPECT_EQ(out.minutes_until_main_light_on.count(), 0);
    }

    // 08:00 (Sunset)
    {
        const auto out = PhotoperiodScheduler::evaluate(setup, schedule, 480min);
        EXPECT_FALSE(out.main_light_on);
        EXPECT_EQ(out.minutes_until_main_light_on.count(), 720); // 12h until 20:00
    }

    // 12:00 (Daytime darkness)
    {
        const auto out = PhotoperiodScheduler::evaluate(setup, schedule, 720min);
        EXPECT_FALSE(out.main_light_on);
        EXPECT_EQ(out.minutes_until_main_light_on.count(), 480); // 8h until 20:00
    }
}

TEST(PhotoperiodSchedulerTest, EvaluatesAlwaysOnAndAlwaysOffPresets)
{
    TentSetupConfig setup{};
    setup.has_main_light = true;

    PhotoperiodConfig schedule{};

    // AlwaysOn_24_0
    schedule.preset = PhotoperiodPreset::AlwaysOn_24_0;
    for (int m : {0, 360, 720, 1080, 1439})
    {
        const auto out = PhotoperiodScheduler::evaluate(setup, schedule, std::chrono::minutes(m));
        EXPECT_TRUE(out.main_light_on);
        EXPECT_EQ(out.minutes_until_main_light_on.count(), 0);
    }

    // AlwaysOff_0_24
    schedule.preset = PhotoperiodPreset::AlwaysOff_0_24;
    for (int m : {0, 360, 720, 1080, 1439})
    {
        const auto out = PhotoperiodScheduler::evaluate(setup, schedule, std::chrono::minutes(m));
        EXPECT_FALSE(out.main_light_on);
        EXPECT_EQ(out.minutes_until_main_light_on.count(), 0);
    }
}

// ============================================================================
// 4. Auxiliary Spectral Lighting (660nm Deep Red & 730nm Far Red)
// ============================================================================

TEST(PhotoperiodSchedulerTest, AuxLightFollowMainLight660nm)
{
    TentSetupConfig setup{};
    setup.has_main_light = true;
    setup.num_aux_lights = 2;

    setup.aux_lights[0].enabled = true;
    setup.aux_lights[0].mode = AuxLightMode::FollowMainLight_660nm;

    setup.aux_lights[1].enabled = false; // Disabled channel

    PhotoperiodConfig schedule{};
    schedule.preset = PhotoperiodPreset::Flower_12_12;
    schedule.lights_on_time = 360min;
    schedule.lights_off_time = 1080min;

    // During daylight
    {
        const auto out = PhotoperiodScheduler::evaluate(setup, schedule, 720min);
        EXPECT_TRUE(out.main_light_on);
        EXPECT_TRUE(out.aux_lights_on[0]);
        EXPECT_FALSE(out.aux_lights_on[1]);
    }

    // During darkness
    {
        const auto out = PhotoperiodScheduler::evaluate(setup, schedule, 1200min);
        EXPECT_FALSE(out.main_light_on);
        EXPECT_FALSE(out.aux_lights_on[0]);
        EXPECT_FALSE(out.aux_lights_on[1]);
    }
}

TEST(PhotoperiodSchedulerTest, AuxLightSunsetLag15Min730nmFarRed)
{
    TentSetupConfig setup{};
    setup.has_main_light = true;
    setup.num_aux_lights = 1;

    setup.aux_lights[0].enabled = true;
    setup.aux_lights[0].mode = AuxLightMode::SunsetLag15Min_730nm;

    PhotoperiodConfig schedule{};
    schedule.preset = PhotoperiodPreset::Flower_12_12;
    schedule.lights_on_time = 360min;   // 06:00
    schedule.lights_off_time = 1080min; // 18:00
    // Window is [1080 - 15, 1080 + 15) = [1065, 1095) -> [17:45, 18:15)

    // 17:44 (Before Far-Red window)
    {
        const auto out = PhotoperiodScheduler::evaluate(setup, schedule, 1064min);
        EXPECT_TRUE(out.main_light_on);
        EXPECT_FALSE(out.aux_lights_on[0]);
    }

    // 17:45 (Far-Red ignition, pre-sleep induction)
    {
        const auto out = PhotoperiodScheduler::evaluate(setup, schedule, 1065min);
        EXPECT_TRUE(out.main_light_on);
        EXPECT_TRUE(out.aux_lights_on[0]);
    }

    // 18:00 (Sunset: Main light turns off, Far-Red remains ON for Emerson effect)
    {
        const auto out = PhotoperiodScheduler::evaluate(setup, schedule, 1080min);
        EXPECT_FALSE(out.main_light_on);
        EXPECT_TRUE(out.aux_lights_on[0]);
    }

    // 18:14 (Last minute of Far-Red)
    {
        const auto out = PhotoperiodScheduler::evaluate(setup, schedule, 1094min);
        EXPECT_FALSE(out.main_light_on);
        EXPECT_TRUE(out.aux_lights_on[0]);
    }

    // 18:15 (Far-Red turns OFF, complete darkness)
    {
        const auto out = PhotoperiodScheduler::evaluate(setup, schedule, 1095min);
        EXPECT_FALSE(out.main_light_on);
        EXPECT_FALSE(out.aux_lights_on[0]);
    }
}

TEST(PhotoperiodSchedulerTest, AuxLightFarRedMidnightWrapAround)
{
    TentSetupConfig setup{};
    setup.has_main_light = true;
    setup.num_aux_lights = 1;

    setup.aux_lights[0].enabled = true;
    setup.aux_lights[0].mode = AuxLightMode::SunsetLag15Min_730nm;

    // Lights off at 00:05 (5 min since midnight)
    PhotoperiodConfig schedule{};
    schedule.preset = PhotoperiodPreset::Custom;
    schedule.lights_on_time = 725min; // 12:05
    schedule.lights_off_time = 5min;  // 00:05
    // Window: [5 - 15, 5 + 15) = [-10, 20) -> [1430, 20) (23:50 to 00:20)

    // 23:49 (Before Far-Red window)
    {
        const auto out = PhotoperiodScheduler::evaluate(setup, schedule, 1429min);
        EXPECT_TRUE(out.main_light_on);
        EXPECT_FALSE(out.aux_lights_on[0]);
    }

    // 23:50 (Far-Red turns on before midnight)
    {
        const auto out = PhotoperiodScheduler::evaluate(setup, schedule, 1430min);
        EXPECT_TRUE(out.main_light_on);
        EXPECT_TRUE(out.aux_lights_on[0]);
    }

    // 00:00 (Midnight wrap)
    {
        const auto out = PhotoperiodScheduler::evaluate(setup, schedule, 0min);
        EXPECT_TRUE(out.main_light_on);
        EXPECT_TRUE(out.aux_lights_on[0]);
    }

    // 00:05 (Main light turns off, Far-Red still on)
    {
        const auto out = PhotoperiodScheduler::evaluate(setup, schedule, 5min);
        EXPECT_FALSE(out.main_light_on);
        EXPECT_TRUE(out.aux_lights_on[0]);
    }

    // 00:19 (Last minute of Far-Red)
    {
        const auto out = PhotoperiodScheduler::evaluate(setup, schedule, 19min);
        EXPECT_FALSE(out.main_light_on);
        EXPECT_TRUE(out.aux_lights_on[0]);
    }

    // 00:20 (Far-Red extinguished)
    {
        const auto out = PhotoperiodScheduler::evaluate(setup, schedule, 20min);
        EXPECT_FALSE(out.main_light_on);
        EXPECT_FALSE(out.aux_lights_on[0]);
    }
}

TEST(PhotoperiodSchedulerTest, AuxLightFarRedSuppressedUnder24_0And0_24)
{
    TentSetupConfig setup{};
    setup.has_main_light = true;
    setup.num_aux_lights = 1;

    setup.aux_lights[0].enabled = true;
    setup.aux_lights[0].mode = AuxLightMode::SunsetLag15Min_730nm;

    PhotoperiodConfig schedule{};

    schedule.preset = PhotoperiodPreset::AlwaysOn_24_0;
    schedule.lights_off_time = 1080min;
    {
        const auto out = PhotoperiodScheduler::evaluate(setup, schedule, 1080min);
        EXPECT_FALSE(out.aux_lights_on[0]);
    }

    schedule.preset = PhotoperiodPreset::AlwaysOff_0_24;
    {
        const auto out = PhotoperiodScheduler::evaluate(setup, schedule, 1080min);
        EXPECT_FALSE(out.aux_lights_on[0]);
    }
}
