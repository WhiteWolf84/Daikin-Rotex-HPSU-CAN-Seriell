#include "esphome/components/daikin_rotex_can/pid.h"
#include "esphome/core/hal.h"

#include <gtest/gtest.h>
#include <iostream>
#include <math.h>
#include <string>
#include <regex>

using namespace esphome::daikin_rotex_can;

TEST(PIDTest, compute) {
    auto delta = 0.00001f;

    PID pid(0.2, 0.05f, 0.05f, 0.2, 0.2, 0.1f);
    EXPECT_EQ(0, pid.get_last_update());

    std::string logstr;

    float dt = 10.f;
    float setpoint = 5.f;
    float current = 0.f;

    current += pid.compute(setpoint, current, dt, logstr);
    EXPECT_EQ("sp: 5.000000, cv: 0.000000, e: 5.000000, p: 0.200000, i: 0.010000, d: 0.002500, map: 0.200000, fp: 1.000000, mfp: 1.000000, mp: 0.200000, o: 0.212500", logstr);

    current += pid.compute(setpoint, current, dt, logstr);
    EXPECT_EQ("sp: 5.000000, cv: 0.212500, e: 4.787500, p: 0.351500, i: 0.010000, d: 0.002144, map: 0.200000, fp: 1.757500, mfp: 1.757500, mp: 0.200000, o: 0.363644", logstr);

    current += pid.compute(setpoint, current, dt, logstr);
    EXPECT_EQ("sp: 5.000000, cv: 0.576144, e: 4.423856, p: 0.458154, i: 0.010000, d: 0.001748, map: 0.200000, fp: 2.290771, mfp: 2.290771, mp: 0.200000, o: 0.469902", logstr);

    EXPECT_GE(esphome::millis(), pid.get_last_update());
}

TEST(PIDTest, invalid_dt) {
    auto delta = 0.00001f;

    PID pid(0.2, 0.05f, 0.05f, 0.2, 0.2, 0.1f);

    std::string logstr;

    float dt = 10.f;
    float setpoint = 5.f;
    float current = 4.f;

    current += pid.compute(setpoint, current, dt, logstr);
    EXPECT_EQ("sp: 5.000000, cv: 4.000000, e: 1.000000, p: 0.040000, i: 0.010000, d: 0.000500, map: 0.200000, fp: 0.200000, mfp: 0.200000, mp: 0.200000, o: 0.050500", logstr);

    current += pid.compute(setpoint, current, 0, logstr);
    EXPECT_EQ("sp: 5.000000, cv: 4.000000, e: 1.000000, p: 0.040000, i: 0.010000, d: 0.000500, map: 0.200000, fp: 0.200000, mfp: 0.200000, mp: 0.200000, o: 0.050500", logstr);

    current += pid.compute(setpoint, current, dt, logstr);
    EXPECT_EQ("sp: 5.000000, cv: 4.050500, e: 0.949500, p: 0.069980, i: 0.010000, d: 0.000425, map: 0.200000, fp: 0.349900, mfp: 0.349900, mp: 0.200000, o: 0.080405", logstr);

    current += pid.compute(setpoint, current, std::numeric_limits<float>::quiet_NaN(), logstr);
    EXPECT_EQ("sp: 5.000000, cv: 4.050500, e: 0.949500, p: 0.069980, i: 0.010000, d: 0.000425, map: 0.200000, fp: 0.349900, mfp: 0.349900, mp: 0.200000, o: 0.080405", logstr);

    current += pid.compute(setpoint, current, dt, logstr);
    EXPECT_EQ("sp: 5.000000, cv: 4.130905, e: 0.869095, p: 0.090748, i: 0.010000, d: 0.000342, map: 0.200000, fp: 0.453739, mfp: 0.453739, mp: 0.200000, o: 0.101090", logstr);

    current += pid.compute(setpoint, current, std::numeric_limits<float>::infinity(), logstr);
    EXPECT_EQ("sp: 5.000000, cv: 4.130905, e: 0.869095, p: 0.090748, i: 0.010000, d: 0.000342, map: 0.200000, fp: 0.453739, mfp: 0.453739, mp: 0.200000, o: 0.101090", logstr);

    current += pid.compute(setpoint, current, dt, logstr);
    EXPECT_EQ("sp: 5.000000, cv: 4.231995, e: 0.768005, p: 0.103318, i: 0.010000, d: 0.000257, map: 0.200000, fp: 0.516592, mfp: 0.516592, mp: 0.200000, o: 0.113576", logstr);
}

TEST(PIDTest, invalid_setpoint) {
    auto delta = 0.00001f;

    PID pid(0.2, 0.05f, 0.05f, 0.2, 0.2, 0.1f);

    std::string logstr;

    float dt = 10.f;
    float setpoint = 5.f;
    float current = 4.f;

    current += pid.compute(setpoint, current, dt, logstr);
    EXPECT_EQ("sp: 5.000000, cv: 4.000000, e: 1.000000, p: 0.040000, i: 0.010000, d: 0.000500, map: 0.200000, fp: 0.200000, mfp: 0.200000, mp: 0.200000, o: 0.050500", logstr);

    current += pid.compute(std::numeric_limits<float>::quiet_NaN(), current, dt, logstr);
    EXPECT_EQ("sp: 5.000000, cv: 4.000000, e: 1.000000, p: 0.040000, i: 0.010000, d: 0.000500, map: 0.200000, fp: 0.200000, mfp: 0.200000, mp: 0.200000, o: 0.050500", logstr);

    current += pid.compute(std::numeric_limits<float>::infinity(), current, dt, logstr);
    EXPECT_EQ("sp: 5.000000, cv: 4.000000, e: 1.000000, p: 0.040000, i: 0.010000, d: 0.000500, map: 0.200000, fp: 0.200000, mfp: 0.200000, mp: 0.200000, o: 0.050500", logstr);
}

TEST(PIDTest, reach_setpoint) {
    auto delta = 0.00001f;

    PID pid(0.2, 0.05f, 0.05f, 0.2, 0.2, 0.1f);

    std::string logstr;

    float dt = 10.f;
    float setpoint = 5.f;
    float current = 0.f;

    for (uint32_t i = 0; i < 2; ++i)
        current += pid.compute(setpoint, current, dt, logstr);
    EXPECT_NEAR(current, 0.5761437416, 0.00001);

    for (uint32_t i = 0; i < 5; ++i)
        current += pid.compute(setpoint, current, dt, logstr);
    EXPECT_NEAR(current, 3.2680168151, 0.00001);

    for (uint32_t i = 0; i < 100; ++i)
        current += pid.compute(setpoint, current, dt, logstr);
    EXPECT_NEAR(current, 5.0076870918, 0.00001);
}
TEST(PIDTest, reset_restarts_clock) {
    PID pid(0.2, 0.05f, 0.05f, 0.2, 0.2, 0.1f);
    pid.reset(123456u);
    EXPECT_EQ(123456u, pid.get_last_update());
}

TEST(PIDTest, reset_clears_history) {
    // After reset() the controller must behave exactly like a fresh one: no
    // integral, filtered P/D or previous error carried over from before a gap.
    std::string fresh_log, reset_log;

    PID fresh(0.2, 0.05f, 0.05f, 0.2, 0.2, 0.1f);
    fresh.compute(3.f, 1.f, 10.f, fresh_log);

    PID used(0.2, 0.05f, 0.05f, 0.2, 0.2, 0.1f);
    std::string ignored;
    float current = 0.f;
    for (int i = 0; i < 20; ++i)
        current += used.compute(-5.f, current, 10.f, ignored);   // build up history
    used.reset(0u);
    used.compute(3.f, 1.f, 10.f, reset_log);

    EXPECT_EQ(fresh_log, reset_log);
}

TEST(PIDTest, reset_then_matching_input_is_still) {
    // CanSensor restarts its smoothed value from the input itself after a gap,
    // so the first step after reset() must not move it.
    PID pid(0.2, 0.05f, 0.05f, 0.2, 0.2, 0.1f);
    std::string logstr;
    float current = 0.f;
    for (int i = 0; i < 20; ++i)
        current += pid.compute(-5.f, current, 10.f, logstr);
    pid.reset(0u);

    current = 2.5f;
    EXPECT_FLOAT_EQ(0.f, pid.compute(2.5f, current, 10.f, logstr));
}

TEST(PIDTest, dt_survives_millis_wrap) {
    // CanSensor computes dt as uint32 now - last_update. Across the 49.7-day
    // millis() wrap that stays a small positive number; the former float
    // subtraction went hugely negative and froze the filter for good.
    const uint32_t last = 0xFFFFF000u;           // 4096 ms before the wrap
    const uint32_t now = 0x00002000u;            // 8192 ms after it
    const float dt_new = static_cast<float>(now - last) / 1000.0f;
    const float dt_old = (static_cast<float>(now) - last) / 1000.0f;
    EXPECT_NEAR(12.288f, dt_new, 0.001f);
    EXPECT_LT(dt_old, 0.f);
}
