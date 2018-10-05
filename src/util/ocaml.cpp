#include "seq/seq.h"

using namespace std;
using namespace seq;

#define FOREIGN extern "C"

#ifdef IBRAHIM_DEBUG
#define FMT_HEADER_ONLY
#include "format.h"
#define E(f, ...) fmt::print(stderr, f "\n", ##__VA_ARGS__)
#else
#define E(...)
#endif


/***** Types *****/

FOREIGN types::Type *void_type()        { return types::Void; }
FOREIGN types::Type *bool_type()        { return types::Bool; }
FOREIGN types::Type *int_type()         { return types::Int; }
FOREIGN types::Type *float_type()       { return types::Float; }
FOREIGN types::Type *str_type()         { return types::Str; }
FOREIGN types::Type *str_seq_type()     { return types::Seq; }
FOREIGN types::Type *source_type()      { return types::Raw; }
FOREIGN types::Type *generic_type()     { return types::GenericType::get(); }

FOREIGN types::Type *array_type(types::Type *base)
{
	return types::ArrayType::get(base);
}

FOREIGN types::Type *record_type(const char **names, types::Type **ty, size_t sz)
{
	return types::RecordType::get(
	         vector<types::Type*>(ty, ty + sz),
	         vector<string>(names, names + sz));
}

FOREIGN types::Type *func_type(types::Type *ret, types::Type **ty, size_t sz)
{
	return types::FuncType::get(vector<types::Type*>(ty, ty + sz), ret);
}

FOREIGN types::Type *gen_type(types::Type *ret)
{
	return types::GenType::get(ret);
}

FOREIGN types::Type *ref_type(const char *name)
{
	auto f = types::RefType::get(name);
	return f;
}

FOREIGN void set_ref_record(types::RefType *f, types::RecordType *rec)
{
	E("set_ref_record[{}, {}]", f->getName(), rec->getName());
	f->setContents(rec);
}

FOREIGN void add_ref_method(types::Type *ref, const char *name, Func *fn)
{
	E("add_ref_method[{}, {}, {}]", ref->getName(), name, fn->genericName());
	ref->addMethod(name, fn, false);
}

FOREIGN void set_ref_done(types::RefType *ref)
{
	E("set_ref_done[{}]", ref->getName());
	ref->setDone();
}

/***** Expressions *****/

FOREIGN Expr *bool_expr(char b)           { E("bool_expr[{}]", b); return new BoolExpr(b); }
FOREIGN Expr *int_expr(int i)             { E("int_expr[{}]", i); return new IntExpr(i); }
FOREIGN Expr *float_expr(double f)        { E("float_expr[{}]", f); return new FloatExpr(f); }
FOREIGN Expr *str_expr(const char *s)     { E("str_expr[{}]", s); return new StrExpr(string(s)); }
FOREIGN Expr *str_seq_expr(const char *s) { E("seq_expr[{}]", s); return new SeqExpr(string(s)); }
FOREIGN Expr *func_expr(Func *f)          { E("func_expr[{}]", f->genericName()); return new FuncExpr(f); }
FOREIGN Expr *var_expr(Var *v)            { E("var_expr"); return new VarExpr(v); }
FOREIGN Expr *type_expr(types::Type *t)   { E("type_expr"); return new TypeExpr(t); }

FOREIGN Expr *cond_expr(Expr *cond, Expr *ift, Expr *iff)
{
	E("cond_expr");
	return new CondExpr(cond, ift, iff);
}

FOREIGN Expr *uop_expr(const char *op, Expr *lhs)
{
	E("uop_expr[{}]", op);
	return new UOpExpr(uop(string(op)), lhs);
}

FOREIGN Expr *bop_expr(const char *op, Expr *lhs, Expr *rhs)
{
	E("bop_expr[{}]", op);
	return new BOpExpr(bop(string(op)), lhs, rhs);
}

FOREIGN Expr *call_expr(Expr *fn, Expr **args, size_t size)
{
	E("call_expr[{}]", size);
	return new CallExpr(fn, vector<Expr*>(args, args + size));
}

FOREIGN Expr *partial_expr(Expr *fn, Expr **args, size_t size)
{
	E("partial_expr[{}]", size);
	return new PartialCallExpr(fn, vector<Expr*>(args, args + size));
}

FOREIGN Expr *pipe_expr(Expr **args, size_t size)
{
	E("pipe_expr[{}]", size);
	return new PipeExpr(vector<Expr*>(args, args + size));
}

FOREIGN Expr *get_elem_expr(Expr *lhs, const char *rhs)
{
	E("get_elem_expr[{}]", rhs);
	return new GetElemExpr(lhs, string(rhs));
}

FOREIGN Expr *array_expr(types::Type *ty, Expr *cnt)
{
	E("array_expr[{}]", ty->getName());
	return new ArrayExpr(ty, cnt);
}

FOREIGN Expr *construct_expr(types::Type *ty, Expr **args, size_t len)
{
	E("construct_expr[{}, {}]", ty->getName(), len);
	return new ConstructExpr(ty, vector<Expr*>(args, args + len));
}

FOREIGN Expr *array_lookup_expr(Expr *lhs, Expr *rhs)
{
	E("array_lookup_expr");
	return new ArrayLookupExpr(lhs, rhs);
}

FOREIGN Expr *array_slice_expr(Expr *arr, Expr *st, Expr *ed)
{
	E("array_slice_expr");
	return new ArraySliceExpr(arr, st, ed);
}

FOREIGN Expr *record_expr(Expr **args, size_t size)
{
	E("record_expr[{}]", size);
	return new RecordExpr(
	             vector<Expr*>(args, args + size),
	             vector<string>(size, " "));
}

FOREIGN Expr *static_expr(types::Type *ty, const char *name)
{
	E("static_expr[{}, {}]", ty->getName(), name);
	return new GetStaticElemExpr(ty, string(name));
}

FOREIGN Expr *method_expr(Expr *expr, Func *method)
{
	E("method_expr<?>");
	throw exc::SeqException("wrooong!");
	// return new MethodExpr(expr, method);
}

/***** Statements *****/

FOREIGN Stmt *pass_stmt()  // Needed at all?
{
	E("pass_stmt");
	return nullptr;
}

FOREIGN Stmt *break_stmt()
{
	E("break_stmt");
	return new Break();
}

FOREIGN Stmt *continue_stmt()
{
	E("continue_stmt");
	return new Continue();
}

FOREIGN Stmt *expr_stmt(Expr *e)
{
	E("expr_stmt");
	return new ExprStmt(e);
}

FOREIGN Stmt *var_stmt(Expr *v)
{
	E("var_stmt");
	return new VarStmt(v);
}

FOREIGN Var *var_stmt_var(VarStmt *v)
{
	E("var_stmt_var[{}]", v->getName());
	return v->getVar();
}

FOREIGN Stmt *assign_stmt(Var *v, Expr *rhs)
{
	E("assign_stmt");
	return new Assign(v, rhs);
}

FOREIGN Stmt *assign_member_stmt(Expr *lh, const char *name, Expr *rh)
{
	E("assign_member_stmt[{}]", name);
	return new AssignMember(lh, name, rh);
}

FOREIGN Stmt *assign_index_stmt(Expr *lh, Expr *rh, Expr *rhs)
{
	E("assign_index_stmt");
	return new AssignIndex(lh, rh, rhs);
}

FOREIGN Stmt *print_stmt(Expr *e)
{
	E("print_stmt");
	return new Print(e);
}

FOREIGN Stmt *if_stmt()
{
	E("if_stmt");
	return new If();
}

FOREIGN Stmt *while_stmt(Expr *cond)
{
	E("while_stmt");
	return new While(cond);
}

FOREIGN Stmt *for_stmt(Expr *gen)
{
	E("for_stmt");
	return new For(gen);
}

FOREIGN Stmt *return_stmt(Expr *e)
{
	E("return_stmt");
	return new Return(e);
}

FOREIGN Stmt *yield_stmt(Expr *e)
{
	E("yield_stmt");
	return new Yield(e);
}

FOREIGN Stmt *func_stmt(Func *f)
{
	E("func_stmt[{}]", f->genericName());
	return new FuncStmt(f);
}

/***** Functions *****/

FOREIGN Func *func(const char *name)
{
	E("func[{}]", name);
	auto *f = new Func();
	f->setName(string(name));
	return f;
}

FOREIGN void set_func_out(Func *f, types::Type *typ)
{
	f->setOut(typ);
}

FOREIGN Block *get_func_block(Func *st)
{
	E("get_func_block[{}]", st->genericName());
	return st->getBlock();
}

FOREIGN Var *get_func_arg(Func *f, const char *arg)
{
	E("get_func_arg[{}, {}]", f->genericName(), arg);
	return f->getArgVar(string(arg));
}

FOREIGN void set_func_return(Func *f, Return *ret)
{
	E("set_func_return[{}]", f->genericName());
	f->sawReturn(ret);
}

FOREIGN void set_func_yield(Func *f, Yield *ret)
{
	E("set_func_yield[{}]", f->genericName());
	f->sawYield(ret);
}

FOREIGN void set_func_params(Func *f, const char **names, types::Type **types, size_t len)
{
	E("set_func_params[{}, {}]", f->genericName(), len);
	f->setIns(vector<types::Type*>(types, types + len));
	f->setArgNames(vector<string>(names, names + len));
}

FOREIGN void set_func_generics(Func *fn, int n)
{
	E("set_func_generics[{}, {}]", fn->genericName(), n);
	fn->addGenerics(n);
}

FOREIGN types::Type *get_func_generic(Func *fn, int idx)
{
	E("get_func_generic[{}, {}]", fn->genericName(), idx);
	return fn->getGeneric(idx);
}

FOREIGN char *get_expr_name(Expr *ex)
{
	string s = ex->getName();
	auto *c = new char[s.size() + 1];
	strncpy(c, s.c_str(), s.size());
	c[s.size()] = 0;
	return c;
}

FOREIGN void set_func_generic_name(Func *fn, int idx, const char *name)
{
	E("set_func_generic_name[{}, {}, {}]", fn->genericName(), idx, name);
	fn->getGeneric(idx)->setName(name);
}

FOREIGN void set_ref_generics(types::RefType *fn, int n)
{
	E("set_ref_generics[{}, {}]", fn->getName(), n);
	fn->addGenerics(n);
}

FOREIGN types::Type *get_ref_generic(types::RefType *fn, int idx)
{
	E("get_ref_generic[{}, {}]", fn->getName(), idx);
	return fn->getGeneric(idx);
}

FOREIGN void set_ref_generic_name(types::RefType *fn, int idx, const char *name)
{
	E("set_ref_generic_name[{}, {}, {}]", fn->getName(), idx, name);
	fn->getGeneric(idx)->setName(name);
}

FOREIGN types::Type *realize_type(types::RefType *t, types::Type **types, size_t sz)
{
	if (sz == 0) return t;
	return t->realize(vector<types::Type*>(types, types + sz));
}

FOREIGN BaseFunc *realize_func(FuncExpr *e, types::Type **types, size_t sz)
{
	Func *fn = dynamic_cast<Func*>(e->getFunc());
	if (sz == 0) return fn;
	return fn->realize(vector<types::Type*>(types, types + sz));
}

FOREIGN Stmt *match_stmt(Expr *cond)
{
	E("match_stmt");
	auto m = new Match();
	m->setValue(cond);
	return m;
}

FOREIGN Block *add_match_case(Match *m, Pattern *p)
{
	E("add_match_case");
	return m->addCase(p);
}

FOREIGN Pattern *bound_pattern(Pattern *p)
{
	E("bound_pattern");
	return new BoundPattern(p);
}

FOREIGN Pattern *wildcard_pattern(void)
{
	E("wildcard_pattern");
	return new Wildcard();
}

FOREIGN Pattern *int_pattern(int i)
{
	E("int_pattern[{}]", i);
	return new IntPattern(i);
}

FOREIGN Pattern *str_pattern(const char *c)
{
	E("str_pattern[{}]", c);
	return new StrPattern(string(c));
}

FOREIGN Pattern *seq_pattern(const char *c)
{
	E("seq_pattern[{}]", c);
	return new SeqPattern(string(c));
}

FOREIGN Pattern *bool_pattern(bool b)
{
	E("bool_pattern[{}]", b);
	return new BoolPattern(b);
}

FOREIGN Var *get_bound_pattern_var(BoundPattern *b)
{
	E("get_bound_pattern_var");
	return b->getVar();
}

/***** Block utils *****/

FOREIGN Block *get_module_block(SeqModule *st)  { return st->getBlock(); }
FOREIGN Block *get_while_block(While *st)       { return st->getBlock(); }
FOREIGN Block *get_for_block(For *st)           { return st->getBlock(); }
FOREIGN Block *get_else_block(If *st)           { E("else_block"); return st->addElse(); }
FOREIGN Block *get_elif_block(If *st, Expr *ex) { E("cond_block"); return st->addCond(ex); }

FOREIGN char type_eq(types::Type *a, types::Type *b)
{
	return types::is(a, b);
}


FOREIGN struct seq_srcinfo {
	char *file;
	int line;
	int col;
};

FOREIGN void set_pos(SrcObject *obj, const char *f, int l, int c)
{
	if (obj) {
		// int type = dynamic_cast<Expr*>(obj) ? 1 : (dynamic_cast<Stmt*>(obj) ? 2 : 0);
		// E("set_pos[{}]", obj->getName());
		obj->setSrcInfo(SrcInfo(string(f), l, c));
	}
}

FOREIGN seq_srcinfo get_pos(SrcObject *obj)
{
	if (!obj) {
		return seq_srcinfo { (char*)"", 0, 0 };
	}
	auto info = obj->getSrcInfo();
	auto *c = new char[info.file.size() + 1];
	strncpy(c, info.file.c_str(), info.file.size());
	c[info.file.size()] = 0;
	return seq_srcinfo { c, info.line, info.col };
}

FOREIGN Var *get_for_var(For *f)
{
	E("get_for_var");
	return f->getVar();
}

FOREIGN types::Type *get_type(Expr *e, BaseFunc *base)
{
	E("get_type");
	auto ret = e->getType();
	return ret;
}

FOREIGN BaseFunc *get_func(FuncExpr *e)
{
	E("get_func");
	return e->getFunc();
}

FOREIGN types::Type *get_var_type(Var *e)
{
	E("get_var_type");
	return e->getType();
}

FOREIGN void set_base(Stmt *st, BaseFunc *base)
{
	if (st) {
		E("set_base[{}]", st->getName());
		st->setBase(base);
	}
}

FOREIGN void add_stmt(Stmt *st, Block *block)
{
	if (st) {
		E("add_stmt[{}]", st->getName());
		block->add(st);
	}
}

FOREIGN void *init_module()
{
	E("init_module");
	return new SeqModule();
}

FOREIGN bool exec_module(SeqModule *sm, char debug, char **error, seq_srcinfo **srcInfo)
{
	E("exec_module");
	try {
		sm->execute(vector<string>(), debug);
		*error = nullptr;
		*srcInfo = nullptr;
		return true;
	} catch (exc::SeqException &e) {
		string msg = e.what();
		*error = new char[msg.size() + 1];
		strncpy(*error, msg.c_str(), msg.size());
		error[msg.size()] = nullptr;

		auto info = e.getSrcInfo();
		*srcInfo = new seq_srcinfo;
		(*srcInfo)->line = info.line;
		(*srcInfo)->col = info.col;
		(*srcInfo)->file = new char[info.file.size() + 1];
		strncpy((*srcInfo)->file, info.file.c_str(), info.file.size());
		(*srcInfo)->file[info.file.size()] = 0;

		return false;
	}
}
