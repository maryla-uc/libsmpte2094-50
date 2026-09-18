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

#ifndef LIBSMPTE2094_50_INCLUDE_SMPTE2094_50_TONEMAP_H_
#define LIBSMPTE2094_50_INCLUDE_SMPTE2094_50_TONEMAP_H_

#include <array>
#include <memory>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "smpte2094_50/smpte2094_50.h"

namespace smpte2094_50 {

// Headroom-adaptive tone mapper, implementing SMPTE ST 2094-50 Section 6.
class ToneMapper {
 public:
  // Creates a ToneMapper from dynamic metadata and target HDR headroom in log2
  // space. The target HDR headroom is the ratio between the display's maximum 
  // light level over SDR white light level.
  static absl::StatusOr<ToneMapper> Create(const DynamicMetadata& metadata,
                                           float target_headroom_log2);

  ToneMapper(const ToneMapper& other);
  ToneMapper& operator=(const ToneMapper& other);
  ToneMapper(ToneMapper&& other) noexcept;
  ToneMapper& operator=(ToneMapper&& other) noexcept;
  ~ToneMapper();

  // Returns true if tone mapping is a no-op identity transform.
  bool IsIdentity() const;

  // Tone maps a single RGB pixel. The RGB values must be in gain application
  // color space, i.e. linear SDR-relative in the gain application
  // chromaticities.
  std::array<float, 3> ToneMapPixel(const std::array<float, 3>& rgb) const;

  // In-place tone mapping of an interleaved float RGB buffer.
  // Buffer size must be a multiple of 3.
  absl::Status ToneMapBuffer(absl::Span<float> buffer) const;

  struct Impl;
  explicit ToneMapper(std::unique_ptr<Impl> impl);

 private:
  std::unique_ptr<Impl> impl_;
};

}  // namespace smpte2094_50

#endif  // LIBSMPTE2094_50_INCLUDE_SMPTE2094_50_TONEMAP_H_
