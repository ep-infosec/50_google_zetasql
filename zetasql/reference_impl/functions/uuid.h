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

#ifndef ZETASQL_REFERENCE_IMPL_FUNCTIONS_UUID_H_
#define ZETASQL_REFERENCE_IMPL_FUNCTIONS_UUID_H_

namespace zetasql {

// This module registers the following function implementation: GENERATE_UUID
void RegisterBuiltinUuidFunctions();

}  // namespace zetasql

#endif  // ZETASQL_REFERENCE_IMPL_FUNCTIONS_UUID_H_
