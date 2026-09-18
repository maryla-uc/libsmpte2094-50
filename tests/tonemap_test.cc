/**
 * Copyright 2026 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "smpte2094_50/tonemap.h"

#include <array>
#include <vector>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "smpte2094_50/smpte2094_50.h"

namespace smpte2094_50 {
namespace {

constexpr float kEpsilon = 1e-4f;

TEST(ToneMapperTest, ZeroGainRuleDoesNotChangeImage) {
  DynamicMetadata agtm;
  agtm.baseline_hdr_headroom_log2 = 1.0f;
  agtm.gain_application_space_chromaticities =
      DynamicMetadata::kChromaticitiesRec2020;

  auto mapper_or = ToneMapper::Create(agtm, 1.0f);
  ASSERT_TRUE(mapper_or.ok());
  const auto& mapper = *mapper_or;
  EXPECT_TRUE(mapper.IsIdentity());

  std::array<float, 3> in_pixel = {0.5f, 0.5f, 0.5f};
  auto out_pixel = mapper.ToneMapPixel(in_pixel);
  EXPECT_NEAR(out_pixel[0], 0.5f, kEpsilon);
  EXPECT_NEAR(out_pixel[1], 0.5f, kEpsilon);
  EXPECT_NEAR(out_pixel[2], 0.5f, kEpsilon);

  std::vector<float> buffer = {0.5f, 0.5f, 0.5f, 0.2f, 0.3f, 0.4f};
  EXPECT_TRUE(mapper.ToneMapBuffer(absl::MakeSpan(buffer)).ok());
  EXPECT_NEAR(buffer[0], 0.5f, kEpsilon);
  EXPECT_NEAR(buffer[3], 0.2f, kEpsilon);
}

TEST(ToneMapperTest, LinearGainRuleAppliesExpectedGain) {
  DynamicMetadata agtm;
  agtm.baseline_hdr_headroom_log2 = 0.0f;
  agtm.gain_application_space_chromaticities =
      DynamicMetadata::kChromaticitiesRec2020;

  // Set up a simple tone mapping rule for headroom 2.0f.
  // Constant gain curve of +1 in log2 space (multiplier of 2.0).
  ToneMappingRule rule;
  rule.alternate_hdr_headroom_log2 = 2.0f;
  rule.use_pchip_slope = true;
  rule.mix = {
      .rgb = {0.0f, 0.0f, 0.0f},
      .max = 0.0f,
      .min = 0.0f,
      .component = 1.0f,
  };
  ControlPoint cp0, cp1;
  cp0.x = 0.0f;
  cp0.y = 1.0f;
  cp0.m = 0.0f;
  cp1.x = 64.0f;
  cp1.y = 1.0f;
  cp1.m = 0.0f;
  rule.curve = {cp0, cp1};
  agtm.rules.push_back(rule);

  // Target headroom 2.0f: full interpolation to the alternate rule.
  auto mapper_2_or = ToneMapper::Create(agtm, 2.0f);
  ASSERT_TRUE(mapper_2_or.ok());
  const auto& mapper_2 = *mapper_2_or;

  // Output should be input * 2^1 = input * 2 = 1.0f.
  auto out_2 = mapper_2.ToneMapPixel({0.5f, 0.5f, 0.5f});
  EXPECT_NEAR(out_2[0], 1.0f, kEpsilon);
  EXPECT_NEAR(out_2[1], 1.0f, kEpsilon);
  EXPECT_NEAR(out_2[2], 1.0f, kEpsilon);

  // Target headroom 1.0f: halfway between baseline 0.0f and alternate 2.0f
  // (weight = 0.5). Expected gain = 2^(0.5 * 1.0) = sqrt(2) ≈ 1.41421356.
  // Expected output = 0.5 * sqrt(2) ≈ 0.70710678.
  auto mapper_1_or = ToneMapper::Create(agtm, 1.0f);
  ASSERT_TRUE(mapper_1_or.ok());
  const auto& mapper_1 = *mapper_1_or;

  auto out_1 = mapper_1.ToneMapPixel({0.5f, 0.5f, 0.5f});
  EXPECT_NEAR(out_1[0], 0.707107f, kEpsilon);
  EXPECT_NEAR(out_1[1], 0.707107f, kEpsilon);
  EXPECT_NEAR(out_1[2], 0.707107f, kEpsilon);

  // In-place buffer mapping.
  std::vector<float> buffer = {0.5f, 0.5f, 0.5f};
  EXPECT_TRUE(mapper_1.ToneMapBuffer(absl::MakeSpan(buffer)).ok());
  EXPECT_NEAR(buffer[0], 0.707107f, kEpsilon);
  EXPECT_NEAR(buffer[1], 0.707107f, kEpsilon);
  EXPECT_NEAR(buffer[2], 0.707107f, kEpsilon);
}

TEST(ToneMapperTest, InvalidMetadataReturnsError) {
  DynamicMetadata agtm;
  agtm.baseline_hdr_headroom_log2 = 99.0f;  // Out of range [0, 6]

  auto mapper_or = ToneMapper::Create(agtm, 1.0f);
  EXPECT_FALSE(mapper_or.ok());
  EXPECT_EQ(mapper_or.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST(ToneMapperTest, BufferLengthMustBeMultipleOfThree) {
  DynamicMetadata agtm;
  auto mapper_or = ToneMapper::Create(agtm, 0.0f);
  ASSERT_TRUE(mapper_or.ok());

  std::vector<float> invalid_buffer = {0.5f, 0.5f, 0.5f, 0.5f};
  auto status = mapper_or->ToneMapBuffer(absl::MakeSpan(invalid_buffer));
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

TEST(ToneMapperTest, EmptyBufferSucceeds) {
  DynamicMetadata agtm;
  auto mapper_or = ToneMapper::Create(agtm, 0.0f);
  ASSERT_TRUE(mapper_or.ok());

  EXPECT_TRUE(mapper_or->ToneMapBuffer({}).ok());
  std::vector<float> empty_vec;
  EXPECT_TRUE(mapper_or->ToneMapBuffer(absl::MakeSpan(empty_vec)).ok());
}

TEST(ToneMapperTest, ZeroKComponentOptimized) {
  DynamicMetadata agtm;
  agtm.baseline_hdr_headroom_log2 = 0.0f;

  ToneMappingRule rule;
  rule.alternate_hdr_headroom_log2 = 1.0f;
  rule.use_pchip_slope = true;
  // Isotropic mix (k_component = 0)
  rule.mix = {
      .rgb = {0.2627f, 0.6780f, 0.0593f},
      .max = 0.0f,
      .min = 0.0f,
      .component = 0.0f,
  };
  ControlPoint cp0, cp1;
  cp0.x = 0.0f;
  cp0.y = 0.0f;
  cp0.m = 0.0f;
  cp1.x = 1.0f;
  cp1.y = 1.0f;
  cp1.m = 1.0f;
  rule.curve = {cp0, cp1};
  agtm.rules.push_back(rule);

  auto mapper_or = ToneMapper::Create(agtm, 1.0f);
  ASSERT_TRUE(mapper_or.ok());

  // A neutral gray input: c_r = c_g = c_b = 0.5f.
  // Mixed coordinate is 0.5f. Gain is 0.5f in log2. Output is 0.5 * 2^0.5 ≈
  // 0.7071f.
  auto out = mapper_or->ToneMapPixel({0.5f, 0.5f, 0.5f});
  EXPECT_NEAR(out[0], 0.707107f, kEpsilon);
  EXPECT_NEAR(out[1], 0.707107f, kEpsilon);
  EXPECT_NEAR(out[2], 0.707107f, kEpsilon);
}

TEST(ToneMapperTest, HdrHighlightInputsLargerThanOne) {
  DynamicMetadata agtm;
  agtm.baseline_hdr_headroom_log2 = 2.0f;

  ToneMappingRule rule;
  rule.alternate_hdr_headroom_log2 = 0.0f;
  rule.use_pchip_slope = true;
  rule.mix = {
      .rgb = {0.0f, 0.0f, 0.0f},
      .max = 0.0f,
      .min = 0.0f,
      .component = 1.0f,
  };
  // Compressive tone mapping curve: (0, 0), (1.0, -0.5), (4.0, -1.0)
  ControlPoint cp0, cp1, cp2;
  cp0.x = 0.0f;
  cp0.y = 0.0f;
  cp0.m = 0.0f;
  cp1.x = 1.0f;
  cp1.y = -0.5f;
  cp1.m = 0.0f;
  cp2.x = 4.0f;
  cp2.y = -1.0f;
  cp2.m = 0.0f;
  rule.curve = {cp0, cp1, cp2};
  agtm.rules.push_back(rule);

  auto mapper_or = ToneMapper::Create(agtm, 0.0f);
  ASSERT_TRUE(mapper_or.ok());

  // Input highlight pixel with values > 1.0 (e.g. 1.0, 2.0, 4.0).
  // At x = 4.0, y = -1.0 in log2 space, gain is 2^(-1) = 0.5.
  // Output should be 4.0 * 0.5 = 2.0.
  auto out = mapper_or->ToneMapPixel({4.0f, 4.0f, 4.0f});
  EXPECT_NEAR(out[0], 2.0f, kEpsilon);
  EXPECT_NEAR(out[1], 2.0f, kEpsilon);
  EXPECT_NEAR(out[2], 2.0f, kEpsilon);

  // Buffer mapping with SDR-relative HDR values > 1.0
  std::vector<float> hdr_buffer = {1.0f, 2.0f, 4.0f};
  EXPECT_TRUE(mapper_or->ToneMapBuffer(absl::MakeSpan(hdr_buffer)).ok());
  // x = 1.0 -> y = -0.5 -> gain = 2^(-0.5) -> 1.0 / sqrt(2) ≈ 0.70710678
  EXPECT_NEAR(hdr_buffer[0], 0.707107f, kEpsilon);
  // x = 4.0 -> y = -1.0 -> gain = 0.5 -> 4.0 * 0.5 = 2.0
  EXPECT_NEAR(hdr_buffer[2], 2.0f, kEpsilon);
}

TEST(ToneMapperTest, LogarithmicExtrapolationRollsOffHighlights) {
  DynamicMetadata agtm;
  agtm.baseline_hdr_headroom_log2 = 0.0f;

  ToneMappingRule rule;
  rule.alternate_hdr_headroom_log2 = 2.0f;
  rule.use_pchip_slope = true;
  rule.mix = {
      .rgb = {0.0f, 0.0f, 0.0f},
      .max = 0.0f,
      .min = 0.0f,
      .component = 1.0f,
  };
  // Control points end at x = 1.0, y = 0.0
  ControlPoint cp0, cp1;
  cp0.x = 0.0f;
  cp0.y = 0.0f;
  cp0.m = 0.0f;
  cp1.x = 1.0f;
  cp1.y = 0.0f;
  cp1.m = 0.0f;
  rule.curve = {cp0, cp1};
  agtm.rules.push_back(rule);

  auto mapper_or = ToneMapper::Create(agtm, 2.0f);
  ASSERT_TRUE(mapper_or.ok());

  // According to Section 6.5.3, for x > x_{N-1}:
  // gain(x) = y_{N-1} + log2(x_{N-1} / x)
  // Mapped value = x * 2^gain(x) = x_{N-1} * 2^y_{N-1} = 1.0 * 2^0 = 1.0.
  // The output should be clamped/rolled-off to exactly 1.0 for all x >= 1.0.
  for (float highlight_val : {1.0f, 1.5f, 2.0f, 4.0f, 8.0f}) {
    auto out = mapper_or->ToneMapPixel({highlight_val, highlight_val, highlight_val});
    EXPECT_NEAR(out[0], 1.0f, kEpsilon);
    EXPECT_NEAR(out[1], 1.0f, kEpsilon);
    EXPECT_NEAR(out[2], 1.0f, kEpsilon);
  }
}

}  // namespace
}  // namespace smpte2094_50
