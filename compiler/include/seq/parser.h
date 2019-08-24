#ifndef SEQ_PARSER_H
#define SEQ_PARSER_H

#include "seq/seq.h"
#include <caml/alloc.h>
#include <caml/callback.h>
#include <caml/mlvalues.h>
#include <iostream>
#include <string>

namespace seq {
class SeqModule;

value *init(bool repl) {
  static value *closure_f = nullptr;
  if (!closure_f) {
    static char *caml_argv[] = {(char *)"main.exe", (char *)"--parse", nullptr};
    if (repl)
      caml_argv[1] = nullptr;
    caml_startup(caml_argv);
    closure_f = caml_named_value("parse_c");
  }
  return closure_f;
}

SeqModule *parse(const std::string &file) {
  value *closure_f = init(false);
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

void repl() {
  static value *closure_f = init(true);
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
