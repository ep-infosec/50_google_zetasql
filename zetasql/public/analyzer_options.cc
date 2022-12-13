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

#include "zetasql/public/analyzer_options.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "zetasql/public/options.pb.h"
#include "zetasql/public/time_zone_util.h"
#include "zetasql/resolved_ast/resolved_ast.h"
#include "zetasql/base/case.h"
#include "absl/flags/flag.h"
#include "absl/status/status.h"

ABSL_FLAG(bool, zetasql_validate_resolved_ast, true,
          "Run validator on resolved AST before returning it.");

namespace zetasql {

bool StringVectorCaseLess::operator()(
    const std::vector<std::string>& v1,
    const std::vector<std::string>& v2) const {
  auto common_length = std::min(v1.size(), v2.size());
  for (int idx = 0; idx < common_length; ++idx) {
    const int cmp = zetasql_base::CaseCompare(v1[idx], v2[idx]);
    if (cmp < 0) return true;
    if (cmp > 0) return false;
    // Otherwise the strings compared equal, so compare the next array
    // elements.
  }
  // The first <common_length> elements are equal, so if the array length
  // of v1 < v2 then v1 is less.
  return (v1.size() < v2.size());
}

void AllowedHintsAndOptions::AddOption(const std::string& name,
                                       const Type* type) {
  ZETASQL_CHECK_OK(AddOptionImpl(options_lower, name, type));
}

void AllowedHintsAndOptions::AddAnonymizationOption(const std::string& name,
                                                    const Type* type) {
  ZETASQL_CHECK_OK(AddOptionImpl(anonymization_options_lower, name, type));
}

absl::Status AllowedHintsAndOptions::AddOptionImpl(
    absl::flat_hash_map<std::string, const Type*>& options_map,
    const std::string& name, const Type* type) {
  if (name.empty()) {
    return MakeSqlError() << "Option name should not be empty.";
  }
  if (!zetasql_base::InsertIfNotPresent(&options_map, absl::AsciiStrToLower(name),
                               type)) {
    return MakeSqlError() << "Duplicate option: " << name;
  }
  return absl::OkStatus();
}

void AllowedHintsAndOptions::AddHint(const std::string& qualifier,
                                     const std::string& name, const Type* type,
                                     bool allow_unqualified) {
  ZETASQL_CHECK_OK(AddHintImpl(qualifier, name, type, allow_unqualified));
}

absl::Status AllowedHintsAndOptions::AddHintImpl(const std::string& qualifier,
                                                 const std::string& name,
                                                 const Type* type,
                                                 bool allow_unqualified) {
  if (name.empty()) {
    return MakeSqlError() << "Hint name should not be empty";
  }
  if (qualifier.empty() && !allow_unqualified) {
    return MakeSqlError()
           << "Cannot have hint with no qualifier and !allow_unqualified";
  }
  if (!zetasql_base::InsertIfNotPresent(&hints_lower,
                               std::make_pair(absl::AsciiStrToLower(qualifier),
                                              absl::AsciiStrToLower(name)),
                               type)) {
    return MakeSqlError() << "Duplicate hint: " << name
                          << ", with qualifier: " << qualifier;
  }
  if (allow_unqualified && !qualifier.empty()) {
    if (!zetasql_base::InsertIfNotPresent(
            &hints_lower, std::make_pair("", absl::AsciiStrToLower(name)),
            type)) {
      return MakeSqlError()
             << "Duplicate hint: " << name << ", with no qualifier";
    }
  }
  return absl::OkStatus();
}

absl::Status AllowedHintsAndOptions::Deserialize(
    const AllowedHintsAndOptionsProto& proto,
    const std::vector<const google::protobuf::DescriptorPool*>& pools,
    TypeFactory* factory, AllowedHintsAndOptions* result) {
  *result = AllowedHintsAndOptions();
  // We need to clear anonymization_options_lower since they are filled in
  // constructor.
  result->anonymization_options_lower.clear();
  for (const auto& qualifier : proto.disallow_unknown_hints_with_qualifier()) {
    if (!zetasql_base::InsertIfNotPresent(
            &(result->disallow_unknown_hints_with_qualifiers), qualifier)) {
      return ::zetasql_base::UnknownErrorBuilder()
             << "Duplicate qualifier:" << qualifier;
    }
  }
  result->disallow_unknown_options = proto.disallow_unknown_options();
  for (const auto& hint : proto.hint()) {
    if (hint.has_type()) {
      const Type* type;
      ZETASQL_RETURN_IF_ERROR(factory->DeserializeFromProtoUsingExistingPools(
          hint.type(), pools, &type));
      ZETASQL_RETURN_IF_ERROR(result->AddHintImpl(hint.qualifier(), hint.name(), type,
                                          hint.allow_unqualified()));
    } else {
      ZETASQL_RETURN_IF_ERROR(result->AddHintImpl(hint.qualifier(), hint.name(),
                                          nullptr, hint.allow_unqualified()));
    }
  }
  auto deserialize_options =
      [&pools, &result, &factory](
          const google::protobuf::RepeatedPtrField<
              ::zetasql::AllowedHintsAndOptionsProto_OptionProto>&
              options_proto,
          absl::flat_hash_map<std::string, const Type*>& options)
      -> absl::Status {
    for (const auto& option : options_proto) {
      if (option.has_type()) {
        const Type* type;
        ZETASQL_RETURN_IF_ERROR(factory->DeserializeFromProtoUsingExistingPools(
            option.type(), pools, &type));
        ZETASQL_RETURN_IF_ERROR(result->AddOptionImpl(options, option.name(), type));
      } else {
        ZETASQL_RETURN_IF_ERROR(result->AddOptionImpl(options, option.name(), nullptr));
      }
    }

    return absl::OkStatus();
  };
  ZETASQL_RETURN_IF_ERROR(deserialize_options(proto.option(), result->options_lower));
  ZETASQL_RETURN_IF_ERROR(deserialize_options(proto.anonymization_option(),
                                      result->anonymization_options_lower));
  return absl::OkStatus();
}

absl::Status AllowedHintsAndOptions::Serialize(
    FileDescriptorSetMap* file_descriptor_set_map,
    AllowedHintsAndOptionsProto* proto) const {
  proto->set_disallow_unknown_options(disallow_unknown_options);
  for (const auto& qualifier : disallow_unknown_hints_with_qualifiers) {
    proto->add_disallow_unknown_hints_with_qualifier(qualifier);
  }

  // Add hints to proto.
  //
  // For hints, the key is (qualifier, hint).  Unqualified hints are declared
  // using an empty qualifier. The same hint is typically added twice, once
  // qualified and once unqualified.
  //
  // To make the proto a minimum one, the unqualified ones which have the
  // same name and type with a qualified one should be omitted
  // by setting <allow_unqualified> field of the qualified one
  // as true in HintProto.
  //
  // To do this, we use <serialized_unqualified_hints_lower> to store those
  // unqualified hints. The detailed logic is as below:
  //
  // for each qualified hint:
  //   if
  //      it has an unqualified version and the unqualified hint has not been
  //      added into <serialized_unqualified_hints_lower>:
  //   then
  //     add the qualified one to <serialized_unqualified_hints_lower>
  //     set <allow_unqualified> true in proto
  //
  //  for each unqualified hint:
  //    if
  //      it is not in <serialized_unqualified_hints_lower>
  //    then
  //      serialize it into proto
  absl::flat_hash_set<std::string> serialized_unqualified_hints_lower;
  for (const auto& hint : hints_lower) {
    if (!hint.first.first.empty()) {
      auto* hint_proto = proto->add_hint();
      hint_proto->set_qualifier(hint.first.first);
      hint_proto->set_name(hint.first.second);
      const std::string name_lower = absl::AsciiStrToLower(hint.first.second);
      const std::pair<std::string, std::string> unqualified_key =
          std::make_pair("", name_lower);
      if (hints_lower.contains(unqualified_key) &&
          !serialized_unqualified_hints_lower.contains(name_lower)) {
        const auto unqualified_hint = hints_lower.find(unqualified_key);
        if ((unqualified_hint->second == nullptr && hint.second == nullptr) ||
            (unqualified_hint->second != nullptr && hint.second != nullptr &&
             unqualified_hint->second->Equals(hint.second))) {
          hint_proto->set_allow_unqualified(true);
          serialized_unqualified_hints_lower.insert(name_lower);
        }
      }
      if (hint.second != nullptr) {
        ZETASQL_RETURN_IF_ERROR(hint.second->SerializeToProtoAndDistinctFileDescriptors(
            hint_proto->mutable_type(), file_descriptor_set_map));
      }
    }
  }
  for (const auto& hint : hints_lower) {
    if (hint.first.first.empty() &&
        !serialized_unqualified_hints_lower.contains(
            absl::AsciiStrToLower(hint.first.second))) {
      auto* hint_proto = proto->add_hint();
      hint_proto->set_name(hint.first.second);
      hint_proto->set_allow_unqualified(true);
      if (hint.second != nullptr) {
        ZETASQL_RETURN_IF_ERROR(hint.second->SerializeToProtoAndDistinctFileDescriptors(
            hint_proto->mutable_type(), file_descriptor_set_map));
      }
    }
  }
  auto serialize_options =
      [file_descriptor_set_map](
          const absl::flat_hash_map<std::string, const Type*>& options,
          google::protobuf::RepeatedPtrField<
              ::zetasql::AllowedHintsAndOptionsProto_OptionProto>&
              options_proto) -> absl::Status {
    for (const auto& option : options) {
      auto* option_proto = options_proto.Add();
      option_proto->set_name(option.first);
      if (option.second != nullptr) {
        ZETASQL_RETURN_IF_ERROR(
            option.second->SerializeToProtoAndDistinctFileDescriptors(
                option_proto->mutable_type(), file_descriptor_set_map));
      }
    }
    return absl::OkStatus();
  };
  ZETASQL_RETURN_IF_ERROR(serialize_options(options_lower, *proto->mutable_option()));
  ZETASQL_RETURN_IF_ERROR(serialize_options(anonymization_options_lower,
                                    *proto->mutable_anonymization_option()));
  return absl::OkStatus();
}

AnalyzerOptions::AnalyzerOptions() : AnalyzerOptions(LanguageOptions()) {}

AnalyzerOptions::AnalyzerOptions(const LanguageOptions& language_options)
    : data_(new Data{.language_options = language_options,
                     .validate_resolved_ast = absl::GetFlag(
                         FLAGS_zetasql_validate_resolved_ast)}) {
  ZETASQL_CHECK_OK(FindTimeZoneByName("America/Los_Angeles",  // Crash OK
                              &data_->default_timezone));
}

AnalyzerOptions::~AnalyzerOptions() = default;

void AnalyzerOptions::CreateDefaultArenasIfNotSet() {
  if (data_->arena == nullptr) {
    data_->arena = std::make_shared<zetasql_base::UnsafeArena>(/*block_size=*/4096);
  }
  if (data_->id_string_pool == nullptr) {
    data_->id_string_pool = std::make_shared<IdStringPool>(data_->arena);
  }
}

absl::Status AnalyzerOptions::Deserialize(
    const AnalyzerOptionsProto& proto,
    const std::vector<const google::protobuf::DescriptorPool*>& pools,
    TypeFactory* factory, AnalyzerOptions* result) {
  *result = AnalyzerOptions();
  result->set_language(LanguageOptions(proto.language_options()));

  for (const auto& param : proto.query_parameters()) {
    const Type* type;
    ZETASQL_RETURN_IF_ERROR(factory->DeserializeFromProtoUsingExistingPools(
        param.type(), pools, &type));
    ZETASQL_RETURN_IF_ERROR(result->AddQueryParameter(param.name(), type));
  }

  for (const auto& system_variable_proto : proto.system_variables()) {
    std::vector<std::string> name_path;
    for (const auto& path_part : system_variable_proto.name_path()) {
      name_path.push_back(path_part);
    }

    const Type* type;
    ZETASQL_RETURN_IF_ERROR(factory->DeserializeFromProtoUsingExistingPools(
        system_variable_proto.type(), pools, &type));

    ZETASQL_RETURN_IF_ERROR(result->AddSystemVariable(name_path, type));
  }

  for (const TypeProto& param_type : proto.positional_query_parameters()) {
    const Type* type;
    ZETASQL_RETURN_IF_ERROR(factory->DeserializeFromProtoUsingExistingPools(
        param_type, pools, &type));
    ZETASQL_RETURN_IF_ERROR(result->AddPositionalQueryParameter(type));
  }

  for (const auto& column : proto.expression_columns()) {
    const Type* type;
    ZETASQL_RETURN_IF_ERROR(factory->DeserializeFromProtoUsingExistingPools(
        column.type(), pools, &type));
    ZETASQL_RETURN_IF_ERROR(result->AddExpressionColumn(column.name(), type));
  }

  if (proto.has_in_scope_expression_column()) {
    const Type* type;
    ZETASQL_RETURN_IF_ERROR(factory->DeserializeFromProtoUsingExistingPools(
        proto.in_scope_expression_column().type(), pools, &type));
    ZETASQL_RETURN_IF_ERROR(result->SetInScopeExpressionColumn(
        proto.in_scope_expression_column().name(), type));
  }

  std::vector<std::pair<std::string, const Type*>> ddl_pseudo_columns;
  for (const auto& column : proto.ddl_pseudo_columns()) {
    const Type* type;
    ZETASQL_RETURN_IF_ERROR(factory->DeserializeFromProtoUsingExistingPools(
        column.type(), pools, &type));
    ddl_pseudo_columns.push_back(std::make_pair(column.name(), type));
  }
  result->SetDdlPseudoColumns(ddl_pseudo_columns);

  if (proto.has_default_timezone()) {
    ZETASQL_RETURN_IF_ERROR(FindTimeZoneByName(proto.default_timezone(),
                                       &result->data_->default_timezone));
  }

  if (proto.has_default_anon_function_report_format()) {
    result->set_default_anon_function_report_format(
        proto.default_anon_function_report_format());
  }

  if (proto.has_default_anon_kappa_value()) {
    ZETASQL_RETURN_IF_ERROR(
        result->set_default_anon_kappa_value(proto.default_anon_kappa_value()));
  }

  std::vector<const Type*> expected_types;
  for (const TypeProto& type_proto : proto.target_column_types()) {
    const Type* type;
    ZETASQL_RETURN_IF_ERROR(factory->DeserializeFromProtoUsingExistingPools(
        type_proto, pools, &type));
    expected_types.push_back(type);
  }
  result->set_target_column_types(expected_types);

  result->set_statement_context(proto.statement_context());
  result->set_error_message_mode(proto.error_message_mode());
  result->set_create_new_column_for_each_projected_output(
      proto.create_new_column_for_each_projected_output());
  result->set_prune_unused_columns(proto.prune_unused_columns());
  result->set_allow_undeclared_parameters(proto.allow_undeclared_parameters());
  result->set_parameter_mode(proto.parameter_mode());
  result->set_preserve_column_aliases(proto.preserve_column_aliases());
  result->set_preserve_unnecessary_cast(proto.preserve_unnecessary_cast());

  if (proto.has_allowed_hints_and_options()) {
    AllowedHintsAndOptions hints_and_options("");
    ZETASQL_RETURN_IF_ERROR(AllowedHintsAndOptions::Deserialize(
        proto.allowed_hints_and_options(), pools, factory, &hints_and_options));
    result->set_allowed_hints_and_options(hints_and_options);
  }

  result->data_->enabled_rewrites.clear();
  for (int rewrite : proto.enabled_rewrites()) {
    result->data_->enabled_rewrites.insert(
        static_cast<ResolvedASTRewrite>(rewrite));
  }

  if (proto.has_parse_location_record_type()) {
    result->data_->parse_location_record_type =
        proto.parse_location_record_type();
  }
  return absl::OkStatus();
}

absl::Status AnalyzerOptions::Serialize(FileDescriptorSetMap* map,
                                        AnalyzerOptionsProto* proto) const {
  data_->language_options.Serialize(proto->mutable_language_options());

  for (const auto& param : data_->query_parameters) {
    auto* param_proto = proto->add_query_parameters();
    param_proto->set_name(param.first);
    ZETASQL_RETURN_IF_ERROR(param.second->SerializeToProtoAndDistinctFileDescriptors(
        param_proto->mutable_type(), map));
  }

  for (const auto& system_variable : data_->system_variables) {
    auto* system_variable_proto = proto->add_system_variables();
    for (const std::string& path_part : system_variable.first) {
      system_variable_proto->add_name_path(path_part);
    }
    ZETASQL_RETURN_IF_ERROR(
        system_variable.second->SerializeToProtoAndDistinctFileDescriptors(
            system_variable_proto->mutable_type(), map));
  }

  for (const Type* param_type : data_->positional_query_parameters) {
    ZETASQL_RETURN_IF_ERROR(param_type->SerializeToProtoAndDistinctFileDescriptors(
        proto->add_positional_query_parameters(), map));
  }

  for (const auto& column : data_->expression_columns) {
    auto* column_proto = proto->add_expression_columns();
    column_proto->set_name(column.first);
    ZETASQL_RETURN_IF_ERROR(column.second->SerializeToProtoAndDistinctFileDescriptors(
        column_proto->mutable_type(), map));
  }

  if (!data_->in_scope_expression_column.first.empty()) {
    auto* in_scope_expression = proto->mutable_in_scope_expression_column();
    in_scope_expression->set_name(data_->in_scope_expression_column.first);
    const Type* type = data_->in_scope_expression_column.second;
    ZETASQL_RETURN_IF_ERROR(type->SerializeToProtoAndDistinctFileDescriptors(
        in_scope_expression->mutable_type(), map));
  }

  for (const auto& ddl_pseudo_column : data_->ddl_pseudo_columns) {
    auto* column_proto = proto->add_ddl_pseudo_columns();
    column_proto->set_name(ddl_pseudo_column.first);
    ZETASQL_RETURN_IF_ERROR(
        ddl_pseudo_column.second->SerializeToProtoAndDistinctFileDescriptors(
            column_proto->mutable_type(), map));
  }

  proto->set_default_timezone(data_->default_timezone.name());
  proto->set_default_anon_function_report_format(
      data_->default_anon_function_report_format);
  proto->set_default_anon_kappa_value(data_->default_anon_kappa_value);
  proto->set_statement_context(data_->statement_context);
  proto->set_error_message_mode(data_->error_message_mode);
  proto->set_create_new_column_for_each_projected_output(
      data_->create_new_column_for_each_projected_output);
  proto->set_prune_unused_columns(data_->prune_unused_columns);
  proto->set_allow_undeclared_parameters(data_->allow_undeclared_parameters);
  proto->set_parameter_mode(data_->parameter_mode);
  proto->set_preserve_column_aliases(data_->preserve_column_aliases);
  proto->set_preserve_unnecessary_cast(data_->preserve_unnecessary_cast);

  ZETASQL_RETURN_IF_ERROR(data_->allowed_hints_and_options.Serialize(
      map, proto->mutable_allowed_hints_and_options()));

  if (data_->parse_location_record_type != PARSE_LOCATION_RECORD_NONE) {
    proto->set_parse_location_record_type(data_->parse_location_record_type);
  }

  for (const Type* type : data_->target_column_types) {
    ZETASQL_RETURN_IF_ERROR(type->SerializeToProtoAndDistinctFileDescriptors(
        proto->add_target_column_types(), map));
  }

  for (ResolvedASTRewrite rewrite : data_->enabled_rewrites) {
    proto->add_enabled_rewrites(rewrite);
  }
  return absl::OkStatus();
}

absl::Status AnalyzerOptions::AddSystemVariable(
    const std::vector<std::string>& name_path, const Type* type) {
  if (type == nullptr) {
    return MakeSqlError()
           << "Type associated with system variable cannot be NULL";
  }
  if (name_path.empty()) {
    return MakeSqlError() << "System variable cannot have empty name path";
  }
  for (const std::string& name_path_part : name_path) {
    if (name_path_part.empty()) {
      return MakeSqlError()
             << "System variable cannot have empty string as path part";
    }
  }

  if (!type->IsSupportedType(language())) {
    return MakeSqlError() << "System variable " << absl::StrJoin(name_path, ".")
                          << " has unsupported type: "
                          << type->TypeName(language().product_mode());
  }

  if (!zetasql_base::InsertIfNotPresent(&data_->system_variables,
                               std::make_pair(name_path, type))) {
    return MakeSqlError() << "Duplicate system variable "
                          << absl::StrJoin(name_path, ".");
  }

  return absl::OkStatus();
}

absl::Status AnalyzerOptions::AddQueryParameter(const std::string& name,
                                                const Type* type) {
  if (type == nullptr) {
    return MakeSqlError()
           << "Type associated with query parameter cannot be NULL";
  }
  if (name.empty()) {
    return MakeSqlError() << "Query parameter cannot have empty name";
  }

  if (!type->IsSupportedType(language())) {
    return MakeSqlError() << "Parameter " << name << " has unsupported type: "
                          << type->TypeName(language().product_mode());
  }

  if (!zetasql_base::InsertIfNotPresent(
          &data_->query_parameters,
          std::make_pair(absl::AsciiStrToLower(name), type))) {
    return MakeSqlError() << "Duplicate parameter name "
                          << absl::AsciiStrToLower(name);
  }

  return absl::OkStatus();
}

absl::Status AnalyzerOptions::AddPositionalQueryParameter(const Type* type) {
  if (type == nullptr) {
    return MakeSqlError()
           << "Type associated with query parameter cannot be NULL";
  }

  if (data_->allow_undeclared_parameters) {
    return MakeSqlError()
           << "Positional query parameters cannot be provided when "
              "undeclared parameters are allowed";
  }

  if (!type->IsSupportedType(language())) {
    return MakeSqlError() << "Parameter at position "
                          << data_->positional_query_parameters.size()
                          << " has unsupported type: "
                          << type->TypeName(language().product_mode());
  }

  data_->positional_query_parameters.push_back(type);

  return absl::OkStatus();
}

absl::Status AnalyzerOptions::AddExpressionColumn(const std::string& name,
                                                  const Type* type) {
  if (type == nullptr) {
    return MakeSqlError()
           << "Type associated with expression column cannot be NULL";
  }
  if (name.empty()) {
    return MakeSqlError() << "Expression column cannot have empty name";
  }

  if (!type->IsSupportedType(language())) {
    return MakeSqlError() << "Parameter " << name << " has unsupported type: "
                          << type->TypeName(language().product_mode());
  }

  if (!zetasql_base::InsertIfNotPresent(
          &data_->expression_columns,
          std::make_pair(absl::AsciiStrToLower(name), type))) {
    return MakeSqlError() << "Duplicate expression column name "
                          << absl::AsciiStrToLower(name);
  }

  return absl::OkStatus();
}

absl::Status AnalyzerOptions::SetInScopeExpressionColumn(
    const std::string& name, const Type* type) {
  if (type == nullptr) {
    return MakeSqlError()
           << "Type associated with in-scope expression column cannot be NULL";
  }
  if (has_in_scope_expression_column()) {
    return MakeSqlError() << "Cannot call SetInScopeExpressionColumn twice";
  }

  if (!type->IsSupportedType(language())) {
    return MakeSqlError() << "Parameter " << name << " has unsupported type: "
                          << type->TypeName(language().product_mode());
  }

  const std::pair<std::string, const Type*> name_and_type(
      absl::AsciiStrToLower(name), type);
  if (!zetasql_base::InsertIfNotPresent(&data_->expression_columns, name_and_type)) {
    return MakeSqlError() << "Duplicate expression column name "
                          << absl::AsciiStrToLower(name);
  }
  data_->in_scope_expression_column = name_and_type;

  return absl::OkStatus();
}

void AnalyzerOptions::SetLookupExpressionColumnCallback(
    const LookupExpressionColumnCallback& lookup_expression_column_callback) {
  data_->lookup_expression_column_callback = lookup_expression_column_callback;
  data_->lookup_expression_callback =
      [callback = std::move(lookup_expression_column_callback)](
          const std::string& column_name,
          std::unique_ptr<const ResolvedExpr>& expr) -> absl::Status {
    const Type* column_type = nullptr;
    ZETASQL_RETURN_IF_ERROR(callback(column_name, &column_type));
    if (column_type != nullptr) {
      expr = MakeResolvedExpressionColumn(column_type, column_name);
    }
    return absl::OkStatus();
  };
}

void AnalyzerOptions::SetDdlPseudoColumnsCallback(
    DdlPseudoColumnsCallback ddl_pseudo_columns_callback) {
  data_->ddl_pseudo_columns_callback = std::move(ddl_pseudo_columns_callback);
  data_->ddl_pseudo_columns.clear();
}
void AnalyzerOptions::SetDdlPseudoColumns(
    const std::vector<std::pair<std::string, const Type*>>&
        ddl_pseudo_columns) {
  data_->ddl_pseudo_columns = ddl_pseudo_columns;
  // We explicitly make the lambda capture a copy of ddl_pseudo_columns to be
  // safe. If we capture by reference instead ("this" or "&"), then when a copy
  // of AnalyzerOptions is made, the copied lambda would still point to
  // references of the old AnalyzerOptions that may not exist anymore leading to
  // memory errors.
  data_->ddl_pseudo_columns_callback =
      [ddl_pseudo_columns](
          const std::vector<std::string>& table_name,
          const std::vector<const ResolvedOption*>& options,
          std::vector<std::pair<std::string, const Type*>>* pseudo_columns) {
        *pseudo_columns = ddl_pseudo_columns;
        return absl::OkStatus();
      };
}

ParserOptions AnalyzerOptions::GetParserOptions() const {
  return ParserOptions(id_string_pool(), arena(), &data_->language_options);
}

void AnalyzerOptions::enable_rewrite(ResolvedASTRewrite rewrite, bool enable) {
  if (enable) {
    data_->enabled_rewrites.insert(rewrite);
  } else {
    data_->enabled_rewrites.erase(rewrite);
  }
}

absl::Status AnalyzerOptions::set_default_anon_kappa_value(int64_t value) {
  // 0 is the default value means it has not been set. Otherwise, we check
  // the valid range here.
  if (value < 0 || value > std::numeric_limits<int32_t>::max()) {
    return MakeSqlError()
           << "The default anonymization option kappa must be between 0 and "
           << std::numeric_limits<int32_t>::max() << " where 0 means unset";
  }
  data_->default_anon_kappa_value = value;
  return absl::OkStatus();
}

absl::btree_set<ResolvedASTRewrite> AnalyzerOptions::DefaultRewrites() {
  absl::btree_set<ResolvedASTRewrite> default_rewrites;
  const google::protobuf::EnumDescriptor* descriptor =
      google::protobuf::GetEnumDescriptor<ResolvedASTRewrite>();
  for (int i = 0; i < descriptor->value_count(); ++i) {
    const google::protobuf::EnumValueDescriptor* value_descriptor = descriptor->value(i);
    const ResolvedASTRewrite rewrite =
        static_cast<ResolvedASTRewrite>(value_descriptor->number());
    if (value_descriptor->options()
            .GetExtension(rewrite_options)
            .default_enabled()) {
      default_rewrites.insert(rewrite);
    }
  }
  return default_rewrites;
}

const AnalyzerOptions& GetOptionsWithArenas(
    const AnalyzerOptions* options, std::unique_ptr<AnalyzerOptions>* copy) {
  if (options->AllArenasAreInitialized()) {
    return *options;
  }
  *copy = std::make_unique<AnalyzerOptions>(*options);
  (*copy)->CreateDefaultArenasIfNotSet();
  return **copy;
}

absl::Status ValidateAnalyzerOptions(const AnalyzerOptions& options) {
  switch (options.parameter_mode()) {
    case PARAMETER_NAMED:
      ZETASQL_RET_CHECK(options.positional_query_parameters().empty())
          << "Positional parameters cannot be provided in named parameter "
             "mode";
      break;
    case PARAMETER_POSITIONAL:
      ZETASQL_RET_CHECK(options.query_parameters().empty())
          << "Named parameters cannot be provided in positional parameter "
             "mode";
      // AddPositionalQueryParameter guards against the case where
      // allow_undeclared_parameters is true and a positional parameter is
      // added, but not the reverse order.
      ZETASQL_RET_CHECK(!options.allow_undeclared_parameters() ||
                options.positional_query_parameters().empty())
          << "When undeclared parameters are allowed, no positional query "
             "parameters can be provided";
      break;
    case PARAMETER_NONE:
      ZETASQL_RET_CHECK(options.query_parameters().empty() &&
                options.positional_query_parameters().empty())
          << "Parameters are disabled and cannot be provided";
      break;
  }
  if (options.language().LanguageFeatureEnabled(
          FEATURE_V_1_3_COLLATION_SUPPORT)) {
    ZETASQL_RET_CHECK(options.language().LanguageFeatureEnabled(
        FEATURE_V_1_3_ANNOTATION_FRAMEWORK))
        << "Invalid analyzer configuration. The COLLATION_SUPPORT language "
           "feature requires the ANNOTATION_FRAMEWORK language feature is also "
           "enabled.";
  }

  return absl::OkStatus();
}

}  // namespace zetasql
