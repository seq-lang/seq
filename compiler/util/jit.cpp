#include <caml/alloc.h>
#include <caml/callback.h>
#include <caml/mlvalues.h>
#include <stdio.h>

#define FOREIGN extern "C"

#define CAMLDEF(x) \
  value *_jit_##x () { \
    static value *closure_f = nullptr; \
    if (!closure_f) { \
      closure_f = caml_named_value("jit_" #x "_c"); \
    } \
    return closure_f; \
  }

CAMLDEF(init)
CAMLDEF(exec)
CAMLDEF(inspect)
CAMLDEF(document)
CAMLDEF(complete)

FOREIGN void *seq_jit_init() {
  static char *caml_argv[] = {(char *)"main.so", (char *)"--parse", nullptr};
  caml_startup(caml_argv);
  return (void *)Nativeint_val(caml_callback(*(_jit_init()), Val_unit));
}

FOREIGN void seq_jit_exec(void *handle, const char *code) {
  caml_callback2(*(_jit_exec()),
    caml_copy_nativeint((unsigned long)handle),
    caml_copy_string(code));
}

FOREIGN char *seq_jit_inspect(void *handle, const char *cell, int line, int col) {
  value args[4] = {
    caml_copy_nativeint((unsigned long)handle),
    caml_copy_string(cell), Val_int(line), Val_int(col)
  };
  return String_val(caml_callbackN(*(_jit_inspect()), 4, args));
}

FOREIGN char *seq_jit_document(void *handle, const char *id) {
  return String_val(caml_callback2(*(_jit_document()),
    caml_copy_nativeint((unsigned long)handle),
    caml_copy_string(id)));
}

FOREIGN char *seq_jit_complete(void *handle, const char *prefix) {
  return String_val(caml_callback2(*(_jit_complete()),
    caml_copy_nativeint((unsigned long)handle),
    caml_copy_string(prefix)));
}
