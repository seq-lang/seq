#pragma once

#include "util/fmt/format.h"
#include "util/fmt/ostream.h"
#include <iostream>
#include <string>
#include <vector>

#include "lang/seq.h"

extern int __level__;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#define DBG(c, ...)                                                            \
  fmt::print("{}" c "\n", string(2 * __level__, ' '), ##__VA_ARGS__)
#pragma clang diagnostic pop

namespace seq {
namespace ast {

template <typename T>
std::string combine(const std::vector<T> &items, std::string delim = " ") {
  std::string s = "";
  for (int i = 0; i < items.size(); i++)
    s += (i ? delim : "") + items[i]->toString();
  return s;
}

std::string escape(std::string s);

std::string executable_path(const char *argv0);

void error(const char *format);
void error(const seq::SrcInfo &p, const char *format);

template <typename... TArgs> void error(const char *format, TArgs &&... args) {
  throw seq::exc::SeqException(fmt::format(format, args...));
}

template <typename... TArgs>
void error(const seq::SrcInfo &p, const char *format, TArgs &&... args) {
  throw seq::exc::SeqException(fmt::format(format, args...), p);
}

std::string getTemporaryVar(const std::string &prefix = "");

template <typename T> T &&fwdSrcInfo(T &&t, const seq::SrcInfo &i) {
  t->setSrcInfo(i);
  return std::forward<T>(t);
}

} // namespace ast
} // namespace seq
