#include <iostream>
#include <string>
#include <vector>
#include <stack>
#include <queue>
#include <deque>
#include <set>
#include <map>
#include <cassert>
#include <tao/pegtl/analyze.hpp>
#include "seq/seq.h"
#include "grammar.h"

using namespace seq;

struct SeqEntity {
	enum {
		EMPTY = 0,
		INT,
		FLOAT,
		BOOL,
		NAME,
		VAR,
		FUNC,
		TYPE,
		MODULE,
		EXPR,
		PATTERN,
		STMT,
		OP
	} type = EMPTY;

	union U {
		U() : ival(0) {}
		U(seq_int_t ival) : ival(ival) {}
		U(double fval) : fval(fval) {}
		U(bool bval) : bval(bval) {}
		U(std::string name) : name(std::move(name)) {}
		U(Var *var) : var(var) {}
		U(Func *func) : func(func) {}
		U(types::Type *type) : type(type) {}
		U(SeqModule *module) : module(module) {}
		U(Expr *expr) : expr(expr) {}
		U(Pattern *pattern) : pattern(pattern) {}
		U(Stmt *stmt) : stmt(stmt) {}
		U(Op op) : op(std::move(op)) {}
		~U() {}

		seq_int_t ival;
		double fval;
		bool bval;
		std::string name;
		Var *var;
		Func *func;
		types::Type *type;
		SeqModule *module;
		Expr *expr;
		Pattern *pattern;
		Stmt *stmt;
		Op op;
	} value;

	SeqEntity() : type(EMPTY), value() {}
	SeqEntity(seq_int_t ival) : type(SeqEntity::INT), value(ival) {}
	SeqEntity(double fval) : type(SeqEntity::FLOAT), value(fval) {}
	SeqEntity(bool bval) : type(SeqEntity::BOOL), value(bval) {}
	SeqEntity(std::string name) : type(SeqEntity::NAME) { new (&value.name) std::string(std::move(name)); }
	SeqEntity(Var *var) : type(SeqEntity::VAR), value(var) {}
	SeqEntity(Func *func) : type(SeqEntity::FUNC), value(func) {}
	SeqEntity(types::Type *type) : type(SeqEntity::TYPE), value(type) {}
	SeqEntity(SeqModule *module) : type(SeqEntity::MODULE), value(module) {}
	SeqEntity(Expr *expr) : type(SeqEntity::EXPR), value(expr) {}
	SeqEntity(Pattern *pattern) : type(SeqEntity::PATTERN), value(pattern) {}
	SeqEntity(Stmt *stmt) : type(SeqEntity::STMT), value(stmt) {}
	SeqEntity(Op op) : type(SeqEntity::OP) { new (&value.op) Op(std::move(op)); }

	SeqEntity& operator=(const SeqEntity& ent)
	{
		type = ent.type;
		switch (type) {
			case SeqEntity::EMPTY:
				break;
			case SeqEntity::INT:
				value.ival = ent.value.ival;
				break;
			case SeqEntity::FLOAT:
				value.fval = ent.value.fval;
				break;
			case SeqEntity::BOOL:
				value.bval = ent.value.bval;
				break;
			case SeqEntity::NAME:
				new (&value.name) std::string(ent.value.name);
				break;
			case SeqEntity::VAR:
				value.var = ent.value.var;
				break;
			case SeqEntity::FUNC:
				value.func = ent.value.func;
				break;
			case SeqEntity::TYPE:
				value.type = ent.value.type;
				break;
			case SeqEntity::MODULE:
				value.module = ent.value.module;
				break;
			case SeqEntity::EXPR:
				value.expr = ent.value.expr;
				break;
			case SeqEntity::PATTERN:
				value.pattern = ent.value.pattern;
				break;
			case SeqEntity::STMT:
				value.stmt = ent.value.stmt;
				break;
			case SeqEntity::OP:
				new (&value.op) Op(ent.value.op);
				break;
		}

		return *this;
	}

	SeqEntity(const SeqEntity& ent)
	{
		*this = ent;
	}

	~SeqEntity()=default;
};

std::ostream& operator<<(std::ostream& os, const SeqEntity& ent);

const std::map<char, int> TYPE_MAP = {{'x', SeqEntity::EMPTY},
                                      {'i', SeqEntity::INT},
                                      {'f', SeqEntity::FLOAT},
                                      {'b', SeqEntity::BOOL},
                                      {'s', SeqEntity::NAME},
                                      {'f', SeqEntity::FUNC},
                                      {'t', SeqEntity::TYPE},
                                      {'m', SeqEntity::MODULE},
                                      {'e', SeqEntity::EXPR},
                                      {'q', SeqEntity::PATTERN}};

class ParseState {
	typedef std::map<std::string, SeqEntity> SymTab;
private:
	std::vector<SymTab> symbols;
	std::stack<std::vector<SeqEntity>> results;
	std::vector<SeqEntity> contexts;
	std::vector<Block *> blocks;
	SeqModule *module;
public:
	std::vector<SeqEntity> get(const std::string& types, bool multi=false, bool pop=true)
	{
		assert(!types.empty() && !results.empty());
		std::vector<SeqEntity> result = results.top();

		if (!multi && result.size() != types.length())
			throw exc::SeqException(
			  "entity count mismatch: got " + std::to_string(result.size()) + " but expected " + std::to_string(types.length()));

		for (unsigned i = 0; i < result.size(); i++) {
			const char token = multi ? types[0] : types[i];

			if (token == '*')
				continue;

			const auto type = TYPE_MAP.find(token);
			assert(type != TYPE_MAP.end());

			if (result[i].type != type->second)
				throw exc::SeqException("unexpected entity type");
		}

		if (pop)
			results.pop();

		return result;
	}

	void add(const SeqEntity& ent)
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
		symbols.emplace_back();
	}

	void unscope()
	{
		assert(!symbols.empty());
		symbols.pop_back();
	}

	static bool isBarrier(std::map<std::string, SeqEntity>& syms)
	{
		return syms.find("*") != syms.end();
	}

	static void symadd(std::string name,
	                   SeqEntity ent,
	                   std::map<std::string, SeqEntity>& syms,
	                   bool override)
	{
		if (syms.find(name) != syms.end()) {
			if (override)
				syms.find(name)->second = ent;
			else
				throw exc::SeqException("duplicate symbol '" + std::string(name) + "'");
		}

		syms.insert({name, ent});
	}

	void sym(std::string name, const SeqEntity& ent, bool override=false)
	{
		assert(!symbols.empty());
		symadd(std::move(name), ent, symbols.back(), override);
	}

	/*
	 * We don't want functions to access non-global variables defined outside of them;
	 * this method prevents that.
	 */
	void scopeBarrier()
	{
		sym("*", {});
	}

	void symparent(std::string name, const SeqEntity& ent, bool override=false)
	{
		assert(symbols.size() >= 2);
		symadd(std::move(name), ent, symbols[symbols.size() - 2], override);
	}

	static SeqEntity lookupInTable(const std::string& name, SymTab symtab)
	{
		auto iter = symtab.find(name);

		if (iter == symtab.end())
			return {};

		return iter->second;
	}

	static bool disallowBrokenBarriers(const SeqEntity& ent)
	{
		return ent.type == SeqEntity::VAR && !ent.value.var->isGlobal();
	}

	SeqEntity lookup(const std::string& name)
	{
		bool barrierBroken = false;
		SeqEntity found;

		for (auto it = symbols.rbegin(); it != symbols.rend(); ++it) {
			SeqEntity ent = lookupInTable(name, *it);
			if (ent.type != SeqEntity::EMPTY) {
				found = ent;
				break;
			}

			if (isBarrier(*it))
				barrierBroken = true;
		}

		if (found.type == SeqEntity::EMPTY || (barrierBroken && disallowBrokenBarriers(found)))
			throw exc::SeqException("undefined reference to '" + std::string(name) + "'");

		return found;
	}

	void addMethods(types::RefType *ref)
	{
		for (auto const& e : symbols.back()) {
			switch (e.second.type) {
				case SeqEntity::EMPTY:
				case SeqEntity::TYPE:
					break;
				case SeqEntity::FUNC:
					ref->addMethod(e.first, e.second.value.func);
					break;
				default:
					throw exc::SeqException("non-function entity present in class definition");
			}
		}
	}

	void enter(Block *block)
	{
		blocks.push_back(block);
	}

	void exit()
	{
		assert(!blocks.empty());
		blocks.pop_back();
	}

	SeqEntity context()
	{
		assert(!contexts.empty());
		return contexts.back();
	}

	void context(const SeqEntity& ent)
	{
		contexts.push_back(ent);
	}

	void uncontext()
	{
		assert(!contexts.empty());
		contexts.pop_back();
	}

	void stmt(Stmt *stmt)
	{
		assert(!blocks.empty() && blocks.back());
		blocks.back()->add(stmt);
	}

	BaseFunc *base()
	{
		assert(!contexts.empty());

		for (int i = (int)contexts.size() - 1; i >= 0; i--) {
			SeqEntity ent = contexts[i];
			switch (ent.type) {
				case SeqEntity::FUNC:
					return ent.value.func;
				case SeqEntity::MODULE:
					return ent.value.module;
				default:
					break;
			}
		}

		assert(0);
		return nullptr;
	}

	void setModule(SeqModule *module)
	{
		assert(this->module == nullptr);
		this->module = module;
	}

	SeqModule& getModule()
	{
		assert(this->module != nullptr);
		return *this->module;
	}

	ParseState() : symbols(), results(), contexts(), blocks(), module(nullptr)
	{
	}
};

std::ostream& operator<<(std::ostream& os, const SeqEntity& ent)
{
	switch (ent.type) {
		case SeqEntity::EMPTY:
			os << "(empty)";
			break;
		case SeqEntity::INT:
			os << ent.value.ival;
			break;
		case SeqEntity::FLOAT:
			os << ent.value.fval;
			break;
		case SeqEntity::BOOL:
			os << ent.value.bval;
			break;
		case SeqEntity::NAME:
			os << ent.value.name;
			break;
		case SeqEntity::VAR:
			os << "(var)";
			break;
		case SeqEntity::FUNC:
			os << "(func)";
			break;
		case SeqEntity::TYPE:
			os << ent.value.type->getName();
			break;
		case SeqEntity::MODULE:
			os << "(module)";
			break;
		case SeqEntity::EXPR:
			os << "(expr)";
			break;
		case SeqEntity::OP:
			os << (ent.value.op.binary ? "" : "u") << ent.value.op.symbol;
			break;
		default:
			assert(0);
	}

	return os;
}

/*
 * Actions
 */
template<typename Rule>
struct action : pegtl::nothing<Rule> {};

template<>
struct action<pos_int_dec> {
	template<typename Input>
	static void apply(const Input& in, ParseState& state)
	{
		const seq_int_t n = std::stol(in.string(), nullptr, 10);
		assert(n >= 0);
		state.add(n);
	}
};

template<>
struct action<neg_int_dec> {
	template<typename Input>
	static void apply(const Input& in, ParseState& state)
	{
		const seq_int_t n = std::stol(in.string(), nullptr, 10);
		assert(n <= 0);
		state.add(n);
	}
};

template<>
struct action<pos_int_hex> {
	template<typename Input>
	static void apply(const Input& in, ParseState& state)
	{
		const seq_int_t n = std::stol(in.string(), nullptr, 16);
		assert(n >= 0);
		state.add(n);
	}
};

template<>
struct action<neg_int_hex> {
	template<typename Input>
	static void apply(const Input& in, ParseState& state)
	{
		const seq_int_t n = std::stol(in.string(), nullptr, 16);
		assert(n <= 0);
		state.add(n);
	}
};

template<>
struct action<pos_int_oct> {
	template<typename Input>
	static void apply(const Input& in, ParseState& state)
	{
		const seq_int_t n = std::stol(in.string(), nullptr, 8);
		assert(n >= 0);
		state.add(n);
	}
};

template<>
struct action<neg_int_oct> {
	template<typename Input>
	static void apply(const Input& in, ParseState& state)
	{
		const seq_int_t n = std::stol(in.string(), nullptr, 8);
		assert(n <= 0);
		state.add(n);
	}
};

template<>
struct action<natural> {
	template<typename Input>
	static void apply(const Input& in, ParseState& state)
	{
		const seq_int_t n = std::stol(in.string(), nullptr, 10);
		assert(n >= 1);
		state.add(n);
	}
};

template<>
struct action<numeral> {
	template<typename Input>
	static void apply(const Input& in, ParseState& state)
	{
		const double f = std::stod(in.string());
		state.add(f);
	}
};

template<>
struct action<name> {
	template<typename Input>
	static void apply(const Input& in, ParseState& state)
	{
		state.add(in.string());
	}
};

static std::string unescape(const std::string& s)
{
	std::string res;
	std::string::const_iterator it = s.begin() + 1;

	while (it != s.end() - 1) {
		char c = *it++;
		if (c == '\\' && it != s.end()) {
			c = *it++;
			switch (c) {
				case 'a':  c = '\a'; break;
				case 'b':  c = '\b'; break;
				case 'f':  c = '\f'; break;
				case 'n':  c = '\n'; break;
				case 'r':  c = '\r'; break;
				case 't':  c = '\t'; break;
				case 'v':  c = '\v'; break;
				case '\\': c = '\\'; break;
				case '"':  c = '"'; break;
				default:
					throw exc::SeqException("undefined escape sequence: '" + std::string(1, c) + "'");
			}
		}
		res += c;
	}

	return res;
}

template<>
struct action<literal_string> {
	template<typename Input>
	static void apply(const Input& in, ParseState& state)
	{
		state.add(unescape(in.string()));
	}
};

template<>
struct action<true_expr> {
	static void apply0(ParseState& state)
	{
		Expr *expr = new BoolExpr(true);
		state.add(expr);
	}
};

template<>
struct action<false_expr> {
	static void apply0(ParseState& state)
	{
		Expr *expr = new BoolExpr(false);
		state.add(expr);
	}
};

template<>
struct action<op_uop> {
	template<typename Input>
	static void apply(const Input& in, ParseState& state)
	{
		Op op = uop(in.string());
		state.add(op);
	}
};

template<>
struct action<op_bop> {
	template<typename Input>
	static void apply(const Input& in, ParseState& state)
	{
		Op op = bop(in.string());
		state.add(op);
	}
};

template<>
struct action<seq_type> {
	static void apply0(ParseState& state)
	{
		state.add((types::Type *)&types::Seq);
	}
};

template<>
struct action<int_type> {
	static void apply0(ParseState& state)
	{
		state.add((types::Type *)&types::Int);
	}
};

template<>
struct action<float_type> {
	static void apply0(ParseState& state)
	{
		state.add((types::Type *)&types::Float);
	}
};

template<>
struct action<bool_type> {
	static void apply0(ParseState& state)
	{
		state.add((types::Type *)&types::Bool);
	}
};

template<>
struct action<str_type> {
	static void apply0(ParseState& state)
	{
		state.add((types::Type *)&types::Str);
	}
};

template<>
struct action<custom_type> {
	template<typename Input>
	static bool apply(const Input& in, ParseState& state)
	{
		SeqEntity ent;

		try {
			ent = state.lookup(in.string());
		} catch (exc::SeqException&) {
			return false;
		}

		if (ent.type != SeqEntity::TYPE)
			return false;

		state.add(ent.value.type);
		return true;
	}
};

template<>
struct action<seq_pattern0> {
	template<typename Input>
	static void apply(const Input& in, ParseState& state)
	{
		state.add(in.string());
	}
};

/*
 * Control
 */

template<typename Rule>
struct control : pegtl::normal<Rule> {};

template<>
struct control<module> : pegtl::normal<module>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.scope();
		state.scopeBarrier();
		auto *module = new SeqModule();
		state.setModule(module);
		state.enter(module->getBlock());
		state.context(module);
		state.sym("args", module->getArgVar());
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		state.unscope();
		state.exit();
		state.uncontext();
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.unscope();
		state.exit();
		state.uncontext();
	}
};

template<>
struct control<func_decl> : pegtl::normal<func_decl>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.scope();
		state.scopeBarrier();
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("*", true);
		std::deque<SeqEntity> deq;

		for (auto ent : vec)
			deq.push_back(ent);

		assert(deq.size() >= 2);
		assert(state.context().type == SeqEntity::FUNC);
		auto *func = state.context().value.func;

		SeqEntity name = deq.front();
		SeqEntity type = deq.back();
		deq.pop_front();
		deq.pop_back();
		assert(name.type == SeqEntity::NAME);
		assert(type.type == SeqEntity::TYPE);

		std::vector<std::string> argNames;
		std::vector<types::Type *> argTypes;

		while (!deq.empty()) {
			assert(deq.size() >= 2);
			SeqEntity argName = deq.front();
			deq.pop_front();
			SeqEntity argType = deq.front();
			deq.pop_front();

			assert(argName.type == SeqEntity::NAME);
			assert(argType.type == SeqEntity::TYPE);

			argNames.push_back(argName.value.name);
			argTypes.push_back(argType.value.type);
		}

		func->setName(name.value.name);
		func->setOut(type.value.type);
		func->setIns(argTypes);
		func->setArgNames(argNames);

		state.symparent(name.value.name, func);

		for (auto& argName : argNames)
			state.sym(argName, func->getArgVar(argName));
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.unscope();
		state.pop();
	}
};

template<>
struct control<func_decl_out_void> : pegtl::normal<func_decl_out_void>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.scope();
		state.scopeBarrier();
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("*", true);
		std::deque<SeqEntity> deq;

		for (auto& ent : vec)
			deq.push_back(ent);

		assert(!deq.empty());
		assert(state.context().type == SeqEntity::FUNC);
		auto *func = state.context().value.func;

		SeqEntity name = deq.front();
		deq.pop_front();
		assert(name.type == SeqEntity::NAME);

		std::vector<std::string> argNames;
		std::vector<types::Type *> argTypes;

		while (!deq.empty()) {
			assert(deq.size() >= 2);
			SeqEntity argName = deq.front();
			deq.pop_front();
			SeqEntity argType = deq.front();
			deq.pop_front();

			assert(argName.type == SeqEntity::NAME);
			assert(argType.type == SeqEntity::TYPE);

			argNames.push_back(argName.value.name);
			argTypes.push_back(argType.value.type);
		}

		func->setName(name.value.name);
		func->setOut(types::VoidType::get());
		func->setIns(argTypes);
		func->setArgNames(argNames);

		state.symparent(name.value.name, func);

		for (auto& argName : argNames)
			state.sym(argName, func->getArgVar(argName));
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.unscope();
		state.pop();
	}
};

template<>
struct control<func_decl_in_out_void> : pegtl::normal<func_decl_in_out_void>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.scope();
		state.scopeBarrier();
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("s");
		assert(state.context().type == SeqEntity::FUNC);
		auto *func = state.context().value.func;
		state.symparent(vec[0].value.name, func);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.unscope();
		state.pop();
	}
};

template<>
struct control<func_generics> : pegtl::normal<func_generics>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("s", true);
		assert(!vec.empty());
		assert(state.context().type == SeqEntity::FUNC);
		auto *func = state.context().value.func;

		func->addGenerics((int)vec.size());
		for (unsigned i = 0; i < vec.size(); i++) {
			state.sym(vec[i].value.name, (types::Type *)func->getGeneric(i));
			func->getGeneric(i)->setName(vec[i].value.name);
		}
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<func_stmt> : pegtl::normal<func_stmt>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		auto *func = new Func();
		state.enter(func->getBlock());
		state.context(func);
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		state.unscope();  // for the scope introduced by `func_decl*`
		state.exit();

		assert(state.context().type == SeqEntity::FUNC);
		auto *func = state.context().value.func;
		state.uncontext();

		auto *p = new FuncStmt(func);
		state.stmt(p);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.exit();
		state.uncontext();
	}
};

template<>
struct control<class_open> : pegtl::normal<class_open>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("s", true);
		assert(!vec.empty());

		types::RefType *ref = types::RefType::get(vec[0].value.name);
		types::Type *type = ref;
		state.sym(vec[0].value.name, type);
		state.scope();
		state.scopeBarrier();

		ref->addGenerics((int)vec.size() - 1);
		for (unsigned i = 1; i < vec.size(); i++) {
			state.sym(vec[i].value.name, (types::Type *)ref->getGeneric(i - 1));
			ref->getGeneric(i - 1)->setName(vec[i].value.name);
		}

		state.context(type);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<class_type> : pegtl::normal<class_type>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("*", true);
		assert(!vec.empty() && vec.size()%2 == 0);
		std::vector<types::Type *> types;
		std::vector<std::string> names;

		for (unsigned i = 0; i < vec.size(); i += 2) {
			assert(vec[i].type == SeqEntity::NAME);
			assert(vec[i+1].type == SeqEntity::TYPE);
			names.push_back(vec[i].value.name);
			types.push_back(vec[i+1].value.type);
		}

		assert(state.context().type == SeqEntity::TYPE);
		auto *ref = dynamic_cast<types::RefType *>(state.context().value.type);
		assert(ref);
		ref->setContents(types::RecordType::get(types, names));
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<class_stmt> : pegtl::normal<class_stmt>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		assert(state.context().type == SeqEntity::TYPE);
		auto *ref = dynamic_cast<types::RefType *>(state.context().value.type);
		assert(ref);
		state.uncontext();
		state.addMethods(ref);
		state.unscope();
		ref->setDone();
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
	}
};

template<>
struct control<print_stmt> : pegtl::normal<print_stmt>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("e");
		auto *p = new Print(vec[0].value.expr);
		p->setBase(state.base());
		state.stmt(p);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<while_args> : pegtl::normal<while_args>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("e");
		auto p = new While(vec[0].value.expr);
		p->setBase(state.base());
		state.scope();
		state.stmt(p);
		state.enter(p->getBlock());
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<while_body> : pegtl::normal<while_body>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		state.unscope();
		state.exit();
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.unscope();
		state.exit();
	}
};

template<>
struct control<for_args> : pegtl::normal<for_args>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("se");
		auto *p = new For(vec[1].value.expr);
		p->setBase(state.base());
		state.scope();
		state.sym(vec[0].value.name, p->getVar());
		state.stmt(p);
		state.enter(p->getBlock());
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<for_body> : pegtl::normal<for_body>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		state.unscope();
		state.exit();
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.unscope();
		state.exit();
	}
};

template<>
struct control<source_args> : pegtl::normal<source_args>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("e", true);

		std::vector<Expr *> sources;
		for (auto e : vec)
			sources.push_back(e.value.expr);

		auto *p = new Source(sources);
		p->setBase(state.base());
		state.scope();
		state.stmt(p);

		state.context((Stmt *)p);
		state.enter(p->getBlock());
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<source_as> : pegtl::normal<source_as>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("s", true);
		assert(vec.size() <= 1);

		SeqEntity ent = state.context();
		assert(ent.type == SeqEntity::STMT);
		auto *p = dynamic_cast<Source *>(ent.value.stmt);
		assert(p);
		state.sym(vec.empty() ? "_" : vec[0].value.name, p->getVar());
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<source_body> : pegtl::normal<source_body>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		state.unscope();
		state.exit();
		state.uncontext();
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.unscope();
		state.exit();
		state.uncontext();
	}
};

template<>
struct control<typedef_stmt> : pegtl::normal<typedef_stmt>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("st");
		std::string name = vec[0].value.name;
		types::Type *type = vec[1].value.type;
		state.sym(name, type);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<var_decl> : pegtl::normal<var_decl>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("se");
		auto *p = new VarStmt(vec[1].value.expr);
		p->setBase(state.base());
		state.stmt(p);
		state.sym(vec[0].value.name, p->getVar());
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<global_decl> : pegtl::normal<global_decl>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("se");
		auto *p = new VarStmt(vec[1].value.expr);
		p->setBase(state.base());
		state.stmt(p);
		state.sym(vec[0].value.name, p->getVar());
		p->getVar()->setGlobal();
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<assign_stmt> : pegtl::normal<assign_stmt>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("se");
		SeqEntity ent = state.lookup(vec[0].value.name);

		if (ent.type != SeqEntity::VAR)
			throw exc::SeqException("can only reassign variables declared with 'var'");

		Var *var = ent.value.var;
		auto *p = new Assign(var, vec[1].value.expr);
		p->setBase(state.base());
		state.stmt(p);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<assign_member_idx_stmt> : pegtl::normal<assign_member_idx_stmt>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("eie");
		auto *p = new AssignMember(vec[0].value.expr, vec[1].value.ival, vec[2].value.expr);
		p->setBase(state.base());
		state.stmt(p);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<assign_member_stmt> : pegtl::normal<assign_member_stmt>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("ese");
		auto *p = new AssignMember(vec[0].value.expr, vec[1].value.name, vec[2].value.expr);
		p->setBase(state.base());
		state.stmt(p);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<assign_array_stmt> : pegtl::normal<assign_array_stmt>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("eee");
		auto *p = new AssignIndex(vec[0].value.expr, vec[1].value.expr, vec[2].value.expr);
		p->setBase(state.base());
		state.stmt(p);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<if_stmt> : pegtl::normal<if_stmt>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		auto *p = new If();
		p->setBase(state.base());
		state.context((Stmt *)p);
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		assert(state.context().type == SeqEntity::STMT && dynamic_cast<If *>(state.context().value.stmt));
		state.stmt(state.context().value.stmt);
		state.uncontext();
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.uncontext();
	}
};

template<>
struct control<if_open> : pegtl::normal<if_open>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("e");
		assert(state.context().type == SeqEntity::STMT);
		auto *p = dynamic_cast<If *>(state.context().value.stmt);
		assert(p);

		Block *branch = p->addCond(vec[0].value.expr);
		state.enter(branch);
		state.scope();
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<elif_open> : pegtl::normal<elif_open>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("e");
		assert(state.context().type == SeqEntity::STMT);
		auto *p = dynamic_cast<If *>(state.context().value.stmt);
		assert(p);

		Block *branch = p->addCond(vec[0].value.expr);
		state.enter(branch);
		state.scope();
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<else_open> : pegtl::normal<else_open>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		assert(state.context().type == SeqEntity::STMT);
		auto *p = dynamic_cast<If *>(state.context().value.stmt);
		assert(p);

		Block *branch = p->addElse();
		state.enter(branch);
		state.scope();
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
	}
};

template<>
struct control<if_close> : pegtl::normal<if_close>
{
	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		state.exit();
		state.unscope();
	}
};

template<>
struct control<elif_close> : pegtl::normal<elif_close>
{
	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		state.exit();
		state.unscope();
	}
};

template<>
struct control<else_close> : pegtl::normal<else_close>
{
	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		state.exit();
		state.unscope();
	}
};

template<>
struct control<match_stmt> : pegtl::normal<match_stmt>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		auto *p = new Match();
		p->setBase(state.base());
		state.context((Stmt *)p);
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		assert(state.context().type == SeqEntity::STMT && dynamic_cast<Match *>(state.context().value.stmt));
		state.stmt(state.context().value.stmt);
		state.uncontext();
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.uncontext();
	}
};

template<>
struct control<match_open> : pegtl::normal<match_open>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("e");
		assert(state.context().type == SeqEntity::STMT);
		auto *p = dynamic_cast<Match *>(state.context().value.stmt);
		assert(p);

		p->setValue(vec[0].value.expr);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<case_open> : pegtl::normal<case_open>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
		state.scope();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("q");
		assert(state.context().type == SeqEntity::STMT);
		auto *p = dynamic_cast<Match *>(state.context().value.stmt);
		assert(p);

		Block *branch = p->addCase(vec[0].value.pattern);
		state.enter(branch);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
		state.unscope();
	}
};

template<>
struct control<case_close> : pegtl::normal<case_close>
{
	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		state.exit();
		state.unscope();
	}
};

template<>
struct control<return_stmt> : pegtl::normal<return_stmt>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("e", true);
		assert(vec.size() <= 1);
		auto *p = new Return(vec.empty() ? nullptr : vec[0].value.expr);

		BaseFunc *base = state.base();

		auto *func = dynamic_cast<Func *>(base);
		if (func)
			func->sawReturn(p);

		p->setBase(base);
		state.stmt(p);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<yield_stmt> : pegtl::normal<yield_stmt>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("e", true);
		assert(vec.size() <= 1);
		auto *p = new Yield(vec.empty() ? nullptr : vec[0].value.expr);

		BaseFunc *base = state.base();

		auto *func = dynamic_cast<Func *>(base);
		if (func)
			func->sawYield(p);

		p->setBase(base);
		state.stmt(p);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<break_stmt> : pegtl::normal<break_stmt>
{
	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto *p = new Break();
		p->setBase(state.base());
		state.stmt(p);
	}
};

template<>
struct control<continue_stmt> : pegtl::normal<continue_stmt>
{
	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto *p = new Continue();
		p->setBase(state.base());
		state.stmt(p);
	}
};

template<>
struct control<expr_stmt> : pegtl::normal<expr_stmt>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("e");
		auto *p = new ExprStmt(vec[0].value.expr);
		p->setBase(state.base());
		state.stmt(p);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

static Expr *precedenceClimb(std::queue<SeqEntity>& q, const int minPrec)
{
	assert(!q.empty());
	SeqEntity ent = q.front();
	q.pop();
	Expr *lhs = nullptr;
	Op op;

	switch (ent.type) {
		case SeqEntity::EXPR:
			lhs = ent.value.expr;
			break;
		case SeqEntity::OP:
			op = ent.value.op;
			assert(!op.binary);
			lhs = precedenceClimb(q, op.prec);
			lhs = new UOpExpr(op, lhs);
			break;
		default:
			assert(0);
	}

	while (!q.empty()) {
		SeqEntity lookahead = q.front();
		assert(lookahead.type == SeqEntity::OP);
		op = lookahead.value.op;
		assert(op.binary);

		if (op.prec < minPrec)
			break;

		q.pop();
		const int nextMinPrec = op.rightAssoc ? op.prec : (op.prec + 1);
		Expr *rhs = precedenceClimb(q, nextMinPrec);
		lhs = new BOpExpr(op, lhs, rhs);
	}

	return lhs;
}

template<>
struct control<expr> : pegtl::normal<expr>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("*", true);
		std::queue<SeqEntity> q;

		for (auto& ent : vec)
			q.push(ent);

		state.add(precedenceClimb(q, 0));
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<int_expr> : pegtl::normal<int_expr>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("i");
		Expr *expr = new IntExpr(vec[0].value.ival);
		state.add(expr);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<float_expr> : pegtl::normal<float_expr>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("f");
		Expr *expr = new FloatExpr(vec[0].value.fval);
		state.add(expr);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<str_expr> : pegtl::normal<str_expr>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("s");
		std::string s(vec[0].value.name);
		Expr *expr = new StrExpr(s);
		state.add(expr);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<var_expr> : pegtl::normal<var_expr>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("s");
		SeqEntity ent = state.lookup(vec[0].value.name);

		Expr *expr = nullptr;
		switch (ent.type) {
			case SeqEntity::VAR:
				expr = new VarExpr(ent.value.var);
				break;
			case SeqEntity::FUNC:
				expr = new FuncExpr(ent.value.func);
				break;
			default:
				throw exc::SeqException("name '" + std::string(vec[0].value.name) + "' does not refer to a variable");
		}

		state.add(expr);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<realized_func_expr> : pegtl::normal<realized_func_expr>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("*", true);
		assert(vec.size() >= 2);
		assert(vec[0].type == SeqEntity::NAME);

		SeqEntity ent = state.lookup(vec[0].value.name);

		if (ent.type != SeqEntity::FUNC)
			throw exc::SeqException("can only type-instantiate generic functions");

		Func *func = ent.value.func;

		std::vector<types::Type *> types;
		for (unsigned i = 1; i < vec.size(); i++) {
			assert(vec[i].type == SeqEntity::TYPE);
			types.push_back(vec[i].value.type);
		}

		Expr *e = new FuncExpr(func, types);
		state.add(e);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<array_expr> : pegtl::normal<array_expr>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("te");
		types::Type *type = vec[0].value.type;
		Expr *count = vec[1].value.expr;
		Expr *e = new ArrayExpr(type, count);
		state.add(e);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<construct_expr> : pegtl::normal<construct_expr>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("*", true);
		assert(!vec.empty() && vec[0].type == SeqEntity::TYPE);

		std::vector<Expr *> args;
		for (unsigned i = 1; i < vec.size(); i++) {
			assert(vec[i].type == SeqEntity::EXPR);
			args.push_back(vec[i].value.expr);
		}

		Expr *e = new ConstructExpr(vec[0].value.type, args);
		state.add(e);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<static_memb_expr> : pegtl::normal<static_memb_expr>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("ts");
		types::Type *type = vec[0].value.type;
		std::string memb = vec[1].value.name;
		Expr *e = new GetStaticElemExpr(type, memb);
		state.add(e);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<static_memb_realized_expr> : pegtl::normal<static_memb_realized_expr>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("*", true);
		assert(vec.size() >= 3);
		assert(vec[0].type == SeqEntity::TYPE);
		assert(vec[1].type == SeqEntity::NAME);

		types::Type *type = vec[0].value.type;
		std::string name = vec[1].value.name;

		BaseFunc *method = type->getMethod(name);
		auto *func = dynamic_cast<Func *>(method);

		if (!func)
			throw exc::SeqException("method '" + name + "' of type '" + type->getName() + "' is not generic");

		std::vector<types::Type *> types;
		for (unsigned i = 2; i < vec.size(); i++) {
			assert(vec[i].type == SeqEntity::TYPE);
			types.push_back(vec[i].value.type);
		}

		Expr *e = new FuncExpr(func, types);
		state.add(e);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<default_expr> : pegtl::normal<default_expr>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("t");
		Expr *e = new DefaultExpr(vec[0].value.type);
		state.add(e);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<record_expr_item_named> : pegtl::normal<record_expr_item_named>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("se");
		state.add(vec[0].value.name);
		state.add(vec[1].value.expr);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<record_expr_item_unnamed> : pegtl::normal<record_expr_item_unnamed>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("e");
		std::string empty;
		state.add(empty);
		state.add(vec[0].value.expr);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<record_expr> : pegtl::normal<record_expr>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("*", true);
		assert(!vec.empty() && vec.size()%2 == 0);
		std::vector<Expr *> exprs;
		std::vector<std::string> names;

		for (unsigned i = 0; i < vec.size(); i += 2) {
			assert(vec[i].type == SeqEntity::NAME);
			assert(vec[i+1].type == SeqEntity::EXPR);
			names.push_back(vec[i].value.name);
			exprs.push_back(vec[i+1].value.expr);
		}

		Expr *e = new RecordExpr(exprs, names);
		state.add(e);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<index_tail> : pegtl::normal<index_tail>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("e");
		assert(state.top().type == SeqEntity::EXPR);
		Expr *arr = state.top().value.expr;
		Expr *idx = vec[0].value.expr;
		Expr *e = new ArrayLookupExpr(arr, idx);
		state.top() = e;
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<slice_tail> : pegtl::normal<slice_tail>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("ee");
		assert(state.top().type == SeqEntity::EXPR);
		Expr *arr = state.top().value.expr;
		Expr *from = vec[0].value.expr;
		Expr *to = vec[1].value.expr;
		Expr *e = new ArraySliceExpr(arr, from, to);
		state.top() = e;
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<slice_tail_no_from> : pegtl::normal<slice_tail_no_from>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("e");
		assert(state.top().type == SeqEntity::EXPR);
		Expr *arr = state.top().value.expr;
		Expr *from = nullptr;
		Expr *to = vec[0].value.expr;
		Expr *e = new ArraySliceExpr(arr, from, to);
		state.top() = e;
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<slice_tail_no_to> : pegtl::normal<slice_tail_no_to>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("e");
		assert(state.top().type == SeqEntity::EXPR);
		Expr *arr = state.top().value.expr;
		Expr *from = vec[0].value.expr;
		Expr *to = nullptr;
		Expr *e = new ArraySliceExpr(arr, from, to);
		state.top() = e;
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<call_tail> : pegtl::normal<call_tail>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("e", true);
		assert(state.top().type == SeqEntity::EXPR);
		Expr *func = state.top().value.expr;
		std::vector<Expr *> args;

		for (auto ent : vec)
			args.push_back(ent.value.expr);

		Expr *e = new CallExpr(func, args);
		state.top() = e;
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<elem_idx_tail> : pegtl::normal<elem_idx_tail>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("i");
		assert(state.top().type == SeqEntity::EXPR);
		Expr *rec = state.top().value.expr;
		seq_int_t idx = vec[0].value.ival;
		Expr *e = new GetElemExpr(rec, idx);
		state.top() = e;
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<elem_memb_tail> : pegtl::normal<elem_memb_tail>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("s");
		assert(state.top().type == SeqEntity::EXPR);
		Expr *rec = state.top().value.expr;
		std::string memb = vec[0].value.name;
		Expr *e = new GetElemExpr(rec, memb);
		state.top() = e;
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<elem_memb_realized_tail> : pegtl::normal<elem_memb_realized_tail>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("*", true);
		assert(vec.size() >= 2);
		assert(vec[0].type == SeqEntity::NAME);
		assert(state.top().type == SeqEntity::EXPR);

		Expr *rec = state.top().value.expr;
		std::string name = vec[0].value.name;

		std::vector<types::Type *> types;
		for (unsigned i = 1; i < vec.size(); i++) {
			assert(vec[i].type == SeqEntity::TYPE);
			types.push_back(vec[i].value.type);
		}

		Expr *e = new MethodExpr(rec, name, types);
		state.top() = e;
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<make_opt_tail> : pegtl::normal<make_opt_tail>
{
	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		assert(state.top().type == SeqEntity::EXPR);
		Expr *val = state.top().value.expr;
		Expr *e = new OptExpr(val);
		state.top() = e;
	}
};

template<>
struct control<cond_tail> : pegtl::normal<cond_tail>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("ee");
		assert(state.top().type == SeqEntity::EXPR);
		Expr *ifTrue = state.top().value.expr;
		Expr *cond = vec[0].value.expr;
		Expr *ifFalse = vec[1].value.expr;
		Expr *e = new CondExpr(cond, ifTrue, ifFalse);
		state.top() = e;
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<paren_expr> : pegtl::normal<paren_expr>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("e");
		Expr *e = vec[0].value.expr;
		state.add(e);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<match_expr> : pegtl::normal<match_expr>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("*", true);
		assert(!vec.empty() && vec[0].type == SeqEntity::EXPR);
		auto *match = new MatchExpr();
		Expr *val = vec[0].value.expr;
		match->setValue(val);

		for (unsigned i = 1; i < vec.size(); i += 2) {
			assert(i + 1 < vec.size() &&
			       vec[i].type == SeqEntity::PATTERN &&
			       vec[i+1].type == SeqEntity::EXPR);

			match->addCase(vec[i].value.pattern, vec[i+1].value.expr);
		}

		state.add((Expr *)match);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<array_tail> : pegtl::normal<array_tail>
{
	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		assert(state.top().type == SeqEntity::TYPE);
		types::Type *type0 = state.top().value.type;
		types::Type *type = types::ArrayType::get(type0);
		state.top() = type;
	}
};

template<>
struct control<opt_tail> : pegtl::normal<opt_tail>
{
	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		assert(state.top().type == SeqEntity::TYPE);
		types::Type *type0 = state.top().value.type;
		types::Type *type = types::OptionalType::get(type0);
		state.top() = type;
	}
};

template<>
struct control<gen_tail> : pegtl::normal<gen_tail>
{
	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		assert(state.top().type == SeqEntity::TYPE);
		types::Type *type0 = state.top().value.type;
		types::Type *type = types::GenType::get(type0);
		state.top() = type;
	}
};

template<>
struct control<record_type_elem_named> : pegtl::normal<record_type_elem_named>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("st");
		state.add(vec[0].value.name);
		state.add(vec[1].value.type);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<record_type_elem_unnamed> : pegtl::normal<record_type_elem_unnamed>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("t");
		std::string empty;
		state.add(empty);
		state.add(vec[0].value.type);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<record_type> : pegtl::normal<record_type>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("*", true);
		assert(!vec.empty() && vec.size()%2 == 0);
		std::vector<types::Type *> types;
		std::vector<std::string> names;

		for (unsigned i = 0; i < vec.size(); i += 2) {
			assert(vec[i].type == SeqEntity::NAME);
			assert(vec[i+1].type == SeqEntity::TYPE);
			names.push_back(vec[i].value.name);
			types.push_back(vec[i+1].value.type);
		}

		state.add((types::Type *)types::RecordType::get(types, names));
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<realized_type> : pegtl::normal<realized_type>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("t", true);
		assert(!vec.empty());
		auto *ref = dynamic_cast<types::RefType *>(vec[0].value.type);

		if (!ref)
			throw exc::SeqException("can only type-instantiate reference types");

		std::vector<types::Type *> types;
		for (unsigned i = 1; i < vec.size(); i++)
			types.push_back(vec[i].value.type);

		types::Type *realized = ref->realize(types);
		state.add(realized);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<func_type_no_void> : pegtl::normal<func_type_no_void>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("t", true);
		assert(vec.size() >= 2);
		auto *outType = vec.back().value.type;
		vec.pop_back();
		std::vector<types::Type *> types;
		for (auto ent : vec)
			types.push_back(ent.value.type);
		state.add((types::Type *)types::FuncType::get(types, outType));
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<func_type_in_void> : pegtl::normal<func_type_in_void>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("t");
		state.add((types::Type *)types::FuncType::get({}, vec[0].value.type));
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<func_type_out_void> : pegtl::normal<func_type_out_void>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("t", true);
		assert(!vec.empty());
		std::vector<types::Type *> types;
		for (auto ent : vec)
			types.push_back(ent.value.type);
		state.add((types::Type *)types::FuncType::get(types, types::VoidType::get()));
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<func_type_in_out_void> : pegtl::normal<func_type_in_out_void>
{
	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		state.add((types::Type *)types::FuncType::get({}, types::VoidType::get()));
	}
};

template<>
struct control<int_pattern> : pegtl::normal<int_pattern>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("i");
		Pattern *p = new IntPattern(vec[0].value.ival);
		state.add(p);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<true_pattern> : pegtl::normal<true_pattern>
{
	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		Pattern *p = new BoolPattern(true);
		state.add(p);
	}
};

template<>
struct control<false_pattern> : pegtl::normal<false_pattern>
{
	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		Pattern *p = new BoolPattern(false);
		state.add(p);
	}
};

template<>
struct control<str_pattern> : pegtl::normal<str_pattern>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("s");
		Pattern *p = new StrPattern(vec[0].value.name);
		state.add(p);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<record_pattern> : pegtl::normal<record_pattern>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("q", true);
		assert(!vec.empty());

		std::vector<Pattern *> patterns;
		for (auto& e : vec)
			patterns.push_back(e.value.pattern);

		Pattern *p = new RecordPattern(patterns);
		state.add(p);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<star_pattern> : pegtl::normal<star_pattern>
{
	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		Pattern *p = new StarPattern();
		state.add(p);
	}
};

template<>
struct control<array_pattern> : pegtl::normal<array_pattern>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("q", true);

		std::vector<Pattern *> patterns;
		for (auto& e : vec)
			patterns.push_back(e.value.pattern);

		Pattern *p = new ArrayPattern(patterns);
		state.add(p);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<seq_pattern> : pegtl::normal<seq_pattern>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("s");
		Pattern *p = new SeqPattern(vec[0].value.name);
		state.add(p);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<wildcard_pattern> : pegtl::normal<wildcard_pattern>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("s");
		std::string name = vec[0].value.name;

		auto *p = new Wildcard();

		if (name != "_")
			state.sym(name, p->getVar(), true);

		state.add((Pattern *)p);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<bound_pattern> : pegtl::normal<bound_pattern>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("sq");
		std::string name = vec[0].value.name;
		auto *p = new BoundPattern(vec[1].value.pattern);
		state.sym(name, p->getVar(), true);
		state.add((Pattern *)p);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<range_pattern> : pegtl::normal<range_pattern>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("ii");
		Pattern *p = new RangePattern(vec[0].value.ival, vec[1].value.ival);
		state.add(p);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<empty_opt_pattern> : pegtl::normal<empty_opt_pattern>
{
	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		Pattern *p = new OptPattern(nullptr);
		state.add(p);
	}
};

template<>
struct control<guarded_pattern> : pegtl::normal<guarded_pattern>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("qe");
		Pattern *p = new GuardedPattern(vec[0].value.pattern, vec[1].value.expr);
		state.add(p);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<opt_pattern_tail> : pegtl::normal<opt_pattern_tail>
{
	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		assert(state.top().type == SeqEntity::PATTERN);
		Pattern *p0 = state.top().value.pattern;
		Pattern *p = new OptPattern(p0);
		state.top() = p;
	}
};

template<>
struct control<pattern> : pegtl::normal<pattern>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("q", true);
		assert(!vec.empty());

		Pattern *p = nullptr;

		if (vec.size() == 1) {
			p = vec[0].value.pattern;
		} else {
			std::vector<Pattern *> patterns;
			for (auto &e : vec)
				patterns.push_back(e.value.pattern);
			p = new OrPattern(patterns);
		}

		state.add(p);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

SeqModule& seq::parse(std::string input)
{
	ParseState state;
	pegtl::file_input<> in(input);
	const size_t issues_found = pegtl::analyze<grammar>();
	assert(issues_found == 0);
	pegtl::parse<grammar, action, control>(in, state);
	return state.getModule();
}
