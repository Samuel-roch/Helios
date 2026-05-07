#if __has_include("gtest/gtest.h")

#include "gtest/gtest.h"

using namespace testing;

#include "../atomic.hpp"

using namespace hel;

// =============================================================================
// Atomic<T> — construction and basic load/store
// =============================================================================

TEST(AtomicTest, DefaultConstructedIsZero)
{
    AtomicU32 a;
    EXPECT_EQ(a.load(), 0U);
}

TEST(AtomicTest, ConstructWithInitialValue)
{
    AtomicU32 a{42U};
    EXPECT_EQ(a.load(), 42U);
}

TEST(AtomicTest, StoreAndLoad)
{
    AtomicU32 a{0U};
    a.store(100U);
    EXPECT_EQ(a.load(), 100U);
}

TEST(AtomicTest, ImplicitConversionEqualsLoad)
{
    AtomicU32 a{7U};
    uint32_t val = a; // implicit operator T()
    EXPECT_EQ(val, 7U);
}

TEST(AtomicTest, AssignmentOperatorEqualsStore)
{
    AtomicU32 a{0U};
    a = 55U;
    EXPECT_EQ(a.load(), 55U);
}

// =============================================================================
// Atomic<bool>
// =============================================================================

TEST(AtomicTest, AtomicBoolDefaultFalse)
{
    AtomicBool flag;
    EXPECT_FALSE(flag.load());
}

TEST(AtomicTest, AtomicBoolStoreTrue)
{
    AtomicBool flag{false};
    flag.store(true);
    EXPECT_TRUE(flag.load());
}

// =============================================================================
// exchange
// =============================================================================

TEST(AtomicTest, ExchangeReturnsPreviousValue)
{
    AtomicU32 a{10U};
    const uint32_t old = a.exchange(20U);
    EXPECT_EQ(old, 10U);
    EXPECT_EQ(a.load(), 20U);
}

// =============================================================================
// compareExchange
// =============================================================================

TEST(AtomicTest, CompareExchangeSucceedsWhenExpectedMatches)
{
    AtomicU32 a{5U};
    uint32_t expected{5U};
    const bool ok = a.compareExchange(expected, 99U);
    EXPECT_TRUE(ok);
    EXPECT_EQ(a.load(), 99U);
}

TEST(AtomicTest, CompareExchangeFailsWhenExpectedDiffers)
{
    AtomicU32 a{5U};
    uint32_t expected{999U};
    const bool ok = a.compareExchange(expected, 99U);
    EXPECT_FALSE(ok);
    EXPECT_EQ(expected, 5U);   // updated with actual value
    EXPECT_EQ(a.load(), 5U);   // unchanged
}

// =============================================================================
// Arithmetic — fetchAdd / fetchSub
// =============================================================================

TEST(AtomicTest, FetchAddReturnsOldValue)
{
    AtomicU32 a{10U};
    const uint32_t old = a.fetchAdd(5U);
    EXPECT_EQ(old, 10U);
    EXPECT_EQ(a.load(), 15U);
}

TEST(AtomicTest, FetchSubReturnsOldValue)
{
    AtomicU32 a{10U};
    const uint32_t old = a.fetchSub(3U);
    EXPECT_EQ(old, 10U);
    EXPECT_EQ(a.load(), 7U);
}

TEST(AtomicTest, FetchAddMultipleTimes)
{
    AtomicU32 counter{0U};
    for (uint32_t i = 0U; i < 100U; ++i) { counter.fetchAdd(1U); }
    EXPECT_EQ(counter.load(), 100U);
}

// =============================================================================
// Bitwise — fetchOr / fetchAnd / fetchXor
// =============================================================================

TEST(AtomicTest, FetchOrSetsOnlySpecifiedBits)
{
    AtomicU32 a{0b0000U};
    a.fetchOr(0b0101U);
    EXPECT_EQ(a.load(), 0b0101U);
    a.fetchOr(0b0110U);
    EXPECT_EQ(a.load(), 0b0111U);
}

TEST(AtomicTest, FetchAndClearsOnlySpecifiedBits)
{
    AtomicU32 a{0b1111U};
    a.fetchAnd(0b1010U);
    EXPECT_EQ(a.load(), 0b1010U);
}

TEST(AtomicTest, FetchXorTogglesSpecifiedBits)
{
    AtomicU32 a{0b1010U};
    a.fetchXor(0b1111U);
    EXPECT_EQ(a.load(), 0b0101U);
}

// =============================================================================
// MemoryOrder — load / store with explicit ordering
// =============================================================================

TEST(AtomicTest, LoadWithRelaxedOrder)
{
    AtomicU32 a{42U};
    EXPECT_EQ(a.load(MemoryOrder::kRelaxed), 42U);
}

TEST(AtomicTest, StoreWithReleaseOrder)
{
    AtomicU32 a{0U};
    a.store(7U, MemoryOrder::kRelease);
    EXPECT_EQ(a.load(MemoryOrder::kAcquire), 7U);
}

TEST(AtomicTest, ExchangeWithAcqRelOrder)
{
    AtomicU32 a{1U};
    const uint32_t old = a.exchange(2U, MemoryOrder::kAcqRel);
    EXPECT_EQ(old, 1U);
    EXPECT_EQ(a.load(), 2U);
}

// =============================================================================
// AtomicFlag — basic behaviour
// =============================================================================

TEST(AtomicFlagTest, StartsCleared)
{
    AtomicFlag flag;
    EXPECT_FALSE(flag.test());
}

TEST(AtomicFlagTest, SetMakesFlagTrue)
{
    AtomicFlag flag;
    flag.set();
    EXPECT_TRUE(flag.test());
}

TEST(AtomicFlagTest, ClearMakesFlagFalse)
{
    AtomicFlag flag;
    flag.set();
    flag.clear();
    EXPECT_FALSE(flag.test());
}

// =============================================================================
// AtomicFlag — testAndSet
// =============================================================================

TEST(AtomicFlagTest, TestAndSetReturnsFalseWhenWasClear)
{
    AtomicFlag flag;
    const bool wasSet = flag.testAndSet();
    EXPECT_FALSE(wasSet); // was clear before
    EXPECT_TRUE(flag.test());
}

TEST(AtomicFlagTest, TestAndSetReturnsTrueWhenWasSet)
{
    AtomicFlag flag;
    flag.set();
    const bool wasSet = flag.testAndSet();
    EXPECT_TRUE(wasSet); // was already set
}

// =============================================================================
// AtomicFlag — testAndClear
// =============================================================================

TEST(AtomicFlagTest, TestAndClearReturnsTrueAndClearsFlag)
{
    AtomicFlag flag;
    flag.set();
    const bool fired = flag.testAndClear();
    EXPECT_TRUE(fired);
    EXPECT_FALSE(flag.test()); // cleared
}

TEST(AtomicFlagTest, TestAndClearReturnsFalseWhenAlreadyClear)
{
    AtomicFlag flag;
    const bool fired = flag.testAndClear();
    EXPECT_FALSE(fired);
    EXPECT_FALSE(flag.test());
}

TEST(AtomicFlagTest, TestAndClearIdempotent)
{
    AtomicFlag flag;
    flag.set();
    EXPECT_TRUE(flag.testAndClear());
    EXPECT_FALSE(flag.testAndClear()); // second call — already clear
}

// =============================================================================
// Convenience aliases
// =============================================================================

TEST(AtomicAliasTest, AtomicU8Works)
{
    AtomicU8 a{0xFFU};
    EXPECT_EQ(a.load(), 0xFFU);
    a.fetchAnd(static_cast<uint8_t>(0x0FU));
    EXPECT_EQ(a.load(), 0x0FU);
}

TEST(AtomicAliasTest, AtomicI32Works)
{
    AtomicI32 a{-1};
    EXPECT_EQ(a.load(), -1);
    a.fetchAdd(1);
    EXPECT_EQ(a.load(), 0);
}

// =============================================================================
// ISR-safe producer/consumer simulation (single-threaded)
// =============================================================================

TEST(AtomicIntegrationTest, IsrSafeProducerConsumerPattern)
{
    AtomicFlag dataReady;
    AtomicU32  dataValue{0U};

    // Simulate ISR
    dataValue.store(42U, MemoryOrder::kRelaxed);
    dataReady.set(MemoryOrder::kRelease);

    // Simulate task
    EXPECT_TRUE(dataReady.testAndClear(MemoryOrder::kAcqRel));
    EXPECT_EQ(dataValue.load(MemoryOrder::kAcquire), 42U);
}

TEST(AtomicIntegrationTest, AtomicCounterPattern)
{
    AtomicU32 errors{0U};
    AtomicU32 processed{0U};

    for (int i = 0; i < 10; ++i)
    {
        processed.fetchAdd(1U, MemoryOrder::kRelaxed);
        if (i % 3 == 0) { errors.fetchAdd(1U, MemoryOrder::kRelaxed); }
    }

    EXPECT_EQ(processed.load(), 10U);
    EXPECT_EQ(errors.load(), 4U); // i = 0, 3, 6, 9
}

#endif
