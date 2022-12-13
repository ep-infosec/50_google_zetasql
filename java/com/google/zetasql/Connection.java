/*
 * Copyright 2019 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

package com.google.zetasql;

import java.io.Serializable;

/** A Connection object in a ZetaSQL query. */
public interface Connection extends Serializable {

  /** Get the connection name. */
  public String getName();

  /**
   * Get a fully-qualified description of this Connection. Suitable for log messages, but not
   * necessarily a valid SQL path expression.
   */
  public String getFullName();
}
