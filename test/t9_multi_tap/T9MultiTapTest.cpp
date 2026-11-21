#include <gtest/gtest.h>

#include <set>
#include <string>

#include "util/T9MultiTap.h"

namespace {
using Case = T9MultiTap::Case;
using Page = T9MultiTap::Page;

// Applies taps to a string the way the entry screen does.
struct Typist {
  T9MultiTap t9{1000};
  std::string text;

  explicit Typist(Case c = Case::Lower) { t9.setCase(c); }

  void tap(uint8_t key, uint32_t now) {
    const T9MultiTap::Output out = t9.tap(key, now);
    if (!out.text) return;
    if (out.replacesPending) text.pop_back();
    text += out.text;
  }
};
}  // namespace

TEST(T9MultiTap, RepeatedTapsCycleOneCharacter) {
  Typist t;
  t.tap(2, 0);
  t.tap(2, 100);
  t.tap(2, 200);
  EXPECT_EQ(t.text, "c");
  EXPECT_TRUE(t.t9.hasPending());
}

TEST(T9MultiTap, CycleWrapsThroughDigit) {
  Typist t;
  for (uint32_t i = 0; i < 5; ++i) t.tap(2, i * 100);
  EXPECT_EQ(t.text, "a");  // a b c 2 a
}

TEST(T9MultiTap, OtherKeyCommitsAtOnce) {
  Typist t;
  t.tap(4, 0);
  t.tap(4, 100);  // h
  t.tap(5, 200);
  EXPECT_EQ(t.text, "hj");
}

TEST(T9MultiTap, SameKeyAfterTimeoutStartsNewLetter) {
  Typist t;
  for (uint32_t i = 0; i < 3; ++i) t.tap(5, i * 100);  // l
  for (uint32_t i = 0; i < 3; ++i) t.tap(5, 1300 + i * 100);
  EXPECT_EQ(t.text, "ll");
}

TEST(T9MultiTap, CommitEndsPendingWithoutWaiting) {
  Typist t;
  t.tap(5, 0);
  EXPECT_TRUE(t.t9.commit());
  EXPECT_FALSE(t.t9.commit());
  t.tap(5, 100);
  EXPECT_EQ(t.text, "jj");
}

TEST(T9MultiTap, CancelDropsPending) {
  T9MultiTap t9;
  EXPECT_FALSE(t9.cancel());
  t9.tap(2, 0);
  EXPECT_TRUE(t9.cancel());
  EXPECT_FALSE(t9.hasPending());
  EXPECT_FALSE(t9.tap(2, 100).replacesPending);  // a new character, not a cycle
}

TEST(T9MultiTap, ExpireOnlyAfterTimeout) {
  T9MultiTap t9(1000);
  t9.tap(2, 0);
  EXPECT_FALSE(t9.expire(999));
  EXPECT_TRUE(t9.expire(1000));
  EXPECT_FALSE(t9.hasPending());
  EXPECT_FALSE(t9.expire(5000));
}

TEST(T9MultiTap, TimeoutSurvivesMillisWrap) {
  Typist t;
  t.tap(2, 0xFFFFFF00u);
  t.tap(2, 0x00000010u);  // 0x110 ms later
  EXPECT_EQ(t.text, "b");
}

TEST(T9MultiTap, CapitalAppliesToOneLetter) {
  Typist t(Case::Capital);
  t.tap(4, 0);
  t.tap(4, 100);  // cycling keeps the capital: G H
  EXPECT_EQ(t.text, "H");
  for (uint32_t i = 0; i < 3; ++i) t.tap(4, 1200 + i * 100);
  EXPECT_EQ(t.text, "Hi");
  EXPECT_EQ(t.t9.letterCase(), Case::Lower);
}

TEST(T9MultiTap, SpaceDoesNotUseUpCapital) {
  Typist t(Case::Capital);
  t.tap(0, 0);
  t.tap(2, 100);
  EXPECT_EQ(t.text, " A");
}

TEST(T9MultiTap, UpperStaysUpper) {
  Typist t(Case::Upper);
  t.tap(2, 0);
  t.tap(3, 100);
  t.t9.commit();
  EXPECT_EQ(t.text, "AD");
  EXPECT_EQ(t.t9.letterCase(), Case::Upper);
}

TEST(T9MultiTap, DigitsTypeAtOnce) {
  Typist t;
  t.tap(2, 0);
  t.t9.setPage(Page::Digits);  // commits the pending 'a'
  EXPECT_FALSE(t.t9.hasPending());
  t.tap(7, 100);
  t.tap(7, 200);
  EXPECT_EQ(t.text, "a77");
  EXPECT_FALSE(t.t9.hasPending());
}

TEST(T9MultiTap, SymbolsCycleLikeLetters) {
  Typist t(Case::Upper);
  t.t9.setPage(Page::Symbols);
  t.tap(1, 0);
  t.tap(1, 100);  // # $
  t.tap(4, 200);  // (
  EXPECT_EQ(t.text, "$(");
}

TEST(T9MultiTap, NextPageCycles) {
  T9MultiTap t9;
  EXPECT_EQ(t9.page(), Page::Letters);
  t9.nextPage();
  EXPECT_EQ(t9.page(), Page::Digits);
  t9.nextPage();
  EXPECT_EQ(t9.page(), Page::Symbols);
  t9.nextPage();
  EXPECT_EQ(t9.page(), Page::Letters);
}

TEST(T9MultiTap, CycleCase) {
  T9MultiTap t9;
  EXPECT_EQ(t9.letterCase(), Case::Capital);
  t9.cycleCase();
  EXPECT_EQ(t9.letterCase(), Case::Lower);
  t9.cycleCase();
  EXPECT_EQ(t9.letterCase(), Case::Upper);
  t9.cycleCase();
  EXPECT_EQ(t9.letterCase(), Case::Capital);
}

TEST(T9MultiTap, EveryPrintableAsciiIsReachable) {
  std::set<char> reachable;
  for (const Page page : {Page::Letters, Page::Digits, Page::Symbols}) {
    for (uint8_t key = 0; key < T9MultiTap::KEY_COUNT; ++key) {
      for (const char* c = T9MultiTap::keyChars(page, key); *c; ++c) {
        reachable.insert(*c);
        if (*c >= 'a' && *c <= 'z') reachable.insert(static_cast<char>(*c - 'a' + 'A'));
      }
    }
  }
  for (char c = 0x20; c < 0x7F; ++c) EXPECT_TRUE(reachable.count(c)) << "missing '" << c << "'";
}

TEST(T9MultiTap, InvalidKeyDoesNothing) {
  T9MultiTap t9;
  EXPECT_EQ(t9.tap(10, 0).text, nullptr);
  EXPECT_FALSE(t9.hasPending());
  EXPECT_STREQ(T9MultiTap::keyChars(Page::Letters, 10), "");
}
