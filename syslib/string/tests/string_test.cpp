#if __has_include("gtest/gtest.h")

#include "gtest/gtest.h"

using namespace testing;

#include "../string.hpp"

class StringTest : public ::testing::Test
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
