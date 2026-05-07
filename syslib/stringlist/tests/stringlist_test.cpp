#if __has_include("gtest/gtest.h")

#include "gtest/gtest.h"

using namespace testing;

#include "../stringlist.hpp"

using namespace hel;

class StringListTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// =============================================================================
// append
// =============================================================================

TEST_F(StringListTest, AppendIncreasesCount)
{
    StringList<32> sl;
    EXPECT_TRUE(sl.append("foo"));
    EXPECT_EQ(sl.count(), 1u);
}

TEST_F(StringListTest, AppendMultiple)
{
    StringList<32> sl;
    EXPECT_TRUE(sl.append("a"));
    EXPECT_TRUE(sl.append("bb"));
    EXPECT_TRUE(sl.append("ccc"));
    EXPECT_EQ(sl.count(), 3u);
    EXPECT_STREQ(sl.at(0), "a");
    EXPECT_STREQ(sl.at(1), "bb");
    EXPECT_STREQ(sl.at(2), "ccc");
}

TEST_F(StringListTest, AppendNullReturnsFalse)
{
    StringList<32> sl;
    EXPECT_FALSE(sl.append(nullptr));
    EXPECT_EQ(sl.count(), 0u);
}

TEST_F(StringListTest, AppendFullBufferReturnsFalse)
{
    StringList<5> sl; // exactly "ab\0" + "cd\0" = 6 > 5
    EXPECT_TRUE(sl.append("ab"));
    EXPECT_FALSE(sl.append("cd"));
    EXPECT_EQ(sl.count(), 1u);
}

TEST_F(StringListTest, AppendExactFit)
{
    StringList<4> sl; // "abc\0" = 4 bytes
    EXPECT_TRUE(sl.append("abc"));
    EXPECT_EQ(sl.count(), 1u);
    EXPECT_EQ(sl.bytesUsed(), 4u);
    EXPECT_EQ(sl.bytesFree(), 0u);
}

TEST_F(StringListTest, AppendEmptyString)
{
    StringList<8> sl;
    EXPECT_TRUE(sl.append(""));
    EXPECT_EQ(sl.count(), 1u);
    EXPECT_EQ(sl.bytesUsed(), 1u);
    EXPECT_STREQ(sl.at(0), "");
}

// =============================================================================
// insert
// =============================================================================

TEST_F(StringListTest, InsertAtFront)
{
    StringList<32> sl;
    sl.append("b");
    sl.append("c");
    EXPECT_TRUE(sl.insert(0u, "a"));
    EXPECT_EQ(sl.count(), 3u);
    EXPECT_STREQ(sl.at(0), "a");
    EXPECT_STREQ(sl.at(1), "b");
    EXPECT_STREQ(sl.at(2), "c");
}

TEST_F(StringListTest, InsertInMiddle)
{
    StringList<32> sl;
    sl.append("a");
    sl.append("c");
    EXPECT_TRUE(sl.insert(1u, "b"));
    EXPECT_STREQ(sl.at(0), "a");
    EXPECT_STREQ(sl.at(1), "b");
    EXPECT_STREQ(sl.at(2), "c");
}

TEST_F(StringListTest, InsertAtEndEquivalentToAppend)
{
    StringList<32> sl;
    sl.append("x");
    EXPECT_TRUE(sl.insert(1u, "y"));
    EXPECT_STREQ(sl.at(1), "y");
}

TEST_F(StringListTest, InsertOutOfRangeReturnsFalse)
{
    StringList<32> sl;
    sl.append("a");
    EXPECT_FALSE(sl.insert(5u, "b"));
    EXPECT_EQ(sl.count(), 1u);
}

TEST_F(StringListTest, InsertNullReturnsFalse)
{
    StringList<32> sl;
    EXPECT_FALSE(sl.insert(0u, nullptr));
}

TEST_F(StringListTest, InsertNoSpaceReturnsFalse)
{
    StringList<4> sl; // "abc\0" fills it
    sl.append("abc");
    EXPECT_FALSE(sl.insert(0u, "x"));
    EXPECT_EQ(sl.count(), 1u);
}

// =============================================================================
// remove
// =============================================================================

TEST_F(StringListTest, RemoveMiddleCompacts)
{
    StringList<32> sl;
    sl.append("a");
    sl.append("b");
    sl.append("c");
    const std::size_t usedBefore = sl.bytesUsed();
    EXPECT_TRUE(sl.remove(1u));
    EXPECT_EQ(sl.count(), 2u);
    EXPECT_STREQ(sl.at(0), "a");
    EXPECT_STREQ(sl.at(1), "c");
    EXPECT_EQ(sl.bytesUsed(), usedBefore - 2u); // "b\0" = 2 bytes removed
}

TEST_F(StringListTest, RemoveFirst)
{
    StringList<32> sl;
    sl.append("x");
    sl.append("y");
    EXPECT_TRUE(sl.remove(0u));
    EXPECT_EQ(sl.count(), 1u);
    EXPECT_STREQ(sl.at(0), "y");
}

TEST_F(StringListTest, RemoveLast)
{
    StringList<32> sl;
    sl.append("a");
    sl.append("b");
    EXPECT_TRUE(sl.remove(1u));
    EXPECT_EQ(sl.count(), 1u);
    EXPECT_STREQ(sl.at(0), "a");
}

TEST_F(StringListTest, RemoveOnlyEntry)
{
    StringList<16> sl;
    sl.append("hello");
    EXPECT_TRUE(sl.remove(0u));
    EXPECT_TRUE(sl.isEmpty());
    EXPECT_EQ(sl.bytesUsed(), 0u);
}

TEST_F(StringListTest, RemoveOutOfRangeReturnsFalse)
{
    StringList<16> sl;
    sl.append("a");
    EXPECT_FALSE(sl.remove(1u));
    EXPECT_FALSE(sl.remove(99u));
}

TEST_F(StringListTest, RemoveFreedBytesAreZeroed)
{
    StringList<16> sl;
    sl.append("hello");
    sl.remove(0u);
    // After removal bytesUsed == 0; buffer tail must be zero
    EXPECT_EQ(sl.bytesUsed(), 0u);
    EXPECT_EQ(sl.count(), 0u);
}

TEST_F(StringListTest, RemoveAndReuseSpace)
{
    StringList<8> sl; // "abc\0" = 4, "de\0" = 3 → 7 ≤ 8
    sl.append("abc");
    sl.append("de");
    EXPECT_TRUE(sl.remove(0u));       // free 4 bytes; used = 3
    EXPECT_TRUE(sl.append("xyz"));    // needs 4 bytes; used = 7 ≤ 8
    EXPECT_EQ(sl.count(), 2u);
    EXPECT_STREQ(sl.at(0), "de");
    EXPECT_STREQ(sl.at(1), "xyz");
}

// =============================================================================
// replace
// =============================================================================

TEST_F(StringListTest, ReplaceSameLength)
{
    StringList<16> sl;
    sl.append("abc");
    EXPECT_TRUE(sl.replace(0u, "xyz"));
    EXPECT_STREQ(sl.at(0), "xyz");
    EXPECT_EQ(sl.count(), 1u);
}

TEST_F(StringListTest, ReplaceShorter)
{
    StringList<16> sl;
    sl.append("hello");
    sl.append("world");
    EXPECT_TRUE(sl.replace(0u, "hi"));
    EXPECT_STREQ(sl.at(0), "hi");
    EXPECT_STREQ(sl.at(1), "world");
}

TEST_F(StringListTest, ReplaceLonger)
{
    StringList<32> sl;
    sl.append("hi");
    sl.append("ok");
    EXPECT_TRUE(sl.replace(0u, "hello"));
    EXPECT_STREQ(sl.at(0), "hello");
    EXPECT_STREQ(sl.at(1), "ok");
}

TEST_F(StringListTest, ReplaceDoesNotFitReturnsFalse)
{
    StringList<8> sl; // "ab\0" = 3; free = 5
    sl.append("ab");
    // Replace with 7-char string needs 8 bytes; only 5 free
    EXPECT_FALSE(sl.replace(0u, "longstr"));
    EXPECT_STREQ(sl.at(0), "ab"); // unchanged
}

TEST_F(StringListTest, ReplaceOutOfRangeReturnsFalse)
{
    StringList<16> sl;
    sl.append("a");
    EXPECT_FALSE(sl.replace(1u, "b"));
}

TEST_F(StringListTest, ReplaceNullReturnsFalse)
{
    StringList<16> sl;
    sl.append("a");
    EXPECT_FALSE(sl.replace(0u, nullptr));
}

// =============================================================================
// clear
// =============================================================================

TEST_F(StringListTest, ClearResetsState)
{
    StringList<32> sl;
    sl.append("one");
    sl.append("two");
    sl.clear();
    EXPECT_EQ(sl.count(), 0u);
    EXPECT_EQ(sl.bytesUsed(), 0u);
    EXPECT_TRUE(sl.isEmpty());
}

TEST_F(StringListTest, ClearThenAppend)
{
    StringList<8> sl;
    sl.append("full12"); // 7 bytes
    sl.clear();
    EXPECT_TRUE(sl.append("hello")); // 6 bytes ≤ 8
    EXPECT_STREQ(sl.at(0), "hello");
}

// =============================================================================
// at / operator[] / first / last
// =============================================================================

TEST_F(StringListTest, AtOutOfRangeReturnsNull)
{
    StringList<16> sl;
    EXPECT_EQ(sl.at(0), nullptr);
    sl.append("a");
    EXPECT_EQ(sl.at(1), nullptr);
}

TEST_F(StringListTest, FirstAndLast)
{
    StringList<32> sl;
    EXPECT_EQ(sl.first(), nullptr);
    EXPECT_EQ(sl.last(), nullptr);
    sl.append("alpha");
    sl.append("beta");
    sl.append("gamma");
    EXPECT_STREQ(sl.first(), "alpha");
    EXPECT_STREQ(sl.last(), "gamma");
}

TEST_F(StringListTest, SubscriptOperator)
{
    StringList<16> sl;
    sl.append("foo");
    sl.append("bar");
    EXPECT_STREQ(sl[0], "foo");
    EXPECT_STREQ(sl[1], "bar");
}

// =============================================================================
// indexOf / contains
// =============================================================================

TEST_F(StringListTest, IndexOfFound)
{
    StringList<32> sl;
    sl.append("one");
    sl.append("two");
    sl.append("three");
    EXPECT_EQ(sl.indexOf("two"), 1u);
}

TEST_F(StringListTest, IndexOfNotFound)
{
    StringList<32> sl;
    sl.append("a");
    EXPECT_EQ(sl.indexOf("z"), StringList<32>::npos);
}

TEST_F(StringListTest, IndexOfNullReturnsNpos)
{
    StringList<16> sl;
    sl.append("a");
    EXPECT_EQ(sl.indexOf(nullptr), StringList<16>::npos);
}

TEST_F(StringListTest, Contains)
{
    StringList<32> sl;
    sl.append("hello");
    EXPECT_TRUE(sl.contains("hello"));
    EXPECT_FALSE(sl.contains("world"));
}

// =============================================================================
// join
// =============================================================================

TEST_F(StringListTest, JoinDefaultSeparator)
{
    StringList<32> sl;
    sl.append("a");
    sl.append("b");
    sl.append("c");
    char buf[16];
    const std::size_t written = sl.join(buf, sizeof(buf));
    EXPECT_EQ(written, 5u); // "a b c"
    EXPECT_STREQ(buf, "a b c");
}

TEST_F(StringListTest, JoinCustomSeparator)
{
    StringList<32> sl;
    sl.append("x");
    sl.append("y");
    char buf[8];
    const std::size_t written = sl.join(buf, sizeof(buf), ',');
    EXPECT_EQ(written, 3u); // "x,y"
    EXPECT_STREQ(buf, "x,y");
}

TEST_F(StringListTest, JoinNullSeparator)
{
    StringList<32> sl;
    sl.append("ab");
    sl.append("cd");
    char buf[8];
    const std::size_t written = sl.join(buf, sizeof(buf), '\0');
    EXPECT_EQ(written, 4u); // "abcd"
    EXPECT_STREQ(buf, "abcd");
}

TEST_F(StringListTest, JoinSingleEntry)
{
    StringList<16> sl;
    sl.append("only");
    char buf[8];
    const std::size_t written = sl.join(buf, sizeof(buf));
    EXPECT_EQ(written, 4u);
    EXPECT_STREQ(buf, "only");
}

TEST_F(StringListTest, JoinEmptyList)
{
    StringList<16> sl;
    char buf[8];
    const std::size_t written = sl.join(buf, sizeof(buf));
    EXPECT_EQ(written, 0u);
    EXPECT_STREQ(buf, "");
}

TEST_F(StringListTest, JoinBufferTooSmallReturnsNpos)
{
    StringList<32> sl;
    sl.append("hello");
    sl.append("world");
    char buf[4]; // "hello world\0" needs 12 bytes
    EXPECT_EQ(sl.join(buf, sizeof(buf)), StringList<32>::npos);
}

TEST_F(StringListTest, JoinNullDestReturnsNpos)
{
    StringList<16> sl;
    sl.append("a");
    EXPECT_EQ(sl.join(nullptr, 8u), StringList<16>::npos);
}

// =============================================================================
// canFit / bytesFree / bytesUsed
// =============================================================================

TEST_F(StringListTest, CanFit)
{
    StringList<8> sl;
    EXPECT_TRUE(sl.canFit(7u));  // 7+1=8 ≤ 8
    EXPECT_FALSE(sl.canFit(8u)); // 8+1=9 > 8
    sl.append("ab");             // 3 bytes used; 5 free
    EXPECT_TRUE(sl.canFit(4u));  // 4+1=5 ≤ 5
    EXPECT_FALSE(sl.canFit(5u)); // 5+1=6 > 5
}

TEST_F(StringListTest, BytesUsedAndFree)
{
    StringList<16> sl;
    EXPECT_EQ(sl.bytesUsed(), 0u);
    EXPECT_EQ(sl.bytesFree(), 16u);
    sl.append("hi"); // 3 bytes
    EXPECT_EQ(sl.bytesUsed(), 3u);
    EXPECT_EQ(sl.bytesFree(), 13u);
}

// =============================================================================
// String<N> and StringView compatibility
// =============================================================================

TEST_F(StringListTest, AppendStringObject)
{
    StringList<32> sl;
    String<16> s("hello");
    EXPECT_TRUE(sl.append(s));
    EXPECT_STREQ(sl.at(0), "hello");
}

TEST_F(StringListTest, InsertStringObject)
{
    StringList<32> sl;
    sl.append("first");
    String<16> s("inserted");
    EXPECT_TRUE(sl.insert(0u, s));
    EXPECT_STREQ(sl.at(0), "inserted");
    EXPECT_STREQ(sl.at(1), "first");
}

TEST_F(StringListTest, ReplaceWithStringObject)
{
    StringList<32> sl;
    sl.append("old");
    String<16> s("new");
    EXPECT_TRUE(sl.replace(0u, s));
    EXPECT_STREQ(sl.at(0), "new");
}

TEST_F(StringListTest, IndexOfStringObject)
{
    StringList<32> sl;
    sl.append("alpha");
    sl.append("beta");
    String<16> s("beta");
    EXPECT_EQ(sl.indexOf(s), 1u);
}

TEST_F(StringListTest, ContainsStringObject)
{
    StringList<32> sl;
    sl.append("foo");
    String<16> s("foo");
    EXPECT_TRUE(sl.contains(s));
    String<16> t("bar");
    EXPECT_FALSE(sl.contains(t));
}

TEST_F(StringListTest, AppendStringView)
{
    StringList<32> sl;
    StringView sv("world");
    EXPECT_TRUE(sl.append(sv));
    EXPECT_STREQ(sl.at(0), "world");
}

TEST_F(StringListTest, IndexOfStringViewSubstr)
{
    StringList<32> sl;
    sl.append("ab");
    // StringView over first 2 chars of a longer string (not null-terminated)
    const char* raw = "abXXX";
    StringView sv(raw, 2u); // "ab" without null terminator
    EXPECT_EQ(sl.indexOf(sv), 0u);
}

// =============================================================================
// atView / firstView / lastView
// =============================================================================

TEST_F(StringListTest, AtViewReturnsCorrectView)
{
    StringList<32> sl;
    sl.append("hello");
    sl.append("world");
    const StringView v = sl.atView(1u);
    EXPECT_EQ(v.size(), 5u);
    EXPECT_EQ(std::memcmp(v.data(), "world", 5u), 0);
}

TEST_F(StringListTest, AtViewOutOfRangeReturnsEmpty)
{
    StringList<16> sl;
    sl.append("x");
    const StringView v = sl.atView(1u);
    EXPECT_EQ(v.data(), nullptr);
    EXPECT_EQ(v.size(), 0u);
}

TEST_F(StringListTest, FirstViewAndLastView)
{
    StringList<32> sl;
    EXPECT_EQ(sl.firstView().data(), nullptr);
    EXPECT_EQ(sl.lastView().data(), nullptr);

    sl.append("alpha");
    sl.append("beta");
    sl.append("gamma");

    const StringView fv = sl.firstView();
    EXPECT_EQ(fv.size(), 5u);
    EXPECT_EQ(std::memcmp(fv.data(), "alpha", 5u), 0);

    const StringView lv = sl.lastView();
    EXPECT_EQ(lv.size(), 5u);
    EXPECT_EQ(std::memcmp(lv.data(), "gamma", 5u), 0);
}

TEST_F(StringListTest, AtViewCompatibleWithString)
{
    StringList<32> sl;
    sl.append("test");
    // StringView can be used to construct a String<N>
    String<16> s(sl.atView(0u));
    EXPECT_STREQ(s.c_str(), "test");
}

#endif
