#include <memory>
#include <string>
#include <vector>

#define FOREIGN extern "C"

struct JitInstance {
  int counter;
  // TODO: JIT later
  // std::shared_ptr<seq::ast::Context> context;
};

FOREIGN JitInstance *jit_init();
FOREIGN void jit_execute(JitInstance *jit, const char *code);
FOREIGN char *jit_inspect(JitInstance *jit, const char *file, int line, int col);
FOREIGN char *jit_document(JitInstance *jit, const char *id);
FOREIGN char *jit_complete(JitInstance *jit, const char *prefix);
