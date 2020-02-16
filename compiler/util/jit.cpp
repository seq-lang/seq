#include <memory>
#include <string>
#include <vector>

#include "lang/seq.h"
#include "parser/ast/codegen/stmt.h"
#include "parser/ast/format/stmt.h"
#include "parser/ast/transform/stmt.h"
#include "parser/context.h"
#include "parser/ocaml.h"
#include "parser/parser.h"

using fmt::format;
using std::make_pair;
using std::make_shared;
using std::pair;
using std::shared_ptr;
using std::string;
using std::vector;

#define FOREIGN extern "C"

// #if 1 || LLVM_VERSION_MAJOR == 6

struct JitInstance {
  int counter;
  shared_ptr<ast::Context> ctx;

  JitInstance(shared_ptr<ast::Context> c): counter(0), ctx(x) {}
};

FOREIGN JitInstance *jit_init() {
  try {
    seq::SeqJIT::init();
    auto module = new seq::SeqJIT();
    auto fn = new seq::Func("<anon_init>");
    auto cache = ast::ImportCache{string(argv0), nullptr, {}};
    auto stdlib = make_shared<ast::Context>(module, cache);
    auto context = make_shared<ast::Context>(module, cache, file);
    jit->addFunc(fn);
    fflush(stdout);
    return new JitInstance(context);
  } catch (seq::exc::SeqException &e) {
    seq::compilationError(e.what(), e.getSrcInfo().file, e.getSrcInfo().line,
                          e.getSrcInfo().col);
    return nullptr;
  }
}

FOREIGN void jit_execute(JitInstance *jit, const char *code) {
  try {
    auto file = format("<jit_{}>", ctx->counter);
    auto fn = new seq::Func(format("<jit_{}>", ctx->counter));
    ctx->context->addBlock(fn->getBlock(), fn);
    ctx->counter += 1;

    auto stmts = ast::parse_code("", file);
    auto tv = ast::TransformStmtVisitor::apply(move(stmts));
    ast::CodegenStmtVisitor::apply(*context, tv);
    jit->addFunc(fn);
    vector<pair<string, shared_ptr<ContextItem>>> items;
    for (auto &i: ctx->context->top()) {
      if (i->second->isGlobal() && i->second->isInternal()) {
        items.push_back(i);
      }
    }
    ctx->context->popBlock();
    for (auto &i: items) {
      ctx->context->add(i.first, i.second);
    }
  } catch (seq::exc::SeqException &e) {
    seq::compilationMessage("\033[1;31merror:\033[0m",
      e.what(), e.getSrcInfo().file, e.getSrcInfo().line,
      e.getSrcInfo().col);
  }
}

FOREGIN char *jit_inspect(JitInstance *jit, const char *file, int line, int col) {
  return "";
}

FOREGIN char *jit_document(JitInstance *jit, const char *id) {
  return "";
}

FOREGIN char *jit_complete(JitInstance *jit, const char *prefix) {
  return "";
}
