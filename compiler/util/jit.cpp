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
  shared_ptr<seq::ast::Context> context;

  JitInstance(shared_ptr<seq::ast::Context> c) : counter(0), context(c) {}
};

FOREIGN JitInstance *jit_init() {
  try {
    seq::SeqJIT::init();
    auto fn = new seq::Func();
    fn->setName("<anon_init>");
    auto jit = new seq::SeqJIT();
    auto cache = seq::ast::ImportCache{"", nullptr, {}};
    auto context = make_shared<seq::ast::Context>(jit, fn, cache, "");
    return new JitInstance(context);
  } catch (seq::exc::SeqException &e) {
    seq::compilationError(e.what(), e.getSrcInfo().file, e.getSrcInfo().line,
                          e.getSrcInfo().col);
    return nullptr;
  }
}

FOREIGN void jit_execute(JitInstance *jit, const char *code) {
  try {
    auto file = format("<jit_{}>", jit->counter);
    auto fn = new seq::Func();
    fn->setName(format("<jit_{}>", jit->counter));
    jit->context->addBlock(fn->getBlock(), fn);
    jit->counter += 1;

    auto stmts = seq::ast::parse_code("", file);
    auto tv = seq::ast::TransformStmtVisitor::apply(move(stmts));
    seq::ast::CodegenStmtVisitor::apply(*jit->context, tv);
    jit->context->getJIT()->addFunc(fn);
    auto items = jit->context->top();
    jit->context->popBlock();
    for (auto &i : items) {
      if (i.second->isGlobal()) {
        jit->context->add(i.first, i.second);
      }
    }
  } catch (seq::exc::SeqException &e) {
    fmt::print(stderr, "error ({}:{}): {}", e.getSrcInfo().line,
               e.getSrcInfo().col, e.what());
  }
}

FOREIGN char *jit_inspect(JitInstance *jit, const char *file, int line, int col) {
  return nullptr;
}

FOREIGN char *jit_document(JitInstance *jit, const char *id) {
  return nullptr;
}

FOREIGN char *jit_complete(JitInstance *jit, const char *prefix) {
  return nullptr;
}
