#include "json_value.h"

#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>

namespace lumora::scene {

const JsonValue* JsonValue::find(std::string_view key) const {
  if (type_ != Type::Object) return nullptr;
  for (const auto& [k, v] : object_)
    if (k == key) return &v;
  return nullptr;
}

namespace {

class Parser {
 public:
  Parser(std::string_view t, unsigned max_depth) : t_(t), max_depth_(max_depth) {}

  JsonParseResult run() {
    JsonParseResult r;
    skip_ws();
    if (!value(r.value, 0)) return fail(r);
    skip_ws();
    if (pos_ != t_.size()) {
      err_ = "unexpected text after the value";
      return fail(r);
    }
    r.ok = true;
    return r;
  }

 private:
  JsonParseResult fail(JsonParseResult& r) {
    r.ok = false;
    r.value = {};
    r.error = err_.empty() ? "malformed JSON" : err_;
    r.offset = err_pos_;
    return r;
  }
  bool bad(const char* msg) {
    if (err_.empty()) {
      err_ = msg;
      err_pos_ = pos_;
    }
    return false;
  }
  void skip_ws() {
    while (pos_ < t_.size() && (t_[pos_] == ' ' || t_[pos_] == '\t' || t_[pos_] == '\n' || t_[pos_] == '\r')) ++pos_;
  }
  bool literal(std::string_view word) {
    if (t_.substr(pos_, word.size()) != word) return bad("invalid literal");
    pos_ += word.size();
    return true;
  }

  bool value(JsonValue& out, unsigned depth) {
    if (pos_ >= t_.size()) return bad("unexpected end of text");
    switch (t_[pos_]) {
      case '{': return object(out, depth + 1);
      case '[': return array(out, depth + 1);
      case '"': {
        std::string s;
        if (!string(s)) return false;
        out = JsonValue::string(std::move(s));
        return true;
      }
      case 't':
        if (!literal("true")) return false;
        out = JsonValue::boolean(true);
        return true;
      case 'f':
        if (!literal("false")) return false;
        out = JsonValue::boolean(false);
        return true;
      case 'n':
        if (!literal("null")) return false;
        out = JsonValue::null();
        return true;
      default: return number(out);
    }
  }

  bool number(JsonValue& out) {
    const std::size_t start = pos_;
    if (pos_ < t_.size() && t_[pos_] == '-') ++pos_;
    if (pos_ >= t_.size() || t_[pos_] < '0' || t_[pos_] > '9') {
      pos_ = start;
      return bad("invalid value");
    }
    if (t_[pos_] == '0') {
      ++pos_;
    } else {
      while (pos_ < t_.size() && t_[pos_] >= '0' && t_[pos_] <= '9') ++pos_;
    }
    if (pos_ < t_.size() && t_[pos_] == '.') {
      ++pos_;
      const std::size_t d = pos_;
      while (pos_ < t_.size() && t_[pos_] >= '0' && t_[pos_] <= '9') ++pos_;
      if (pos_ == d) return bad("digits expected after the decimal point");
    }
    if (pos_ < t_.size() && (t_[pos_] == 'e' || t_[pos_] == 'E')) {
      ++pos_;
      if (pos_ < t_.size() && (t_[pos_] == '+' || t_[pos_] == '-')) ++pos_;
      const std::size_t d = pos_;
      while (pos_ < t_.size() && t_[pos_] >= '0' && t_[pos_] <= '9') ++pos_;
      if (pos_ == d) return bad("digits expected in the exponent");
    }
    const std::string text(t_.substr(start, pos_ - start));
    errno = 0;
    const double v = std::strtod(text.c_str(), nullptr);
    if (!std::isfinite(v)) {
      pos_ = start;
      return bad("number out of range");
    }
    out = JsonValue::number(v);
    return true;
  }

  static void append_utf8(std::string& s, std::uint32_t cp) {
    if (cp < 0x80) {
      s += static_cast<char>(cp);
    } else if (cp < 0x800) {
      s += static_cast<char>(0xC0 | (cp >> 6));
      s += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
      s += static_cast<char>(0xE0 | (cp >> 12));
      s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
      s += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
      s += static_cast<char>(0xF0 | (cp >> 18));
      s += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
      s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
      s += static_cast<char>(0x80 | (cp & 0x3F));
    }
  }

  bool hex4(std::uint32_t& v) {
    if (pos_ + 4 > t_.size()) return bad("short \\u escape");
    v = 0;
    for (int i = 0; i < 4; ++i) {
      const char c = t_[pos_ + i];
      v <<= 4;
      if (c >= '0' && c <= '9') v |= static_cast<std::uint32_t>(c - '0');
      else if (c >= 'a' && c <= 'f') v |= static_cast<std::uint32_t>(c - 'a' + 10);
      else if (c >= 'A' && c <= 'F') v |= static_cast<std::uint32_t>(c - 'A' + 10);
      else return bad("bad \\u escape");
    }
    pos_ += 4;
    return true;
  }

  bool string(std::string& out) {
    ++pos_;  // opening quote
    while (true) {
      if (pos_ >= t_.size()) return bad("unterminated string");
      const unsigned char c = static_cast<unsigned char>(t_[pos_]);
      if (c == '"') {
        ++pos_;
        return true;
      }
      if (c < 0x20) return bad("control character in string");
      if (c != '\\') {
        out += static_cast<char>(c);
        ++pos_;
        continue;
      }
      ++pos_;
      if (pos_ >= t_.size()) return bad("unterminated escape");
      const char e = t_[pos_++];
      switch (e) {
        case '"': out += '"'; break;
        case '\\': out += '\\'; break;
        case '/': out += '/'; break;
        case 'b': out += '\b'; break;
        case 'f': out += '\f'; break;
        case 'n': out += '\n'; break;
        case 'r': out += '\r'; break;
        case 't': out += '\t'; break;
        case 'u': {
          std::uint32_t cp = 0;
          if (!hex4(cp)) return false;
          if (cp >= 0xD800 && cp <= 0xDBFF) {
            if (t_.substr(pos_, 2) != "\\u") return bad("lone high surrogate");
            pos_ += 2;
            std::uint32_t lo = 0;
            if (!hex4(lo)) return false;
            if (lo < 0xDC00 || lo > 0xDFFF) return bad("bad low surrogate");
            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
          } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
            return bad("lone low surrogate");
          }
          append_utf8(out, cp);
          break;
        }
        default: --pos_; return bad("invalid escape");
      }
    }
  }

  bool array(JsonValue& out, unsigned depth) {
    if (depth > max_depth_) return bad("nesting too deep");
    ++pos_;
    JsonValue::Array items;
    skip_ws();
    if (pos_ < t_.size() && t_[pos_] == ']') {
      ++pos_;
      out = JsonValue::array({});
      return true;
    }
    while (true) {
      skip_ws();
      JsonValue v;
      if (!value(v, depth)) return false;
      items.push_back(std::move(v));
      skip_ws();
      if (pos_ >= t_.size()) return bad("unterminated array");
      if (t_[pos_] == ',') {
        ++pos_;
        continue;
      }
      if (t_[pos_] == ']') {
        ++pos_;
        out = JsonValue::array(std::move(items));
        return true;
      }
      return bad("expected ',' or ']'");
    }
  }

  bool object(JsonValue& out, unsigned depth) {
    if (depth > max_depth_) return bad("nesting too deep");
    ++pos_;
    JsonValue::Object members;
    skip_ws();
    if (pos_ < t_.size() && t_[pos_] == '}') {
      ++pos_;
      out = JsonValue::object({});
      return true;
    }
    while (true) {
      skip_ws();
      if (pos_ >= t_.size() || t_[pos_] != '"') return bad("expected a string key");
      const std::size_t key_pos = pos_;
      std::string key;
      if (!string(key)) return false;
      for (const auto& m : members)
        if (m.first == key) {
          pos_ = key_pos;
          return bad("duplicate key");
        }
      skip_ws();
      if (pos_ >= t_.size() || t_[pos_] != ':') return bad("expected ':'");
      ++pos_;
      skip_ws();
      JsonValue v;
      if (!value(v, depth)) return false;
      members.emplace_back(std::move(key), std::move(v));
      skip_ws();
      if (pos_ >= t_.size()) return bad("unterminated object");
      if (t_[pos_] == ',') {
        ++pos_;
        continue;
      }
      if (t_[pos_] == '}') {
        ++pos_;
        out = JsonValue::object(std::move(members));
        return true;
      }
      return bad("expected ',' or '}'");
    }
  }

  std::string_view t_;
  unsigned max_depth_;
  std::size_t pos_ = 0;
  std::string err_;
  std::size_t err_pos_ = 0;
};

}  // namespace

JsonParseResult parse_json(std::string_view text, unsigned max_depth) { return Parser(text, max_depth).run(); }

}  // namespace lumora::scene
