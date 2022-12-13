

# Work with protocol buffers

Protocol buffers are a flexible, efficient mechanism for serializing structured
data. They are easy to create, small in size, and efficient to send over RPCs.

As efficient and as popular as protocol buffers are, however, when it comes to
data storage they do have one drawback: they do not map well to SQL syntax. For
example, SQL syntax expects that a given field can support a `NULL` or default
value. Protocol buffers, on the other hand, do not support `NULL`s very well,
and there isn't a standard way to determine whether a missing field should get a
`NULL` or a default value.

If you're going to query protocol buffers, you need to understand how they are
represented, what features they support, and what data they can contain. If
you're unfamiliar with protocol buffers in general, or would like a refresher on
how they work in languages other than SQL, see the
[Protocol Buffers Developer Guide][protocol-buffers-dev-guide].

## Constructing protocol buffers

This section covers how to construct protocol buffers using ZetaSQL.

### NEW protocol_buffer {...} {: #using_new_map_constructor }

You can create a protocol buffer using the [`NEW`][new-operator]
operator with a map constructor:

<section class="tabs">

#### Format {.new-tab}

```sql
NEW protocol_buffer {
  field_name: literal_or_expression
  field_name { ... }
  repeated_field_name: [literal_or_expression, ... ]
  map_field_name: [{key: literal_or_expression value: literal_or_expression}, ...],
  (extension_name): literal_or_expression
}
```

Where:

+ `protocol_buffer`: The full protocol buffer name including the package name.
+ `field_name`: The name of a field.
+ `literal_or_expression`: The field value.
+  `map_field_name`: The name of a map-typed field. The value is a list of
   key/value pair entries for the map.
+  `extension_name`: The name of the proto extension, including the package
   name.

#### Example {.new-tab}

```sql
NEW Universe {
  name: "Sol"
  closest_planets: ["Mercury", "Venus", "Earth" ]
  star {
    radius_miles: 432,690
    age: 4,603,000,000
  }
  constellations [{
    name: "Libra"
    index: 0
  }, {
    name: "Scorpio"
    index: 1
  }]
  planet_distances: [{
    key: "Mercury"
    distance: 46,507,000
  }, {
    key: "Venus"
    distance: 107,480,000
  }],
  (UniverseExtraInfo.extension) {
    ...
  }
  all_planets: (SELECT planets FROM SolTable)
}
```

<!-- mdlint off(WHITESPACE_LINE_LENGTH) -->

</section>

> NOTE: The syntax is very similar to the Protocol Buffer Text Format
>  syntax.
> The differences are:
>
> +   Values can be arbitrary SQL expressions instead of having to be literals.
> +   Repeated fields are written as `x_array: [1, 2, 3]` instead of `x_array:`
>     appearing multiple times.
> +   Extensions use parentheses instead of square brackets.

<!-- mdlint on -->

When using this syntax:

+   The field values must be expressions that are implicitly coercible or
    literal-coercible to the type of the corresponding protocol buffer field.
+   Commas between fields are optional.
+   Extension names must have parentheses around the path and must have a comma
    preceding the extension field (unless it is the first field).
+   A colon is required between field name and values unless the value is a map
    constructor.
+   The `NEW protocol_buffer` prefix is optional if the protocol buffer type can
    be inferred from the context.
+   The type of submessages inside the map constructor can be inferred.

**Examples**

Simple:

```sql
SELECT
  key,
  name,
  NEW zetasql.examples.music.Chart { rank: 1 chart_name: "2" }
```

Nested messages and arrays:

```sql
SELECT
  NEW zetasql.examples.music.Album {
    album_name: "New Moon"
    singer {
      nationality: "Canadian"
      residence: [{ city: "Victoria" }, { city: "Toronto" }]
    }
    song: ["Sandstorm","Wait"]
  }
```

With an extension field (note a comma is required before the extension field):

```sql
SELECT
  NEW zetasql.examples.music.Album {
    album_name: "New Moon",
    (zetasql.examples.music.downloads): 30
  }
```

Non-literal expressions as values:

```sql
SELECT NEW zetasql.examples.music.Chart {
  rank: (SELECT COUNT(*) FROM table_name WHERE foo="bar")
  chart_name: CONCAT("best", "hits")
}
```

Examples of the type protocol buffer being inferred from context:

+   From `ARRAY` constructor:

    ```sql
    SELECT ARRAY<zetasql.examples.music.Chart>[
        { rank: 1 chart_name: "2" },
        { rank: 2 chart_name: "3" }]
    ```
+   From `STRUCT` constructor:

    ```sql
    SELECT STRUCT<STRING,zetasql.examples.music.Chart,INT64>
      ("foo", { rank: 1 chart_name: "2" }, 7)
    ```
+   From column names through `SET`:

    +   Simple column:

    ```sql
    UPDATE table_name SET ProtoColumn = { rank: 1 chart_name: "2" }
    ```

    +   Array column:

    ```sql
    UPDATE table_name SET ProtoArrayColumn =
      [{ rank: 1 chart_name: "2" }, { rank: 2 chart_name: "3" }]
    ```

    +   Struct column:

    ```sql
    UPDATE table_name SET ProtoStructColumn =
      ("foo", { rank: 1 chart_name: "2" }, 7)
    ```
+   From generated column names in `CREATE`:

    ```sql
    CREATE TABLE T (
      IntColumn INT32,
      ProtoColumn zetasql.examples.music.Chart AS ({ rank: 1 chart_name: "2" })
    )
    ```
+   From column names in default values in `CREATE`:

    ```sql
    CREATE TABLE T (
      IntColumn INT32,
      ProtoColumn zetasql.examples.music.Chart DEFAULT ({ rank: 1 chart_name: "2" })
    )
    ```
+   From return types in SQL function body:

    ```sql
    CREATE FUNCTION myfunc (  ) RETURNS zetasql.examples.music.Chart AS ({ rank: 1 chart_name: "2" })
    ```
+   From system variable type:

    ```sql
    SET @@proto_system_variable = { rank: 1 chart_name: "2" }
    ```

### NEW protocol_buffer (...) 
<a id="using_new"></a>

You can create a protocol buffer using the [`NEW`][new-operator] operator with a
parenthesized list of arguments and aliases to specify field names:

<section class="tabs">

#### Format {.new-tab}

```sql
NEW protocol_buffer(field [AS alias], ...)
```

#### Example {.new-tab}

```sql
SELECT
  key,
  name,
  NEW zetasql.examples.music.Chart(key AS rank, name AS chart_name)
FROM
  (SELECT 1 AS key, "2" AS name);
```

<!-- mdlint off(WHITESPACE_LINE_LENGTH) -->

</section>

When using this syntax:

+   All field expressions must have an [explicit alias][explicit-alias] or end
    with an identifier. For example, the expression `a.b.c` has the [implicit
    alias][implicit-alias] `c`.
+   `NEW` matches fields by alias to the field names of the protocol buffer.
    Aliases must be unique.
+   The expressions must be implicitly coercible or literal-coercible to the
    type of the corresponding protocol buffer field.

To create a protocol buffer with an extension, use this syntax:

```sql
NEW protocol_buffer(expression AS (path.to.extension), ...)
```

+   For `path.to.extension`, provide the path to the extension. Place the
    extension path inside parentheses.
+   `expression` provides the value to set for the extension. `expression` must be of the
    same type as the extension or [coercible to that type][conversion-rules].

    Example:

    ```sql
    SELECT
     NEW zetasql.examples.music.Album (
       album AS album_name,
       count AS (zetasql.examples.music.downloads)
     )
     FROM (SELECT 'New Moon' AS album, 30 AS count);

    +---------------------------------------------------+
    | $col1                                             |
    +---------------------------------------------------+
    | {album_name: 'New Moon' [...music.downloads]: 30} |
    +---------------------------------------------------+
    ```
+   If `path.to.extension` points to a nested protocol buffer extension, `expr1`
    provides an instance or a text format string of that protocol buffer.

    Example:

    ```sql
    SELECT
     NEW zetasql.examples.music.Album(
       'New Moon' AS album_name,
       NEW zetasql.examples.music.AlbumExtension(
        DATE(1956,1,1) AS release_date
       )
     AS (zetasql.examples.music.AlbumExtension.album_extension));

    +---------------------------------------------+
    | $col1                                       |
    +---------------------------------------------+
    | album_name: "New Moon"                      |
    | [...music.AlbumExtension.album_extension] { |
    |   release_date: -5114                       |
    | }                                           |
    +---------------------------------------------+
    ```

### SELECT AS typename 
<a id="select_as_typename_proto"></a>

The [`SELECT AS typename`][select-as-typename] statement can produce a value table where the
row type is a specific named protocol buffer type.

`SELECT AS` does not support setting protocol buffer extensions. To do so, use
the [NEW][new-keyword] keyword instead. For example,  to create a
protocol buffer with an extension, change a query like this:

```sql
SELECT AS typename field1, field2, ...
```

to a query like this:

```sql
SELECT AS VALUE NEW ProtoType(field1, field2, field3 AS (path.to.extension), ...)
```

## Casting protocol buffers

You can cast `PROTO` to or from `BYTES` or `STRING`.

```sql
SELECT CAST('first_name: "Alana", last_name: "Yah", customer_no: 1234'
  AS example.CustomerInfo);
```

Casting to or from `BYTES` produces or parses proto2 wire format bytes. If
there is a failure during the serialization or deserialization process, an error
is raised. This can happen, for example, if no value is specified for a
required field.

Casting to or from `STRING` produces or parses the proto2 text format. When
casting from `STRING`, unknown field names aren't parseable. This means you need
to be cautious, because round-tripping from `PROTO` to `STRING` back to `PROTO`
could result in loss of data.

`STRING` literals used where a `PROTO` value is expected will be implicitly cast
to `PROTO`. If the literal value cannot be parsed using the expected `PROTO`
type, an error will be raised. To return `NULL`
instead, use [`SAFE_CAST`][link_to_safe_cast].

## Type mapping 
<a id="type_mapping"></a>

Protocol buffers are represented using the `PROTO` data type.  A column can
contain `PROTO` values the same way it can contain `INT32` or `STRING` values.

A protocol buffer contains zero or more fields inside it. Each field inside a
protocol buffer has its own type. All data types except `STRUCT` can be
contained inside a `PROTO`. Repeated fields in a protocol buffer are represented
as `ARRAY`s. The following table gives examples of the mapping between various
protocol buffer field types and the resulting ZetaSQL types.

<table>
<thead>
<tr>
<th>Protocol Buffer Field Type</th>
<th style="white-space:nowrap">ZetaSQL Type</th>
</tr>
</thead>
<tbody>
<tr>
<td><code>optional MessageType msg = 1;</code></td>
<td><code>PROTO&lt;MessageType&gt;</code></td>
</tr>
<tr>
<td><code>required int64 int = 1;</code></td>
<td><code>INT64</code></td>
</tr>
<tr>
<td>
<code>optional int64 int = 1;</code><br>
When reading, if this field isn't set, the default value (0) is returned. By
default, protocol buffer fields do not return <code>NULL</code>.
</td>
<td><code>INT64</code></td>
</tr>
<tr>
<td>
<code>optional int64 int = 1 [( zetasql.use_defaults ) = false];</code><br>
When reading, if this field isn't set, a <code>NULL</code> value is returned.
This example uses an annotation, which is described in
<a href="#default_values_and_nulls">Defaults and <code>NULL</code>s</a>.
</td>
<td><code>INT64</code></td>
</tr>

<tr>
<td>
<code>optional int32 date = 1 [( zetasql.format ) = DATE];</code>

</td>
<td><code>DATE</code></td>
</tr>
<tr>
<td>
<code>optional int64 time = 1 [( zetasql.format ) = TIMESTAMP_MICROS];</code>

</td>
<td><code>TIMESTAMP</code></td>
</tr>

<tr>
<td><code>repeated int64 repeat = 1;</code></td>
<td><code>ARRAY&lt;INT64&gt;</code></td>
</tr>
</tbody>
</table>

### Default values and `NULL`s  {#default_values_and_nulls}

Protocol buffer messages themselves do not have a default value &mdash; only the
fields contained inside a protocol buffer have defaults. When a full protocol
buffer value is returned in the result of a query, it is returned as a blob and
all fields are preserved as they were stored, including unset fields. This means
that you can run a query that returns a protocol buffer, and then extract fields
or check field presence in your client code with normal protocol buffer default
behavior.

By default, `NULL` values are never returned when accessing non-repeated leaf
fields contained in a `PROTO` from within a SQL statement, unless a containing
value is also `NULL`.  If the field value is not explicitly set, the default
value for the field is returned.  A change to the default value for a protocol
buffer field affects all future reads of that field for records where the value
is unset.

For example, suppose that `proto_msg` of type `PROTO` has a field named
`leaf_field`. A reference to `proto_msg.leaf_field` returns:

+ `NULL` if `proto_msg` is `NULL`.
+ A default value if `proto_msg` is not `NULL` but `leaf_field` is not set.
+ The value of `leaf_field` if `proto_msg` is not `NULL` and `leaf_field`
  is set.

### zetasql.use_defaults

You can change this default behavior using a special annotation on your protocol
message definition, `zetasql.use_defaults`, which you set on an individual
field to cause `NULL` values to be returned whenever a field value is not
explicitly set.

This annotation takes a boolean value.  The default is `true`, which means to
use the protocol buffer field defaults.  The annotation normally is written with
the value `false`, meaning that defaults should be ignored and `NULL`s should be
returned.

The following example shows how you can use the `use_defaults` annotation for an
optional protocol buffer field.

```proto
import "zetasql/public/proto/type_annotation.proto";

message SimpleMessage {
  // String field, where ZetaSQL interprets missing values as NULLs.
  optional string str = 2 [( zetasql.use_defaults ) = false];
}
```

In the case where protocol buffers have empty repeated fields, an empty `ARRAY`
is returned rather than a `NULL`-valued `ARRAY`. This behavior cannot be
changed.

After a value has been read out of a protocol buffer field, that value is
treated like any other value of that type. For non-`PROTO` values, such as
`INT64`, this means that after you get the value, you will not be able to tell
if the value for that field was set explicitly, or if it was read as a default
value.

### zetasql.use_field_defaults

The `zetasql.use_field_defaults` annotation is just like
`zetasql.use_defaults`, but you set it on a message and it applies to all
unset fields within a given protocol buffer message. If both are present, the
field-level annotation takes precedence.

```proto
import "zetasql/public/proto/type_annotation.proto";

message AnotherSimpleMessage {
  // Interpret missing value as NULLs for all fields in this message.
  option ( zetasql.use_field_defaults ) = false;

  optional int64 nullable_int = 1;
  optional string nullable_string = 2;
}
```

### Checking if a non-repeated field has a value 
<a id="checking_if_a_field_has_a_value"></a>

You can detect whether `optional` fields are set using a virtual field, `has_X`,
where `X` is the name of the field being checked. The type of the `has_X` field
is `BOOL`. The `has_` field is available for any non-repeated field of a `PROTO`
value. This field equals true if the value `X` is explicitly set in the message.

This field is useful for determining if a protocol buffer field has an explicit
value, or if reads will return a default value. Consider the protocol buffer
example, which has a field `country`. You can construct a query to determine if
a Customer protocol buffer message has a value for the country field by using
the virtual field `has_country`:

```proto
message ShippingAddress {
  optional string name = 1;
  optional string address = 2;
  optional string country = 3;
}
```

```sql
SELECT
  c.Orders.shipping_address.has_country
FROM
  Customer c;
```

If `has_country` returns `TRUE`, it indicates that the value for the `country`
field has been explicitly set. If it returns `FALSE` or `NULL`, it means the
value is not explicitly set.

### Checking for a repeated value 
<a id="checking_for_a_repeated_value"></a>

You can use an `EXISTS` subquery to scan inside a repeated field and check if
any value exists with some desired property. For example, the following query
returns the name of every customer who has placed an order for the product
"Foo".

```sql
SELECT
  C.name
FROM
  Customers AS C
WHERE
  EXISTS(
    SELECT
      *
    FROM
      C.Orders.line_item AS item
    WHERE
      item.product_name = 'Foo'
  );
```

### Nullness and nested fields

A `PROTO` value may contain fields which are themselves `PROTO`s. When this
happens it is possible for the nested `PROTO` itself to be `NULL`. In such a
case, the fields contained within that nested field are also `NULL`
regardless of their `use_default_value` settings.

Consider this example proto:

```proto
syntax = "proto2";

import "zetasql/public/proto/type_annotation.proto";

package some.package;

message NestedMessage {
  optional int64 value = 1 [( zetasql.use_defaults ) = true];
}

message OuterMessage {
  optional NestedMessage nested = 1;
}
```

Running the following query returns a `5` for `value` because it is
explicitly defined.

```sql
SELECT
  proto_field.nested.value
FROM
  (SELECT
     CAST("nested { value: 5 }" as some.package.OuterMessage) as proto_field);
```

If `value` is not explicitly defined but `nested` is, you get a `0` because
the annotation on the protocol buffer definition says to use default values.

```sql
SELECT
  proto_field.nested.value
FROM
  (SELECT
     CAST("nested { }" as some.package.OuterMessage) as proto_field);
```

However, if `nested` is not explicitly defined, you get a `NULL` even
though the annotation says to use default values for the `value` field. This is
because the containing message is `NULL`. This behavior applies to both
repeated and non-repeated fields within a nested message.

```sql
SELECT
  proto_field.nested.value
FROM
  (SELECT
     CAST("" as some.package.OuterMessage) as proto_field);
```

### Annotations to extend the type system 
<a id="proto_annotations"></a>

The ZetaSQL type system contains more types than the protocol buffer
type system.
<a href="https://developers.google.com/protocol-buffers/docs/proto?csw=1#options">
Proto annotations</a> are used to store non-protocol-buffer types inside
serialized protos and read them back as the correct type.

While protocol buffers themselves do not support `DATE` or `TIMESTAMP` types,
you can use annotations on your protocol message definition to indicate that
certain fields should be interpreted as `DATE` or `TIMESTAMP` values when read
using SQL. For instance, a protocol message definition could contain the
following line:

```proto
optional int32 date = 2 [( zetasql.format ) = DATE];
```

The `zetasql.format` annotation indicates that this field, which stores an
`int32` in the protocol buffer, should be interpreted as a `DATE`. Queries over
the `date` field return a `DATE` type instead of an `INT32` because of the
annotation.

This result is the equivalent of having an `INT32` column and querying it as
follows:

```sql
SELECT
  DATE_FROM_UNIX_DATE(date)...
```

## Querying protocol buffers 
<a id="querying_protocol_buffers"></a>

You use the dot operator to access the fields contained within a protocol
buffer. This can not be used to get values of ambiguous fields.
If you need to reference an ambiguous field,
see [`EXTRACT`][proto-extract].

### Example protocol buffer message 
<a id="example_protocol_buffer_message"></a>

To illustrate how to query protocol buffers, consider a table, `Customers`, that
contains a column `Orders` of type `PROTO`. The proto stored in `Orders`
contains fields such as the items ordered and the shipping address. The `.proto`
file that defines this protocol buffer might look like this:

```proto
import "zetasql/public/proto/type_annotation.proto";

message Orders {
  optional string order_number = 1;
  optional int64 date = 2 [( zetasql.format ) = DATE];

  message Address {
    optional string street = 1;
    optional string city = 2;
    optional string state = 3;
    optional string country = 4 [( zetasql.use_defaults ) = true,
                                  default = "United States"];
  }

  optional Address shipping_address = 3;

  message Item {
    optional string product_name = 1;
    optional int32 quantity = 2;
  }

  repeated Item line_item = 4;

  map<string, string> labels = 5;
}
```

An instance of this message might be:

```
{
  order_number: 1234567
  date: 16242
  shipping_address: {
      street: "1234 Main St"
      city: "AnyCity"
      state: "AnyState"
      country: "United States"
  }
  line_item: {
      product_name: "Foo"
      quantity: 10
  }
  line_item: {
      product_name: "Bar"
      quantity: 5
  }
}
```

### Querying top-level fields 
<a id="querying_top-level_fields"></a>

You can write a query to return an entire protocol buffer message, or to return
a top-level or nested field of the message.

Using our example protocol buffer message, the following query returns all
protocol buffer values from the `Orders` column:

```sql
SELECT
  c.Orders
FROM
  Customers c;
```

This query returns the top-level field `order_number` from all protocol buffer
messages in the `Orders` column:

```sql
SELECT
  c.Orders.order_number
FROM
  Customers c;
```

### Querying nested paths 
<a id="querying_nested_paths"></a>

Notice that the `Order` protocol buffer contains another protocol buffer
message, `Address`, in the `shipping_address` field. You can create a query that
returns all orders that have a shipping address in the United States:

```sql
SELECT
  c.Orders.order_number,
  c.Orders.shipping_address
FROM
  Customers c
WHERE
  c.Orders.shipping_address.country = "United States";
```

### Returning repeated fields

Often, a protocol buffer message contains one or more repeated fields which are
returned as `ARRAY` values when referenced in SQL statements. For example, our
protocol buffer message contains a repeated field, `line_item`.

The following query returns a collection of `ARRAY`s containing the line items,
each holding all the line items for one order:

```sql
SELECT
  c.Orders.line_item
FROM
  Customers c;
```

For more information, see
[Work with arrays][working-with-arrays].

### Returning the number of elements in an array

As with any other `ARRAY` value, you can return the number of repeated fields in
a protocol buffer using the `ARRAY_LENGTH` function.

```sql
SELECT
  c.Orders.order_number,
  ARRAY_LENGTH(c.Orders.line_item)
FROM
  Customers c;
```

### Querying map fields

Maps are not a supported type in ZetaSQL. However, maps are
[implemented in proto3 as repeated fields][protocol-buffer-compatibility],
so you can query maps by querying
the underlying repeated field. The underlying repeated field has `key` and
`value` fields that can be queried.

```sql
SELECT
  C.Orders.order_number
FROM
  Customers AS C
WHERE
  EXISTS(
    SELECT
      *
    FROM
      C.Orders.Labels label
    WHERE
      label.key = 'color' AND label.value = 'red'
  );
```

## Extensions

[extensions][protocol-extensions]
can be queried from `PROTO` values.

### Top-level extensions 
<a id="extensions"></a>

If your `PROTO` value contains extensions, you can query those fields using the
following syntax:

```
<identifier_of_proto_value>.(<package_of_extension>.<path_expression_to_extension_field>)
```

For example, consider this proto definition:

```proto
package some.package;

message Foo {
  optional int32 x = 1;
  extensions 100 to 130;
}

message Point {
  optional int32 x = 1;
  optional int32 y = 2;
}

extend Foo {
  optional int32 bar = 126;
  optional Point point = 127;
}
```

The following sections use this proto definition in a Table, `Test`, which
contains a field, `foo_field` of type `Foo`.

A query that returns the value of the `bar` extension field would resemble the
following:

```sql
SELECT
  foo_field.(some.package.bar)
FROM
  Test;
```

These types of extensions are often referred to as *top-level extensions*.

If you want your statement to return a specific value from a top-level
extension, you would modify it as follows:

```sql
SELECT
  foo_field.(some.package.point).y
FROM
  Test;
```

You can refine your statement to look for a specific value of a top-level
extension as well.

```sql
SELECT
  foo_field.(some.package.bar)
FROM
  Test
WHERE
  foo_field.(some.package.bar) = 5;
```

Note that you can also put back quotes around the components in the extension
path name in case they need to be escaped to avoid collisions with reserved
keywords. For example:

```sql
SELECT
  foo_field.(`some.package`.`bar`).value = 5
FROM
  Test;
```

### Nested extensions

[Nested extensions][nested-extensions]
are also supported. These are protocol buffer extensions that are declared
within the scope of some other protocol message. For example:

```proto
package some.package;

message Baz {
  extend Foo {
    optional Baz foo_ext = 127;
  }
  optional int32 a = 1;
  optional int32 b = 2;
  ...
}
```

To construct queries for nested extensions, you use the same parenthetical
syntax as described in the previous section. To reference a nested extension,
in addition to specifying the package name, you must also specify the name of
the message where the extension is declared. For example:

```sql
SELECT
  foo_field.(some.package.Baz.foo_ext)
FROM
  Test;
```

You can reference a specific field in a nested extension using the same syntax
described in the previous section. For example:

```sql
SELECT
  foo_field.(some.package.Baz.foo_ext).a
FROM
  Test;
```

### Unnesting repeated fields and extensions 
<a id="unnest_repeated_fields"></a>

You can use a [correlated join][correlated-join] to [unnest][unnest-operator]
standard repeated fields or repeated extension fields and return a table with
one row for each instance of the field. A standard repeated field does not
require an explicit `UNNEST`, but a repeated extension field does.

Consider the following protocol buffer:

```proto
syntax = "proto2";

package some.package;

message Example {
  optional int64 record_key = 1;
  repeated int64 repeated_value = 2;
  extensions 3 to 3;
}

message Extension {
  extend Example {
    repeated int64 repeated_extension_value = 3;
  }
}
```

The following query uses a standard repeated field,
`repeated_value`, in a correlated comma `CROSS JOIN` and runs without an
explicit `UNNEST`.

```sql
WITH t AS
  (SELECT
     CAST("""
       record_key: 1
       repeated_value: 1
       repeated_value: 2
       repeated_value: 3
       [some.package.Extension.repeated_extension_value]: 4
       [some.package.Extension.repeated_extension_value]: 5
       [some.package.Extension.repeated_extension_value]: 6"""
     as some.package.Example) as proto_field)
SELECT
  t.proto_field.record_key,
  value
FROM
  t,
  t.proto_field.repeated_value value;
```

The following query uses a repeated extension field,
`repeated_extension_value`, in a correlated comma `CROSS JOIN` and requires an
explicit `UNNEST`.

```sql
WITH t AS
  (SELECT
     CAST("""
       record_key: 1
       repeated_value: 1
       repeated_value: 2
       repeated_value: 3
       [some.package.Extension.repeated_extension_value]: 4
       [some.package.Extension.repeated_extension_value]: 5
       [some.package.Extension.repeated_extension_value]: 6"""
     as some.package.Example) as proto_field)
SELECT
  t.proto_field.record_key,
  value
FROM
  t,
  UNNEST(t.proto_field.(some.package.Extension.repeated_extension_value)) value;
```

<!-- mdlint off(WHITESPACE_LINE_LENGTH) -->

[protocol-buffer-compatibility]: https://developers.google.com/protocol-buffers/docs/proto3#backwards-compatibility

[protocol-buffers-dev-guide]: https://developers.google.com/protocol-buffers

[nested-extensions]: https://developers.google.com/protocol-buffers/docs/proto#nested-extensions

[new-keyword]: #using_new

[explicit-alias]: https://github.com/google/zetasql/blob/master/docs/query-syntax.md#explicit_alias_syntax

[implicit-alias]: https://github.com/google/zetasql/blob/master/docs/query-syntax.md#implicit_aliases

[correlated-join]: https://github.com/google/zetasql/blob/master/docs/query-syntax.md#correlated_join

[unnest-operator]: https://github.com/google/zetasql/blob/master/docs/query-syntax.md#unnest_operator

[new-operator]: https://github.com/google/zetasql/blob/master/docs/operators.md#new_operator

[select-as-typename]: https://github.com/google/zetasql/blob/master/docs/query-syntax.md#select_as_typename

[conversion-rules]: https://github.com/google/zetasql/blob/master/docs/conversion_rules.md

[working-with-arrays]: https://github.com/google/zetasql/blob/master/docs/arrays.md#working_with_arrays

[link_to_safe_cast]: https://github.com/google/zetasql/blob/master/docs/conversion_functions.md#safe_casting

[link_to_expression_subquery]: https://github.com/google/zetasql/blob/master/docs/subqueries.md#expression_subquery_concepts

[proto-extract]: https://github.com/google/zetasql/blob/master/docs/protocol_buffer_functions.md#proto_extract

<!-- mdlint on -->

