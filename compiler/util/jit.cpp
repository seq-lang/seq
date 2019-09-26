#include <caml/alloc.h>
#include <caml/callback.h>
#include <caml/mlvalues.h>
#include <stdio.h>

#define FOREIGN extern "C"

value *caml_jit_init_f() {
  static value *closure_f = nullptr;
  if (!closure_f) {
    closure_f = caml_named_value("jit_init_c");
  }
  return closure_f;
}

value *caml_jit_exec_f() {
  static value *closure_f = nullptr;
  if (!closure_f) {
    closure_f = caml_named_value("jit_exec_c");
  }
  return closure_f;
}

FOREIGN void *caml_jit_init() {
  static char *caml_argv[] = {(char *)"main.so", (char *)"--parse", nullptr};
  caml_startup(caml_argv);

  value *closure_f = caml_jit_init_f();
  void *res = (void *)Nativeint_val(caml_callback(*closure_f, Val_unit));
  // fprintf(stderr, "[link] got %llx\n", res);
  return res;
}

FOREIGN void caml_jit_exec(void *handle, const char *code) {
  value *closure_f = caml_jit_exec_f();
  // fprintf(stderr, "[link] ex.got %llx\n", handle);
  caml_callback2(*closure_f, caml_copy_nativeint((unsigned long)handle),
                 caml_copy_string(code));
}
