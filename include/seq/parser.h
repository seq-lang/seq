#ifndef SEQ_PARSER_H
#define SEQ_PARSER_H

#include <caml/mlvalues.h>
#include <caml/callback.h>
#include <caml/alloc.h>

namespace seq {
	class SeqModule;

	SeqModule *parse(std::string file)
	{
		static value *closure_f = nullptr;
		if (!closure_f) {
			static char *caml_argv[1] = {nullptr};
			caml_startup(caml_argv);
			closure_f = caml_named_value("parse_c");
		}

		return (SeqModule *)Nativeint_val(caml_callback(*closure_f, caml_copy_string(file.c_str())));
	}
}

#endif /* SEQ_PARSER_H */
