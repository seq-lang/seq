#ifndef SEQ_PARSER_H
#define SEQ_PARSER_H

#include <iostream>
#include <string>
#include <caml/mlvalues.h>
#include <caml/callback.h>
#include <caml/alloc.h>

namespace seq {
	class SeqModule;

	void compilationError(const std::string& msg, const std::string& file, int line, int col)
	{
		std::cerr << file.substr(file.rfind('/') + 1) << ":" << line << ":" << col << ": error: " << msg << std::endl;
		exit(EXIT_FAILURE);
	}

	SeqModule *parse(const std::string& file)
	{
		try {
			static value *closure_f = nullptr;
			if (!closure_f) {
				static char *caml_argv[1] = {nullptr};
				caml_startup(caml_argv);
				closure_f = caml_named_value("parse_c");
			}

			auto *module = (SeqModule *)Nativeint_val(caml_callback(*closure_f, caml_copy_string(file.c_str())));
			module->setFileName(file);
			return module;
		} catch (exc::SeqException& e) {
			compilationError(e.what(), e.getSrcInfo().file, e.getSrcInfo().line, e.getSrcInfo().col);
			return nullptr;
		}
	}

	void execute(SeqModule *module, std::vector<std::string> args={}, std::vector<std::string> libs={}, bool debug=false)
	{
		try {
			module->execute(args, libs, debug);
		} catch (exc::SeqException& e) {
			compilationError(e.what(), e.getSrcInfo().file, e.getSrcInfo().line, e.getSrcInfo().col);
		}
	}

	void compile(SeqModule *module, const std::string& out, bool debug=false)
	{
		try {
			module->compile(out, debug);
		} catch (exc::SeqException& e) {
			compilationError(e.what(), e.getSrcInfo().file, e.getSrcInfo().line, e.getSrcInfo().col);
		}
	}
}

extern "C" void caml_error_callback(char *msg, int line, int col, char *file)
{
	seq::compilationError(std::string(msg), std::string(file), line, col);
}

#endif /* SEQ_PARSER_H */
