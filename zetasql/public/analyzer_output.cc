//
// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "zetasql/public/analyzer_output.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "zetasql/base/enum_utils.h"
#include "absl/strings/str_format.h"
#include "absl/time/time.h"
#include "zetasql/base/map_util.h"

namespace zetasql {

const AnalyzerRuntimeInfo::RewriterDetails&
AnalyzerRuntimeInfo::rewriters_details(ResolvedASTRewrite rewriter) const {
  return zetasql_base::FindWithDefault(impl_->rewriters_details, rewriter);
}

AnalyzerOutput::AnalyzerOutput(
    std::shared_ptr<IdStringPool> id_string_pool,
    std::shared_ptr<zetasql_base::UnsafeArena> arena,
    std::unique_ptr<const ResolvedStatement> statement,
    const AnalyzerOutputProperties& analyzer_output_properties,
    std::unique_ptr<ParserOutput> parser_output,
    const std::vector<absl::Status>& deprecation_warnings,
    const QueryParametersMap& undeclared_parameters,
    const std::vector<const Type*>& undeclared_positional_parameters,
    int max_column_id, AnalyzerRuntimeInfo runtime_info)
    : id_string_pool_(std::move(id_string_pool)),
      arena_(std::move(arena)),
      statement_(std::move(statement)),
      analyzer_output_properties_(analyzer_output_properties),
      parser_output_(std::move(parser_output)),
      deprecation_warnings_(deprecation_warnings),
      undeclared_parameters_(undeclared_parameters),
      undeclared_positional_parameters_(undeclared_positional_parameters),
      max_column_id_(max_column_id),
      runtime_info_(std::move(runtime_info)) {}

AnalyzerOutput::AnalyzerOutput(
    std::shared_ptr<IdStringPool> id_string_pool,
    std::shared_ptr<zetasql_base::UnsafeArena> arena,
    std::unique_ptr<const ResolvedExpr> expr,
    const AnalyzerOutputProperties& analyzer_output_properties,
    std::unique_ptr<ParserOutput> parser_output,
    const std::vector<absl::Status>& deprecation_warnings,
    const QueryParametersMap& undeclared_parameters,
    const std::vector<const Type*>& undeclared_positional_parameters,
    int max_column_id, AnalyzerRuntimeInfo runtime_info)
    : id_string_pool_(std::move(id_string_pool)),
      arena_(std::move(arena)),
      expr_(std::move(expr)),
      analyzer_output_properties_(analyzer_output_properties),
      parser_output_(std::move(parser_output)),
      deprecation_warnings_(deprecation_warnings),
      undeclared_parameters_(undeclared_parameters),
      undeclared_positional_parameters_(undeclared_positional_parameters),
      max_column_id_(max_column_id),
      runtime_info_(std::move(runtime_info)) {}

AnalyzerOutput::~AnalyzerOutput() {}

AnalyzerRuntimeInfo::AnalyzerRuntimeInfo(const AnalyzerRuntimeInfo& rhs)
    : impl_(new Impl(*rhs.impl_)) {}

AnalyzerRuntimeInfo& AnalyzerRuntimeInfo::operator=(
    const AnalyzerRuntimeInfo& rhs) {
  *impl_ = *rhs.impl_;
  return *this;
}

void AnalyzerRuntimeInfo::RewriterDetails::AccumulateAll(
    const AnalyzerRuntimeInfo::RewriterDetails& rhs) {
  count += rhs.count;
  timed_value.Accumulate(rhs.timed_value);
}

void AnalyzerRuntimeInfo::AccumulateAll(const AnalyzerRuntimeInfo& rhs) {
  impl_->parser_runtime_info.AccumulateAll(rhs.impl_->parser_runtime_info);

  resolver_timed_value().Accumulate(rhs.resolver_elapsed_duration());

  for (ResolvedASTRewrite rewriter :
       zetasql_base::EnumerateEnumValues<ResolvedASTRewrite>()) {
    rewriters_details(rewriter).AccumulateAll(rhs.rewriters_details(rewriter));
  }
  rewriters_timed_value().Accumulate(rhs.rewriters_elapsed_duration());
  validator_timed_value().Accumulate(rhs.validator_elapsed_duration());
}

// Modifies the debug-string to assume this is the accumulation of multiple
// runs, and prints averages instead of
std::string AnalyzerRuntimeInfo::DebugString(
    std::optional<int> opt_total_runs) const {
  int total_runs = opt_total_runs.value_or(1);
  std::vector<ResolvedASTRewrite> rewrites_order;
  for (const auto& [ast_rewriter, details] : impl_->rewriters_details) {
    if (details.count > 0) {
      rewrites_order.push_back(ast_rewriter);
    }
  }
  std::sort(rewrites_order.begin(), rewrites_order.end(),
            [this](ResolvedASTRewrite lhs, ResolvedASTRewrite rhs) {
              return this->rewriters_details(lhs).count <
                     this->rewriters_details(rhs).count;
            });

  absl::Duration analyzer_total = sum_elapsed_duration();
  auto print_latency = [total_runs, analyzer_total](absl::Duration latency) {
    return absl::StrCat(100.0 * absl::FDivDuration(latency, analyzer_total),
                        "% ", absl::ToDoubleMicroseconds(latency), "μs ",
                        absl::ToDoubleMicroseconds(latency) / total_runs, "μs");
  };
  std::string rewriter_str;

  for (ResolvedASTRewrite rewrite : rewrites_order) {
    size_t count_total = rewriters_details(rewrite).count;
    absl::Duration latency = rewriters_details(rewrite).elapsed_duration();
    absl::StrAppendFormat(
        &rewriter_str, "    %20s: %3.1f%% %6.0fμs %3.1fμs %2d %3.1fμs\n",
        absl::string_view(ResolvedASTRewrite_Name(rewrite)),
        100.0 * latency / analyzer_total, absl::ToDoubleMicroseconds(latency),
        absl::ToDoubleMicroseconds(latency) / total_runs, count_total,
        absl::ToDoubleMicroseconds(latency) / count_total);
  }
  return absl::StrFormat(
      R"(Sum Total    : %s
  Parser     : %s
  Resolver   : %s
  Validator  : %s
  Rewriters  : %s
     %s)",
      print_latency(sum_elapsed_duration()),
      print_latency(parser_elapsed_duration()),
      print_latency(resolver_elapsed_duration()),
      print_latency(validator_elapsed_duration()),
      print_latency(rewriters_elapsed_duration()), rewriter_str);
}

}  // namespace zetasql
