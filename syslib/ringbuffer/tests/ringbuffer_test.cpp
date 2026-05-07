#if __has_include("gtest/gtest.h")

#include "gtest/gtest.h"

using namespace testing;

#include "../ringbuffer.hpp"

using namespace hel;

// =============================================================================
// Test fixture
// =============================================================================

class RingBufferTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}

    RingBuffer<int, 4> buf; // capacity = 4, for most tests
};

// =============================================================================
// Construction
// =============================================================================

TEST_F(RingBufferTest, InitialStateIsEmpty)
{
    EXPECT_TRUE(buf.isEmpty());
    EXPECT_FALSE(buf.isFull());
    EXPECT_EQ(buf.size(), 0U);
    EXPECT_EQ(buf.capacity(), 4U);
    EXPECT_EQ(buf.available(), 4U);
}

// =============================================================================
// push — normal mode
// =============================================================================

TEST_F(RingBufferTest, PushOneElement)
{
    EXPECT_EQ(buf.push(10), RingBufferStatus::kOk);
    EXPECT_EQ(buf.size(), 1U);
    EXPECT_FALSE(buf.isEmpty());
    EXPECT_EQ(buf.available(), 3U);
}

TEST_F(RingBufferTest, PushUntilFull)
{
    EXPECT_EQ(buf.push(1), RingBufferStatus::kOk);
    EXPECT_EQ(buf.push(2), RingBufferStatus::kOk);
    EXPECT_EQ(buf.push(3), RingBufferStatus::kOk);
    EXPECT_EQ(buf.push(4), RingBufferStatus::kOk);
    EXPECT_TRUE(buf.isFull());
    EXPECT_EQ(buf.size(), 4U);
    EXPECT_EQ(buf.available(), 0U);
}

TEST_F(RingBufferTest, PushOnFullReturnsKFull)
{
    buf.push(1); buf.push(2); buf.push(3); buf.push(4);
    EXPECT_EQ(buf.push(5), RingBufferStatus::kFull);
    EXPECT_EQ(buf.size(), 4U); // unchanged
}

// =============================================================================
// pop
// =============================================================================

TEST_F(RingBufferTest, PopOnEmptyReturnsKEmpty)
{
    int val{};
    EXPECT_EQ(buf.pop(val), RingBufferStatus::kEmpty);
}

TEST_F(RingBufferTest, PopDiscardOnEmptyReturnsKEmpty)
{
    EXPECT_EQ(buf.pop(), RingBufferStatus::kEmpty);
}

TEST_F(RingBufferTest, PopReturnsElementsInFifoOrder)
{
    buf.push(10); buf.push(20); buf.push(30);

    int val{};
    EXPECT_EQ(buf.pop(val), RingBufferStatus::kOk);
    EXPECT_EQ(val, 10);
    EXPECT_EQ(buf.pop(val), RingBufferStatus::kOk);
    EXPECT_EQ(val, 20);
    EXPECT_EQ(buf.pop(val), RingBufferStatus::kOk);
    EXPECT_EQ(val, 30);
    EXPECT_TRUE(buf.isEmpty());
}

TEST_F(RingBufferTest, PopDiscardDecreasesSize)
{
    buf.push(1); buf.push(2);
    EXPECT_EQ(buf.pop(), RingBufferStatus::kOk);
    EXPECT_EQ(buf.size(), 1U);
}

// =============================================================================
// peek
// =============================================================================

TEST_F(RingBufferTest, PeekOnEmptyReturnsKEmpty)
{
    int val{};
    EXPECT_EQ(buf.peek(val), RingBufferStatus::kEmpty);
}

TEST_F(RingBufferTest, PeekDoesNotRemoveElement)
{
    buf.push(42);
    int val{};
    EXPECT_EQ(buf.peek(val), RingBufferStatus::kOk);
    EXPECT_EQ(val, 42);
    EXPECT_EQ(buf.size(), 1U); // unchanged
}

TEST_F(RingBufferTest, PeekAtOffsetReadsCorrectElement)
{
    buf.push(10); buf.push(20); buf.push(30);

    int val{};
    EXPECT_EQ(buf.peek(0U, val), RingBufferStatus::kOk); EXPECT_EQ(val, 10);
    EXPECT_EQ(buf.peek(1U, val), RingBufferStatus::kOk); EXPECT_EQ(val, 20);
    EXPECT_EQ(buf.peek(2U, val), RingBufferStatus::kOk); EXPECT_EQ(val, 30);
    EXPECT_EQ(buf.size(), 3U); // unchanged
}

// =============================================================================
// operator[] / front / back
// =============================================================================

TEST_F(RingBufferTest, IndexedAccessReturnsCorrectElement)
{
    buf.push(5); buf.push(10); buf.push(15);
    EXPECT_EQ(buf[0], 5);
    EXPECT_EQ(buf[1], 10);
    EXPECT_EQ(buf[2], 15);
}

TEST_F(RingBufferTest, FrontReturnsOldestElement)
{
    buf.push(1); buf.push(2); buf.push(3);
    EXPECT_EQ(buf.front(), 1);
}

TEST_F(RingBufferTest, BackReturnsNewestElement)
{
    buf.push(1); buf.push(2); buf.push(3);
    EXPECT_EQ(buf.back(), 3);
}

// =============================================================================
// push_overwrite
// =============================================================================

TEST_F(RingBufferTest, PushOverwriteOnFullDiscardsOldest)
{
    buf.push(1); buf.push(2); buf.push(3); buf.push(4); // full

    buf.push_overwrite(5); // overwrites 1

    EXPECT_EQ(buf.size(), 4U); // still full
    int val{};
    buf.pop(val); EXPECT_EQ(val, 2);
    buf.pop(val); EXPECT_EQ(val, 3);
    buf.pop(val); EXPECT_EQ(val, 4);
    buf.pop(val); EXPECT_EQ(val, 5);
}

TEST_F(RingBufferTest, PushOverwriteOnNonFullBehavesLikePush)
{
    buf.push_overwrite(99);
    EXPECT_EQ(buf.size(), 1U);
    EXPECT_EQ(buf.front(), 99);
}

TEST_F(RingBufferTest, PushOverwriteMultipleTimesKeepsLastN)
{
    RingBuffer<int, 3> rb;
    rb.push_overwrite(1);
    rb.push_overwrite(2);
    rb.push_overwrite(3);
    rb.push_overwrite(4); // overwrites 1
    rb.push_overwrite(5); // overwrites 2

    EXPECT_EQ(rb.size(), 3U);
    EXPECT_EQ(rb[0], 3);
    EXPECT_EQ(rb[1], 4);
    EXPECT_EQ(rb[2], 5);
}

// =============================================================================
// Circular wraparound
// =============================================================================

TEST_F(RingBufferTest, WrapAroundMaintainsFifoOrder)
{
    // Fill, drain two, push two — forces wraparound
    buf.push(1); buf.push(2); buf.push(3); buf.push(4);
    int v{};
    buf.pop(v); // remove 1
    buf.pop(v); // remove 2
    buf.push(5);
    buf.push(6);

    buf.pop(v); EXPECT_EQ(v, 3);
    buf.pop(v); EXPECT_EQ(v, 4);
    buf.pop(v); EXPECT_EQ(v, 5);
    buf.pop(v); EXPECT_EQ(v, 6);
    EXPECT_TRUE(buf.isEmpty());
}

// =============================================================================
// clear
// =============================================================================

TEST_F(RingBufferTest, ClearResetsBuffer)
{
    buf.push(1); buf.push(2); buf.push(3);
    buf.clear();
    EXPECT_TRUE(buf.isEmpty());
    EXPECT_EQ(buf.size(), 0U);
    EXPECT_EQ(buf.available(), 4U);
}

TEST_F(RingBufferTest, ClearThenPushWorks)
{
    buf.push(10); buf.push(20);
    buf.clear();
    EXPECT_EQ(buf.push(99), RingBufferStatus::kOk);
    EXPECT_EQ(buf.front(), 99);
}

// =============================================================================
// Typed element (struct)
// =============================================================================

struct Point { int x; int y; };

TEST(RingBufferStructTest, PushPopStruct)
{
    RingBuffer<Point, 2> rb;
    rb.push({1, 2});
    rb.push({3, 4});

    Point p{};
    rb.pop(p); EXPECT_EQ(p.x, 1); EXPECT_EQ(p.y, 2);
    rb.pop(p); EXPECT_EQ(p.x, 3); EXPECT_EQ(p.y, 4);
}

// =============================================================================
// Capacity = 1 edge case
// =============================================================================

TEST(RingBufferEdgeTest, CapacityOne)
{
    RingBuffer<int, 1> rb;
    EXPECT_TRUE(rb.isEmpty());
    EXPECT_EQ(rb.push(7), RingBufferStatus::kOk);
    EXPECT_TRUE(rb.isFull());
    EXPECT_EQ(rb.push(8), RingBufferStatus::kFull);

    int v{};
    EXPECT_EQ(rb.pop(v), RingBufferStatus::kOk);
    EXPECT_EQ(v, 7);
    EXPECT_TRUE(rb.isEmpty());
}

TEST(RingBufferEdgeTest, PushOverwriteCapacityOne)
{
    RingBuffer<int, 1> rb;
    rb.push_overwrite(1);
    rb.push_overwrite(2); // overwrites 1
    EXPECT_EQ(rb.front(), 2);
    EXPECT_EQ(rb.size(), 1U);
}

#endif
