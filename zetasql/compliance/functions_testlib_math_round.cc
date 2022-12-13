//
// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <math.h>

#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <utility>
#include <vector>

#include "zetasql/common/float_margin.h"
#include "zetasql/compliance/functions_testlib_common.h"
#include "zetasql/public/numeric_value.h"
#include "zetasql/public/options.pb.h"
#include "zetasql/public/types/type_factory.h"
#include "zetasql/public/value.h"
#include "zetasql/testing/test_function.h"
#include "zetasql/testing/test_value.h"
#include "zetasql/testing/using_test_value.cc"  // NOLINT
#include "absl/status/statusor.h"
#include "zetasql/base/status.h"

namespace zetasql {
namespace {
constexpr absl::StatusCode OUT_OF_RANGE = absl::StatusCode::kOutOfRange;
}  // namespace

std::vector<FunctionTestCall> GetFunctionTestsRounding() {
  const EnumType* rounding_mode_type = types::RoundingModeEnumType();

  std::vector<FunctionTestCall> all_tests = {
      {"round", {NullDouble()}, NullDouble()},
      {"round", {0.0f}, 0.0f},
      {"round", {2.0f}, 2.0f},
      {"round", {2.3f}, 2.0f},
      {"round", {2.8f}, 3.0f},
      {"round", {2.5f}, 3.0f},
      {"round", {-2.3f}, -2.0f},
      {"round", {-2.8f}, -3.0f},
      {"round", {-2.5f}, -3.0f},
      {"round", {1e-2f}, 0.0f},
      {"round", {1e38f}, 1e38f},

      {"round", {0.0}, 0.0},
      {"round", {2.0}, 2.0},
      {"round", {2.3}, 2.0},
      {"round", {2.8}, 3.0},
      {"round", {2.5}, 3.0},
      {"round", {-2.3}, -2.0},
      {"round", {-2.8}, -3.0},
      {"round", {-2.5}, -3.0},
      {"round", {1e-2}, 0.0},
      {"round", {1e300}, 1e300},

      // round(x, n)
      {"round", {NullDouble(), NullInt64()}, NullDouble()},
      {"round", {NullDouble(), 0ll}, NullDouble()},
      {"round", {0.0, NullInt64()}, NullDouble()},
      {"round", {3.1415925f, 2ll}, 3.14f},
      {"round", {3.1415925f, 3ll}, 3.142f},
      {"round", {314159.25f, -3ll}, 314000.0f},
      {"round", {314159.25f, -2ll}, 314200.0f},
      {"round", {3.4028234e38f, -35ll}, NullFloat(), OUT_OF_RANGE},
      {"round", {3.4028234e38f, -38ll}, 3.0e38f},
      {"round", {3.4028234e38f, -39ll}, 0.0f},
      {"round", {1.4e-45f, 45ll}, 1.4e-45f},
      {"round", {1.4e-45f, 44ll}, 0.0f},
      {"round", {1.4e-45f, 43ll}, 0.0f},
      {"round", {5.88e-39f, 39ll}, 6.0e-39f},
      {"round", {5.88e-39f, 40ll}, 5.9e-39f},
      {"round", {1.0f, 400ll}, 1.0f},
      {"round", {1.0f, -400ll}, 0.0f},
      {"round", {3.1415925f, int64max}, 3.1415925f},
      {"round", {3.1415925f, int64min}, 0.0f},
      {"round", {float_pos_inf, 0ll}, float_pos_inf},
      {"round", {float_neg_inf, 0ll}, float_neg_inf},
      {"round", {float_nan, 0ll}, float_nan},

      {"round", {3.1415925, 2ll}, 3.14},
      {"round", {3.1415925, 3ll}, 3.142},
      {"round", {314159.25, -3ll}, 314000.0},
      {"round", {314159.25, -2ll}, 314200.0},
      {"round", {1.6e308, -308ll}, NullDouble(), OUT_OF_RANGE},
      {"round", {1.6e308, -309ll}, 0.0},
      {"round", {1.6e308, 308ll}, 1.6e308},
      {"round", {4.94e-324, 324ll}, 4.94e-324, kOneUlp},
      {"round", {4.94e-324, 323ll}, 0.0, kOneUlp},
      {"round", {1.1125369292536007e-308, 308ll}, 1.0e-308, kOneUlp},

      // On platforms where long double is the same type as double when rounding
      // subnormal numbers precision is allowed to be worse than one ULP.
      // Computing exact value is hard there because 10^(-digits) cannot be
      // represented with the same relative error.
      {"round", {1.1125369292536007e-308, 311ll}, 1.113e-308, kApproximate},

      {"round", {1.0, 40000ll}, 1.0},
      {"round", {1.0, -40000ll}, 0.0},
      {"round", {3.1415925, int64max}, 3.1415925},
      {"round", {3.1415925, int64min}, 0.0},
      {"round", {double_pos_inf, 0ll}, double_pos_inf},
      {"round", {double_neg_inf, 0ll}, double_neg_inf},
      {"round", {double_nan, 0ll}, double_nan},

      {"trunc", {NullDouble()}, NullDouble()},
      {"trunc", {0.0f}, 0.0f},
      {"trunc", {2.0f}, 2.0f},
      {"trunc", {2.3f}, 2.0f},
      {"trunc", {2.8f}, 2.0f},
      {"trunc", {2.5f}, 2.0f},
      {"trunc", {-2.3f}, -2.0f},
      {"trunc", {-2.8f}, -2.0f},
      {"trunc", {-2.5f}, -2.0f},
      {"trunc", {1e-2f}, 0.0f},
      {"trunc", {1e38f}, 1e38f},

      {"trunc", {0.0}, 0.0},
      {"trunc", {2.0}, 2.0},
      {"trunc", {2.3}, 2.0},
      {"trunc", {2.8}, 2.0},
      {"trunc", {2.5}, 2.0},
      {"trunc", {-2.3}, -2.0},
      {"trunc", {-2.8}, -2.0},
      {"trunc", {-2.5}, -2.0},
      {"trunc", {1e-2}, 0.0},
      {"trunc", {1e300}, 1e300},

      {"trunc", {3.1415925f, 2ll}, 3.14f},
      {"trunc", {3.1415925f, 3ll}, 3.141f},
      {"trunc", {314159.25f, -3ll}, 314000.0f},
      {"trunc", {314159.25f, -2ll}, 314100.0f},
      {"trunc", {3.4028234e38f, -35ll}, 3.402e38f},
      {"trunc", {3.4028234e38f, -38ll}, 3.0e38f},
      {"trunc", {3.4028234e38f, -39ll}, 0.0f},
      {"trunc", {1.4e-45f, 45ll}, 1.4e-45f},
      {"trunc", {1.4e-45f, 44ll}, 0.0f},
      {"trunc", {5.88e-39f, 39ll}, 5.0e-39f},
      {"trunc", {5.88e-39f, 40ll}, 5.8e-39f},
      {"trunc", {1.0f, 400ll}, 1.0f},
      {"trunc", {1.0f, -400ll}, 0.0f},
      {"trunc", {3.1415925f, int64max}, 3.1415925f},
      {"trunc", {3.1415925f, int64min}, 0.0f},
      {"trunc", {float_pos_inf, 0ll}, float_pos_inf},
      {"trunc", {float_neg_inf, 0ll}, float_neg_inf},
      {"trunc", {float_nan, 0ll}, float_nan},

      {"trunc", {3.1415925, 2ll}, 3.14},
      {"trunc", {3.1415925, 3ll}, 3.141},
      {"trunc", {314159.25, -3ll}, 314000.0},
      {"trunc", {314159.25, -2ll}, 314100.0},
      {"trunc", {1.6e308, -308ll}, 1.0e308},
      {"trunc", {1.6e308, -309ll}, 0.0},
      {"trunc", {1.6e308, 308ll}, 1.6e308},
      {"trunc", {4.94e-324, 324ll}, 4.94e-324},
      {"trunc", {4.94e-324, 323ll}, 0.0},
      {"trunc", {1.1125369292536007e-308, 308ll}, 1.0e-308, kOneUlp},

      // On platforms where long double is the same type as double when rounding
      // subnormal numbers precision is allowed to be worse than one ULP.
      // Computing exact value is hard there because 10^(-digits) cannot be
      // represented with the same relative error.
      {"trunc", {1.1125369292536007e-308, 311ll}, 1.112e-308, kApproximate},

      {"trunc", {1.0, 40000ll}, 1.0},
      {"trunc", {1.0, -40000ll}, 0.0},
      {"trunc", {3.1415925, int64max}, 3.1415925},
      {"trunc", {3.1415925, int64min}, 0.0},
      {"trunc", {double_pos_inf, 0ll}, double_pos_inf},
      {"trunc", {double_neg_inf, 0ll}, double_neg_inf},
      {"trunc", {double_nan, 0ll}, double_nan},

      // CEIL and CEILING are synonymous.
      {"ceil", {NullDouble()}, NullDouble()},
      {"ceiling", {0.0f}, 0.0f},
      {"ceil", {2.0f}, 2.0f},
      {"ceiling", {2.3f}, 3.0f},
      {"ceil", {2.8f}, 3.0f},
      {"ceiling", {2.5f}, 3.0f},
      {"ceil", {-2.3f}, -2.0f},
      {"ceiling", {-2.8f}, -2.0f},
      {"ceil", {-2.5f}, -2.0f},
      {"ceiling", {1e-2f}, 1.0f},
      {"ceil", {1e38f}, 1e38f},

      {"ceiling", {0.0}, 0.0},
      {"ceil", {2.0}, 2.0},
      {"ceiling", {2.3}, 3.0},
      {"ceil", {2.8}, 3.0},
      {"ceiling", {2.5}, 3.0},
      {"ceil", {-2.3}, -2.0},
      {"ceiling", {-2.8}, -2.0},
      {"ceil", {-2.5}, -2.0},
      {"ceiling", {1e-2}, 1.0},
      {"ceil", {1e300}, 1e300},

      {"floor", {NullDouble()}, NullDouble()},
      {"floor", {0.0f}, 0.0f},
      {"floor", {2.0f}, 2.0f},
      {"floor", {2.3f}, 2.0f},
      {"floor", {2.8f}, 2.0f},
      {"floor", {2.5f}, 2.0f},
      {"floor", {-2.3f}, -3.0f},
      {"floor", {-2.8f}, -3.0f},
      {"floor", {-2.5f}, -3.0f},
      {"floor", {1e-2f}, 0.0f},
      {"floor", {1e38f}, 1e38f},

      {"floor", {0.0}, 0.0},
      {"floor", {2.0}, 2.0},
      {"floor", {2.3}, 2.0},
      {"floor", {2.8}, 2.0},
      {"floor", {2.5}, 2.0},
      {"floor", {-2.3}, -3.0},
      {"floor", {-2.8}, -3.0},
      {"floor", {-2.5}, -3.0},
      {"floor", {1e-2}, 0.0},
      {"floor", {1e300}, 1e300},
  };

  std::vector<FunctionTestCall> numeric_tests = {
      {"round", {NullNumeric()}, NullNumeric()},
      {"round", {NumericValue()}, NumericValue()},
      {"round", {NumericValue(2LL)}, NumericValue(2LL)},
      {"round",
       {NumericValue::FromString("2.3").value()},
       NumericValue::FromString("2.0").value()},
      {"round",
       {NumericValue::FromString("2.8").value()},
       NumericValue::FromString("3.0").value()},
      {"round",
       {NumericValue::FromString("2.5").value()},
       NumericValue::FromString("3.0").value()},
      {"round",
       {NumericValue::FromString("-2.3").value()},
       NumericValue::FromString("-2.0").value()},
      {"round",
       {NumericValue::FromString("-2.8").value()},
       NumericValue::FromString("-3.0").value()},
      {"round",
       {NumericValue::FromString("-2.5").value()},
       NumericValue::FromString("-3.0").value()},
      {"round", {NumericValue::FromString("1e-2").value()}, NumericValue()},
      {"round", {NumericValue::MaxValue()}, NullNumeric(), OUT_OF_RANGE},
      {"round", {NumericValue::MinValue()}, NullNumeric(), OUT_OF_RANGE},

      {"round", {NullNumeric(), NullInt64()}, NullNumeric()},
      {"round", {NullNumeric(), 0ll}, NullNumeric()},
      {"round", {NumericValue(), NullInt64()}, NullNumeric()},
      {"round",
       {NumericValue::FromString("3.1415925").value(), 2ll},
       NumericValue::FromString("3.14").value()},
      {"round",
       {NumericValue::FromString("3.1415925").value(), 3ll},
       NumericValue::FromString("3.142").value()},
      {"round",
       {NumericValue::FromString("314159.25").value(), -3ll},
       NumericValue::FromString("314000").value()},
      {"round",
       {NumericValue::FromString("314159.25").value(), -2ll},
       NumericValue::FromString("314200").value()},
      {"round",
       {NumericValue::FromString("3.4028234").value(), -39ll},
       NumericValue()},
      {"round",
       {NumericValue::FromString("3.1415925").value(), 10ll},
       NumericValue::FromString("3.1415925").value()},
      {"round", {NumericValue::MaxValue(), -1ll}, NullNumeric(), OUT_OF_RANGE},
      {"round", {NumericValue::MaxValue(), 0ll}, NullNumeric(), OUT_OF_RANGE},
      {"round", {NumericValue::MaxValue(), 1ll}, NullNumeric(), OUT_OF_RANGE},
      {"round", {NumericValue::MinValue(), -1ll}, NullNumeric(), OUT_OF_RANGE},
      {"round", {NumericValue::MinValue(), 0ll}, NullNumeric(), OUT_OF_RANGE},
      {"round", {NumericValue::MinValue(), 1ll}, NullNumeric(), OUT_OF_RANGE},

      {"trunc", {NullNumeric()}, NullNumeric()},
      {"trunc", {NumericValue()}, NumericValue()},
      {"trunc", {NumericValue(2ll)}, NumericValue(2ll)},
      {"trunc", {NumericValue::FromString("2.3").value()}, NumericValue(2ll)},
      {"trunc", {NumericValue::FromString("2.8").value()}, NumericValue(2ll)},
      {"trunc", {NumericValue::FromString("2.5").value()}, NumericValue(2ll)},
      {"trunc", {NumericValue::FromString("-2.3").value()}, NumericValue(-2ll)},
      {"trunc", {NumericValue::FromString("-2.8").value()}, NumericValue(-2ll)},
      {"trunc", {NumericValue::FromString("-2.5").value()}, NumericValue(-2ll)},
      {"trunc", {NumericValue::FromString("0.001").value()}, NumericValue()},
      {"trunc",
       {NumericValue::MaxValue()},
       NumericValue::FromString("99999999999999999999999999999").value()},
      {"trunc",
       {NumericValue::MinValue()},
       NumericValue::FromString("-99999999999999999999999999999").value()},

      {"trunc",
       {NumericValue::FromString("3.1415925").value(), 2ll},
       NumericValue::FromString("3.14").value()},
      {"trunc",
       {NumericValue::FromString("3.1415925").value(), 3ll},
       NumericValue::FromString("3.141").value()},
      {"trunc",
       {NumericValue::FromString("314159.25").value(), -3ll},
       NumericValue::FromString("314000").value()},
      {"trunc",
       {NumericValue::FromString("314159.25").value(), -2ll},
       NumericValue::FromString("314100").value()},
      {"trunc",
       {NumericValue::FromString("3.4028234").value(), -39ll},
       NumericValue()},
      {"trunc",
       {NumericValue::FromString("0.0001").value(), 10ll},
       NumericValue::FromString("0.0001").value()},
      {"trunc",
       {NumericValue::MaxValue(), -1ll},
       NumericValue::FromString("99999999999999999999999999990").value()},
      {"trunc",
       {NumericValue::MaxValue(), 0ll},
       NumericValue::FromString("99999999999999999999999999999").value()},
      {"trunc",
       {NumericValue::MaxValue(), 1ll},
       NumericValue::FromString("99999999999999999999999999999.9").value()},
      {"trunc",
       {NumericValue::MinValue(), -1ll},
       NumericValue::FromString("-99999999999999999999999999990").value()},
      {"trunc",
       {NumericValue::MinValue(), 0ll},
       NumericValue::FromString("-99999999999999999999999999999").value()},
      {"trunc",
       {NumericValue::MinValue(), 1ll},
       NumericValue::FromString("-99999999999999999999999999999.9").value()},

      {"ceil", {NullNumeric()}, NullNumeric()},
      {"ceiling", {NumericValue()}, NumericValue()},
      {"ceil", {NumericValue(2LL)}, NumericValue(2LL)},
      {"ceiling", {NumericValue::FromString("2.3").value()}, NumericValue(3LL)},
      {"ceil", {NumericValue::FromString("2.8").value()}, NumericValue(3LL)},
      {"ceiling", {NumericValue::FromString("2.5").value()}, NumericValue(3LL)},
      {"ceil", {NumericValue::FromString("-2.3").value()}, NumericValue(-2LL)},
      {"ceiling",
       {NumericValue::FromString("-2.8").value()},
       NumericValue(-2LL)},
      {"ceil", {NumericValue::FromString("-2.5").value()}, NumericValue(-2LL)},
      {"ceiling",
       {NumericValue::FromString("0.001").value()},
       NumericValue(1LL)},
      {"ceiling", {NumericValue::FromString("-0.001").value()}, NumericValue()},
      {"ceil", {NumericValue::MaxValue()}, NullNumeric(), OUT_OF_RANGE},
      {"ceiling", {NumericValue::MaxValue()}, NullNumeric(), OUT_OF_RANGE},

      {"floor", {NullNumeric()}, NullNumeric()},
      {"floor", {NumericValue()}, NumericValue()},
      {"floor", {NumericValue(2LL)}, NumericValue(2LL)},
      {"floor", {NumericValue::FromString("2.3").value()}, NumericValue(2LL)},
      {"floor", {NumericValue::FromString("2.8").value()}, NumericValue(2LL)},
      {"floor", {NumericValue::FromString("2.5").value()}, NumericValue(2LL)},
      {"floor", {NumericValue::FromString("-2.3").value()}, NumericValue(-3LL)},
      {"floor", {NumericValue::FromString("-2.8").value()}, NumericValue(-3LL)},
      {"floor", {NumericValue::FromString("-2.5").value()}, NumericValue(-3LL)},
      {"floor", {NumericValue::FromString("0.001").value()}, NumericValue()},
      {"floor",
       {NumericValue::FromString("-0.001").value()},
       NumericValue(-1LL)},
  };

  for (auto& test_case : numeric_tests) {
    test_case.params.AddRequiredFeature(FEATURE_NUMERIC_TYPE);
    all_tests.emplace_back(test_case);
  }

  std::vector<FunctionTestCall> numeric_with_rounding_mode_tests = {
      // Round(x, n, round_half_even)
      {"round",
       {NumericValue::FromString("3.145000").value(), 2ll,
        Value::Enum(rounding_mode_type, "ROUND_HALF_EVEN")},
       NumericValue::FromString("3.14").value()},
      {"round",
       {NumericValue::FromString("3144500.00").value(), -3ll,
        Value::Enum(rounding_mode_type, "ROUND_HALF_EVEN")},
       NumericValue::FromString("3144000").value()},
      {"round",
       {NumericValue::FromString("3.1415925").value(), int64max,
        Value::Enum(rounding_mode_type, "ROUND_HALF_EVEN")},
       NumericValue::FromString("3.1415925").value()},
      {"round",
       {NumericValue::FromString("3.1415925").value(), int64min,
        Value::Enum(rounding_mode_type, "ROUND_HALF_EVEN")},
       NumericValue::FromString("0.0").value()},
      {"round",
       {NullNumeric(), 0ll, Value::Enum(rounding_mode_type, "ROUND_HALF_EVEN")},
       NullNumeric()},
      {"round",
       {NumericValue::MaxValue(), -1ll,
        Value::Enum(rounding_mode_type, "ROUND_HALF_EVEN")},
       NullNumeric(),
       OUT_OF_RANGE},
      {"round",
       {NumericValue::MaxValue(), 0ll,
        Value::Enum(rounding_mode_type, "ROUND_HALF_EVEN")},
       NullNumeric(),
       OUT_OF_RANGE},
      {"round",
       {NumericValue::MaxValue(), 1ll,
        Value::Enum(rounding_mode_type, "ROUND_HALF_EVEN")},
       NullNumeric(),
       OUT_OF_RANGE},
      {"round",
       {NumericValue::MinValue(), -1ll,
        Value::Enum(rounding_mode_type, "ROUND_HALF_EVEN")},
       NullNumeric(),
       OUT_OF_RANGE},
      {"round",
       {NumericValue::MinValue(), 0ll,
        Value::Enum(rounding_mode_type, "ROUND_HALF_EVEN")},
       NullNumeric(),
       OUT_OF_RANGE},
      {"round",
       {NumericValue::MinValue(), 1ll,
        Value::Enum(rounding_mode_type, "ROUND_HALF_EVEN")},
       NullNumeric(),
       OUT_OF_RANGE},
  };

  for (auto& test_case : numeric_with_rounding_mode_tests) {
    test_case.params.AddRequiredFeature(FEATURE_NUMERIC_TYPE)
        .AddRequiredFeature(FEATURE_ROUND_WITH_ROUNDING_MODE);
    all_tests.emplace_back(test_case);
  }

  std::vector<FunctionTestCall> bignumeric_tests = {
      {"round", {NullBigNumeric()}, NullBigNumeric()},
      {"round", {BigNumericValue()}, BigNumericValue()},
      {"round", {BigNumericValue(2)}, BigNumericValue(2)},
      {"round",
       {BigNumericValue::FromString("2.3").value()},
       BigNumericValue::FromString("2.0").value()},
      {"round",
       {BigNumericValue::FromString("2.8").value()},
       BigNumericValue::FromString("3.0").value()},
      {"round",
       {BigNumericValue::FromString("2.5").value()},
       BigNumericValue::FromString("3.0").value()},
      {"round",
       {BigNumericValue::FromString("-2.3").value()},
       BigNumericValue::FromString("-2.0").value()},
      {"round",
       {BigNumericValue::FromString("-2.8").value()},
       BigNumericValue::FromString("-3.0").value()},
      {"round",
       {BigNumericValue::FromString("-2.5").value()},
       BigNumericValue::FromString("-3.0").value()},
      {"round",
       {BigNumericValue::FromString("1e-2").value()},
       BigNumericValue()},
      {"round",
       {BigNumericValue::MaxValue()},
       BigNumericValue::FromString("578960446186580977117854925043439539266")
           .value()},
      {"round",
       {BigNumericValue::MinValue()},
       BigNumericValue::FromString("-578960446186580977117854925043439539266")
           .value()},

      {"round", {NullBigNumeric(), NullInt64()}, NullBigNumeric()},
      {"round", {NullBigNumeric(), 0ll}, NullBigNumeric()},
      {"round", {BigNumericValue(), NullInt64()}, NullBigNumeric()},
      {"round",
       {BigNumericValue::FromString("1.12345678901234567890123456789012345678")
            .value(),
        2ll},
       BigNumericValue::FromString("1.12").value()},
      {"round",
       {BigNumericValue::FromString("1.12345678901234567890123456789012345678")
            .value(),
        3ll},
       BigNumericValue::FromString("1.123").value()},
      {"round",
       {BigNumericValue::FromString("1.12345678901234567890123456789012345678")
            .value(),
        18ll},
       BigNumericValue::FromString("1.123456789012345679").value()},
      {"round",
       {BigNumericValue::FromString("1.12345678901234567890123456789012345678")
            .value(),
        19ll},
       BigNumericValue::FromString("1.1234567890123456789").value()},
      {"round",
       {BigNumericValue::FromString("1123456789.01234567890123456789012345678")
            .value(),
        -3ll},
       BigNumericValue::FromString("1123457000").value()},
      {"round",
       {BigNumericValue::FromString("1123456789.01234567890123456789012345678")
            .value(),
        -2ll},
       BigNumericValue::FromString("1123456800").value()},
      {"round",
       {BigNumericValue::FromString("1123456789.01234567890123456789012345678")
            .value(),
        -39ll},
       BigNumericValue()},
      {"round",
       {BigNumericValue::FromString("1.12345678901234567890123456789012345678")
            .value(),
        40ll},
       BigNumericValue::FromString("1.12345678901234567890123456789012345678")
           .value()},
      {"round",
       {BigNumericValue::MaxValue(), -1ll},
       NullBigNumeric(),
       OUT_OF_RANGE},
      {"round",
       {BigNumericValue::MaxValue(), 0ll},
       BigNumericValue::FromString("578960446186580977117854925043439539266")
           .value()},
      {"round",
       {BigNumericValue::MaxValue(), 1ll},
       BigNumericValue::FromString("578960446186580977117854925043439539266.3")
           .value()},
      {"round",
       {BigNumericValue::MinValue(), -1ll},
       NullBigNumeric(),
       OUT_OF_RANGE},
      {"round",
       {BigNumericValue::MinValue(), 0ll},
       BigNumericValue::FromString("-578960446186580977117854925043439539266")
           .value()},
      {"round",
       {BigNumericValue::MinValue(), 1ll},
       BigNumericValue::FromString("-578960446186580977117854925043439539266.3")
           .value()},

      {"trunc", {NullBigNumeric()}, NullBigNumeric()},
      {"trunc", {BigNumericValue()}, BigNumericValue()},
      {"trunc", {BigNumericValue(2)}, BigNumericValue(2)},
      {"trunc",
       {BigNumericValue::FromString("2.3").value()},
       BigNumericValue(2)},
      {"trunc",
       {BigNumericValue::FromString("2.8").value()},
       BigNumericValue(2)},
      {"trunc",
       {BigNumericValue::FromString("2.5").value()},
       BigNumericValue(2)},
      {"trunc",
       {BigNumericValue::FromString("-2.3").value()},
       BigNumericValue(-2)},
      {"trunc",
       {BigNumericValue::FromString("-2.8").value()},
       BigNumericValue(-2)},
      {"trunc",
       {BigNumericValue::FromString("-2.5").value()},
       BigNumericValue(-2)},
      {"trunc",
       {BigNumericValue::FromString("0.001").value()},
       BigNumericValue()},
      {"trunc",
       {BigNumericValue::MaxValue()},
       BigNumericValue::FromString("578960446186580977117854925043439539266")
           .value()},
      {"trunc",
       {BigNumericValue::MinValue()},
       BigNumericValue::FromString("-578960446186580977117854925043439539266")
           .value()},

      {"trunc",
       {BigNumericValue::FromString("1.12345678901234567890123456789012345678")
            .value(),
        2ll},
       BigNumericValue::FromString("1.12").value()},
      {"trunc",
       {BigNumericValue::FromString("1.12345678901234567890123456789012345678")
            .value(),
        3ll},
       BigNumericValue::FromString("1.123").value()},
      {"trunc",
       {BigNumericValue::FromString("1.12345678901234567890123456789012345678")
            .value(),
        18ll},
       BigNumericValue::FromString("1.123456789012345678").value()},
      {"trunc",
       {BigNumericValue::FromString("1.12345678901234567890123456789012345678")
            .value(),
        19ll},
       BigNumericValue::FromString("1.1234567890123456789").value()},
      {"trunc",
       {BigNumericValue::FromString("1123456789.01234567890123456789012345678")
            .value(),
        -3ll},
       BigNumericValue::FromString("1123456000").value()},
      {"trunc",
       {BigNumericValue::FromString("1123456789.01234567890123456789012345678")
            .value(),
        -2ll},
       BigNumericValue::FromString("1123456700").value()},
      {"trunc",
       {BigNumericValue::FromString("1123456789.01234567890123456789012345678")
            .value(),
        -39ll},
       BigNumericValue()},
      {"trunc",
       {BigNumericValue::FromString("1.12345678901234567890123456789012345678")
            .value(),
        40ll},
       BigNumericValue::FromString("1.12345678901234567890123456789012345678")
           .value()},
      {"trunc",
       {BigNumericValue::FromString("0.0001").value(), 10ll},
       BigNumericValue::FromString("0.0001").value()},
      {"trunc",
       {BigNumericValue::MaxValue(), -1ll},
       BigNumericValue::FromString("578960446186580977117854925043439539260")
           .value()},
      {"trunc",
       {BigNumericValue::MaxValue(), 0ll},
       BigNumericValue::FromString("578960446186580977117854925043439539266")
           .value()},
      {"trunc",
       {BigNumericValue::MaxValue(), 1ll},
       BigNumericValue::FromString("578960446186580977117854925043439539266.3")
           .value()},
      {"trunc",
       {BigNumericValue::MinValue(), -1ll},
       BigNumericValue::FromString("-578960446186580977117854925043439539260")
           .value()},
      {"trunc",
       {BigNumericValue::MinValue(), 0ll},
       BigNumericValue::FromString("-578960446186580977117854925043439539266")
           .value()},
      {"trunc",
       {BigNumericValue::MinValue(), 1ll},
       BigNumericValue::FromString("-578960446186580977117854925043439539266.3")
           .value()},

      {"ceil", {NullBigNumeric()}, NullBigNumeric()},
      {"ceiling", {BigNumericValue()}, BigNumericValue()},
      {"ceil", {BigNumericValue(2)}, BigNumericValue(2)},
      {"ceiling",
       {BigNumericValue::FromString("2.3").value()},
       BigNumericValue(3)},
      {"ceil",
       {BigNumericValue::FromString("2.8").value()},
       BigNumericValue(3)},
      {"ceiling",
       {BigNumericValue::FromString("2.5").value()},
       BigNumericValue(3)},
      {"ceil",
       {BigNumericValue::FromString("-2.3").value()},
       BigNumericValue(-2)},
      {"ceiling",
       {BigNumericValue::FromString("-2.8").value()},
       BigNumericValue(-2)},
      {"ceil",
       {BigNumericValue::FromString("-2.5").value()},
       BigNumericValue(-2)},
      {"ceiling",
       {BigNumericValue::FromString("0.001").value()},
       BigNumericValue(1)},
      {"ceiling",
       {BigNumericValue::FromString("1e-38").value()},
       BigNumericValue(1)},
      {"ceiling",
       {BigNumericValue::FromString("-0.001").value()},
       BigNumericValue()},
      {"ceiling",
       {BigNumericValue::FromString("-1e-38").value()},
       BigNumericValue()},
      {"ceil",
       {BigNumericValue::FromString("578960446186580977117854925043439539266.1")
            .value()},
       NullBigNumeric(),
       OUT_OF_RANGE},
      {"ceiling",
       {BigNumericValue::FromString("578960446186580977117854925043439539266."
                                    "00000000000000000000000000000000000001")
            .value()},
       NullBigNumeric(),
       OUT_OF_RANGE},
      {"ceil", {BigNumericValue::MaxValue()}, NullBigNumeric(), OUT_OF_RANGE},
      {"ceiling",
       {BigNumericValue::MaxValue()},
       NullBigNumeric(),
       OUT_OF_RANGE},

      {"floor", {NullBigNumeric()}, NullBigNumeric()},
      {"floor", {BigNumericValue()}, BigNumericValue()},
      {"floor", {BigNumericValue(2)}, BigNumericValue(2)},
      {"floor",
       {BigNumericValue::FromString("2.3").value()},
       BigNumericValue(2)},
      {"floor",
       {BigNumericValue::FromString("2.8").value()},
       BigNumericValue(2)},
      {"floor",
       {BigNumericValue::FromString("2.5").value()},
       BigNumericValue(2)},
      {"floor",
       {BigNumericValue::FromString("-2.3").value()},
       BigNumericValue(-3)},
      {"floor",
       {BigNumericValue::FromString("-2.8").value()},
       BigNumericValue(-3)},
      {"floor",
       {BigNumericValue::FromString("-2.5").value()},
       BigNumericValue(-3)},
      {"floor",
       {BigNumericValue::FromString("0.001").value()},
       BigNumericValue()},
      {"floor",
       {BigNumericValue::FromString("1e-38").value()},
       BigNumericValue()},
      {"floor",
       {BigNumericValue::FromString("2e-38").value()},
       BigNumericValue()},
      {"floor",
       {BigNumericValue::FromString("-0.001").value()},
       BigNumericValue(-1)},
      {"floor",
       {BigNumericValue::FromString("-1e-38").value()},
       BigNumericValue(-1)},
      {"floor",
       {BigNumericValue::FromString(
            "-578960446186580977117854925043439539266.1")
            .value()},
       NullBigNumeric(),
       OUT_OF_RANGE},
      {"floor",
       {BigNumericValue::FromString("-578960446186580977117854925043439539266."
                                    "00000000000000000000000000000000000001")
            .value()},
       NullBigNumeric(),
       OUT_OF_RANGE},
      {"floor", {BigNumericValue::MinValue()}, NullBigNumeric(), OUT_OF_RANGE},
  };

  for (auto& test_case : bignumeric_tests) {
    test_case.params.AddRequiredFeature(FEATURE_BIGNUMERIC_TYPE);
    all_tests.emplace_back(test_case);
  }

  std::vector<FunctionTestCall> bignumeric_with_rounding_mode_tests = {
      // ROUND(x, y, round_half_even)
      {"round",
       {NullBigNumeric(), NullInt64(),
        Value::Enum(rounding_mode_type, "ROUND_HALF_EVEN")},
       NullBigNumeric()},
      {"round",
       {NullBigNumeric(), 0ll,
        Value::Enum(rounding_mode_type, "ROUND_HALF_EVEN")},
       NullBigNumeric()},
      {"round",
       {BigNumericValue(), NullInt64(),
        Value::Enum(rounding_mode_type, "ROUND_HALF_EVEN")},
       NullBigNumeric()},
      {"round",
       {BigNumericValue::FromString("1.12345678901234567885000000").value(),
        19ll, Value::Enum(rounding_mode_type, "ROUND_HALF_EVEN")},
       BigNumericValue::FromString("1.1234567890123456788").value()},
      {"round",
       {BigNumericValue::FromString("1123456500.0000000").value(), -3ll,
        Value::Enum(rounding_mode_type, "ROUND_HALF_EVEN")},
       BigNumericValue::FromString("1123456000").value()},
      {"round",
       {BigNumericValue::FromString("1123456789.01234567890123456789012345678")
            .value(),
        -39ll, Value::Enum(rounding_mode_type, "ROUND_HALF_EVEN")},
       BigNumericValue()},
      {"round",
       {BigNumericValue::MaxValue(), -1ll,
        Value::Enum(rounding_mode_type, "ROUND_HALF_EVEN")},
       NullBigNumeric(),
       OUT_OF_RANGE},
      {"round",
       {BigNumericValue::MaxValue(), 0ll,
        Value::Enum(rounding_mode_type, "ROUND_HALF_EVEN")},
       BigNumericValue::FromString("578960446186580977117854925043439539266")
           .value()},
      {"round",
       {BigNumericValue::MaxValue(), 1ll,
        Value::Enum(rounding_mode_type, "ROUND_HALF_EVEN")},
       BigNumericValue::FromString("578960446186580977117854925043439539266.3")
           .value()},
      {"round",
       {BigNumericValue::MinValue(), -1ll,
        Value::Enum(rounding_mode_type, "ROUND_HALF_EVEN")},
       NullBigNumeric(),
       OUT_OF_RANGE},
      {"round",
       {BigNumericValue::MinValue(), 0ll,
        Value::Enum(rounding_mode_type, "ROUND_HALF_EVEN")},
       BigNumericValue::FromString("-578960446186580977117854925043439539266")
           .value()},
      {"round",
       {BigNumericValue::MinValue(), 1ll,
        Value::Enum(rounding_mode_type, "ROUND_HALF_EVEN")},
       BigNumericValue::FromString("-578960446186580977117854925043439539266.3")
           .value()},
  };

  for (auto& test_case : bignumeric_with_rounding_mode_tests) {
    test_case.params.AddRequiredFeature(FEATURE_BIGNUMERIC_TYPE)
        .AddRequiredFeature(FEATURE_ROUND_WITH_ROUNDING_MODE);
    all_tests.emplace_back(test_case);
  }

  return all_tests;
}

}  // namespace zetasql
