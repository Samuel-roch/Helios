#if __has_include("gtest/gtest.h")

#include "gtest/gtest.h"

using namespace testing;

#include "../calendar.hpp"

class CalendarTest : public ::testing::Test
{
protected:
    void SetUp() override
    {

    }

    void TearDown() override
    {

    }
};

#endif
