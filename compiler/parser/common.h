#pragma once

#include "util/fmt/format.h"
#include "util/fmt/ostream.h"
#include <iostream>
#include <string>
#include <vector>

#include "lang/seq.h"

using std::string;
using std::vector;

#define DBG(c, ...) fmt::print(c "\n", __VA_ARGS__)

template <typename T>
string combine(const vector<T> &items, string delim = " ") {
  string s = "";
  for (int i = 0; i < items.size(); i++)
    s += (i ? delim : "") + items[i]->to_string();
  return s;
}

string escape(string s);

string executable_path(const char *argv0);

void error(const char *format);
void error(const seq::SrcInfo &p, const char *format);

template <typename... TArgs> void error(const char *format, TArgs &&... args) {
  throw seq::exc::SeqException(fmt::format(format, args...));
}

template <typename... TArgs>
void error(const seq::SrcInfo &p, const char *format, TArgs &&... args) {
  throw seq::exc::SeqException(fmt::format(format, args...), p);
}

string getTemporaryVar(const string &prefix = "");

template <typename T> T &&fwdSrcInfo(T &&t, const seq::SrcInfo &i) {
  t->setSrcInfo(i);
  return std::forward<T>(t);
}
