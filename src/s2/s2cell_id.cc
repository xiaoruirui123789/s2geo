// Copyright 2005 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "s2/s2cell_id.h"

#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

bool AbslParseFlag(absl::string_view input, S2CellId* id, std::string* error) {
  *id = S2CellId::FromToken(input);
  if (!id->is_valid()) {
    *error = absl::StrCat("Error. Expected valid S2 token got: '", input, "'");
    return false;
  }
  return true;
}

std::string AbslUnparseFlag(S2CellId id) { 
  return id.ToToken(); 
}
