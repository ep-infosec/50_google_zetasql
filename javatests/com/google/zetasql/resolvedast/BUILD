#
# Copyright 2019 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

load("//javatests/com/google/zetasql:junit_test_suites.bzl", "junit_test_suites")

# Tests for ZetaSQL resolvedast classes.
package(
    default_testonly = 1,
    default_visibility = ["//zetasql/base:zetasql_implementation"],
)

java_library(
    name = "tests",
    srcs = glob(["*.java"]),
    deps = [
        "//java/com/google/zetasql:analyzer",
        "//java/com/google/zetasql:client",
        "//java/com/google/zetasql:testaccess",
        "//java/com/google/zetasql:types",
        "//java/com/google/zetasql/resolvedast",
        "//javatests/com/google/zetasql:util",
        "//zetasql/proto:function_java_proto",
        "//zetasql/public:function_java_proto",
        "//zetasql/public:options_java_proto",
        "//zetasql/public:type_proto_java_proto",
        "//zetasql/public:value_java_proto",
        "//zetasql/resolved_ast:resolved_ast_enums_java_proto",
        "//zetasql/resolved_ast:serialization_java_proto",
        "@maven//:com_google_guava_guava",
        "@maven//:com_google_truth_truth",
        "@maven//:junit_junit",
    ],
)

junit_test_suites(
    name = "gen_tests",
    runtime_deps = ["//java/com/google/zetasql:jni_channel"],
    deps = [":tests"],
)
