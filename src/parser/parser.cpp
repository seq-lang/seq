#include "grammar.h"
#include "seq/parser.h"

using namespace seq;

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
struct action<positive_integer> {
	template<typename Input>
	static void apply(const Input& in, ParseState& state)
	{
		const seq_int_t n = std::stol(in.string());
		assert(n >= 0);
		state.add(n);
	}
};

template<>
struct action<negative_integer> {
	template<typename Input>
	static void apply(const Input& in, ParseState& state)
	{
		const seq_int_t n = std::stol(in.string());
		assert(n < 0);
		state.add(n);
	}
};

template<>
struct action<name> {
	template<typename Input>
	static void apply(const Input& in, ParseState& state)
	{
		const char *name = strdup(in.string().c_str());
		state.add(name);
	}
};

template<>
struct action<call_stage> {
	static void apply0(ParseState& state)
	{
		auto vec = state.get("s");
		SeqEntity ent = state.lookup(vec[0].value.name);

		if (ent.type != SeqEntity::FUNC)
			throw exc::SeqException("cannot call non-function");

		Func *func = ent.value.func;
		Pipeline p = (*func)();
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
struct action<getitem_stage> {
	static void apply0(ParseState& state)
	{
		auto vec = state.get("i");
		Pipeline p = stageutil::get(vec[0].value.ival);
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
struct action<split_stage> {
	static void apply0(ParseState& state)
	{
		auto vec = state.get("ii");
		Pipeline p = stageutil::split(vec[0].value.ival, vec[1].value.ival);
		state.add(p);
	}
};

static BaseFunc *getBaseFromEnt(SeqEntity ent)
{
	switch (ent.type) {
		case SeqEntity::FUNC:
			return ent.value.func;
		case SeqEntity::MODULE:
			return ent.value.module;
		default:
			assert(0);
	}

	return nullptr;
}

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
struct action<var_expr> {
	static void apply0(ParseState& state)
	{
		auto vec = state.get("s");
		SeqEntity ent = state.lookup(vec[0].value.name);

		if (ent.type != SeqEntity::VAR)
			throw exc::SeqException("name '" + std::string(vec[0].value.name) + "' does not refer to a variable");

		Expr *expr = new VarExpr(ent.value.var);
		state.add(expr);
	}
};

template<>
struct action<void_type> {
	static void apply0(ParseState& state)
	{
		state.add((types::Type *)&types::Void);
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
struct action<array_type> {
	static void apply0(ParseState& state)
	{
		auto vec = state.get("t");
		state.add((types::Type *)types::ArrayType::get(vec[0].value.type));
	}
};

template<>
struct action<record_type> {
	static void apply0(ParseState& state)
	{
		auto vec = state.get("t", true);
		std::vector<types::Type *> types;

		for (auto ent : vec)
			types.push_back(ent.value.type);

		state.add((types::Type *)types::RecordType::get(types));
	}
};

/*
 * Control
 */
static Pipeline addPipelineGeneric(SeqEntity ent, Pipeline p)
{
	switch (ent.type) {
		case SeqEntity::MODULE:
			return *ent.value.module | p;
		case SeqEntity::FUNC:
			return *ent.value.func | p;
		case SeqEntity::PIPELINE:
			ent.value.pipeline | p;
			break;
		case SeqEntity::VAR:
			p = *ent.value.var | p;
			break;
		default:
			throw exc::SeqException("misplaced pipeline");
	}

	return p;
}

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
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

static PipelineList& makePL(SeqEntity ent, ParseState& state)
{
	switch (ent.type) {
		case SeqEntity::PIPELINE:
			return *new PipelineList(ent.value.pipeline);
		case SeqEntity::VAR:
			return *new PipelineList(ent.value.var);
		case SeqEntity::NAME:
			return makePL(state.lookup(ent.value.name), state);
		default:
			throw exc::SeqException("misplaced expression in record expression");
	}
}

static PipelineList& addPL(PipelineList& pl, SeqEntity ent, ParseState& state)
{
	switch (ent.type) {
		case SeqEntity::PIPELINE:
			return (pl, ent.value.pipeline);
		case SeqEntity::VAR:
			return (pl, *ent.value.var);
		case SeqEntity::NAME:
			return addPL(pl, state.lookup(ent.value.name), state);
		default:
			throw exc::SeqException("misplaced expression in record expression");
	}
}

template<>
struct control<getitem_stage> : pegtl::normal<getitem_stage>
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
		auto vec = state.get("*", true);
		PipelineList& pl = makePL(vec[0], state);
		for (int i = 1; i < vec.size(); i++)
			pl = addPL(pl, vec[i], state);
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
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<pipeline> : pegtl::normal<pipeline>
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

		p.getHead()->setBase(getBaseFromEnt(state.base()));
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
		state.push();
		state.enter(new SeqModule());
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("s");
		assert(state.context().type == SeqEntity::MODULE);
		auto *module = state.context().value.module;
		state.unscope();
		state.exit();
		state.addmod(vec[0].value.name, module);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.unscope();
		state.pop();
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
		auto vec = state.get("stt");
		assert(state.context().type == SeqEntity::FUNC);
		auto *func = state.context().value.func;
		func->setInOut(vec[1].value.type, vec[2].value.type);
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
		state.enter(state.top());
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		state.unscope();
		state.pop();
		assert(state.top().type == SeqEntity::PIPELINE &&
		       state.context().type == SeqEntity::PIPELINE);
		state.top().value.pipeline = makePipelineFromLinkedStage(state.context().value.pipeline.getHead());
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
struct control<pipeline_module_stmt_toplevel> : pegtl::normal<pipeline_module_stmt_toplevel>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("p");
		SeqEntity ent = state.context();
		Pipeline p = vec[0].value.pipeline;
		addPipelineGeneric(ent, p);
	}

	template<typename Input>
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

template<>
struct control<pipeline_module_stmt_nested> : pegtl::normal<pipeline_module_stmt_nested>
{
	template<typename Input>
	static void start(Input&, ParseState& state)
	{
		state.push();
	}

	template<typename Input>
	static void success(Input&, ParseState& state)
	{
		auto vec = state.get("p");
		SeqEntity ent = state.context();
		Pipeline p = vec[0].value.pipeline;
		p = addPipelineGeneric(ent, p);
		state.add(p);
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
		addPipelineGeneric(ent, p);
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
		p = addPipelineGeneric(ent, p);
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
		p.getHead()->setBase(getBaseFromEnt(state.base()));
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
		p.getHead()->setBase(getBaseFromEnt(state.base()));
		auto *var = new Var(true);
		p = addPipelineGeneric(state.context(), p);
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
struct control<array_lookup_expr> : pegtl::normal<array_lookup_expr>
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
		Expr *arr = vec[0].value.expr;
		Expr *idx = vec[1].value.expr;
		Expr *e = new ArrayLookupExpr(arr, idx);
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
	static void failure(Input&, ParseState& state)
	{
		state.pop();
	}
};

#include <tao/pegtl/analyze.hpp>

ParseState seq::parse(std::string input)
{
	ParseState state;
	pegtl::file_input<> in(input);
	const size_t issues_found = pegtl::analyze<grammar>();
	assert(issues_found == 0);
	pegtl::parse<grammar, action, control>(in, state);
	return state;
}
