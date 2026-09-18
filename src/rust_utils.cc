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

#include "rust_utils.h"

#include <utility>
#include <vector>

#include "smpte2094_50/smpte2094_50.h"
#include "tonemap_ffi.h"
#include "utils_ffi.h"

namespace smpte2094_50 {

utils_rs::DynamicMetadata ToUtilsRsMetadata(const DynamicMetadata& cpp) {
  utils_rs::DynamicMetadata rust;
  rust.has_adaptive_tone_map_flag = cpp.has_adaptive_tone_map_flag;
  rust.use_reference_white_tone_mapping_flag =
      cpp.use_reference_white_tone_mapping_flag;
  rust.hdr_reference_white = cpp.hdr_reference_white;
  rust.baseline_hdr_headroom_log2 = cpp.baseline_hdr_headroom_log2;
  for (int i = 0; i < 8; ++i) {
    rust.gain_application_space_chromaticities[i] =
        cpp.gain_application_space_chromaticities[i];
  }

  for (const auto& cpp_rule : cpp.rules) {
    utils_rs::ToneMappingRule rs_rule;
    rs_rule.alternate_hdr_headroom_log2 = cpp_rule.alternate_hdr_headroom_log2;
    rs_rule.use_pchip_slope = cpp_rule.use_pchip_slope;

    rs_rule.mix.rgb[0] = cpp_rule.mix.rgb[0];
    rs_rule.mix.rgb[1] = cpp_rule.mix.rgb[1];
    rs_rule.mix.rgb[2] = cpp_rule.mix.rgb[2];
    rs_rule.mix.max = cpp_rule.mix.max;
    rs_rule.mix.min = cpp_rule.mix.min;
    rs_rule.mix.component = cpp_rule.mix.component;

    for (const auto& cpp_pt : cpp_rule.curve) {
      utils_rs::ControlPoint rs_pt;
      rs_pt.x = cpp_pt.x;
      rs_pt.y = cpp_pt.y;
      rs_pt.m = cpp_pt.m;
      rs_rule.add_point(rs_pt);
    }
    rust.add_rule(rs_rule);
  }
  return rust;
}

void FromUtilsRsMetadata(const utils_rs::DynamicMetadata& rust,
                         DynamicMetadata& cpp) {
  cpp.has_adaptive_tone_map_flag = rust.has_adaptive_tone_map_flag;
  cpp.use_reference_white_tone_mapping_flag =
      rust.use_reference_white_tone_mapping_flag;
  cpp.hdr_reference_white = rust.hdr_reference_white;
  cpp.baseline_hdr_headroom_log2 = rust.baseline_hdr_headroom_log2;
  for (int i = 0; i < 8; ++i) {
    cpp.gain_application_space_chromaticities[i] =
        rust.gain_application_space_chromaticities[i];
  }

  cpp.rules.clear();
  cpp.rules.reserve(rust.get_rules().size());
  for (const auto& rs_rule : rust.get_rules().to_span()) {
    ToneMappingRule cpp_rule;
    cpp_rule.alternate_hdr_headroom_log2 = rs_rule.alternate_hdr_headroom_log2;
    cpp_rule.use_pchip_slope = rs_rule.use_pchip_slope;

    cpp_rule.mix.rgb[0] = rs_rule.mix.rgb[0];
    cpp_rule.mix.rgb[1] = rs_rule.mix.rgb[1];
    cpp_rule.mix.rgb[2] = rs_rule.mix.rgb[2];
    cpp_rule.mix.max = rs_rule.mix.max;
    cpp_rule.mix.min = rs_rule.mix.min;
    cpp_rule.mix.component = rs_rule.mix.component;

    for (const auto& rs_pt : rs_rule.get_curve().to_span()) {
      ControlPoint cpp_pt;
      cpp_pt.x = rs_pt.x;
      cpp_pt.y = rs_pt.y;
      cpp_pt.m = rs_pt.m;
      cpp_rule.curve.push_back(cpp_pt);
    }
    cpp.rules.push_back(cpp_rule);
  }
}

tonemap_ffi::FfiDynamicMetadata ToTonemapFfiMetadata(
    const DynamicMetadata& cpp) {
  tonemap_ffi::FfiDynamicMetadata rust;
  rust.has_adaptive_tone_map_flag = cpp.has_adaptive_tone_map_flag;
  rust.use_reference_white_tone_mapping_flag =
      cpp.use_reference_white_tone_mapping_flag;
  rust.hdr_reference_white = cpp.hdr_reference_white;
  rust.baseline_hdr_headroom_log2 = cpp.baseline_hdr_headroom_log2;
  for (int i = 0; i < 8; ++i) {
    rust.gain_application_space_chromaticities[i] =
        cpp.gain_application_space_chromaticities[i];
  }

  for (const auto& cpp_rule : cpp.rules) {
    tonemap_ffi::FfiToneMappingRule rs_rule;
    rs_rule.alternate_hdr_headroom_log2 = cpp_rule.alternate_hdr_headroom_log2;
    rs_rule.use_pchip_slope = cpp_rule.use_pchip_slope;

    rs_rule.mix.rgb[0] = cpp_rule.mix.rgb[0];
    rs_rule.mix.rgb[1] = cpp_rule.mix.rgb[1];
    rs_rule.mix.rgb[2] = cpp_rule.mix.rgb[2];
    rs_rule.mix.max = cpp_rule.mix.max;
    rs_rule.mix.min = cpp_rule.mix.min;
    rs_rule.mix.component = cpp_rule.mix.component;

    for (const auto& cpp_pt : cpp_rule.curve) {
      tonemap_ffi::FfiControlPoint rs_pt;
      rs_pt.x = cpp_pt.x;
      rs_pt.y = cpp_pt.y;
      rs_pt.m = cpp_pt.m;
      rs_rule.curve.push_back(rs_pt);
    }
    rust.rules.push_back(std::move(rs_rule));
  }
  return rust;
}

}  // namespace smpte2094_50
