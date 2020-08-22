#include <memory>
#include <string>
#include <vector>

#include "lang/seq.h"
#include "parser/ast/codegen.h"
#include "parser/ast/format.h"
#include "parser/ast/transform.h"
#include "parser/ocaml.h"
#include "parser/parser.h"
#include "util/jit.h"

using fmt::format;
using std::make_pair;
using std::make_shared;
using std::pair;
using std::shared_ptr;
using std::string;
using std::vector;

#define FOREIGN extern "C"

// #if 1 || LLVM_VERSION_MAJOR == 6

FOREIGN JitInstance *jit_init() {
  // try {
  //   seq::SeqJIT::init();
  //   auto jit = new JitInstance{
  //       0,
  //       make_shared<seq::ast::Context>(make_shared<seq::ast::ImportCache>(),
  //                                         nullptr, nullptr, nullptr, "")};
  //   jit->context->initJIT();
  //   return jit;
  // } catch (seq::exc::SeqException &e) {
  //   seq::compilationError(e.what(), e.getSrcInfo().file, e.getSrcInfo().line,
  //                         e.getSrcInfo().col);
  return nullptr;
  // }
}

FOREIGN void jit_execute(JitInstance *jit, const char *code) {
  try {
    seq::compilationError("not implemented");
    // auto tv = seq::ast::TransformStmtVisitor().transform(
    //     seq::ast::parse_code("jit", code));
    // seq::ast::CodegenStmtVisitor(*jit->context).transform(tv);
    // jit->context->execJIT();
  } catch (seq::exc::SeqException &e) {
    fmt::print(stderr, "error ({}:{}): {}", e.getSrcInfo().line, e.getSrcInfo().col,
               e.what());
  }
}

FOREIGN char *jit_inspect(JitInstance *jit, const char *file, int line, int col) {
  return nullptr;
}

FOREIGN char *jit_document(JitInstance *jit, const char *id) { return nullptr; }

FOREIGN char *jit_complete(JitInstance *jit, const char *prefix) { return nullptr; }
