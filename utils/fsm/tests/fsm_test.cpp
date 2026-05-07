
#if __has_include("gtest/gtest.h")

#include "gtest/gtest.h"

using namespace testing;

#include "../fsm.hpp"

class ModuleTest : public ::testing::Test
{
protected:
    void SetUp() override
    {

    }

    void TearDown() override
    {
        // Cleanup if needed
    }
};

#endif
