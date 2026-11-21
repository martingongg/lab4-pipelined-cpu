#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace perf {

// Minimal, zero-dependency RFC 8259 JSON value, parser, and deterministic emitter.
// Object keys preserve insertion order so emitted golden JSON is byte-for-byte stable.
class JsonValue {
 public:
  enum class Type { Null, Bool, Int, UInt, Double, String, Array, Object };

  using ArrayType = std::vector<JsonValue>;
  using ObjectType = std::vector<std::pair<std::string, JsonValue>>;

  JsonValue() : type_(Type::Null) {}
  JsonValue(std::nullptr_t) : type_(Type::Null) {}
  JsonValue(bool v) : type_(Type::Bool), bool_val_(v) {}
  JsonValue(int v) : type_(Type::Int), int_val_(v) {}
  JsonValue(int64_t v) : type_(Type::Int), int_val_(v) {}
  JsonValue(uint32_t v) : type_(Type::UInt), uint_val_(v) {}
  JsonValue(uint64_t v) : type_(Type::UInt), uint_val_(v) {}
  JsonValue(double v) : type_(Type::Double), double_val_(v) {}
  JsonValue(const char* v) : type_(Type::String), str_val_(v) {}
  JsonValue(std::string v) : type_(Type::String), str_val_(std::move(v)) {}

  static JsonValue make_array();
  static JsonValue make_object();

  Type type() const { return type_; }
  bool is_null() const { return type_ == Type::Null; }
  bool is_bool() const { return type_ == Type::Bool; }
  bool is_number() const {
    return type_ == Type::Int || type_ == Type::UInt || type_ == Type::Double;
  }
  bool is_string() const { return type_ == Type::String; }
  bool is_array() const { return type_ == Type::Array; }
  bool is_object() const { return type_ == Type::Object; }

  bool as_bool() const;
  int64_t as_int() const;
  uint64_t as_uint() const;
  double as_double() const;
  const std::string& as_string() const;
  const ArrayType& as_array() const;
  const ObjectType& as_object() const;

  void push_back(JsonValue value);
  void set(const std::string& key, JsonValue value);
  bool contains(const std::string& key) const;
  const JsonValue& at(const std::string& key) const;

  std::string dump(int indent = 2) const;

 private:
  void dump_impl(std::string& out, int indent, int level) const;

  Type type_ = Type::Null;
  bool bool_val_ = false;
  int64_t int_val_ = 0;
  uint64_t uint_val_ = 0;
  double double_val_ = 0.0;
  std::string str_val_;
  ArrayType arr_val_;
  ObjectType obj_val_;
};

JsonValue parse_json(const std::string& text);
JsonValue parse_json_file(const std::string& path);

}  // namespace perf
