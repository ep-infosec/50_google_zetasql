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

#include "zetasql/public/types/struct_type.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "zetasql/base/logging.h"
#include "zetasql/common/errors.h"
#include "zetasql/public/language_options.h"
#include "zetasql/public/options.pb.h"
#include "zetasql/public/strings.h"
#include "zetasql/public/type.pb.h"
#include "zetasql/public/types/internal_utils.h"
#include "zetasql/public/types/type.h"
#include "zetasql/public/types/type_modifiers.h"
#include "zetasql/public/types/type_parameters.h"
#include "zetasql/public/types/value_representations.h"
#include "zetasql/public/value_content.h"
#include "absl/base/optimization.h"
#include "absl/container/flat_hash_map.h"
#include "absl/hash/hash.h"
#include "absl/meta/type_traits.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "absl/synchronization/mutex.h"
#include "zetasql/base/simple_reference_counted.h"
#include "zetasql/base/ret_check.h"
#include "zetasql/base/status_macros.h"

namespace zetasql {

StructType::StructType(const TypeFactory* factory,
                       std::vector<StructField> fields, int nesting_depth)
    : ContainerType(factory, TYPE_STRUCT),
      fields_(std::move(fields)),
      nesting_depth_(nesting_depth) {}

bool StructType::IsSupportedType(
    const LanguageOptions& language_options) const {
  // A Struct is supported if all of its fields are supported.
  for (const StructField& field : AsStruct()->fields()) {
    if (!field.type->IsSupportedType(language_options)) {
      return false;
    }
  }
  return true;
}

bool StructType::EqualsForSameKind(const Type* that, bool equivalent) const {
  const StructType* other = that->AsStruct();
  ZETASQL_DCHECK(other);
  return StructType::EqualsImpl(this, other, equivalent);
}

void StructType::DebugStringImpl(bool details, TypeOrStringVector* stack,
                                 std::string* debug_string) const {
  absl::StrAppend(debug_string, "STRUCT<");
  stack->push_back(">");
  for (int i = num_fields() - 1; i >= 0; --i) {
    const StructField& field = this->field(i);
    stack->push_back(field.type);
    std::string prefix = (i > 0) ? ", " : "";
    if (!field.name.empty()) {
      absl::StrAppend(&prefix, ToIdentifierLiteral(field.name), " ");
    }
    stack->push_back(prefix);
  }
}

bool StructType::SupportsGroupingImpl(const LanguageOptions& language_options,
                                      const Type** no_grouping_type) const {
  if (!language_options.LanguageFeatureEnabled(FEATURE_V_1_2_GROUP_BY_STRUCT)) {
    if (no_grouping_type != nullptr) *no_grouping_type = this;
    return false;
  }

  for (const StructField& field : this->AsStruct()->fields()) {
    if (!field.type->SupportsGroupingImpl(language_options, no_grouping_type)) {
      return false;
    }
  }
  if (no_grouping_type != nullptr) *no_grouping_type = nullptr;
  return true;
}

bool StructType::SupportsPartitioningImpl(
    const LanguageOptions& language_options,
    const Type** no_partitioning_type) const {
  if (!language_options.LanguageFeatureEnabled(FEATURE_V_1_2_GROUP_BY_STRUCT)) {
    if (no_partitioning_type != nullptr) *no_partitioning_type = this;
    return false;
  }

  for (const StructField& field : this->AsStruct()->fields()) {
    if (!field.type->SupportsPartitioningImpl(language_options,
                                              no_partitioning_type)) {
      return false;
    }
  }

  if (no_partitioning_type != nullptr) *no_partitioning_type = nullptr;
  return true;
}

StructType::~StructType() {}

bool StructType::SupportsOrdering(const LanguageOptions& language_options,
                                  std::string* type_description) const {
  if (type_description != nullptr) {
    *type_description = TypeKindToString(this->kind(),
                                         language_options.product_mode());
  }
  return false;
}

bool StructType::SupportsEquality() const {
  for (const StructField& field : fields_) {
    if (!field.type->SupportsEquality()) {
      return false;
    }
  }
  return true;
}

bool StructType::UsingFeatureV12CivilTimeType() const {
  for (const StructField& field : fields_) {
    if (field.type->UsingFeatureV12CivilTimeType()) {
      return true;
    }
  }
  return false;
}

absl::Status StructType::SerializeToProtoAndDistinctFileDescriptorsImpl(
    const BuildFileDescriptorMapOptions& options, TypeProto* type_proto,
    FileDescriptorSetMap* file_descriptor_set_map) const {
  // Note - we cannot type_proto->Clear(), because it might have a
  // FileDescriptorSet that we are trying to populate.
  type_proto->set_type_kind(kind_);
  StructTypeProto* struct_type_proto = type_proto->mutable_struct_type();
  for (const StructField& field : fields_) {
    StructFieldProto* struct_field_proto = struct_type_proto->add_field();
    struct_field_proto->set_field_name(field.name);
    ZETASQL_RETURN_IF_ERROR(field.type->SerializeToProtoAndDistinctFileDescriptorsImpl(
        options, struct_field_proto->mutable_field_type(),
        file_descriptor_set_map));
  }
  return absl::OkStatus();
}

// TODO DebugString and other recursive methods on struct types
// may cause a stack overflow for deeply nested types.
absl::StatusOr<std::string> StructType::TypeNameImpl(
    int field_limit,
    const std::function<absl::StatusOr<std::string>(
        const zetasql::Type*, int field_index)>& field_debug_fn) const {
  const int num_fields_to_show = std::min<int>(field_limit, fields_.size());
  const bool output_truncated = num_fields_to_show < fields_.size();

  std::string ret = "STRUCT<";
  for (int i = 0; i < num_fields_to_show; ++i) {
    const StructField& field = fields_[i];
    if (i != 0) absl::StrAppend(&ret, ", ");
    if (!field.name.empty()) {
      absl::StrAppend(&ret, ToIdentifierLiteral(field.name), " ");
    }
    ZETASQL_ASSIGN_OR_RETURN(std::string field_parameters,
                     field_debug_fn(field.type, i));
    absl::StrAppend(&ret, field_parameters);
  }
  if (output_truncated) {
    absl::StrAppend(&ret, ", ...");
  }
  absl::StrAppend(&ret, ">");
  return ret;
}

std::string StructType::ShortTypeName(ProductMode mode) const {
  // Limit the output to three struct fields to avoid long error messages.
  const int field_limit = 3;
  const auto field_debug_fn = [=](const zetasql::Type* type,
                                  int field_index) {
    return type->ShortTypeName(mode);
  };
  return TypeNameImpl(field_limit, field_debug_fn).value();
}

std::string StructType::TypeName(ProductMode mode) const {
  const auto field_debug_fn = [=](const zetasql::Type* type,
                                  int field_index) {
    return type->TypeName(mode);
  };
  return TypeNameImpl(std::numeric_limits<int>::max(), field_debug_fn).value();
}

absl::StatusOr<std::string> StructType::TypeNameWithModifiers(
    const TypeModifiers& type_modifiers, ProductMode mode) const {
  const TypeParameters& type_params = type_modifiers.type_parameters();
  if (!type_params.IsEmpty() && (type_params.num_children() != num_fields())) {
    return MakeSqlError()
           << "Input type parameter does not correspond to this StructType";
  }
  const Collation& collation = type_modifiers.collation();
  if (!collation.HasCompatibleStructure(this)) {
    return MakeSqlError() << "Input collation" << collation.DebugString()
                          << " is not compatible with type " << DebugString();
  }
  ZETASQL_RET_CHECK(collation.Empty() || collation.num_children() == num_fields());
  const auto field_debug_fn = [=](const zetasql::Type* type,
                                  int field_index) {
    return type->TypeNameWithModifiers(
        TypeModifiers::MakeTypeModifiers(
            type_params.IsEmpty() ? TypeParameters()
                                  : type_params.child(field_index),
            collation.Empty() ? Collation() : collation.child(field_index)),
        mode);
  };
  return TypeNameImpl(std::numeric_limits<int>::max(), field_debug_fn);
}

absl::StatusOr<TypeParameters> StructType::ValidateAndResolveTypeParameters(
    const std::vector<TypeParameterValue>& type_parameter_values,
    ProductMode mode) const {
  return MakeSqlError() << ShortTypeName(mode)
                        << " type cannot have type parameters by itself, it "
                           "can only have type parameters on its struct fields";
}

absl::Status StructType::ValidateResolvedTypeParameters(
    const TypeParameters& type_parameters, ProductMode mode) const {
  // type_parameters must be empty or has the same number of children as struct.
  if (type_parameters.IsEmpty()) {
    return absl::OkStatus();
  }
  ZETASQL_RET_CHECK_EQ(type_parameters.num_children(), num_fields());
  for (int i = 0; i < num_fields(); ++i) {
    ZETASQL_RETURN_IF_ERROR(fields_[i].type->ValidateResolvedTypeParameters(
        type_parameters.child(i), mode));
  }
  return absl::OkStatus();
}

const StructType::StructField* StructType::FindField(absl::string_view name,
                                                     bool* is_ambiguous,
                                                     int* found_idx) const {
  *is_ambiguous = false;
  if (found_idx != nullptr) *found_idx = -1;

  // Empty names indicate unnamed fields, not fields named "".
  if (ABSL_PREDICT_FALSE(name.empty())) {
    return nullptr;
  }

  int field_index;
  {
    absl::MutexLock lock(&mutex_);
    if (ABSL_PREDICT_FALSE(field_name_to_index_map_.empty())) {
      for (int i = 0; i < num_fields(); ++i) {
        const std::string& field_name = field(i).name;
        // Empty names indicate unnamed fields, not fields which can be looked
        // up by name. They are not added to the map.
        if (!field_name.empty()) {
          auto result = field_name_to_index_map_.emplace(field_name, i);
          // If the name has already been added to the map, we know any lookup
          // on that name would be ambiguous.
          if (!result.second) result.first->second = -1;
        }
      }
    }
    const auto iter = field_name_to_index_map_.find(name);
    if (ABSL_PREDICT_FALSE(iter == field_name_to_index_map_.end())) {
      return nullptr;
    }
    field_index = iter->second;
  }

  if (ABSL_PREDICT_FALSE(field_index == -1)) {
    *is_ambiguous = true;
    return nullptr;
  } else {
    if (found_idx != nullptr) *found_idx = field_index;
    return &fields_[field_index];
  }
}

Type::HasFieldResult StructType::HasFieldImpl(
    const std::string& name, int* field_id, bool include_pseudo_fields) const {
  bool is_ambiguous;
  const StructField* field = FindField(name, &is_ambiguous, field_id);
  if (is_ambiguous) {
    return HAS_AMBIGUOUS_FIELD;
  }

  if (!field) {
    return HAS_NO_FIELD;
  }

  return HAS_FIELD;
}

bool StructType::HasAnyFields() const { return num_fields() != 0; }

int64_t GetEstimatedStructFieldOwnedMemoryBytesSize(const StructField& field) {
  static_assert(
      sizeof(field) ==
          sizeof(std::tuple<decltype(field.name), decltype(field.type)>),
      "You need to update GetEstimatedStructFieldOwnedMemoryBytesSize "
      "when you change StructField");

  return sizeof(field) +
         internal::GetExternallyAllocatedMemoryEstimate(field.name);
}

int64_t StructType::GetEstimatedOwnedMemoryBytesSize() const {
  int64_t result = sizeof(*this);

  for (const StructField& field : fields_) {
    result += GetEstimatedStructFieldOwnedMemoryBytesSize(field);
  }

  // Map field_name_to_index_map_ is built lazily, we account its memory
  // in advance, which potentially can lead to overestimation.
  int64_t fields_to_load = fields_.size() - field_name_to_index_map_.size();
  if (fields_to_load < 0) {
    fields_to_load = 0;
  }
  result += internal::GetExternallyAllocatedMemoryEstimate(
      field_name_to_index_map_, fields_to_load);

  return result;
}

bool StructType::FieldEqualsImpl(const StructType::StructField& field1,
                                 const StructType::StructField& field2,
                                 bool equivalent) {
  // Ignore field names if we are doing an equivalence check.
  if (!equivalent && !zetasql_base::CaseEqual(field1.name, field2.name)) {
    return false;
  }
  return field1.type->EqualsImpl(field2.type, equivalent);
}

bool StructType::EqualsImpl(const StructType* const type1,
                            const StructType* const type2, bool equivalent) {
  if (type1->num_fields() != type2->num_fields()) {
    return false;
  }
  for (int idx = 0; idx < type1->num_fields(); ++idx) {
    if (!FieldEqualsImpl(type1->field(idx), type2->field(idx), equivalent)) {
      return false;
    }
  }
  return true;
}

void StructType::CopyValueContent(const ValueContent& from,
                                  ValueContent* to) const {
  from.GetAs<zetasql_base::SimpleReferenceCounted*>()->Ref();
  *to = from;
}

void StructType::ClearValueContent(const ValueContent& value) const {
  value.GetAs<zetasql_base::SimpleReferenceCounted*>()->Unref();
}

absl::HashState StructType::HashTypeParameter(absl::HashState state) const {
  for (const StructField& field : fields_) {
    state = field.type->Hash(std::move(state));
  }

  return state;
}

absl::HashState StructType::HashValueContent(const ValueContent& value,
                                             absl::HashState state) const {
  absl::HashState result = absl::HashState::Create(&state);
  const internal::ValueContentContainer* container =
      value.GetAs<internal::ValueContentContainerRef*>()->value();
  for (int i = 0; i < container->num_elements(); i++) {
    ValueContentContainerElementHasher hasher(field(i).type);
    result = absl::HashState::combine(std::move(result),
                                      hasher(container->element(i)));
  }
  return result;
}

bool StructType::ValueContentEquals(
    const ValueContent& x, const ValueContent& y,
    const ValueEqualityCheckOptions& options) const {
  const internal::ValueContentContainer* x_container =
      x.GetAs<internal::ValueContentContainerRef*>()->value();
  const internal::ValueContentContainer* y_container =
      y.GetAs<internal::ValueContentContainerRef*>()->value();
  if (x_container->num_elements() != y_container->num_elements()) {
    if (options.reason) {
      const auto& format_options = DebugFormatValueContentOptions();
      absl::StrAppend(
          options.reason,
          absl::Substitute(
              "Number of struct fields is {$0} and {$1} in respective "
              "structs {$2} and {$3}\n",
              x_container->num_elements(), y_container->num_elements(),
              FormatValueContent(x, format_options),
              FormatValueContent(y, format_options)));
    }
    return false;
  }

  for (int i = 0; i < x_container->num_elements(); i++) {
    // By default use options provided in arguments
    ValueEqualityCheckOptions const* field_options = &options;
    std::unique_ptr<ValueEqualityCheckOptions> options_copy = nullptr;
    // If "allow_bags" is true, create a copy of options with populated
    // deep_order_spec
    if (options.deep_order_spec != nullptr) {
      options_copy = std::make_unique<ValueEqualityCheckOptions>(options);
      options_copy->deep_order_spec = &options.deep_order_spec->children[i];
      field_options = options_copy.get();
    }
    const auto& e1 = x_container->element(i);
    const auto& e2 = y_container->element(i);
    if (e1.is_null() != e2.is_null()) {
      return false;
    }
    if (e1.is_null()) {
      // Consider those elements to be equal and continue
      continue;
    }
    if (!field(i).type->ValueContentEquals(
            e1.value_content(), e2.value_content(), *field_options)) {
      return false;
    }
  }
  return true;
}

bool StructType::ValueContentLess(const ValueContent& x, const ValueContent& y,
                                  const Type* other_type) const {
  const internal::ValueContentContainer* x_container =
      x.GetAs<internal::ValueContentContainerRef*>()->value();
  const internal::ValueContentContainer* y_container =
      y.GetAs<internal::ValueContentContainerRef*>()->value();
  if (x_container->num_elements() != y_container->num_elements()) return false;
  // Because we return true as soon as 'LessThan' returns true for a
  // field (without checking types of all fields), we may return true for
  // incompatible types. This behavior is OK for now because we consider
  // LessThan for incompatible types is undefined.
  const StructType* other_struct_type = other_type->AsStruct();
  for (int i = 0; i < x_container->num_elements(); i++) {
    const Type* x_field_type = field(i).type;
    const Type* y_field_type = other_struct_type->field(i).type;
    const std::optional<bool> is_less = ValueContentContainerElementLess(
        x_container->element(i), y_container->element(i), x_field_type,
        y_field_type);
    if (is_less.has_value()) return *is_less;
  }
  return false;
}

absl::Status StructType::SerializeValueContent(const ValueContent& value,
                                               ValueProto* value_proto) const {
  return absl::FailedPreconditionError(
      "SerializeValueContent should never be called for StructType, since its "
      "value content is maintained in the Value class");
}

absl::Status StructType::DeserializeValueContent(const ValueProto& value_proto,
                                                 ValueContent* value) const {
  return absl::FailedPreconditionError(
      "DeserializeValueContent should never be called for StructType, since "
      "its value content is maintained in the Value class");
}

std::string StructType::GetFormatPrefix(
    const ValueContent& value_content,
    const Type::FormatValueContentOptions& options) const {
  std::string prefix;
  switch (options.mode) {
    case Type::FormatValueContentOptions::Mode::kDebug:
      if (options.verbose) {
        prefix.append(CapitalizedName());
      }
      prefix.push_back('{');
      break;
    case Type::FormatValueContentOptions::Mode::kSQLLiteral:
      if (num_fields() <= 1) {
        prefix.append("STRUCT");
      }
      prefix.push_back('(');
      break;
    case Type::FormatValueContentOptions::Mode::kSQLExpression:
      prefix.append(TypeName(options.product_mode));
      prefix.push_back('(');
      break;
  }
  return prefix;
}

char StructType::GetFormatClosingCharacter(
    const Type::FormatValueContentOptions& options) const {
  return options.mode == Type::FormatValueContentOptions::Mode::kDebug ? '}'
                                                                       : ')';
}

const Type* StructType::GetElementType(int index) const {
  return fields()[index].type;
}

std::string StructType::GetFormatElementPrefix(
    const int index, const bool is_null,
    const FormatValueContentOptions& options) const {
  if (options.mode == FormatValueContentOptions::Mode::kDebug) {
    const std::string& field_name = fields()[index].name;
    if (!field_name.empty()) {
      return absl::StrCat(field_name, ":");
    }
  }
  return "";
}

}  // namespace zetasql
