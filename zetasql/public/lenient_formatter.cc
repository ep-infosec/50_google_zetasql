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

#include "zetasql/public/lenient_formatter.h"

#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "zetasql/public/formatter_options.h"
#include "zetasql/tools/formatter/internal/layout.h"
#include "zetasql/tools/formatter/internal/parsed_file.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "zetasql/base/status_macros.h"

namespace zetasql {

namespace {

absl::StatusOr<std::string> FormatParsedFile(
    const formatter::internal::ParsedFile& parsed_file,
    const FormatterOptions& options) {
  ZETASQL_ASSIGN_OR_RETURN(auto layout, formatter::internal::FileLayout::FromParsedFile(
                                    parsed_file, options));
  layout->BestLayout();
  return layout->PrintableString();
}

}  // namespace

absl::Status LenientFormatSql(absl::string_view sql, std::string* formatted_sql,
                              const FormatterOptions& options) {
  ZETASQL_ASSIGN_OR_RETURN(auto parsed_file,
                   formatter::internal::ParsedFile::ParseFrom(sql, options));
  ZETASQL_ASSIGN_OR_RETURN(*formatted_sql, FormatParsedFile(*parsed_file, options));
  return absl::OkStatus();
}

absl::StatusOr<std::string> LenientFormatSql(absl::string_view sql,
                                             const FormatterOptions& options) {
  ZETASQL_ASSIGN_OR_RETURN(auto parsed_file,
                   formatter::internal::ParsedFile::ParseFrom(sql, options));
  return FormatParsedFile(*parsed_file, options);
}

absl::StatusOr<std::string> LenientFormatSqlByteRanges(
    absl::string_view sql, const std::vector<FormatterRange>& byte_ranges,
    const FormatterOptions& options) {
  ZETASQL_ASSIGN_OR_RETURN(auto parsed_file,
                   formatter::internal::ParsedFile::ParseByteRanges(
                       sql, byte_ranges, options));
  return FormatParsedFile(*parsed_file, options);
}

absl::StatusOr<std::string> LenientFormatSqlLines(
    absl::string_view sql, const std::vector<FormatterRange>& line_ranges,
    const FormatterOptions& options) {
  ZETASQL_ASSIGN_OR_RETURN(auto parsed_file,
                   formatter::internal::ParsedFile::ParseLineRanges(
                       sql, line_ranges, options));
  return FormatParsedFile(*parsed_file, options);
}

}  // namespace zetasql
