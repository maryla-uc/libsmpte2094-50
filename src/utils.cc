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

#include "smpte2094_50/utils.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "rust_utils.h"
#include "smpte2094_50/smpte2094_50.h"
#include "utils_ffi.h"

namespace smpte2094_50 {

absl::StatusOr<std::string> ToSt209450(const DynamicMetadata& metadata) {
  utils_rs::DynamicMetadata rs_metadata = ToUtilsRsMetadata(metadata);
  const ::utils_rs::ToSt209450Result result =
      utils_rs::to_st209450_ffi(rs_metadata);
  if (!result.success) {
    return absl::InvalidArgumentError(result.get_error_message());
  }
  const auto& bytes = result.get_data();
  return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

absl::StatusOr<DynamicMetadata> FromSt209450(absl::string_view data) {
  const ::utils_rs::FromSt209450Result result =
      utils_rs::from_st209450_ffi(absl::MakeConstSpan(
          reinterpret_cast<const uint8_t*>(data.data()), data.size()));
  if (!result.success) {
    return absl::InvalidArgumentError(result.get_error_message());
  }
  DynamicMetadata metadata;
  FromUtilsRsMetadata(result.metadata, metadata);
  return metadata;
}

void PopulateUsingRwtm(DynamicMetadata& metadata) {
  utils_rs::DynamicMetadata rs_metadata = ToUtilsRsMetadata(metadata);
  utils_rs::dynamic_metadata_populate_using_rwtm(rs_metadata);
  FromUtilsRsMetadata(rs_metadata, metadata);
}

absl::Status PopulatePchipSlopes(DynamicMetadata& metadata) {
  utils_rs::DynamicMetadata rs_metadata = ToUtilsRsMetadata(metadata);
  const auto result =
      utils_rs::dynamic_metadata_populate_pchip_slopes_ffi(rs_metadata);
  if (!result.success) {
    return absl::InvalidArgumentError(result.get_error_message());
  }
  FromUtilsRsMetadata(rs_metadata, metadata);
  return absl::OkStatus();
}

absl::Status PopulateImplicitParameters(DynamicMetadata& metadata) {
  utils_rs::DynamicMetadata rs_metadata = ToUtilsRsMetadata(metadata);
  const auto result = utils_rs::populate_implicit_parameters_ffi(rs_metadata);
  if (!result.success) {
    return absl::InvalidArgumentError(result.get_error_message());
  }
  FromUtilsRsMetadata(rs_metadata, metadata);
  return absl::OkStatus();
}

bool IsValid(const DynamicMetadata& metadata) {
  utils_rs::DynamicMetadata rs_metadata = ToUtilsRsMetadata(metadata);
  return utils_rs::is_valid_ffi(rs_metadata);
}

}  // namespace smpte2094_50
