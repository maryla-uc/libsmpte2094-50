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

# TODO(vrabaud) switch once there is a new version with the fix.
# set(SMPTE2094_50_GTEST_GIT_TAG v1.17.0)
set(SMPTE2094_50_GTEST_GIT_TAG fa8438ae6b70c57010177de47a9f13d7041a6328)

enable_testing()

macro(add_gtest TEST_NAME)
  add_executable(${TEST_NAME} ${TEST_NAME}.cc)
  target_link_libraries(
    ${TEST_NAME} PRIVATE GTest::gmock GTest::GTest GTest::Main smpte2094_50
                         smpte2094_50_utils)
  add_dependencies(${TEST_NAME} generate_include_links_test)
  target_include_directories(
    ${TEST_NAME}
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/virtual_includes>)
  add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
  target_compile_features(${TEST_NAME} PRIVATE cxx_std_20)
endmacro()

FetchContent_Declare(
  googletest
  EXCLUDE_FROM_ALL
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG ${SMPTE2094_50_GTEST_GIT_TAG})
# TODO(vrabaud) re-enable with a proper release GIT_SHALLOW ON)
FetchContent_MakeAvailable(googletest)

add_library(GTest::GTest ALIAS gtest)
add_library(GTest::Main ALIAS gtest_main)
# No need for gmock as absl imports it.

add_custom_target(
  generate_include_links_test ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory
          "${CMAKE_BINARY_DIR}/virtual_includes"
  COMMAND
    ${CMAKE_COMMAND} -E create_symlink
    "${CMAKE_BINARY_DIR}/_deps/googletest-src/googletest/include/gtest"
    "${CMAKE_BINARY_DIR}/virtual_includes/gtest"
  COMMAND
    ${CMAKE_COMMAND} -E create_symlink
    "${CMAKE_BINARY_DIR}/_deps/googletest-src/googlemock/include/gmock"
    "${CMAKE_BINARY_DIR}/virtual_includes/gmock")

add_gtest(base_test)
add_gtest(pchip_test)
add_gtest(tonemap_test)
add_gtest(utils_test)
