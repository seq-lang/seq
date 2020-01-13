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
    auto stmts = parse_file(file);
    auto tv = TransformStmtVisitor::apply(move(stmts));

    auto context = Context(new seq::SeqModule(), file);
    CodegenStmtVisitor::apply(context, tv);
    return context.getModule();
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
