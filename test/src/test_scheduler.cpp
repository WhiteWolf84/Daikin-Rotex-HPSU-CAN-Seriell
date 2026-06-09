#include <gtest/gtest.h>
#include "esphome/components/daikin_rotex_can/scheduler.h"

using namespace esphome::daikin_rotex_can;

TEST(SchedulerTest, CallLaterReturnsValidHandle) {
    auto& scheduler = Scheduler::getInstance();

    bool called = false;
    auto handle = scheduler.call_later([&called]() {
        called = true;
    }, 100);

    EXPECT_TRUE(handle.is_valid());
    EXPECT_FALSE(called);
}

TEST(SchedulerTest, CancelCallWorks) {
    auto& scheduler = Scheduler::getInstance();

    bool called = false;
    auto handle = scheduler.call_later([&called]() {
        called = true;
    }, 100);

    EXPECT_TRUE(handle.cancel());
    EXPECT_FALSE(handle.is_valid());
    EXPECT_FALSE(called);
}

TEST(SchedulerTest, AccelerateCallWorks) {
    auto& scheduler = Scheduler::getInstance();

    bool called = false;
    auto handle = scheduler.call_later([&called]() {
        called = true;
    }, 100);

    EXPECT_TRUE(handle.accelerate());

    scheduler.update();
    EXPECT_TRUE(called);
}

TEST(SchedulerTest, CancelOwnHandleInsideCallbackIsSafe) {
    // Regression: a callback cancelling its own handle while update() held an
    // iterator to that element caused a double-erase (UB).
    auto& scheduler = Scheduler::getInstance();

    bool called = false;
    CallHandle handle;
    handle = scheduler.call_later([&]() {
        called = true;
        handle.cancel();  // detached already -> must be a harmless no-op
    }, 0);

    scheduler.update();
    EXPECT_TRUE(called);
    EXPECT_FALSE(handle.is_valid());
}

TEST(SchedulerTest, ScheduleNewCallInsideCallbackIsSafe) {
    auto& scheduler = Scheduler::getInstance();

    bool nested_called = false;
    scheduler.call_later([&]() {
        scheduler.call_later([&]() { nested_called = true; }, 0);
    }, 0);

    scheduler.update();  // runs the outer call; the nested one may run in this or the next pass
    scheduler.update();
    EXPECT_TRUE(nested_called);
}