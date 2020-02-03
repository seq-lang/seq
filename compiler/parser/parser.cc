#include <fmt/format.h>
#include <iostream>
#include <string>
#include <vector>

#include "parser/ast/codegen/stmt.h"
#include "parser/ast/transform/stmt.h"
#include "parser/context.h"
#include "parser/ocaml.h"
#include "parser/parser.h"
#include "seq/seq.h"

using std::make_shared;
using std::string;
using std::vector;

SEQ_FUNC seq::SeqModule *seq::parse(const char *argv0, const char *file,
                                    bool isCode, bool isTest) {
  try {
    auto stmts = isCode ? parse(argv0, file) : parse_file(file);
    auto tv = TransformStmtVisitor::apply(move(stmts));

    auto module = new seq::SeqModule();
    auto cache = ImportCache{string(argv0), nullptr, {}};
    auto stdlib = make_shared<Context>(module, cache);
    auto context = make_shared<Context>(module, cache, file);
    CodegenStmtVisitor::apply(*context, tv);
    // DBG("done with parsing!", "");
    return context->getModule();
  } catch (seq::exc::SeqException &e) {
    if (isTest) {
      throw;
    }
    seq::compilationError(e.what(), e.getSrcInfo().file, e.getSrcInfo().line,
                          e.getSrcInfo().col);
    return nullptr;
  }
}

void seq::execute(seq::SeqModule *module, vector<string> args,
                  vector<string> libs, bool debug) {
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
