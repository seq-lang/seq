#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "lang/seq.h"

namespace seq {

namespace exc {
class ParserException : public std::runtime_error {
public:
  std::vector<SrcInfo> locations;
  std::vector<std::string> messages;

public:
  ParserException(const std::string &msg, SrcInfo info) noexcept
      : std::runtime_error(msg) {
    messages.push_back(msg);
    locations.push_back(info);
  }
  explicit ParserException(const std::string &msg) noexcept
      : ParserException(msg, {}) {}
  ParserException(const ParserException &e) noexcept : std::runtime_error(e) {}

  void trackRealize(const std::string &msg, SrcInfo i) {
    locations.push_back(i);
    messages.push_back(fmt::format("while realizing {}", msg));
  }
};
} // namespace exc

namespace ast {

struct SrcInfoHash {
  size_t operator()(const seq::SrcInfo &k) const {
    // http://stackoverflow.com/a/1646913/126995
    size_t res = 17;
    res = res * 31 + std::hash<std::string>()(k.file);
    res = res * 31 + std::hash<int>()(k.line);
    res = res * 31 + std::hash<int>()(k.id);
    return res;
  }
};

template <typename T>
std::string join(const T &items, std::string delim = " ") {
  std::string s = "";
  for (int i = 0; i < items.size(); i++)
    s += (i ? delim : "") + items[i];
  return s;
}

std::vector<std::string> split(const std::string &s, char delim);

std::string escape(std::string s);

std::string executable_path(const char *argv0);

void error(const char *format);
void error(const SrcInfo &p, const char *format);

std::string getTemporaryVar(const std::string &prefix = "", char p = '$');

} // namespace ast
} // namespace seq
