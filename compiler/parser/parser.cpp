#include "util/fmt/format.h"
#include <iostream>
#include <string>
#include <vector>

#include "lang/seq.h"
#include "parser/ast/codegen/stmt.h"
#include "parser/ast/format/stmt.h"
#include "parser/ast/transform/stmt.h"
#include "parser/context.h"
#include "parser/ocaml.h"
#include "parser/parser.h"

using std::make_shared;
using std::string;
using std::vector;

namespace seq {

seq::SeqModule *parse(const std::string &argv0, const std::string &file,
                      bool isCode, bool isTest) {
  try {
    // exit(0); // 263M
    auto stmts = isCode ? ast::parse_code(argv0, file) : ast::parse_file(file);
    // exit(0); // 265M
    auto tv = ast::TransformStmtVisitor::apply(move(stmts));
    // exit(0); // 265M
    auto module = new seq::SeqModule();
    auto cache = ast::ImportCache{string(argv0), nullptr, {}};
    auto stdlib = make_shared<ast::Context>(module, cache, "");
    // exit(0); // 266M
    auto context = make_shared<ast::Context>(module, cache, file);
    ast::CodegenStmtVisitor::apply(*context, tv);
    // exit(0); // 265M
    return module;
  } catch (seq::exc::SeqException &e) {
    if (isTest) {
      throw;
    }
    seq::compilationError(e.what(), e.getSrcInfo().file, e.getSrcInfo().line,
                          e.getSrcInfo().col);
    return nullptr;
  }
}

void execute(seq::SeqModule *module, vector<string> args, vector<string> libs,
             bool debug) {
  config::config().debug = debug;
  try {
    module->execute(args, libs);
  } catch (exc::SeqException &e) {
    compilationError(e.what(), e.getSrcInfo().file, e.getSrcInfo().line,
                     e.getSrcInfo().col);
  }
}

void compile(seq::SeqModule *module, const string &out, bool debug) {
  config::config().debug = debug;
  try {
    module->compile(out);
  } catch (exc::SeqException &e) {
    compilationError(e.what(), e.getSrcInfo().file, e.getSrcInfo().line,
                     e.getSrcInfo().col);
  }
}

} // namespace seq
