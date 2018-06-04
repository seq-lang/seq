#include <iostream>
#include <string>
#include <vector>
#include <stack>
#include <queue>
#include <map>
#include <cassert>
#include <tao/pegtl/analyze.hpp>
#include "grammar.h"
#include "seq/parser.h"

using namespace seq;

struct SeqEntity {
	enum {
		EMPTY = 0,
		INT,
		FLOAT,
		BOOL,
		NAME,
		PIPELINE,
		VAR,
		CELL,
		FUNC,
		TYPE,
		MODULE,
		EXPR,
		OP
	} type = EMPTY;

	union U {
		U() : ival(0) {}
		U(seq_int_t ival) : ival(ival) {}
		U(double fval) : fval(fval) {}
		U(bool bval) : bval(bval) {}
		U(std::string name) : name(std::move(name)) {}
		U(Pipeline pipeline) : pipeline(pipeline) {}
		U(Var *var) : var(var) {}
		U(Cell *cell) : cell(cell) {}
		U(Func *func) : func(func) {}
		U(types::Type *type) : type(type) {}
		U(SeqModule *module) : module(module) {}
		U(Expr *expr) : expr(expr) {}
		U(Op op) : op(std::move(op)) {}
		~U() {}

		seq_int_t ival;
		double fval;
		bool bval;
		std::string name;
		Pipeline pipeline;
		Var *var;
		Cell *cell;
		Func *func;
		types::Type *type;
		SeqModule *module;
		Expr *expr;
		Op op;
	} value;

	SeqEntity() : type(EMPTY), value() {}
	SeqEntity(seq_int_t ival) : type(SeqEntity::INT), value(ival) {}
	SeqEntity(double fval) : type(SeqEntity::FLOAT), value(fval) {}
	SeqEntity(bool bval) : type(SeqEntity::BOOL), value(bval) {}
	SeqEntity(std::string name) : type(SeqEntity::NAME), value(std::move(name)) {}
	SeqEntity(Pipeline pipeline) : type(SeqEntity::PIPELINE), value(pipeline) {}
	SeqEntity(Var *var) : type(SeqEntity::VAR), value(var) {}
	SeqEntity(Cell *cell) : type(SeqEntity::CELL), value(cell) {}
	SeqEntity(Func *func) : type(SeqEntity::FUNC), value(func) {}
	SeqEntity(types::Type *type) : type(SeqEntity::TYPE), value(type) {}
	SeqEntity(SeqModule *module) : type(SeqEntity::MODULE), value(module) {}
	SeqEntity(Expr *expr) : type(SeqEntity::EXPR), value(expr) {}
	SeqEntity(Op op) : type(SeqEntity::OP), value(op) {}

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
				value.name = ent.value.name;
				break;
			case SeqEntity::PIPELINE:
				value.pipeline = ent.value.pipeline;
				break;
			case SeqEntity::VAR:
				value.var = ent.value.var;
				break;
			case SeqEntity::CELL:
				value.cell = ent.value.cell;
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
			case SeqEntity::OP:
				value.op = ent.value.op;
				break;
		}

		return *this;
	}

	SeqEntity(const SeqEntity& ent)
	{
		*this = ent;
	}

	~SeqEntity()=default;

	Pipeline add(Pipeline p)
	{
		switch (type) {
			case SeqEntity::MODULE:
				return *value.module | p;
			case SeqEntity::FUNC:
				return *value.func | p;
			case SeqEntity::PIPELINE:
				value.pipeline | p;
				break;
			case SeqEntity::VAR:
				p = *value.var | p;
				break;
			default:
				throw exc::SeqException("misplaced pipeline");
		}

		return p;
	}
};

std::ostream& operator<<(std::ostream& os, const SeqEntity& ent);

const std::map<char, int> TYPE_MAP = {{'x', SeqEntity::EMPTY},
                                      {'i', SeqEntity::INT},
                                      {'f', SeqEntity::FLOAT},
                                      {'b', SeqEntity::BOOL},
                                      {'s', SeqEntity::NAME},
                                      {'p', SeqEntity::PIPELINE},
                                      {'v', SeqEntity::VAR},
                                      {'f', SeqEntity::FUNC},
                                      {'t', SeqEntity::TYPE},
                                      {'m', SeqEntity::MODULE},
                                      {'e', SeqEntity::EXPR}};

class ParseState {
	typedef std::map<std::string, SeqEntity> SymTab;
private:
	std::vector<SymTab> symbols;
	std::stack<std::vector<SeqEntity>> results;
	std::vector<SeqEntity> contexts;
	SeqModule *module;
public:
	std::vector<SeqEntity> get(const std::string& types, bool multi=false, bool pop=true)
	{
		assert(!types.empty() && !results.empty());
		std::vector<SeqEntity> result = results.top();

		if (!multi && result.size() != types.length())
			throw exc::SeqException(
			  "entity count mismatch: got " + std::to_string(result.size()) + " but expected " + std::to_string(types.length()));

		for (int i = 0; i < result.size(); i++) {
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

	static void symadd(std::string name, SeqEntity ent, std::map<std::string, SeqEntity>& syms)
	{
		if (syms.find(name) != syms.end())
			throw exc::SeqException("duplicate symbol '" + std::string(name) + "'");

		syms.insert({name, ent});
	}

	void sym(std::string name, SeqEntity ent)
	{
		assert(!symbols.empty());
		symadd(name, ent, symbols.back());
	}

	void symparent(std::string name, SeqEntity ent)
	{
		assert(symbols.size() >= 2);
		symadd(name, ent, symbols[symbols.size() - 2]);
	}

	static SeqEntity lookupInTable(std::string name, SymTab symtab)
	{
		auto iter = symtab.find(name);

		if (iter == symtab.end())
			return {};

		return iter->second;
	}

	SeqEntity lookup(std::string name)
	{
		for (auto it = symbols.rbegin(); it != symbols.rend(); ++it) {
			SeqEntity ent = lookupInTable(name, *it);
			if (ent.type != SeqEntity::EMPTY)
				return ent;
		}

		throw exc::SeqException("undefined reference to '" + std::string(name) + "'");
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

	ParseState() : symbols(), results(), contexts(), module(nullptr)
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
		case SeqEntity::PIPELINE:
			os << ent.value.pipeline.getHead()->getName();
			break;
		case SeqEntity::VAR:
			os << "(var)";
			break;
		case SeqEntity::CELL:
			os << "(cell)";
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
struct action<nop_stage> {
	static void apply0(ParseState& state)
	{
		Pipeline p = stageutil::nop();
		state.add(p);
	}
};

template<>
struct action<len_stage> {
	static void apply0(ParseState& state)
	{
		Pipeline p = stageutil::len();
		state.add(p);
	}
};

template<>
struct action<revcomp_stage> {
	static void apply0(ParseState& state)
	{
		Pipeline p = stageutil::revcomp();
		state.add(p);
	}
};

template<>
struct action<collect_stage> {
	static void apply0(ParseState& state)
	{
		Pipeline p = stageutil::collect();
		state.add(p);
	}
};

template<>
struct action<count_stage> {
	static void apply0(ParseState& state)
	{
		Pipeline p = stageutil::count();
		state.add(p);
	}
};

template<>
struct action<copy_stage> {
	static void apply0(ParseState& state)
	{
		Pipeline p = stageutil::copy();
		state.add(p);
	}
};

template<>
struct action<foreach_stage> {
	static void apply0(ParseState& state)
	{
		Pipeline p = stageutil::foreach();
		state.add(p);
	}
};

template<>
struct action<print_stage> {
	static void apply0(ParseState& state)
	{
		Pipeline p = stageutil::print();
		state.add(p);
	}
};

template<>
struct action<int_expr> {
	static void apply0(ParseState& state)
	{
		auto vec = state.get("i");
		Expr *expr = new IntExpr(vec[0].value.ival);
		state.add(expr);
	}
};

template<>
struct action<float_expr> {
	static void apply0(ParseState& state)
	{
		auto vec = state.get("f");
		Expr *expr = new FloatExpr(vec[0].value.fval);
		state.add(expr);
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
struct action<str_expr> {
	static void apply0(ParseState& state)
	{
		auto vec = state.get("s");
		std::string s(vec[0].value.name);
		Expr *expr = new StrExpr(s);
		state.add(expr);
	}
};

template<>
struct action<var_expr> {
	static void apply0(ParseState& state)
	{
		auto vec = state.get("s");
		SeqEntity ent = state.lookup(vec[0].value.name);

		Expr *expr = nullptr;
		switch (ent.type) {
			case SeqEntity::VAR:
				expr = new VarExpr(ent.value.var);
				break;
			case SeqEntity::CELL:
				expr = new CellExpr(ent.value.cell);
				break;
			case SeqEntity::FUNC:
				expr = new FuncExpr(ent.value.func);
				break;
			default:
				throw exc::SeqException("name '" + std::string(vec[0].value.name) + "' does not refer to a variable");

		}

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
		} catch (exc::SeqException) {
			return false;
		}

		if (ent.type != SeqEntity::TYPE)
			return false;

		state.add(ent.value.type);
		return true;
	}
};

/*
 * Control
 */

template<typename Rule>
struct control : pegtl::normal<Rule> {};

template<>
struct control<call_stage> : pegtl::normal<call_stage>
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

		if (ent.type != SeqEntity::FUNC)
			throw exc::SeqException("cannot call non-function");

		Func *func = ent.value.func;
		Pipeline p = (*func)();
		state.add(p);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<getitem_stage> : pegtl::normal<getitem_stage>
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
		Pipeline p = stageutil::get(vec[0].value.ival);
		state.add(p);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<record_stage_elem_expr_pipeline> : pegtl::normal<record_stage_elem_expr_pipeline>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("ep");
		Pipeline p = stageutil::expr(vec[0].value.expr) | vec[1].value.pipeline;
		state.add(p);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<record_stage_elem_expr> : pegtl::normal<record_stage_elem_expr>
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
		Pipeline p = stageutil::expr(vec[0].value.expr);
		state.add(p);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<record_stage> : pegtl::normal<record_stage>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("p", true);
		assert(!vec.empty());
		PipelineList& pl = *new PipelineList(vec[0].value.pipeline);
		for (int i = 1; i < vec.size(); i++)
			pl = (pl, vec[i].value.pipeline);
		Pipeline p = MakeRec::make(pl);
		state.add(p);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<split_stage> : pegtl::normal<split_stage>
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
		Pipeline p = stageutil::split(vec[0].value.expr, vec[1].value.expr);
		state.add(p);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<substr_stage> : pegtl::normal<substr_stage>
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
		Pipeline p = stageutil::substr(vec[0].value.expr, vec[1].value.expr);
		state.add(p);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<filter_stage> : pegtl::normal<filter_stage>
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
		Pipeline p = stageutil::filter(vec[0].value.expr);
		state.add(p);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<chunk_stage> : pegtl::normal<chunk_stage>
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
		Pipeline p = vec.empty() ? stageutil::chunk() : stageutil::chunk(vec[0].value.expr);
		state.add(p);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<stage_as> : pegtl::normal<stage_as>
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
		assert(state.top().type == SeqEntity::PIPELINE);
		auto *var = new Var(true);
		*var = state.top().value.pipeline;
		state.sym(vec[0].value.name, var);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<pipeline_stage> : pegtl::normal<pipeline_stage>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("p", true);
		Pipeline p = vec[0].value.pipeline;

		for (int i = 1; i < vec.size(); i++) {
			p = p | vec[i].value.pipeline;
		}

		p.getHead()->setBase(state.base());
		state.add(p);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<pipeline_branch> : pegtl::normal<pipeline_branch>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
		Pipeline p = stageutil::nop();
		state.add(p);
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("p", true);
		Pipeline p = vec[0].value.pipeline;

		for (int i = 1; i < vec.size(); i++) {
			p = p | vec[i].value.pipeline;
		}

		p.getHead()->setBase(state.base());
		state.add(p);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<module> : pegtl::normal<module>
{
	template<typename Input>
	static void start(Input& in, ParseState& state)
	{
		state.scope();
		auto *module = new SeqModule(true);
		state.enter(module);
		state.sym("args", module->getArgVar());
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		assert(state.context().type == SeqEntity::MODULE);
		auto *module = state.context().value.module;
		state.unscope();
		state.exit();
		state.setModule(module);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.unscope();
		state.exit();
	}
};

template<>
struct control<func_decl> : pegtl::normal<func_decl>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("sstt");
		assert(state.context().type == SeqEntity::FUNC);
		auto *func = state.context().value.func;
		func->setInOut(vec[2].value.type, vec[3].value.type);
		state.symparent(vec[0].value.name, func);
		state.sym(vec[1].value.name, func->getArgVar());
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<func_decl_in_void> : pegtl::normal<func_decl_in_void>
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
		assert(state.context().type == SeqEntity::FUNC);
		auto *func = state.context().value.func;
		func->setInOut(types::VoidType::get(), vec[1].value.type);
		state.symparent(vec[0].value.name, func);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<func_decl_out_void> : pegtl::normal<func_decl_out_void>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("sst");
		assert(state.context().type == SeqEntity::FUNC);
		auto *func = state.context().value.func;
		func->setInOut(vec[2].value.type, types::VoidType::get());
		state.symparent(vec[0].value.name, func);
		state.sym(vec[1].value.name, func->getArgVar());
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<func_decl_in_out_void> : pegtl::normal<func_decl_in_out_void>
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
		assert(state.context().type == SeqEntity::FUNC);
		auto *func = state.context().value.func;
		func->setInOut(types::VoidType::get(), types::VoidType::get());
		state.symparent(vec[0].value.name, func);
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
		state.scope();
		state.enter(new Func(types::Void, types::Void));
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

static Pipeline makePipelineFromLinkedStage(Stage *stage)
{
	Stage *child = stage;
	while (!child->getNext().empty())
		child = child->getNext().back();
	return {stage, child};
}

template<>
struct control<branch> : pegtl::normal<branch>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.scope();
		assert(state.top().type == SeqEntity::PIPELINE);
		Pipeline p = stageutil::nop();
		state.push();
		state.add(p);
		state.enter(p);

		auto *v = new Var(true);
		*v = p;
		state.sym("_", v);
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		state.unscope();
		auto vec = state.get("p");
		Pipeline p = makePipelineFromLinkedStage(vec[0].value.pipeline.getHead());
		state.add(p);
		state.exit();
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.unscope();
		state.exit();
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
		Pipeline p = stageutil::whilestage(vec[0].value.expr);
		state.scope();
		state.enter(p);
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
		SeqEntity ent = state.context();
		assert(ent.type == SeqEntity::PIPELINE && dynamic_cast<While *>(ent.value.pipeline.getHead()));
		state.exit();

		SeqEntity context = state.context();
		context.add(ent.value.pipeline);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.unscope();
		state.exit();
	}
};

template<>
struct control<range_args> : pegtl::normal<range_args>
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

		Pipeline p;
		switch (vec.size()) {
			case 1:
				p = stageutil::range(vec[0].value.expr);
				break;
			case 2:
				p = stageutil::range(vec[0].value.expr, vec[1].value.expr);
				break;
			case 3:
				p = stageutil::range(vec[0].value.expr, vec[1].value.expr, vec[2].value.expr);
				break;
			default:
				assert(0);
		}

		state.scope();
		state.enter(p);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<range_as> : pegtl::normal<range_as>
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
		assert(ent.type == SeqEntity::PIPELINE && dynamic_cast<Range *>(ent.value.pipeline.getHead()));

		auto *v = new Var(true);
		*v = ent.value.pipeline;
		state.sym(vec.empty() ? "_" : vec[0].value.name, v);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<range_body> : pegtl::normal<range_body>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		state.unscope();
		SeqEntity ent = state.context();
		assert(ent.type == SeqEntity::PIPELINE && dynamic_cast<Range *>(ent.value.pipeline.getHead()));
		state.exit();

		SeqEntity context = state.context();
		context.add(ent.value.pipeline);
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

		Pipeline p = stageutil::source(sources);

		state.scope();
		state.enter(p);
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
		assert(ent.type == SeqEntity::PIPELINE && dynamic_cast<Source *>(ent.value.pipeline.getHead()));

		auto *v = new Var(true);
		*v = ent.value.pipeline;
		state.sym(vec.empty() ? "_" : vec[0].value.name, v);
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
		SeqEntity ent = state.context();
		assert(ent.type == SeqEntity::PIPELINE && dynamic_cast<Source *>(ent.value.pipeline.getHead()));
		state.exit();

		SeqEntity context = state.context();
		context.add(ent.value.pipeline);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.unscope();
		state.exit();
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
struct control<pipeline_expr_stmt_toplevel> : pegtl::normal<pipeline_expr_stmt_toplevel>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("ep");
		SeqEntity ent = state.context();
		Pipeline p = stageutil::expr(vec[0].value.expr) | vec[1].value.pipeline;
		ent.add(p);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<pipeline_expr_stmt_nested> : pegtl::normal<pipeline_expr_stmt_nested>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("ep");
		SeqEntity ent = state.context();
		Pipeline p = stageutil::expr(vec[0].value.expr) | vec[1].value.pipeline;
		p = ent.add(p);
		state.add(p);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<var_assign_pipeline> : pegtl::normal<var_assign_pipeline>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("sp");
		Pipeline p = vec[1].value.pipeline;
		p.getHead()->setBase(state.base());
		auto *var = new Var(true);
		*var = p;
		state.sym(vec[0].value.name, var);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<var_assign_expr> : pegtl::normal<var_assign_expr>
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
		Pipeline p = stageutil::expr(vec[1].value.expr);
		p.getHead()->setBase(state.base());
		auto *var = new Var(true);
		p = state.context().add(p);
		*var = p;
		state.sym(vec[0].value.name, var);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<cell_decl> : pegtl::normal<cell_decl>
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
		Cell *cell = new Cell(state.base(), vec[1].value.expr);
		Pipeline p = stageutil::cell(cell);
		p.getHead()->setBase(state.base());
		state.context().add(p);
		state.sym(vec[0].value.name, cell);
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

		if (ent.type != SeqEntity::CELL)
			throw exc::SeqException("can only reassign variables declared with 'var'");

		Cell *cell = ent.value.cell;
		Pipeline p = stageutil::assign(cell, vec[1].value.expr);
		p.getHead()->setBase(state.base());
		state.context().add(p);
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
		auto vec = state.get("sie");
		SeqEntity ent = state.lookup(vec[0].value.name);

		if (ent.type != SeqEntity::CELL)
			throw exc::SeqException("can only mutate variables declared with 'var'");

		Cell *cell = ent.value.cell;
		Pipeline p = stageutil::assignmemb(cell, vec[1].value.ival, vec[2].value.expr);
		p.getHead()->setBase(state.base());
		state.context().add(p);
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
		auto vec = state.get("sse");
		SeqEntity ent = state.lookup(vec[0].value.name);

		if (ent.type != SeqEntity::CELL)
			throw exc::SeqException("can only mutate variables declared with 'var'");

		Cell *cell = ent.value.cell;
		Pipeline p = stageutil::assignmemb(cell, vec[1].value.name, vec[2].value.expr);
		p.getHead()->setBase(state.base());
		state.context().add(p);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<assign_expr_stmt> : pegtl::normal<assign_expr_stmt>
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
		auto *lookup = dynamic_cast<ArrayLookupExpr *>(vec[0].value.expr);

		if (lookup == nullptr)
			throw exc::SeqException("can only assign array indices, not general expressions");

		Pipeline p = stageutil::assignindex(lookup->getArr(), lookup->getIdx(), vec[1].value.expr);
		p.getHead()->setBase(state.base());
		state.context().add(p);
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
		state.push();
		Pipeline p = stageutil::ifstage();
		p.getHead()->setBase(state.base());
		state.add(p);
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("p");
		assert(dynamic_cast<If *>(vec[0].value.pipeline.getHead()));
		state.context().add(vec[0].value.pipeline);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
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
		assert(state.top().type == SeqEntity::PIPELINE);
		auto *ifstage = dynamic_cast<If *>(state.top().value.pipeline.getHead());
		assert(ifstage != nullptr);
		Pipeline branch = ifstage->addCond(vec[0].value.expr);
		state.enter(branch);
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
		assert(state.top().type == SeqEntity::PIPELINE);
		auto *ifstage = dynamic_cast<If *>(state.top().value.pipeline.getHead());
		assert(ifstage != nullptr);
		Pipeline branch = ifstage->addCond(vec[0].value.expr);
		state.enter(branch);
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
		assert(state.top().type == SeqEntity::PIPELINE);
		auto *ifstage = dynamic_cast<If *>(state.top().value.pipeline.getHead());
		assert(ifstage != nullptr);
		Pipeline branch = ifstage->addElse();
		state.enter(branch);
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
	}
};

template<>
struct control<elif_close> : pegtl::normal<elif_close>
{
	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		state.exit();
	}
};

template<>
struct control<else_close> : pegtl::normal<else_close>
{
	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		state.exit();
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
		Pipeline p = stageutil::ret(vec.empty() ? nullptr : vec[0].value.expr);
		p.getHead()->setBase(state.base());
		state.context().add(p);
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
		Pipeline p = stageutil::brk();
		p.getHead()->setBase(state.base());
		state.context().add(p);
	}
};

template<>
struct control<continue_stmt> : pegtl::normal<continue_stmt>
{
	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		Pipeline p = stageutil::cnt();
		p.getHead()->setBase(state.base());
		state.context().add(p);
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
		Pipeline p = stageutil::expr(vec[0].value.expr);
		p.getHead()->setBase(state.base());
		state.context().add(p);
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
		std::string empty = "";
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

		for (int i = 0; i < vec.size(); i += 2) {
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
	static void success(Input& in, ParseState& state)
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
struct control<call_tail> : pegtl::normal<call_tail>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input& in, ParseState& state)
	{
		auto vec = state.get("e", true);
		assert(state.top().type == SeqEntity::EXPR);
		Expr *func = state.top().value.expr;
		Expr *arg = nullptr;

		if (vec.size() == 1) {
			arg = vec[0].value.expr;
		} else if (vec.size() > 1) {
			std::vector<Expr *> exprs;

			for (auto ent : vec)
				exprs.push_back(ent.value.expr);

			arg = new RecordExpr(exprs);
		}

		Expr *e = new CallExpr(func, arg);
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
struct control<cond_expr> : pegtl::normal<cond_expr>
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
		Expr *e = new CondExpr(vec[0].value.expr, vec[1].value.expr, vec[2].value.expr);
		state.add(e);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<array_type> : pegtl::normal<array_type>
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
		state.add((types::Type *)types::ArrayType::get(vec[0].value.type));
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
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
		std::string empty = "";
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

		for (int i = 0; i < vec.size(); i += 2) {
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
		auto vec = state.get("tt");
		state.add((types::Type *)types::FuncType::get(vec[0].value.type, vec[1].value.type));
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
		state.add((types::Type *)types::FuncType::get(types::VoidType::get(), vec[0].value.type));
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
		auto vec = state.get("t");
		state.add((types::Type *)types::FuncType::get(vec[0].value.type, types::VoidType::get()));
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
		state.add((types::Type *)types::FuncType::get(types::VoidType::get(), types::VoidType::get()));
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
