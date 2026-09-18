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

#ifndef LIBSMPTE2094_50_INCLUDE_SMPTE2094_50_PCHIP_H_
#define LIBSMPTE2094_50_INCLUDE_SMPTE2094_50_PCHIP_H_

#include <memory>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/types/span.h"

namespace smpte2094_50 {

// Computes the PCHIP slopes for a given set of control points.
// Returns an error if x is not sorted or if sizes don't match.
absl::StatusOr<std::vector<float>> ComputePchipSlopes(
    absl::Span<const float> x, absl::Span<const float> y);

// A Piecewise Cubic Hermite Interpolating Polynomial (PCHIP) interpolator.
// If the input points are monotonic and no slopes are provided, this
// interpolator ensures monotonicity.
// For values outside the domain of the control points, it extrapolates by
// clamping to the nearest y-value (i.e. flat extrapolation).
class PchipInterpolator {
 public:
  static absl::StatusOr<PchipInterpolator> Create(absl::Span<const float> x,
                                                  absl::Span<const float> y);
  static absl::StatusOr<PchipInterpolator> Create(absl::Span<const float> x,
                                                  absl::Span<const float> y,
                                                  absl::Span<const float> slopes);

  PchipInterpolator(const PchipInterpolator& other);
  PchipInterpolator& operator=(const PchipInterpolator& other);
  PchipInterpolator(PchipInterpolator&& other) noexcept;
  PchipInterpolator& operator=(PchipInterpolator&& other) noexcept;
  ~PchipInterpolator();

  float Interpolate(float x) const;
  absl::StatusOr<float> ReverseInterpolate(float y) const;

  std::vector<float> x() const;
  std::vector<float> y() const;
  std::vector<float> slopes() const;

  struct Impl;
  explicit PchipInterpolator(std::unique_ptr<Impl> impl);

 private:
  std::unique_ptr<Impl> impl_;
};

// A specialized interpolator for SMPTE ST 2094-50 tone mapping curves.
// Within the domain of the control points, it behaves exactly like a PCHIP
// interpolator. However, for values greater than the maximum x control point,
// it extrapolates using a logarithmic roll-off, resulting in a flat tone 
// mapping curve after the last control point.
class GainCurve {
 public:
  static absl::StatusOr<GainCurve> Create(absl::Span<const float> x,
                                          absl::Span<const float> y);
  static absl::StatusOr<GainCurve> Create(absl::Span<const float> x,
                                          absl::Span<const float> y,
                                          absl::Span<const float> slopes);

  GainCurve(const GainCurve& other);
  GainCurve& operator=(const GainCurve& other);
  GainCurve(GainCurve&& other) noexcept;
  GainCurve& operator=(GainCurve&& other) noexcept;
  ~GainCurve();

  float Interpolate(float x) const;
  absl::StatusOr<float> ReverseInterpolate(float y) const;

  struct Impl;
  explicit GainCurve(std::unique_ptr<Impl> impl);

 private:
  std::unique_ptr<Impl> impl_;
};

// Resamples an existing curve defined by the control points (x, y) down to
// a piecewise cubic interpolator with exactly `num_control_points`, minimizing
// the approximation error. Useful for compressing complex curves.
absl::StatusOr<PchipInterpolator> CreateSubsampledPchip(
    absl::Span<const float> x, absl::Span<const float> y,
    size_t num_control_points);

}  // namespace smpte2094_50

#endif  // LIBSMPTE2094_50_INCLUDE_SMPTE2094_50_PCHIP_H_
