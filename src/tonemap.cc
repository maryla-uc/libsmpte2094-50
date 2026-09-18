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
#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "rust_utils.h"
#include "smpte2094_50/smpte2094_50.h"
#include "tonemap_ffi.h"

namespace smpte2094_50 {

struct ToneMapper::Impl {
  ::rust::Box<tonemap_ffi::ToneMapperWrapper> wrapper;
};

absl::StatusOr<ToneMapper> ToneMapper::Create(const DynamicMetadata& metadata,
                                              float target_headroom_log2) {
  tonemap_ffi::FfiDynamicMetadata rs_metadata = ToTonemapFfiMetadata(metadata);
  auto result =
      tonemap_ffi::tone_mapper_create_ffi(rs_metadata, target_headroom_log2);
  if (!result.success) {
    return absl::InvalidArgumentError(std::string(result.error_message));
  }
  return ToneMapper(std::make_unique<Impl>(Impl{std::move(result.mapper)}));
}

ToneMapper::ToneMapper(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

ToneMapper::ToneMapper(const ToneMapper& other)
    : impl_(other.impl_
                ? std::make_unique<Impl>(Impl{other.impl_->wrapper->clone()})
                : nullptr) {}

ToneMapper& ToneMapper::operator=(const ToneMapper& other) {
  if (this != &other) {
    impl_ = other.impl_
                ? std::make_unique<Impl>(Impl{other.impl_->wrapper->clone()})
                : nullptr;
  }
  return *this;
}

ToneMapper::ToneMapper(ToneMapper&& other) noexcept = default;

ToneMapper& ToneMapper::operator=(ToneMapper&& other) noexcept = default;

ToneMapper::~ToneMapper() = default;

bool ToneMapper::IsIdentity() const { return impl_->wrapper->is_identity(); }

std::array<float, 3> ToneMapper::ToneMapPixel(
    const std::array<float, 3>& rgb) const {
  auto out = impl_->wrapper->tone_map_pixel(rgb[0], rgb[1], rgb[2]);
  return {out[0], out[1], out[2]};
}

absl::Status ToneMapper::ToneMapBuffer(absl::Span<float> buffer) const {
  if (buffer.empty()) {
    return absl::OkStatus();
  }
  ::rust::Slice<float> rust_slice(buffer.data(), buffer.size());
  auto result = impl_->wrapper->tone_map_buffer(rust_slice);
  if (!result.success) {
    return absl::InvalidArgumentError(std::string(result.error_message));
  }
  return absl::OkStatus();
}

}  // namespace smpte2094_50
