# settings.json parser (core/scene/settings)

Parses and validates `settings.json` as described in `core/scene/settings.h`. This page records the choices
the header leaves open.

## Parsing

- The JSON layer is `parse_json`: invalid JSON, duplicate keys and nesting over 32 levels are `NotJson`.
- `settings_version` and `settings` are required. Every setting needs `id`, `label`, `type` and `default`;
  enum options need `id` and `label`. A missing required field is `MissingField`, a known field of the wrong
  JSON type is `WrongType`, a bad value is `BadValue`. Unknown fields are ignored.
- `DuplicateId`: repeated setting ids, repeated option ids inside one enum, repeated `from_version`.
- `BadReference`: `visible_if.setting` names a setting that is not earlier in the list (self and forward
  references included, and unknown ids).
- `TooMany`: more than 256 settings or more than 64 enum options. An enum with no options is `BadValue`.
- A `visible_if` value must pass `validate_value` for the referenced setting; it is stored snapped to the
  referenced setting's step grid. Exactly one of `equals` and `not_equals` is allowed.
- A default must be inside the range. A default that is in range but off the step grid is kept as written;
  one whose nearest grid point is above `max` is `BadValue`.
- A migration with `from_version >= settings_version` parses but never applies.

## Values

- `validate_value` returns `WrongType` when the value's type differs from the definition (an int value is
  not a float value and the other way round). Out of range, NaN and infinity are `OutOfRange`.
- Step snapping goes to the nearest grid point `min + k*step`, ties round up. If that point is above `max`
  (the range is not a whole number of steps) the result is `NotOnStep`.
- Colour components outside [0,1] or NaN are `BadColor`.
- `UnknownSetting` is returned only for a definition with an empty id. `validate_value` takes a definition,
  so it cannot look an id up itself; `resolve_values` reports unknown stored ids as problems instead.
- `resolve_values`: schema order, one value per setting. Problems are listed for invalid stored values
  first, in schema order, then for unknown stored ids in id order.

## Visibility, migration, reset

- `is_visible` follows the whole `visible_if` chain: a setting is shown only if its rule holds and the setting
  it refers to is shown. A missing value uses the referenced setting's default.
- `migrate_values`: entries apply in ascending `from_version` order, only when `stored_version <=
  from_version < settings_version`. Within an entry, renames run in listed order, then removals. If a value
  already exists under the new id, it wins and the old one is dropped.
- `reset_values`: restores the default of every setting that is not `keep` (and, with
  `only_scene_scope`, is scene scope). Settings missing from the input get a default only when reset applies
  to them. Unknown ids in the input are left alone.

## Header gaps for the project owner

1. `validate_value` has no way to return `UnknownSetting` for a real setting (see above).
2. The parameter of `reset_values` is named `only_scene_scope` but the comment calls it `scope_filter`.
3. "Within half a step" always holds for the nearest grid point, so `NotOnStep` is defined here as "nearest
   grid point is above max".
4. Whether `settings_version` and `settings` are required, whether defaults must lie on the grid, and
   whether a hidden parent hides its children are not stated; the choices above are the ones made.
