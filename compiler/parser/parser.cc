#include <iostream>
#include <string>
#include <vector>

#include "seq/seq.h"
#include "fmt/format.h"
#include "parser/parser.h"
#include "parser/context.h"
#include "parser/transform.h"
#include "parser/codegen.h"
#include "parser/ocaml.h"

using std::string;
using std::vector;

SEQ_FUNC seq::SeqModule *seq::parse(const char *argv0, const char *file) {
  try {
    auto module = new seq::SeqModule();
    auto stmts = parse_file(file);
    auto tv = TransformStmtVisitor();
    for (auto &s: stmts) {
      s->accept(tv);
    }
    auto context = Context(module, file);
    auto cv = CodegenStmtVisitor(context);
    for (auto &s: stmts) {
      fmt::print("> {}", s->to_string());
      s->accept(cv);
    }
    module->setFileName(file);
    return module;
  } catch (seq::exc::SeqException &e) {
    seq::compilationError(e.what(), e.getSrcInfo().file, e.getSrcInfo().line,
                     e.getSrcInfo().col);
    return nullptr;
  }
}

void seq::execute(seq::SeqModule *module, vector<string> args, vector<string> libs, bool debug) {
  try {
    module->execute(args, libs, debug);
  } catch (exc::SeqException &e) {
    compilationError(e.what(), e.getSrcInfo().file, e.getSrcInfo().line,
                     e.getSrcInfo().col);
  }
}

void seq::compile(seq::SeqModule *module, const string &out, bool debug) {
  try {
    module->compile(out, debug);
  } catch (exc::SeqException &e) {
    compilationError(e.what(), e.getSrcInfo().file, e.getSrcInfo().line,
                     e.getSrcInfo().col);
  }
}
