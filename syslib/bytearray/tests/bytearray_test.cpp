#if __has_include("gtest/gtest.h")

#include "gtest/gtest.h"

using namespace testing;

#include "../bytearray.hpp"

class ByteArrayTest : public ::testing::Test
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
