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

#ifndef ZETASQL_PUBLIC_ANALYZER_OPTIONS_H_
#define ZETASQL_PUBLIC_ANALYZER_OPTIONS_H_

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "zetasql/base/atomic_sequence_num.h"
#include "zetasql/parser/parser.h"
#include "zetasql/public/catalog.h"
#include "zetasql/public/id_string.h"
#include "zetasql/public/type.h"
#include "zetasql/public/types/annotation.h"
#include "zetasql/resolved_ast/resolved_ast.h"
#include "zetasql/base/case.h"
#include "absl/base/attributes.h"
#include "absl/container/btree_set.h"

namespace zetasql {

class ResolvedOption;
class AnalyzerOutput;

// Performs a case-insensitive less-than vector<string> comparison, element
// by element, using the C/POSIX locale for element comparisons. This function
// object is useful as a template parameter for STL set/map of
// std::vector<string>s, if uniqueness of the vector keys is case-insensitive.
struct StringVectorCaseLess {
  bool operator()(const std::vector<std::string>& v1,
                  const std::vector<std::string>& v2) const;
};

// Map associating each query parameter with its type. Keys are lowercase to
// achieve case-insensitive matching.
typedef std::map<std::string, const Type*> QueryParametersMap;

// Key = name path of system variable.  Value = type of variable.
// Name elements in the key do not include the "@@" prefix.
// For example, if @@foo.bar has type INT32, the corresponding map entry is:
//    key = {"foo", "bar"}
//    value = type_factory->get_int32()
typedef std::map<std::vector<std::string>, const Type*, StringVectorCaseLess>
    SystemVariablesMap;

// This class specifies a set of allowed hints and options, and their expected
// types.
//
// Each hint or option has an expected Type, which can be NULL.  If the
// expected type is NULL, then any type is allowed.
// If a type is specified, the resolved value for the hint will always have
// the expected type, and the analyzer will give an error if coercion is
// not possible.
//
// Hint, option and qualifier names are all case insensitive.
// The resolved AST will contain the original case as written by the user.
//
// The <disallow_unknown_options> and <disallow_unknown_hints_with_qualifiers>
// fields can be set to indicate that errors should be given on unknown
// options or hints (with specific qualifiers).  Unknown hints with other
// qualifiers do not cause errors.
struct AllowedHintsAndOptions {
  AllowedHintsAndOptions() {}
  AllowedHintsAndOptions(const AllowedHintsAndOptions&) = default;
  AllowedHintsAndOptions& operator=(const AllowedHintsAndOptions&) = default;

  // This is recommended constructor to use for normal settings.
  // All supported hints and options should be added with the Add methods.
  // Unknown options will be errors.
  // Unknown hints without qualifiers, or with <qualifier>, will be errors.
  // Unkonwn hints with other qualifiers will be allowed (because these are
  // typically interpreted as hints intended for other engines).
  explicit AllowedHintsAndOptions(const std::string& qualifier) {
    disallow_unknown_options = true;
    disallow_unknown_hints_with_qualifiers.insert("");
    disallow_unknown_hints_with_qualifiers.insert(qualifier);
  }

  // Add an option.  <type> may be NULL to indicate that all Types are allowed.
  void AddOption(const std::string& name, const Type* type);

  // Add an anonymization option.  <type> may be NULL to indicate that all Types
  // are allowed.
  void AddAnonymizationOption(const std::string& name, const Type* type);

  // Add a hint.
  // <qualifier> may be empty to add this hint only unqualified, but hints
  //    for some engine should normally allow the engine name as a qualifier.
  // <type> may be NULL to indicate that all Types are allowed.
  // If <allow_unqualified> is true, this hint is allowed both unqualified
  //   and qualified with <qualifier>.
  void AddHint(const std::string& qualifier, const std::string& name,
               const Type* type, bool allow_unqualified = true);

  // Deserialize AllowedHintsAndOptions from proto. Types will be deserialized
  // using the given TypeFactory and Descriptors from the given DescriptorPools.
  // The TypeFactory and the DescriptorPools must both outlive the result
  // AllowedHintsAndOptions.
  static absl::Status Deserialize(
      const AllowedHintsAndOptionsProto& proto,
      const std::vector<const google::protobuf::DescriptorPool*>& pools,
      TypeFactory* factory, AllowedHintsAndOptions* result);

  // Serialize this AllowedHIntsAndOptions into protobuf. The provided map
  // is used to store serialized FileDescriptorSets, which can be deserialized
  // into separate DescriptorPools in order to reconstruct the Type. The map
  // may be non-empty and may be used across calls to this method in order to
  // serialize multiple types. The map may NOT be null.
  absl::Status Serialize(FileDescriptorSetMap* file_descriptor_set_map,
                         AllowedHintsAndOptionsProto* proto) const;

  // If true, give an error for an unknown option.
  bool disallow_unknown_options = false;

  // For each qualifier in this set, give errors for unknown hints with that
  // qualifier.  If "" is in the set, give errors for unknown unqualified hints.
  std::set<std::string, zetasql_base::CaseLess>
      disallow_unknown_hints_with_qualifiers;

  // Maps containing declared hints and options, keyed on lower case strings.
  //
  // For hints, the key is (qualifier, hint).  Unqualified hints are declared
  // using an empty qualifier. The same hint is typically added twice, once
  // qualified and once unqualified.
  //
  // If the map value (a Type*) is nullptr, that declares that the hint exists
  // but that it does not have an enforced Type, so any Type is allowed.
  // (We could allow callbacks or some other mechanism to specify more rules
  // about what types or values are allowed.)
  absl::flat_hash_map<std::pair<std::string, std::string>, const Type*>
      hints_lower;
  absl::flat_hash_map<std::string, const Type*> options_lower;
  absl::flat_hash_map<std::string, const Type*> anonymization_options_lower = {
      {"delta", types::DoubleType()},
      {"epsilon", types::DoubleType()},
      {"k_threshold", types::Int64Type()},
      {"kappa", types::Int64Type()}};

 private:
  absl::Status AddHintImpl(const std::string& qualifier,
                           const std::string& name, const Type* type,
                           bool allow_unqualified = true);
  absl::Status AddOptionImpl(
      absl::flat_hash_map<std::string, const Type*>& options_map,
      const std::string& name, const Type* type);
};

// AnalyzerOptions contains options that affect analyzer behavior. The language
// options that control the language accepted are accessible via the
// language() member.
class AnalyzerOptions {
 public:
  // Represents a set of ASTRewrites.
  using ASTRewriteSet = absl::btree_set<ResolvedASTRewrite>;

  using LookupExpressionColumnCallback =
      std::function<absl::Status(const std::string&, const Type**)>;

  typedef std::function<absl::Status(const std::string&,
                                     std::unique_ptr<const ResolvedExpr>&)>
      LookupExpressionCallback;

  // Callback function runs after the initial resolve, before any rewriters run.
  // AnalyzerOutput from analyzer is passed in to this callback and
  // then to rewriters if any.
  // Note that if the callback returns an error, the analyzer
  // will return that same error.
  using PreRewriteCallback = std::function<absl::Status(const AnalyzerOutput&)>;

  // Callback to retrieve pseudo-columns for the target of a DDL statement.
  // <options> is the contents of the OPTIONS clause attached to the statement,
  // if any. The callback populates <pseudo_columns> with the names and types of
  // the pseudo-columns for the table. The names must be valid identifiers and
  // must be distinct.
  using DdlPseudoColumnsCallback = std::function<absl::Status(
      const std::vector<std::string>& table_name,
      const std::vector<const ResolvedOption*>& options,
      std::vector<std::pair<std::string, const Type*>>* pseudo_columns)>;

  AnalyzerOptions();
  explicit AnalyzerOptions(const LanguageOptions& language_options);
  AnalyzerOptions(const AnalyzerOptions& options)
      : data_(std::make_unique<Data>(*options.data_)) {}
  AnalyzerOptions(AnalyzerOptions&& options)
      : data_(std::move(options.data_)) {}
  AnalyzerOptions& operator=(const AnalyzerOptions& options) {
    data_ = std::make_unique<Data>(*options.data_);
    return *this;
  }
  AnalyzerOptions& operator=(AnalyzerOptions&& options) {
    data_ = std::move(options).data_;
    return *this;
  }
  ~AnalyzerOptions();

  // Deserialize AnalyzerOptions from proto. Types will be deserialized using
  // the given TypeFactory and Descriptors from the given DescriptorPools.
  // The TypeFactory and the DescriptorPools must both outlive the result
  // AnalyzerOptions.
  static absl::Status Deserialize(
      const AnalyzerOptionsProto& proto,
      const std::vector<const google::protobuf::DescriptorPool*>& pools,
      TypeFactory* factory, AnalyzerOptions* result);

  // Serialize the options into protobuf. The provided map is used to store
  // serialized FileDescriptorSets, which can be deserialized into separate
  // DescriptorPools in order to reconstruct the Type. The map may be
  // non-empty and may be used across calls to this method in order to
  // serialize multiple types. The map may NOT be null.
  absl::Status Serialize(FileDescriptorSetMap* map,
                         AnalyzerOptionsProto* proto) const;

  // Options for the language.
  const LanguageOptions& language() const { return data_->language_options; }
  LanguageOptions* mutable_language() { return &data_->language_options; }
  void set_language(const LanguageOptions& options) {
    data_->language_options = options;
  }

  // Allows updating the set of enabled AST rewrites.
  // By default rewrites in DefaultResolvedASTRewrites() are enabled.
  // These are documented with the ResolvedASTRewrite enum.
  void set_enabled_rewrites(absl::btree_set<ResolvedASTRewrite> rewrites) {
    data_->enabled_rewrites = std::move(rewrites);
  }
  const absl::btree_set<ResolvedASTRewrite>& enabled_rewrites() const {
    return data_->enabled_rewrites;
  }
  // Enables or disables a particular rewrite.
  void enable_rewrite(ResolvedASTRewrite rewrite, bool enable = true);
  // Returns if a given AST rewrite is enabled.
  ABSL_MUST_USE_RESULT bool rewrite_enabled(ResolvedASTRewrite rewrite) const {
    return data_->enabled_rewrites.contains(rewrite);
  }
  // Returns the set of rewrites that are enabled by default.
  static absl::btree_set<ResolvedASTRewrite> DefaultRewrites();

  // Options for Find*() name lookups into the Catalog.
  const Catalog::FindOptions& find_options() const {
    return data_->find_options;
  }
  Catalog::FindOptions* mutable_find_options() { return &data_->find_options; }
  void set_find_options(const Catalog::FindOptions& options) {
    data_->find_options = options;
  }

  // Adds a named query parameter.
  // Parameter name lookups are case insensitive. Paramater names in the output
  // ResolvedParameter nodes will always be in lowercase.
  //
  // ZetaSQL only uses the parameter Type and not the Value. Query analysis is
  // not dependent on the value, and query engines may substitute a value after
  // analysis.
  //
  // For example, for the query
  //   SELECT * FROM table WHERE CustomerId = @customer_id
  // the parameter can be added using
  //   analyzer_options.AddQueryParameter("customer_id", types::Int64Type());
  //
  // Note that an error will be produced if type is not supported according to
  // the current language options.
  absl::Status AddQueryParameter(const std::string& name, const Type* type);

  const QueryParametersMap& query_parameters() const {
    return data_->query_parameters;
  }

  // Clears <query_parameters_>.
  void clear_query_parameters() { data_->query_parameters.clear(); }

  // Adds a positional query parameter.
  //
  // ZetaSQL only uses the parameter Type and not the Value. Query analysis is
  // not dependent on the value, and query engines may substitute a value after
  // analysis.
  //
  // For example, for the query
  //   SELECT * FROM table WHERE CustomerId = ?
  // the parameter can be added using
  //   analyzer_options.AddPositionalQueryParameter(types::Int64Type());
  //
  // Note that an error will be produced if type is not supported according to
  // the current language options. At least as many positional parameters must
  // be provided as there are ? in the query. When allow_undeclared_parameters
  // is true, no positional parameters may be provided.
  absl::Status AddPositionalQueryParameter(const Type* type);

  // Defined positional parameters. Only used in positional parameter mode.
  // Index 0 corresponds with the query parameter at position 1 and so on.
  const std::vector<const Type*>& positional_query_parameters() const {
    return data_->positional_query_parameters;
  }

  // Clears <positional_query_parameters_>.
  void clear_positional_query_parameters() {
    data_->positional_query_parameters.clear();
  }

  // Add columns that are visible when resolving standalone expressions.
  // These are used only in AnalyzeExpression, and have no effect on other
  // analyzer entrypoints.
  //
  // AddExpressionColumn is used to add one or more columns resolvable by name.
  //
  // SetInScopeExpressionColumn is used to add at most one expression column
  // that can be resolved by name (if <name> is non-empty), and is also
  // implicitly in scope so that fields on the value can be used directly,
  // without qualifiers.
  // Expression column names take precedence over in-scope field names.
  //
  // SetLookupExpressionColumnCallback is used to add a callback function to
  // resolve expression columns. The columns referenced in the expressions but
  // not added in the above functions will be resolved using the callback
  // function. The column name passed in the callback function is always in the
  // lower case.
  //
  // Column name lookups are case insensitive.  Columns names in the output
  // ResolvedExpressionColumn nodes will always be in lowercase.
  //
  // For example, to support the expression
  //   enabled = true AND cost > 0.0
  // those columns can be added using
  //   analyzer_options.AddExpressionColumn("enabled", types::BoolType());
  //   analyzer_options.AddExpressionColumn("cost", types::DoubleType());
  //
  // To evaluate an expression in the scope of a particular proto, like
  //   has_cost AND cost > 0 AND value.cost != 10
  // we can add that proto type as an in-scope expression column.
  //   TypeFactory type_factory;
  //   const ProtoType* proto_type;
  //   ZETASQL_CHECK_OK(type_factory.MakeProtoType(MyProto::descriptor(), &proto_type);
  //   analyzer_options.SetInScopeExpressionColumn("value", proto_type);
  // The proto in the example has a `cost` field.  We can also access the
  // whole proto as `value` because we provided that name.
  //
  // Note that an error will be produced if type is not supported according to
  // the current language options.
  absl::Status AddExpressionColumn(const std::string& name, const Type* type);
  absl::Status SetInScopeExpressionColumn(const std::string& name,
                                          const Type* type);

  void SetLookupExpressionColumnCallback(
      const LookupExpressionColumnCallback& lookup_expression_column_callback);

  ABSL_DEPRECATED("This function is going away. Please don't add new uses.")
  LookupExpressionColumnCallback lookup_expression_column_callback() const {
    return data_->lookup_expression_column_callback;
  }

  void SetPreRewriteCallback(const PreRewriteCallback& pre_rewrite_callback) {
    data_->pre_rewrite_callback = std::move(pre_rewrite_callback);
  }

  PreRewriteCallback pre_rewrite_callback() const {
    return data_->pre_rewrite_callback;
  }

  // Get the named expression columns added.
  // This will include the in-scope expression column if one was set.
  // This doesn't include the columns resolved using the
  // 'lookup_expression_callback_' function.
  const QueryParametersMap& expression_columns() const {
    return data_->expression_columns;
  }

  bool has_in_scope_expression_column() const {
    return in_scope_expression_column_type() != nullptr;
  }
  // Get the name and Type of the in-scope expression column.
  // These return "" and NULL if there is no in-scope expression column.
  const std::string& in_scope_expression_column_name() const {
    return data_->in_scope_expression_column.first;
  }
  const Type* in_scope_expression_column_type() const {
    return data_->in_scope_expression_column.second;
  }

  // Provides the set of pseudo-columns that will be visible on tables created
  // with DDL statements. These columns can be referenced in the PARTITION BY
  // and CLUSTER BY clauses on CREATE TABLE.
  // A fixed set of pseudo-columns can be provided using SetDdlPseudoColumns, or
  // the set can be computed by a callback per table using
  // SetDdlPseudoColumnsCallback.
  // Note that the callback version does not support serialization in
  // AnalyzerOptionsProto.
  void SetDdlPseudoColumnsCallback(
      DdlPseudoColumnsCallback ddl_pseudo_columns_callback);
  void SetDdlPseudoColumns(
      const std::vector<std::pair<std::string, const Type*>>&
          ddl_pseudo_columns);
  // Returns the callback to access pseudo-columns for the target of a DDL
  // statement.
  const DdlPseudoColumnsCallback& ddl_pseudo_columns_callback() const {
    return data_->ddl_pseudo_columns_callback;
  }

  void set_column_id_sequence_number(zetasql_base::SequenceNumber* sequence) {
    data_->column_id_sequence_number = sequence;
  }
  zetasql_base::SequenceNumber* column_id_sequence_number() const {
    return data_->column_id_sequence_number;
  }

  // Sets an IdStringPool for storing strings used in parsing and analysis.
  // If it is not set, then analysis will create a new IdStringPool for every
  // query that is analyzed. WARNING: If this is set, calling Analyze functions
  // concurrently with the same AnalyzerOptions is not allowed.
  void set_id_string_pool(const std::shared_ptr<IdStringPool>& id_string_pool) {
    data_->id_string_pool = id_string_pool;
  }
  std::shared_ptr<IdStringPool> id_string_pool() const {
    return data_->id_string_pool;
  }

  // Sets an zetasql_base::UnsafeArena for storing objects created during parsing and
  // analysis. If it is not set, then analysis will create a new zetasql_base::UnsafeArena for
  // every query that is analyzed. WARNING: If this is set, calling Analyze
  // functions concurrently with the same AnalyzerOptions is not allowed.
  void set_arena(std::shared_ptr<zetasql_base::UnsafeArena> arena) {
    data_->arena = std::move(arena);
  }
  std::shared_ptr<zetasql_base::UnsafeArena> arena() const { return data_->arena; }

  // Creates default-sized id_string_pool() and arena().
  // WARNING: After calling this, calling Analyze functions concurrently with
  // the same AnalyzerOptions is no longer allowed.
  void CreateDefaultArenasIfNotSet();

  // Returns true if arena() and id_string_pool() are both non-NULL.
  bool AllArenasAreInitialized() const {
    return data_->arena != nullptr && data_->id_string_pool != nullptr;
  }

  static constexpr ErrorMessageMode ERROR_MESSAGE_WITH_PAYLOAD =
      zetasql::ERROR_MESSAGE_WITH_PAYLOAD;
  static constexpr ErrorMessageMode ERROR_MESSAGE_ONE_LINE =
      zetasql::ERROR_MESSAGE_ONE_LINE;
  static constexpr ErrorMessageMode ERROR_MESSAGE_MULTI_LINE_WITH_CARET =
      zetasql::ERROR_MESSAGE_MULTI_LINE_WITH_CARET;

  void set_error_message_mode(ErrorMessageMode mode) {
    data_->error_message_mode = mode;
  }
  ErrorMessageMode error_message_mode() const {
    return data_->error_message_mode;
  }

  // TODO Should this be a LanguageOption instead?
  void set_default_time_zone(absl::TimeZone timezone) {
    data_->default_timezone = timezone;
  }
  const absl::TimeZone default_time_zone() const {
    return data_->default_timezone;
  }

  void set_default_anon_function_report_format(absl::string_view format) {
    data_->default_anon_function_report_format = format;
  }
  absl::string_view default_anon_function_report_format() const {
    return data_->default_anon_function_report_format;
  }

  absl::Status set_default_anon_kappa_value(int64_t value);
  int64_t default_anon_kappa_value() const {
    return data_->default_anon_kappa_value;
  }

  void set_statement_context(StatementContext context) {
    data_->statement_context = context;
  }
  StatementContext statement_context() const {
    return data_->statement_context;
  }

  void set_parse_location_record_type(
      ParseLocationRecordType parse_location_record_type) {
    data_->parse_location_record_type = parse_location_record_type;
  }
  const ParseLocationRecordType& parse_location_record_type() const {
    return data_->parse_location_record_type;
  }
  // Set this to true to record parse locations in resolved AST nodes,
  ABSL_DEPRECATED("Inline me!")
  void set_record_parse_locations(bool value) {
    data_->parse_location_record_type =
        (value ? PARSE_LOCATION_RECORD_CODE_SEARCH
               : PARSE_LOCATION_RECORD_NONE);
  }

  void set_create_new_column_for_each_projected_output(bool value) {
    data_->create_new_column_for_each_projected_output = value;
  }
  bool create_new_column_for_each_projected_output() const {
    return data_->create_new_column_for_each_projected_output;
  }

  // Controls whether undeclared parameters are allowed. Undeclared parameters
  // don't appear in query_parameters(). Their type will be assigned by the
  // analyzer in the output AST and returned in
  // AnalyzerOutput::undeclared_parameters() or
  // AnalyzerOutput::undeclared_positional_parameters() depending on the
  // parameter mode. When allow_undeclared_parameters is true and the parameter
  // mode is positional, no positional parameters may be provided in
  // AnalyzerOptions.
  void set_allow_undeclared_parameters(bool value) {
    data_->allow_undeclared_parameters = value;
  }
  bool allow_undeclared_parameters() const {
    return data_->allow_undeclared_parameters;
  }

  // Controls whether positional parameters are allowed. The analyzer supports
  // either named parameters or positional parameters but not both in the same
  // query.
  void set_parameter_mode(ParameterMode mode) { data_->parameter_mode = mode; }
  ParameterMode parameter_mode() const { return data_->parameter_mode; }

  void set_prune_unused_columns(bool value) {
    data_->prune_unused_columns = value;
  }
  bool prune_unused_columns() const { return data_->prune_unused_columns; }

  void set_allowed_hints_and_options(const AllowedHintsAndOptions& allowed) {
    data_->allowed_hints_and_options = allowed;
  }
  const AllowedHintsAndOptions& allowed_hints_and_options() const {
    return data_->allowed_hints_and_options;
  }

  // If false (default), the analyzer will avoid adding a ResolvedCast node for
  // a CAST operation in the query when the source and target types are the
  // same. Otherwise, the ResolvedCast will be generated.
  void set_preserve_unnecessary_cast(bool value) {
    data_->preserve_unnecessary_cast = value;
  }
  bool preserve_unnecessary_cast() const {
    return data_->preserve_unnecessary_cast;
  }

  // Controls whether to preserve aliases of aggregate columns and analytic
  // function columns. This option has no effect on query semantics and just
  // changes what names are used inside ResolvedColumns.
  //
  // If true, the analyzer uses column aliases as names of aggregate columns and
  // analytic function columns if they exist, and falls back to using internal
  // names such as "$agg1" otherwise. If false, the analyzer uses internal names
  // unconditionally.
  //
  // TODO: Make this the default and remove this option.
  void set_preserve_column_aliases(bool value) {
    data_->preserve_column_aliases = value;
  }
  bool preserve_column_aliases() const {
    return data_->preserve_column_aliases;
  }

  // Returns the ParserOptions to use for these AnalyzerOptions, including the
  // same id_string_pool() and arena() values.
  ParserOptions GetParserOptions() const;

  const SystemVariablesMap& system_variables() const {
    return data_->system_variables;
  }
  void clear_system_variables() { data_->system_variables.clear(); }
  absl::Status AddSystemVariable(const std::vector<std::string>& name_path,
                                 const Type* type);

  // DEPRECATED: WILL BE REMOVED SOON
  // If <types> is non empty, the result of analyzed SQL will be coerced to the
  // given types using assignment semantics, requiring that the passed in SQL:
  // * Is a query statement
  // * Has a matching number of columns as the number of given types
  // * Has columns whose types are coercible to the given types based on
  //   positional order
  // * Does not use untyped parameters as column output
  // Otherwise an analysis error will be returned.
  // Note that if a query produces a value table, the provided types must
  // only contain a single struct or proto type.
  // TODO: Remove this last condition
  void set_target_column_types(absl::Span<const Type* const> types) {
    data_->target_column_types =
        std::vector<const Type*>(types.begin(), types.end());
  }
  absl::Span<const Type* const> get_target_column_types() const {
    return data_->target_column_types;
  }

  void set_annotation_specs(std::vector<AnnotationSpec*> annotation_specs) {
    data_->annotation_specs = annotation_specs;
  }

  const std::vector<AnnotationSpec*>& get_annotation_specs() const {
    return data_->annotation_specs;
  }

 private:
  // Defined in zetasql/common/internal_analyzer_options.h.
  friend class InternalAnalyzerOptions;

  // ======================================================================
  // NOTE: Please update options.proto and AnalyzerOptions.java accordingly
  // when adding new fields here.
  // ======================================================================

  // AnalyzerOptions are frequently allocated on the stack. The huge contents
  // of this object makes for expensive stack frames, and in the recursive
  // nature of much ZetaSQL processing this becomes a problem. Therefore,
  // we always allocate the class data on the heap. (Initializing the
  // AnalyzerOptions was already so expensive that throwing one more heap
  // allocation into the mix was in the noise.)
  struct Data {
    // These options determine the language that is accepted.
    LanguageOptions language_options;

    // These options are used for name lookups into the catalog, i.e., for
    // Catalog::Find*() calls.
    Catalog::FindOptions find_options;

    // Maps of defined parameters and expression columns (including in-scope).
    // The keys are lowercased.  Only used in named parameter mode.
    // This doesn't include the columns resolved using the
    // 'lookup_expression_column_callback_' function.
    QueryParametersMap query_parameters;
    QueryParametersMap expression_columns;

    // Maps system variables to their types.
    SystemVariablesMap system_variables;

    // TODO: Clean up the legacy callback once all getters are removed.
    LookupExpressionColumnCallback lookup_expression_column_callback = nullptr;

    // Callback function to resolve columns in standalone expressions.
    LookupExpressionCallback lookup_expression_callback = nullptr;

    // Callback function runs after the initial resolve, before any rewriters
    // run. This can be used for query validations before rewriters making
    // changes (e.g. rewriter can introduce nodes that are unsupported for
    // public queries and we want throw an error if the node was also included
    // in the original query) Note that if the callback returns an error, the
    // analyzer will return that same error.
    PreRewriteCallback pre_rewrite_callback = nullptr;

    // Defined positional parameters. Only used in positional parameter mode.
    // Index 0 corresponds with the query parameter at position 1 and so on.
    std::vector<const Type*> positional_query_parameters;

    // If we have an in-scope expression column, its name and Type are stored
    // here (and also in expression_columns_).  The name may be empty.
    std::pair<std::string, const Type*> in_scope_expression_column;

    // Callback to retrieve pseudo-columns visible in top-level PARTITION BY and
    // CLUSTER BY clauses of DDL statements analyzed using AnalyzeStatement, or
    // else an explicit list of columns. If ddl_pseudo_columns_ is non-empty,
    // the callback is a wrapper that returns ddl_pseudo_columns_ as output
    // regardless of the input. NOTE: Ensure that variables captured by this
    // lamda either outlive this AnalyzerOptions and any copy of it OR are
    // captured by value instead.
    DdlPseudoColumnsCallback ddl_pseudo_columns_callback;
    std::vector<std::pair<std::string, const Type*>> ddl_pseudo_columns;

    // If set, use this to allocate column_ids for the resolved AST.
    // This can be used to analyze multiple queries and ensure that
    // all resolved ASTs have non-overlapping column_ids.
    zetasql_base::SequenceNumber* column_id_sequence_number = nullptr;  // Not owned.

    // Allocate parts of the parse tree and resolved AST in this arena.
    // The arena will also be referenced in AnalyzerOutput to keep it alive.
    std::shared_ptr<zetasql_base::UnsafeArena> arena;

    // Allocate all IdStrings in the resolved AST in this pool.
    // The pool will also be referenced in AnalyzerOutput to keep it alive.
    std::shared_ptr<IdStringPool> id_string_pool;

    ErrorMessageMode error_message_mode = ERROR_MESSAGE_ONE_LINE;

    // Some timestamp-related functions take an optional timezone argument, and
    // allow a default timezone to be used if the argument is not provided.
    // The <default_timezone_> may also be used when coercing string literals
    // to timestamp literals during analysis.  Defaults to America/Los_Angeles.
    absl::TimeZone default_timezone;

    // Some anonymized functions take an optional report format option, and
    // allow a default report format to be used if the option is not provided.
    std::string default_anon_function_report_format;

    // Anonymized functions take an optional kappa value, and allow a default
    // kappa value to be used if the option is not provided. If it is unset, we
    // initialize it as 0, which is not a valid kappa.
    int64_t default_anon_kappa_value = 0;

    // This identifies the ZetaSQL resolution context - whether we are in
    // a normal statement context or whether we are resolving statements
    // in a module.  See (broken link) for details.
    StatementContext statement_context = CONTEXT_DEFAULT;

    // Option that controls whether and how parse locations are recorded in
    // ResolvedNodes.
    ParseLocationRecordType parse_location_record_type =
        PARSE_LOCATION_RECORD_NONE;

    // If set to true, creates a new column for each output produced by each
    // ResolvedProjectScan. This means that each entry in the column_list will
    // always have a corresponding entry in the expr_list.
    //
    // Here is an example:
    //
    //  SELECT * FROM (SELECT a AS b FROM (SELECT 1 AS a));
    //
    //  option      scan      column_list        expr_list
    // -------- ------------- ----------- -----------------------
    //          SELECT 1 AS a    [a#1]      [a#1 := Literal(1)]
    //   false  SELECT a AS b    [a#1]              []
    //          SELECT *         [a#1]              []
    // -------- ------------- ----------- -----------------------
    //          SELECT 1 AS a    [a#1]      [a#1 := Literal(1)]
    //   true   SELECT a AS b    [b#2]    [b#2 := ColumnRef(a#1)]
    //          SELECT *         [b#3]    [b#3 := ColumnRef(b#2)]
    // -------- ------------- ----------- -----------------------
    //
    // Setting this option to true results in a larger resolved AST, but has the
    // benefit that it provides a place to store additional information about
    // how the query was parsed. In combination with record_parse_locations, it
    // allows keeping track of all places in the query where columns are
    // referenced, which is useful to highlight long chains of dependencies
    // through complex queries composed of deeply nested subqueries.
    bool create_new_column_for_each_projected_output = false;

    bool allow_undeclared_parameters = false;

    ParameterMode parameter_mode = PARAMETER_NAMED;

    // This controls whether or not current resolved AST needs validation.
    // It is initialized with <zetasql_validate_resolved_ast> global flag
    // value.
    bool validate_resolved_ast = false;

    // If true, columns that were never referenced in the query will be pruned
    // from column_lists of all ResolvedScans.  This allows using the
    // column_list on ResolvedTableScan for column-level ACL checking. If false,
    // ResolvedTableScans will include all columns on the table, regardless of
    // what the user selected.
    //
    // TODO I want to make this the default once engines are updated,
    // and then remove this option.
    bool prune_unused_columns = false;

    // This specifies the set of allowed hints and options, their expected
    // types, and whether to give errors on unrecognized names.
    // See the class definition for details.
    AllowedHintsAndOptions allowed_hints_and_options;

    // Controls whether to preserve aliases of aggregate columns and analytic
    // function columns. See set_preserve_column_aliases() for details.
    bool preserve_column_aliases = true;

    // Target output column types for a query.
    std::vector<const Type*> target_column_types;

    // The set of ASTRewrites that are enabled.
    // Note that we store these as a btree_set to make the order in which the
    // rewrites are applied consistent, and thus prevent instability in the
    // analyzer test column ids.
    absl::btree_set<ResolvedASTRewrite> enabled_rewrites = DefaultRewrites();

    // Controls whether the analyzer will add a ResolvedCAST node for a CAST
    // operation in the query even when the source and target types are the
    // same.
    bool preserve_unnecessary_cast = false;

    // The annotations specs that are passed in and should be handled by
    // the annotation framework.
    std::vector<AnnotationSpec*> annotation_specs;  // Not owned.
  };
  std::unique_ptr<Data> data_;

  // Copyable
};

// Returns <options> if it already has all arenas initialized, or otherwise
// populates <copy> as a copy for <options>, creates arenas in <copy> and
// returns it. This avoids unnecessary duplication of AnalyzerOptions, which
// might be expensive.
const AnalyzerOptions& GetOptionsWithArenas(
    const AnalyzerOptions* options, std::unique_ptr<AnalyzerOptions>* copy);

// Verifies that the provided AnalyzerOptions have a valid combination of
// settings.
absl::Status ValidateAnalyzerOptions(const AnalyzerOptions& options);

}  // namespace zetasql

#endif  // ZETASQL_PUBLIC_ANALYZER_OPTIONS_H_
