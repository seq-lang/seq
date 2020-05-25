#include "util/fmt/format.h"
#include <libgen.h>
#include <string>

#include "lang/seq.h"
#include "parser/common.h"

using std::string;
using std::vector;

namespace seq {

std::ostream &operator<<(std::ostream &out, const ::seq::SrcInfo &c) {
  char buf[PATH_MAX + 1];
  strncpy(buf, c.file.c_str(), PATH_MAX);
  auto f = basename(buf);
  out << f << ":" << c.line << ":" << c.col;
  return out;
}

namespace ast {

int tmpVarCounter = 0;
string getTemporaryVar(const string &prefix) {
  return fmt::format("$_{}_{}", prefix, ++tmpVarCounter);
}

vector<string> split(const string &s, char delim) {
  vector<string> items;
  string item;
  std::istringstream iss(s);
  while (std::getline(iss, item, delim))
    items.push_back(item);
  return items;
}

string escape(string s) {
  string r;
  for (unsigned char c : s) {
    switch (c) {
    case '\a':
      r += "\\a";
      break;
    case '\b':
      r += "\\b";
      break;
    case '\f':
      r += "\\f";
      break;
    case '\n':
      r += "\\n";
      break;
    case '\r':
      r += "\\r";
      break;
    case '\t':
      r += "\\t";
      break;
    case '\v':
      r += "\\v";
      break;
    case '\'':
      r += "\\'";
      break;
    case '\\':
      r += "\\\\";
      break;
    default:
      if (c < 32 || c >= 127) {
        r += fmt::format("\\x{:x}", c);
      } else {
        r += c;
      }
    }
  }
  return r;
}

void error(const char *format) { throw ::seq::exc::SeqException(format); }

void error(const ::seq::SrcInfo &p, const char *format) {
  throw ::seq::exc::SeqException(format, p);
}

#ifdef __APPLE__
#include <mach-o/dyld.h>

string executable_path(const char *argv0) {
  typedef std::vector<char> char_vector;
  char_vector buf(1024, 0);
  uint32_t size = static_cast<uint32_t>(buf.size());
  bool havePath = false;
  bool shouldContinue = true;
  do {
    int result = _NSGetExecutablePath(&buf[0], &size);
    if (result == -1) {
      buf.resize(size + 1);
      std::fill(std::begin(buf), std::end(buf), 0);
    } else {
      shouldContinue = false;
      if (buf.at(0) != 0) {
        havePath = true;
      }
    }
  } while (shouldContinue);
  if (!havePath) {
    return string(argv0);
  }
  return string(&buf[0], size);
}
#elif __linux__
#include <unistd.h>

string executable_path(const char *argv0) {
  typedef std::vector<char> char_vector;
  typedef std::vector<char>::size_type size_type;
  char_vector buf(1024, 0);
  size_type size = buf.size();
  bool havePath = false;
  bool shouldContinue = true;
  do {
    ssize_t result = readlink("/proc/self/exe", &buf[0], size);
    if (result < 0) {
      shouldContinue = false;
    } else if (static_cast<size_type>(result) < size) {
      havePath = true;
      shouldContinue = false;
      size = result;
    } else {
      size *= 2;
      buf.resize(size);
      std::fill(std::begin(buf), std::end(buf), 0);
    }
  } while (shouldContinue);
  if (!havePath) {
    return string(argv0);
  }
  return string(&buf[0], size);
}
#else
string executable_path(const char *argv0) { return string(argv0); }
#endif

} // namespace ast
} // namespace seq
