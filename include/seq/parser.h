#ifndef SEQ_PARSER_H
#define SEQ_PARSER_H

#include <iostream>
#include <string>
#include <vector>
#include <stack>
#include <map>
#include <cassert>
#include "seq/seq.h"

namespace seq {

	struct SeqEntity {
		enum {
			EMPTY = 0,
			INT,
			FLOAT,
			BOOL,
			NAME,
			PIPELINE,
			VAR,
			FUNC,
			TYPE,
			MODULE
		} type = EMPTY;

		union U {
			U() : ival(0) {}
			U(seq_int_t ival) : ival(ival) {}
			U(double fval) : fval(fval) {}
			U(bool bval) : bval(bval) {}
			U(const char * name) : name(name) {}
			U(Pipeline pipeline) : pipeline(pipeline) {}
			U(Var *var) : var(var) {}
			U(Func *func) : func(func) {}
			U(types::Type *type) : type(type) {}
			U(SeqModule *module) : module(module) {}

			seq_int_t ival;
			double fval;
			bool bval;
			const char *name;
			Pipeline pipeline;
			Var *var;
			Func *func;
			types::Type *type;
			SeqModule *module;
		} value;

		SeqEntity() : type(EMPTY), value() {}
		SeqEntity(seq_int_t ival) : type(SeqEntity::INT), value(ival) {}
		SeqEntity(double fval) : type(SeqEntity::FLOAT), value(fval) {}
		SeqEntity(bool bval) : type(SeqEntity::BOOL), value(bval) {}
		SeqEntity(const char * name) : type(SeqEntity::NAME), value(name) {}
		SeqEntity(Pipeline pipeline) : type(SeqEntity::PIPELINE), value(pipeline) {}
		SeqEntity(Var *var) : type(SeqEntity::VAR), value(var) {}
		SeqEntity(Func *func) : type(SeqEntity::FUNC), value(func) {}
		SeqEntity(types::Type *type) : type(SeqEntity::TYPE), value(type) {}
		SeqEntity(SeqModule *module) : type(SeqEntity::MODULE), value(module) {}
	};

	const std::map<char, int> TYPE_MAP = {{'x', SeqEntity::EMPTY},
	                                      {'i', SeqEntity::INT},
	                                      {'f', SeqEntity::FLOAT},
	                                      {'b', SeqEntity::BOOL},
	                                      {'s', SeqEntity::NAME},
	                                      {'p', SeqEntity::PIPELINE},
	                                      {'v', SeqEntity::VAR},
	                                      {'f', SeqEntity::FUNC},
	                                      {'t', SeqEntity::TYPE},
	                                      {'m', SeqEntity::MODULE}};

	class ParseState {
	private:
		std::vector<std::map<std::string, SeqEntity>> symbols;
		std::stack<std::vector<SeqEntity>> results;
		std::vector<SeqEntity> contexts;
		std::map<std::string, SeqModule *> modules;

	public:
		std::vector<SeqEntity> get(const std::string& types, bool multi=false, bool pop=true)
		{
			assert(!types.empty() && !results.empty());
			std::vector<SeqEntity> result = results.top();

			if (!multi && result.size() != types.length())
				throw exc::SeqException(
				  "too many arguments: got " + std::to_string(result.size()) + " but expected " + std::to_string(types.length()));

			for (int i = 0; i < result.size(); i++) {
				const char token = multi ? types[0] : types[i];

				if (token == '*')
					continue;

				const auto type = TYPE_MAP.find(token);
				assert(type != TYPE_MAP.end());

				if (result[i].type != type->second)
					throw exc::SeqException("unexpected expression");
			}

			if (pop)
				results.pop();

			return result;
		}

		void add(SeqEntity ent)
		{
			assert(!results.empty());
			results.top().push_back(ent);
		}

		SeqEntity& top()
		{
			assert(!results.empty() && !results.top().empty());
			return results.top().back();
		}

		void push()
		{
			results.push({});
		}

		void pop()
		{
			assert(!results.empty());
			results.pop();
		}

		void scope()
		{
			symbols.push_back({});
		}

		void unscope()
		{
			assert(!symbols.empty());
			symbols.pop_back();
		}

		static void symadd(const char *name, SeqEntity ent, std::map<std::string, SeqEntity>& syms)
		{
			if (strcmp(name, "_") == 0)
				throw exc::SeqException("symbol '_' is reserved and cannot be used");

			if (syms.find(name) != syms.end())
				throw exc::SeqException("duplicate symbol '" + std::string(name) + "'");

			syms.insert({name, ent});
		}

		void sym(const char *name, SeqEntity ent)
		{
			assert(!symbols.empty());
			symadd(name, ent, symbols.back());
		}

		void symparent(const char *name, SeqEntity ent)
		{
			assert(symbols.size() >= 2);
			symadd(name, ent, symbols[symbols.size() - 2]);
		}

		SeqEntity lookup(const char *name)
		{
			if (strcmp(name, "_") == 0)
				return &_;  // this is our special variable for referring to prev outputs

			assert(!symbols.empty());

			auto iter = symbols.back().find(name);

			if (iter == symbols.back().end())
				throw exc::SeqException("undefined reference to '" + std::string(name) + "'");

			return iter->second;
		}

		void enter(SeqEntity context)
		{
			contexts.push_back(context);
		}

		void exit()
		{
			assert(!contexts.empty());
			contexts.pop_back();
		}

		SeqEntity context()
		{
			assert(!contexts.empty());
			return contexts.back();
		}

		SeqEntity base()
		{
			assert(!contexts.empty());
			for (int i = (int)contexts.size() - 1; i >= 0; i--) {
				SeqEntity ent = contexts[i];
				if (ent.type == SeqEntity::MODULE || ent.type == SeqEntity::FUNC)
					return ent;
			}
			assert(0);
		}

		void addmod(std::string name, SeqModule *module)
		{
			if (modules.find(name) != modules.end())
				throw exc::SeqException("duplicate module name '" + name + "'");

			modules.insert({name, module});
		}

		SeqModule& module(std::string name)
		{
			auto iter = modules.find(name);
			if (iter == modules.end())
				throw exc::SeqException("cannot find module '" + name + "'");

			return *iter->second;
		}

		ParseState() : symbols(), results(), contexts(), modules()
		{
		}
	};

	ParseState parse(std::string input);

}

#endif /* SEQ_PARSER_H */
