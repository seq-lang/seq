#include <iostream>
#include <string>
#include <vector>

#include "lang/seq.h"
#include "parser/ast/codegen.h"
#include "parser/ast/doc.h"
#include "parser/ast/format.h"
#include "parser/ast/transform.h"
#include "parser/context.h"
#include "parser/ocaml.h"
#include "parser/parser.h"
#include "util/fmt/format.h"

using std::make_shared;
using std::string;
using std::vector;

namespace seq {

void generateDocstr(const std::string &file) {
  DBG("DOC MODE! {}", 1);
  // ast::DocStmtVisitor d;
  // ast::parse_file(file)->accept(d);
}

seq::SeqModule *parse(const std::string &argv0, const std::string &file,
                      bool isCode, bool isTest) {
  try {
    auto stmts = isCode ? ast::parse_code(argv0, file) : ast::parse_file(file);

    auto ctx = ast::TypeContext::getContext(argv0, file);
    auto tv = ast::TransformVisitor(ctx).realizeBlock(stmts.get());
    exit(0);

    auto module = new seq::SeqModule();
    module->setFileName(file);
    // auto cache = make_shared<ast::ImportCache>(argv0);
    // auto stdlib = make_shared<ast::Context>(cache, module->getBlock(),
    // module,
    // nullptr, "");
    // stdlib->loadStdlib(module->getArgVar());
    // auto context =
    // make_shared<ast::Context>(cache,
    // module->getBlock(), module,
    //  nullptr, file);
    // ast::CodegenStmtVisitor(*context).transform(tv);
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
