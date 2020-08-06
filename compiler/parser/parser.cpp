#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "lang/seq.h"
#include "parser/ast/codegen.h"
#include "parser/ast/codegen_ctx.h"
#include "parser/ast/doc.h"
#include "parser/ast/format.h"
#include "parser/ast/transform.h"
#include "parser/ast/transform_ctx.h"
#include "parser/ocaml.h"
#include "parser/parser.h"
#include "util/fmt/format.h"

using std::make_shared;
using std::string;
using std::vector;

int __level__ = 0;
int __dbg_level__ = 0;

namespace seq {

void generateDocstr(const std::string &file) {
  LOG("DOC MODE! {}", 1);
  // ast::DocStmtVisitor d;
  // ast::parse_file(file)->accept(d);
}

seq::SeqModule *parse(const std::string &argv0, const std::string &file,
                      const string &code, bool isCode, bool isTest, int startLine) {
  try {
    auto d = getenv("SEQ_DEBUG");
    if (d)
      __dbg_level__ = strtol(d, nullptr, 10);

    char abs[PATH_MAX + 1];
    realpath(file.c_str(), abs);

    // fprintf(stderr, "%s\n", fmt::format("{} {} {}", abs, isCode, code).c_str());
    auto ctx = ast::TypeContext::getContext(argv0, abs);
    ast::StmtPtr stmts = nullptr;
    if (!isCode)
      stmts = ast::parseFile(abs);
    else
      stmts = ast::parseCode(abs, code, startLine);
    auto tv = ast::TransformVisitor(ctx).realizeBlock(stmts.get(), false);
    LOG3("--- Done with typecheck ---");

    // FILE *fo = fopen("tmp/out.htm", "w");
    // LOG3("{}", ast::FormatVisitor::format(ctx, tv, false, true));

    seq::SeqModule *module;
    module = new seq::SeqModule();
    module->setFileName(abs);
    auto lctx = ast::LLVMContext::getContext(abs, ctx, module);
    ast::CodegenVisitor(lctx).transform(tv.get());
    LOG3("--- Done with codegen ---");

    // fmt::print(fo, "-------------------------------<hr/>\n");
    return module;
  } catch (seq::exc::SeqException &e) {
    if (isTest) {
      LOG("ERROR: {}", e.what());
    } else {
      seq::compilationError(e.what(), e.getSrcInfo().file, e.getSrcInfo().line,
                            e.getSrcInfo().col);
    }
    exit(EXIT_FAILURE);
    return nullptr;
  } catch (seq::exc::ParserException &e) {
    for (int i = 0; i < e.messages.size(); i++) {
      if (isTest) {
        LOG("ERROR: {}", e.messages[i]);
      } else {
        compilationMessage("\033[1;31merror:\033[0m", e.messages[i],
                           e.locations[i].file, e.locations[i].line,
                           e.locations[i].col);
      }
    }
    exit(EXIT_FAILURE);
    return nullptr;
  }
}

void execute(seq::SeqModule *module, vector<string> args, vector<string> libs,
             bool debug) {
  config::config().debug = debug;
  // try {
  module->execute(args, libs);
  // }
  // catch (exc::SeqException &e) {
  //   compilationError(e.what(), e.getSrcInfo().file, e.getSrcInfo().line,
  //                    e.getSrcInfo().col);
  // }
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
