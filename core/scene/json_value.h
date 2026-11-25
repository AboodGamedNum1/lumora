// Minimal JSON document model and parser for package metadata (manifest.json, later scene.json).
// No dependencies. It parses only text that already passed the structural pre-scan in
// core/package/json_limits.h; it still enforces its own depth cap so it is safe on its own.
// Numbers are kept as doubles. Object keys keep insertion order. Duplicate keys are an error.
#pragma once
#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lumora::scene {

class JsonValue {
 public:
  enum class Type { Null, Bool, Number, String, Array, Object };
  using Array = std::vector<JsonValue>;
  using Object = std::vector<std::pair<std::string, JsonValue>>;

  JsonValue() = default;
  static JsonValue null() { return {}; }
  static JsonValue boolean(bool b) { JsonValue v; v.type_ = Type::Bool; v.bool_ = b; return v; }
  static JsonValue number(double d) { JsonValue v; v.type_ = Type::Number; v.number_ = d; return v; }
  static JsonValue string(std::string s) { JsonValue v; v.type_ = Type::String; v.string_ = std::move(s); return v; }
  static JsonValue array(Array a) { JsonValue v; v.type_ = Type::Array; v.array_ = std::move(a); return v; }
  static JsonValue object(Object o) { JsonValue v; v.type_ = Type::Object; v.object_ = std::move(o); return v; }

  Type type() const { return type_; }
  bool is_null() const { return type_ == Type::Null; }
  bool is_bool() const { return type_ == Type::Bool; }
  bool is_number() const { return type_ == Type::Number; }
  bool is_string() const { return type_ == Type::String; }
  bool is_array() const { return type_ == Type::Array; }
  bool is_object() const { return type_ == Type::Object; }

  bool as_bool() const { return bool_; }
  double as_number() const { return number_; }
  const std::string& as_string() const { return string_; }
  const Array& as_array() const { return array_; }
  const Object& as_object() const { return object_; }

  // Object member by key, or nullptr when this is not an object or the key is absent.
  const JsonValue* find(std::string_view key) const;

 private:
  Type type_ = Type::Null;
  bool bool_ = false;
  double number_ = 0;
  std::string string_;
  Array array_;
  Object object_;
};

struct JsonParseResult {
  bool ok = false;
  JsonValue value;
  std::string error;       // empty when ok
  std::size_t offset = 0;  // byte offset of the error
};

// max_depth counts nested arrays and objects (the top-level container is depth 1).
JsonParseResult parse_json(std::string_view text, unsigned max_depth = 32);

}  // namespace lumora::scene
