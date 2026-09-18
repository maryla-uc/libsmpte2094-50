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

#ifndef LIBSMPTE2094_50_SRC_RUST_UTILS_H_
#define LIBSMPTE2094_50_SRC_RUST_UTILS_H_

#include "smpte2094_50/smpte2094_50.h"
#include "tonemap_ffi.h"
#include "utils_ffi.h"

namespace smpte2094_50 {

// Converts public C++ DynamicMetadata to utils_rs::DynamicMetadata.
utils_rs::DynamicMetadata ToUtilsRsMetadata(const DynamicMetadata& cpp);

// Converts utils_rs::DynamicMetadata to public C++ DynamicMetadata.
void FromUtilsRsMetadata(const utils_rs::DynamicMetadata& rust,
                         DynamicMetadata& cpp);

// Converts public C++ DynamicMetadata to tonemap_ffi::FfiDynamicMetadata.
tonemap_ffi::FfiDynamicMetadata ToTonemapFfiMetadata(
    const DynamicMetadata& cpp);

}  // namespace smpte2094_50

#endif  // LIBSMPTE2094_50_SRC_RUST_UTILS_H_
