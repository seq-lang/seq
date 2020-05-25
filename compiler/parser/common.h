#pragma once

#include "util/fmt/format.h"
#include "util/fmt/ostream.h"
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "lang/seq.h"

extern int __level__;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#define DBG(c, ...)                                                            \
  fmt::print("{}" c "\n", std::string(2 * __level__, ' '), ##__VA_ARGS__)
#pragma clang diagnostic pop

#define CAST(s, T) dynamic_cast<T *>(s.get())

namespace seq {
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

template <typename... TArgs> void error(const char *format, TArgs &&... args) {
  throw seq::exc::SeqException(fmt::format(format, args...));
}

template <typename T, typename... TArgs>
void error(const T &p, const char *format, TArgs &&... args) {
  throw exc::SeqException(fmt::format(format, args...), p->getSrcInfo());
}

template <typename T, typename... TArgs>
void internalError(const T &p, const char *format, TArgs &&... args) {
  throw exc::SeqException(fmt::format(
      "INTERNAL: {}", fmt::format(format, args...), p->getSrcInfo()));
}

std::string getTemporaryVar(const std::string &prefix = "");

} // namespace ast
} // namespace seq
