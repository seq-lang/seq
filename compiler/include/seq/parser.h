#ifndef SEQ_PARSER_H
#define SEQ_PARSER_H

#include "seq/seq.h"
#include <caml/alloc.h>
#include <caml/callback.h>
#include <caml/mlvalues.h>
#include <iostream>
#include <string>

#ifdef __APPLE__
#include <mach-o/dyld.h>
std::string executable_path(const char *argv0) {
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
    return std::string(argv0);
  }
  return std::string(&buf[0], size);
}
#elif __linux__
#include <unistd.h>
std::string executable_path(const char *argv0) {
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
    return std::string(argv0);
  }
  return std::string(&buf[0], size);
}
#else
std::string executable_path(const char *argv0) { return std::string(argv0); }
#endif

namespace seq {
class SeqModule;

value *init(const char *argv0, bool repl) {
  static value *closure_f = nullptr;
  if (!closure_f) {
    std::string s = executable_path(argv0);
    static char *caml_argv[] = {(char *)s.c_str(), (char *)"--parse", nullptr};
    if (repl)
      caml_argv[1] = nullptr;
    caml_startup(caml_argv);
    closure_f = caml_named_value("parse_c");
  }
  return closure_f;
}

SeqModule *parse(const char *argv0, const std::string &file) {
  value *closure_f = init(argv0, false);
  try {
    auto *module = (SeqModule *)Nativeint_val(
        caml_callback(*closure_f, caml_copy_string(file.c_str())));
    module->setFileName(file);
    return module;
  } catch (exc::SeqException &e) {
    compilationError(e.what(), e.getSrcInfo().file, e.getSrcInfo().line,
                     e.getSrcInfo().col);
    return nullptr;
  }
}

void repl(const char *argv0) {
  static value *closure_f = init(argv0, true);
  Nativeint_val(caml_callback(*closure_f, caml_copy_string("")));
}

void execute(SeqModule *module, std::vector<std::string> args = {},
             std::vector<std::string> libs = {}, bool debug = false) {
  try {
    module->execute(args, libs, debug);
  } catch (exc::SeqException &e) {
    compilationError(e.what(), e.getSrcInfo().file, e.getSrcInfo().line,
                     e.getSrcInfo().col);
  }
}

void compile(SeqModule *module, const std::string &out, bool debug = false) {
  try {
    module->compile(out, debug);
  } catch (exc::SeqException &e) {
    compilationError(e.what(), e.getSrcInfo().file, e.getSrcInfo().line,
                     e.getSrcInfo().col);
  }
}
} // namespace seq

#endif /* SEQ_PARSER_H */
