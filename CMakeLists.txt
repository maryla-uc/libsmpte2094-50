# Copyright 2026 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.

cmake_minimum_required(VERSION 3.23)

project(libsmpte2094_50 CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SMPTE2094_ENABLE_TESTS "Enable tests." OFF)
option(SMPTE2094_ENABLE_WERROR "Enable -Werror." OFF)

set(RUST_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/rust_lib")

include(FetchContent)

# Install dependencies.
set(SMPTE2094_ABSL_GIT_TAG 20260107.1)
set(SMPTE2094_CORROSION_GIT_TAG v0.6.1)

FetchContent_Declare(
  abseil-cpp
  EXCLUDE_FROM_ALL
  GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
  GIT_TAG ${SMPTE2094_ABSL_GIT_TAG}
  GIT_SHALLOW ON)
FetchContent_MakeAvailable(abseil-cpp)

FetchContent_Declare(
  Corrosion
  EXCLUDE_FROM_ALL
  GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
  GIT_TAG ${SMPTE2094_CORROSION_GIT_TAG}
  GIT_SHALLOW ON)
FetchContent_MakeAvailable(Corrosion)

# Compile the Rust Library and CXX bridges.
corrosion_import_crate(
  MANIFEST_PATH ${CMAKE_CURRENT_SOURCE_DIR}/rust_lib/Cargo.toml FEATURES
  cxxbridge)

corrosion_add_cxxbridge(pchip_rs_bridge CRATE libsmpte2094_50 FILES
                        pchip_ffi.rs)

corrosion_add_cxxbridge(utils_rs_bridge CRATE libsmpte2094_50 FILES
                        utils_ffi.rs)

corrosion_add_cxxbridge(tonemap_rs_bridge CRATE libsmpte2094_50 FILES
                        tonemap_ffi.rs)

add_custom_target(
  generate_include_links ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory
          "${CMAKE_CURRENT_BINARY_DIR}/virtual_includes/libsmpte2094_50"
  COMMAND
    ${CMAKE_COMMAND} -E create_symlink
    "${CMAKE_CURRENT_SOURCE_DIR}/include/smpte2094_50"
    "${CMAKE_CURRENT_BINARY_DIR}/virtual_includes/libsmpte2094_50/smpte2094_50"
  COMMAND ${CMAKE_COMMAND} -E create_symlink "${abseil-cpp_SOURCE_DIR}/absl"
          "${CMAKE_CURRENT_BINARY_DIR}/virtual_includes/absl")

# -----------------------------------------------------------------------------
# Library Definition
# -----------------------------------------------------------------------------
add_library(smpte2094_50 INTERFACE include/smpte2094_50/smpte2094_50.h)
target_include_directories(
  smpte2094_50 INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
                         $<INSTALL_INTERFACE:include>)

add_library(smpte2094_50_utils src/utils.cc include/smpte2094_50/utils.h
                               src/pchip.cc include/smpte2094_50/pchip.h
                               src/tonemap.cc include/smpte2094_50/tonemap.h
                               src/rust_utils.cc src/rust_utils.h
                               src/tonemap_ffi.h)
add_dependencies(smpte2094_50_utils generate_include_links)

target_link_libraries(
  smpte2094_50_utils
  PUBLIC absl::status absl::statusor absl::strings absl::span smpte2094_50
  PRIVATE pchip_rs_bridge utils_rs_bridge tonemap_rs_bridge)
target_include_directories(
  smpte2094_50_utils
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/virtual_includes>
         $<INSTALL_INTERFACE:include>)

if(SMPTE2094_ENABLE_WERROR)
  # Warnings as errors
  if(MSVC)
    target_compile_options(smpte2094_50_utils INTERFACE /WX)
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES
                                                  "GNU")
    target_compile_options(smpte2094_50_utils INTERFACE -Werror)
  else()
    message(FATAL_ERROR "libsmpte2094-50: Unknown compiler, bailing out")
  endif()
endif()

if(SMPTE2094_ENABLE_TESTS)
  enable_testing()
  # Add Rust tests through cargo, using the same toolchain and target as
  # Corrosion.
  add_test(
    NAME rust_tests
    COMMAND
      ${Rust_CARGO_CACHED} test --manifest-path ${RUST_LIB_DIR}/Cargo.toml
      --features cxxbridge --target ${Rust_CARGO_TARGET_CACHED}
      $<$<CONFIG:Release>:--release>)
  set_tests_properties(rust_tests PROPERTIES ENVIRONMENT
                                             "RUSTUP_AUTO_UPDATE=no")
  add_subdirectory(tests)
endif()
