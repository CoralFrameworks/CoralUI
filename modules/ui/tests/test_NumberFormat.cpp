/*
 * Copyright (C) 2026 The Coral Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ui/NumberFormat.h>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

namespace coral::ui {
namespace {

TEST(UiNumberFormat, GroupsIntegralDigitsAndTrimsFraction) {
  EXPECT_EQ(formatNumber(1234567.891, numberFormatEnUs()), "1,234,567.891");
  EXPECT_EQ(formatNumber(1234.5, numberFormatEnUs()), "1,234.5");
  EXPECT_EQ(formatNumber(999.0, numberFormatEnUs()), "999") << "a whole number has no fraction part";
  EXPECT_EQ(formatNumber(-1234.5, numberFormatEnUs()), "-1,234.5");
}

TEST(UiNumberFormat, RespectsTheDigitPolicy) {
  NumberFormatOptions two;
  two.maxFractionDigits = 2;
  EXPECT_EQ(formatNumber(1234.5678, two), "1,234.57") << "rounds to the maximum";

  NumberFormatOptions padded;
  padded.maxFractionDigits = 2;
  padded.minFractionDigits = 2;
  EXPECT_EQ(formatNumber(3.5, padded), "3.50") << "pads to the minimum";
}

TEST(UiNumberFormat, UsesTheLocaleSeparators) {
  EXPECT_EQ(formatNumber(1234567.5, numberFormatDeDe()), "1.234.567,5");
  EXPECT_EQ(formatNumber(1234567.5, numberFormatFrFr()), "1\xE2\x80\xAF" "234\xE2\x80\xAF" "567,5");
}

TEST(UiNumberFormat, CanDisableGrouping) {
  NumberFormatOptions plain;
  plain.grouping = false;
  EXPECT_EQ(formatNumber(1234567.5, plain), "1234567.5");
}

TEST(UiNumberFormat, KeepsNonFiniteValuesVisible) {
  EXPECT_EQ(formatNumber(std::numeric_limits<double>::quiet_NaN()), "NaN");
  EXPECT_EQ(formatNumber(std::numeric_limits<double>::infinity()), "Infinity");
  EXPECT_EQ(formatNumber(-std::numeric_limits<double>::infinity()), "-Infinity");
}

} // namespace
} // namespace coral::ui
