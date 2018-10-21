#include <caml/mlvalues.h>
#include <caml/callback.h>
#include <caml/alloc.h>

#include <cstdio>
#include <string>

struct seq_srcinfo {
   char *file;
   int line;
   int col;
};

void *parse_file(char *fp)
{
   std::fprintf(stderr, "[C] >>> %s\n", fp);

   static value *closure_f = NULL;
   if (closure_f == NULL) {
      closure_f = caml_named_value("parse_c");
   }

   void *ptr = (void*)Nativeint_val(caml_callback(*closure_f, caml_copy_string(fp)));
   return ptr;
}

extern "C" bool exec_module(void*, bool, char**, seq_srcinfo**);

int main(int argc, char **argv)
{
   char *caml_argv[1] = { NULL };
   caml_startup(caml_argv);
   void *ptr = parse_file(argv[1]);
   std::fprintf(stderr, "[C] pointer addr %tx\n", ptr);

   seq_srcinfo *s;
   char *err;
   bool success = exec_module(ptr, false, &err, &s);

   std::fprintf(stderr, "[C] success %d\n", (int)success);
   
   return 0;
}
