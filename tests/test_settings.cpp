#include "cvatnative/core/Settings.h"

#include <gtest/gtest.h>

using namespace cvat;

TEST(Settings, CtrlAndAltBindingsAreIndependent)
{
    auto& settings = Settings::instance();
    settings.setCtrlHotkeyLabel(1, 101);
    settings.setAltHotkeyLabel(1, 202);
    settings.setCtrlHotkeyLabel(0, 303);

    EXPECT_EQ(settings.ctrlHotkeyLabel(1), 101);
    EXPECT_EQ(settings.altHotkeyLabel(1), 202);
    EXPECT_EQ(settings.ctrlHotkeyLabel(0), 303);
    EXPECT_EQ(settings.altHotkeyLabel(0), -1);
}

TEST(Settings, BindingsRejectOutOfRangeDigits)
{
    auto& settings = Settings::instance();
    settings.setCtrlHotkeyLabel(-1, 42);
    settings.setAltHotkeyLabel(10, 42);

    EXPECT_EQ(settings.ctrlHotkeyLabel(-1), -1);
    EXPECT_EQ(settings.altHotkeyLabel(10), -1);
}

TEST(Settings, AutoActionOptionsAreIndependentAndBounded)
{
    auto& settings = Settings::instance();
    settings.setAutoDrawAfterCreate(true);
    settings.setAutoDrawDelayMs(400);
    settings.setAutoPasteAfterCreate(false);
    settings.setAutoPasteDelayMs(75);

    EXPECT_TRUE(settings.autoDrawAfterCreate());
    EXPECT_EQ(settings.autoDrawDelayMs(), 400);
    EXPECT_FALSE(settings.autoPasteAfterCreate());
    EXPECT_EQ(settings.autoPasteDelayMs(), 75);

    settings.setAutoDrawDelayMs(-10);
    settings.setAutoPasteDelayMs(20000);
    EXPECT_EQ(settings.autoDrawDelayMs(), 0);
    EXPECT_EQ(settings.autoPasteDelayMs(), 10000);
}
