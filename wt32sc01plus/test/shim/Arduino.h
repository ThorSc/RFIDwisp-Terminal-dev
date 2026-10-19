#pragma once
// A minimal stand-in for the Arduino core for the host tests (`pio test -e
// native`): just the String class that src/moonraker_box.cpp and ArduinoJson
// use, backed by std::string.
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <string>

class String {
public:
  String() {}
  String(const char *text) : s_(text ? text : "") {}
  String(const String &other) : s_(other.s_) {}
  String(char c) : s_(1, c) {}
  String(int v) : s_(std::to_string(v)) {}
  String(unsigned v) : s_(std::to_string(v)) {}
  String(long v) : s_(std::to_string(v)) {}
  String(unsigned long v) : s_(std::to_string(v)) {}
  String(long long v) : s_(std::to_string(v)) {}
  String(unsigned long long v) : s_(std::to_string(v)) {}

  String &operator=(const String &other) { s_ = other.s_; return *this; }
  String &operator=(const char *text) { s_ = text ? text : ""; return *this; }

  size_t length() const { return s_.size(); }
  const char *c_str() const { return s_.c_str(); }
  bool reserve(size_t n) { s_.reserve(n); return true; }
  size_t concat(const char *o, size_t n) { s_.append(o, n); return n; }
  bool isEmpty() const { return s_.empty(); }
  char operator[](size_t i) const { return i < s_.size() ? s_[i] : 0; }

  String &operator+=(const String &o) { s_ += o.s_; return *this; }
  String &operator+=(const char *o) { if (o) s_ += o; return *this; }
  String &operator+=(char c) { s_ += c; return *this; }
  bool concat(const char *o) { if (o) s_ += o; return true; }

  bool equals(const String &o) const { return s_ == o.s_; }
  bool equalsIgnoreCase(const String &o) const {
    if (s_.size() != o.s_.size()) return false;
    for (size_t i = 0; i < s_.size(); i++) {
      if (tolower((unsigned char)s_[i]) != tolower((unsigned char)o.s_[i])) return false;
    }
    return true;
  }
  bool startsWith(const String &p) const { return s_.compare(0, p.s_.size(), p.s_) == 0 && s_.size() >= p.s_.size(); }
  bool endsWith(const String &p) const {
    return s_.size() >= p.s_.size() && s_.compare(s_.size() - p.s_.size(), p.s_.size(), p.s_) == 0;
  }
  int indexOf(const String &p, size_t from = 0) const {
    size_t i = s_.find(p.s_, from);
    return i == std::string::npos ? -1 : (int)i;
  }
  int indexOf(char c, size_t from = 0) const {
    size_t i = s_.find(c, from);
    return i == std::string::npos ? -1 : (int)i;
  }
  void remove(size_t index) { if (index < s_.size()) s_.erase(index); }
  void remove(size_t index, size_t count) { if (index < s_.size()) s_.erase(index, count); }
  String substring(size_t from) const { return from < s_.size() ? String(s_.substr(from).c_str()) : String(); }
  String substring(size_t from, size_t to) const {
    if (from >= s_.size() || to <= from) return String();
    return String(s_.substr(from, to - from).c_str());
  }
  void trim() {
    size_t b = 0, e = s_.size();
    while (b < e && isspace((unsigned char)s_[b])) b++;
    while (e > b && isspace((unsigned char)s_[e - 1])) e--;
    s_ = s_.substr(b, e - b);
  }
  void toUpperCase() { for (auto &c : s_) c = (char)toupper((unsigned char)c); }
  void toLowerCase() { for (auto &c : s_) c = (char)tolower((unsigned char)c); }
  long toInt() const { return atol(s_.c_str()); }

  bool operator==(const String &o) const { return s_ == o.s_; }
  bool operator==(const char *o) const { return s_ == (o ? o : ""); }
  bool operator!=(const String &o) const { return s_ != o.s_; }
  bool operator!=(const char *o) const { return s_ != (o ? o : ""); }
  bool operator<(const String &o) const { return s_ < o.s_; }

  friend String operator+(const String &a, const String &b) { String r(a); r += b; return r; }
  friend String operator+(const String &a, const char *b) { String r(a); r += b; return r; }
  friend String operator+(const char *a, const String &b) { String r(a); r += b; return r; }
  friend String operator+(const String &a, char b) { String r(a); r += b; return r; }

private:
  std::string s_;
};

