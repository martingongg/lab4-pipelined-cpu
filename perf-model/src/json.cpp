#include "json.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace perf {

namespace {

void append_escaped(std::string& out, const std::string& s) {
  out.push_back('"');
  for (unsigned char c : s) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (c < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c));
          out += buf;
        } else {
          out.push_back(static_cast<char>(c));
        }
        break;
    }
  }
  out.push_back('"');
}

std::string format_double(double v) {
  if (!std::isfinite(v)) {
    return "0.0";
  }
  if (std::abs(v) < 1e-12) {
    return "0.0";
  }
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.6f", v);
  std::string s(buf);
  auto dot = s.find('.');
  if (dot != std::string::npos) {
    while (s.size() > dot + 2 && s.back() == '0') {
      s.pop_back();
    }
  }
  return s;
}

class Parser {
 public:
  explicit Parser(const std::string& text) : text_(text) {}

  JsonValue parse() {
    skip_ws();
    JsonValue val = parse_value();
    skip_ws();
    if (pos_ != text_.size()) {
      fail("unexpected trailing characters");
    }
    return val;
  }

 private:
  void skip_ws() {
    while (pos_ < text_.size()) {
      char c = text_[pos_];
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
        if (c == '\n') {
          ++line_;
          col_ = 1;
        } else {
          ++col_;
        }
        ++pos_;
      } else {
        break;
      }
    }
  }

  char peek() const { return pos_ < text_.size() ? text_[pos_] : '\0'; }

  char get() {
    if (pos_ >= text_.size()) {
      fail("unexpected end of input");
    }
    char c = text_[pos_++];
    if (c == '\n') {
      ++line_;
      col_ = 1;
    } else {
      ++col_;
    }
    return c;
  }

  [[noreturn]] void fail(const std::string& msg) const {
    std::ostringstream oss;
    oss << "JSON parse error at line " << line_ << ", col " << col_ << ": " << msg;
    throw std::runtime_error(oss.str());
  }

  JsonValue parse_value() {
    skip_ws();
    char c = peek();
    if (c == '{') return parse_object();
    if (c == '[') return parse_array();
    if (c == '"') return JsonValue(parse_string());
    if (c == 't') return parse_literal("true", JsonValue(true));
    if (c == 'f') return parse_literal("false", JsonValue(false));
    if (c == 'n') return parse_literal("null", JsonValue(nullptr));
    if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
      return parse_number();
    }
    fail(std::string("unexpected character '") + c + "'");
  }

  JsonValue parse_literal(const char* lit, JsonValue val) {
    for (const char* p = lit; *p != '\0'; ++p) {
      if (get() != *p) {
        fail(std::string("expected '") + lit + "'");
      }
    }
    return val;
  }

  std::string parse_string() {
    if (get() != '"') fail("expected '\"'");
    std::string out;
    while (pos_ < text_.size()) {
      char c = get();
      if (c == '"') return out;
      if (c == '\\') {
        char esc = get();
        switch (esc) {
          case '"': out.push_back('"'); break;
          case '\\': out.push_back('\\'); break;
          case '/': out.push_back('/'); break;
          case 'b': out.push_back('\b'); break;
          case 'f': out.push_back('\f'); break;
          case 'n': out.push_back('\n'); break;
          case 'r': out.push_back('\r'); break;
          case 't': out.push_back('\t'); break;
          case 'u': {
            unsigned code = 0;
            for (int i = 0; i < 4; ++i) {
              char h = get();
              code <<= 4;
              if (h >= '0' && h <= '9') code |= static_cast<unsigned>(h - '0');
              else if (h >= 'a' && h <= 'f') code |= static_cast<unsigned>(h - 'a' + 10);
              else if (h >= 'A' && h <= 'F') code |= static_cast<unsigned>(h - 'A' + 10);
              else fail("invalid hex digit in \\u escape");
            }
            if (code < 0x80) {
              out.push_back(static_cast<char>(code));
            } else if (code < 0x800) {
              out.push_back(static_cast<char>(0xC0 | (code >> 6)));
              out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            } else {
              out.push_back(static_cast<char>(0xE0 | (code >> 12)));
              out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
              out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            }
            break;
          }
          default:
            fail("invalid escape sequence");
        }
      } else if (static_cast<unsigned char>(c) < 0x20) {
        fail("unescaped control character in string");
      } else {
        out.push_back(c);
      }
    }
    fail("unterminated string");
  }

  JsonValue parse_number() {
    size_t start = pos_;
    bool is_neg = false;
    bool is_float = false;
    if (peek() == '-') {
      is_neg = true;
      get();
    }
    if (peek() == '0') {
      get();
    } else if (std::isdigit(static_cast<unsigned char>(peek()))) {
      while (std::isdigit(static_cast<unsigned char>(peek()))) get();
    } else {
      fail("invalid number");
    }
    if (peek() == '.') {
      is_float = true;
      get();
      if (!std::isdigit(static_cast<unsigned char>(peek()))) {
        fail("expected digit after decimal point");
      }
      while (std::isdigit(static_cast<unsigned char>(peek()))) get();
    }
    if (peek() == 'e' || peek() == 'E') {
      is_float = true;
      get();
      if (peek() == '+' || peek() == '-') get();
      if (!std::isdigit(static_cast<unsigned char>(peek()))) {
        fail("expected digit in exponent");
      }
      while (std::isdigit(static_cast<unsigned char>(peek()))) get();
    }
    std::string token = text_.substr(start, pos_ - start);
    if (is_float) {
      return JsonValue(std::stod(token));
    }
    if (is_neg) {
      return JsonValue(static_cast<int64_t>(std::stoll(token)));
    }
    return JsonValue(static_cast<uint64_t>(std::stoull(token)));
  }

  JsonValue parse_array() {
    get();  // '['
    JsonValue arr = JsonValue::make_array();
    skip_ws();
    if (peek() == ']') {
      get();
      return arr;
    }
    while (true) {
      arr.push_back(parse_value());
      skip_ws();
      char c = peek();
      if (c == ',') {
        get();
        continue;
      }
      if (c == ']') {
        get();
        break;
      }
      fail("expected ',' or ']' in array");
    }
    return arr;
  }

  JsonValue parse_object() {
    get();  // '{'
    JsonValue obj = JsonValue::make_object();
    skip_ws();
    if (peek() == '}') {
      get();
      return obj;
    }
    while (true) {
      skip_ws();
      if (peek() != '"') fail("expected string key in object");
      std::string key = parse_string();
      skip_ws();
      if (get() != ':') fail("expected ':' after object key");
      obj.set(key, parse_value());
      skip_ws();
      char c = peek();
      if (c == ',') {
        get();
        continue;
      }
      if (c == '}') {
        get();
        break;
      }
      fail("expected ',' or '}' in object");
    }
    return obj;
  }

  const std::string& text_;
  size_t pos_ = 0;
  size_t line_ = 1;
  size_t col_ = 1;
};

}  // namespace

JsonValue JsonValue::make_array() {
  JsonValue v;
  v.type_ = Type::Array;
  return v;
}

JsonValue JsonValue::make_object() {
  JsonValue v;
  v.type_ = Type::Object;
  return v;
}

bool JsonValue::as_bool() const {
  if (type_ != Type::Bool) throw std::runtime_error("JSON value is not a boolean");
  return bool_val_;
}

int64_t JsonValue::as_int() const {
  if (type_ == Type::Int) return int_val_;
  if (type_ == Type::UInt) return static_cast<int64_t>(uint_val_);
  throw std::runtime_error("JSON value is not an integer");
}

uint64_t JsonValue::as_uint() const {
  if (type_ == Type::UInt) return uint_val_;
  if (type_ == Type::Int && int_val_ >= 0) return static_cast<uint64_t>(int_val_);
  throw std::runtime_error("JSON value is not a non-negative integer");
}

double JsonValue::as_double() const {
  if (type_ == Type::Double) return double_val_;
  if (type_ == Type::Int) return static_cast<double>(int_val_);
  if (type_ == Type::UInt) return static_cast<double>(uint_val_);
  throw std::runtime_error("JSON value is not a number");
}

const std::string& JsonValue::as_string() const {
  if (type_ != Type::String) throw std::runtime_error("JSON value is not a string");
  return str_val_;
}

const JsonValue::ArrayType& JsonValue::as_array() const {
  if (type_ != Type::Array) throw std::runtime_error("JSON value is not an array");
  return arr_val_;
}

const JsonValue::ObjectType& JsonValue::as_object() const {
  if (type_ != Type::Object) throw std::runtime_error("JSON value is not an object");
  return obj_val_;
}

void JsonValue::push_back(JsonValue value) {
  if (type_ != Type::Array) throw std::runtime_error("push_back on non-array JsonValue");
  arr_val_.push_back(std::move(value));
}

void JsonValue::set(const std::string& key, JsonValue value) {
  if (type_ != Type::Object) throw std::runtime_error("set on non-object JsonValue");
  for (auto& kv : obj_val_) {
    if (kv.first == key) {
      kv.second = std::move(value);
      return;
    }
  }
  obj_val_.emplace_back(key, std::move(value));
}

bool JsonValue::contains(const std::string& key) const {
  if (type_ != Type::Object) return false;
  for (const auto& kv : obj_val_) {
    if (kv.first == key) return true;
  }
  return false;
}

const JsonValue& JsonValue::at(const std::string& key) const {
  if (type_ != Type::Object) throw std::runtime_error("at() on non-object JsonValue");
  for (const auto& kv : obj_val_) {
    if (kv.first == key) return kv.second;
  }
  throw std::runtime_error("JSON key not found: " + key);
}

std::string JsonValue::dump(int indent) const {
  std::string out;
  dump_impl(out, indent, 0);
  if (indent >= 0) out.push_back('\n');
  return out;
}

void JsonValue::dump_impl(std::string& out, int indent, int level) const {
  auto add_indent = [&](int lvl) {
    if (indent > 0) {
      out.append(static_cast<size_t>(indent * lvl), ' ');
    }
  };
  switch (type_) {
    case Type::Null:
      out += "null";
      break;
    case Type::Bool:
      out += bool_val_ ? "true" : "false";
      break;
    case Type::Int:
      out += std::to_string(int_val_);
      break;
    case Type::UInt:
      out += std::to_string(uint_val_);
      break;
    case Type::Double:
      out += format_double(double_val_);
      break;
    case Type::String:
      append_escaped(out, str_val_);
      break;
    case Type::Array: {
      if (arr_val_.empty()) {
        out += "[]";
        break;
      }
      out.push_back('[');
      if (indent >= 0) out.push_back('\n');
      for (size_t i = 0; i < arr_val_.size(); ++i) {
        add_indent(level + 1);
        arr_val_[i].dump_impl(out, indent, level + 1);
        if (i + 1 < arr_val_.size()) out.push_back(',');
        if (indent >= 0) out.push_back('\n');
      }
      add_indent(level);
      out.push_back(']');
      break;
    }
    case Type::Object: {
      if (obj_val_.empty()) {
        out += "{}";
        break;
      }
      out.push_back('{');
      if (indent >= 0) out.push_back('\n');
      for (size_t i = 0; i < obj_val_.size(); ++i) {
        add_indent(level + 1);
        append_escaped(out, obj_val_[i].first);
        out += (indent >= 0) ? ": " : ":";
        obj_val_[i].second.dump_impl(out, indent, level + 1);
        if (i + 1 < obj_val_.size()) out.push_back(',');
        if (indent >= 0) out.push_back('\n');
      }
      add_indent(level);
      out.push_back('}');
      break;
    }
  }
}

JsonValue parse_json(const std::string& text) {
  Parser p(text);
  return p.parse();
}

JsonValue parse_json_file(const std::string& path) {
  std::ifstream ifs(path);
  if (!ifs) {
    throw std::runtime_error("failed to open JSON file: " + path);
  }
  std::ostringstream oss;
  oss << ifs.rdbuf();
  return parse_json(oss.str());
}

}  // namespace perf
