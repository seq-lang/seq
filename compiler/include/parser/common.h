#pragma once

#include <fmt/format.h>
#include <fmt/ostream.h>
#include <string>
#include <vector>
#include <iostream>

#include "seq/seq.h"

using std::string;
using std::vector;

template <typename T> string combine(const vector<T> &items, string delim = " ") {
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
  seq::compilationError(fmt::format(format, args...));
}

template <typename... TArgs>
void error(const seq::SrcInfo &p, const char *format, TArgs &&... args) {
  seq::compilationError(fmt::format(format, args...), p.file, p.line, p.col);
}

string getTemporaryVar(const string &prefix = "");
