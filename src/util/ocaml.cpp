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
FOREIGN types::Type *byte_type()        { return types::Byte; }
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

FOREIGN char *get_type_name(types::Type *ex)
{
	return strdup(ex->getName().c_str());
}

FOREIGN types::Type *record_type_named(const char **names, types::Type **ty, size_t sz, const char *name)
{
	vector<string> s;
	for (size_t i = 0; i < sz; i++) {
		s.emplace_back(string(names[i]));
		free(names[i]);
	}

	return types::RecordType::get(vector<types::Type *>(ty, ty + sz), s, string(name));
}

FOREIGN types::Type *record_type(const char **names, types::Type **ty, size_t sz)
{
	return record_type_named(names, ty, sz, "");
}

FOREIGN types::Type *func_type(types::Type *ret, types::Type **ty, size_t sz)
{
	return types::FuncType::get(vector<types::Type *>(ty, ty + sz), ret);
}

FOREIGN types::Type *generator_type(types::Type *ret)
{
	return types::GenType::get(ret);
}


FOREIGN types::Type *ref_type(const char *name)
{
	auto t = types::RefType::get(name);
	free(name);
	return name;
}

FOREIGN types::Type *ptr_type(types::Type *base)
{
	return types::PtrType::get(base);
}

FOREIGN void set_ref_record(types::RefType *f, types::RecordType *rec)
{
	f->setContents(rec);
}

FOREIGN void add_ref_method(types::Type *ref, const char *name, Func *fn)
{
	ref->addMethod(name, fn, false);
	free(name);
	return t;
}

FOREIGN void set_ref_done(types::RefType *ref)
{
	ref->setDone();
}

/***** Expressions *****/

FOREIGN Expr *none_expr()                 { return new NoneExpr(); }
FOREIGN Expr *bool_expr(char b)           { return new BoolExpr(b); }
FOREIGN Expr *int_expr(int i)             { return new IntExpr(i); }
FOREIGN Expr *float_expr(double f)        { return new FloatExpr(f); }
FOREIGN Expr *str_expr(const char *s)     { 
		auto t = StrExpr(string(s)); free(s); return t; 
	}
FOREIGN Expr *str_seq_expr(const char *s) { 
		auto t = SeqExpr(string(s)); free(s); return t; 
	}
FOREIGN Expr *func_expr(Func *f)          { return new FuncExpr(f); }
FOREIGN Expr *var_expr(Var *v)            { return new VarExpr(v); }
FOREIGN Expr *type_expr(types::Type *t)   { return new TypeExpr(t); }

FOREIGN Expr *is_expr(Expr *lhs, Expr *rhs)
{
	return new IsExpr(lhs, rhs);
}

FOREIGN Expr *cond_expr(Expr *cond, Expr *ift, Expr *iff)
{
	return new CondExpr(cond, ift, iff);
}

FOREIGN Expr *uop_expr(const char *op, Expr *lhs)
{
	auto t = new UOpExpr(uop(string(op)), lhs);
	free(op);
	return t;
}

FOREIGN Expr *bop_expr(const char *op, Expr *lhs, Expr *rhs)
{
	auto t = new BOpExpr(bop(string(op)), lhs, rhs);
	free(op);
	return t;
}

FOREIGN Expr *call_expr(Expr *fn, Expr **args, size_t size)
{
	return new CallExpr(fn, vector<Expr *>(args, args + size));
}

FOREIGN Expr *partial_expr(Expr *fn, Expr **args, size_t size)
{
	return new PartialCallExpr(fn, vector<Expr *>(args, args + size));
}

FOREIGN Expr *pipe_expr(Expr **args, size_t size)
{
	return new PipeExpr(vector<Expr *>(args, args + size));
}

FOREIGN Expr *get_elem_expr(Expr *lhs, const char *rhs)
{
	auto t = new GetElemExpr(lhs, string(rhs));
	free(rhs);
	return t;
}

FOREIGN Expr *array_expr(types::Type *ty, Expr *cnt)
{
	return new ArrayExpr(ty, cnt);
}

FOREIGN Expr *construct_expr(types::Type *ty, Expr **args, size_t len)
{
	return new ConstructExpr(ty, vector<Expr *>(args, args + len));
}

FOREIGN Expr *array_lookup_expr(Expr *lhs, Expr *rhs)
{
	return new ArrayLookupExpr(lhs, rhs);
}

FOREIGN Expr *array_slice_expr(Expr *arr, Expr *st, Expr *ed)
{
	return new ArraySliceExpr(arr, st, ed);
}

FOREIGN Expr *array_contains_expr(Expr *lhs, Expr *rhs)
{
	return new ArrayContainsExpr(lhs, rhs);
}

FOREIGN Expr *record_expr(Expr **args, size_t size)
{
	return new RecordExpr(
	             vector<Expr *>(args, args + size),
	             vector<string>(size, " "));
}

FOREIGN Expr *static_expr(types::Type *ty, const char *name)
{
	auto t = new GetStaticElemExpr(ty, string(name));
	free(name);
	return t;
}

FOREIGN Expr *method_expr(Expr *expr, const char *name, types::Type **types, size_t len)
{
	auto t = new MethodExpr(expr, name, vector<types::Type *>(types, types + len));
	free(name);
	return t;
}

FOREIGN Expr *list_expr(types::Type *ty, Expr **args, size_t len)
{
	return new ListExpr(vector<Expr *>(args, args + len), ty);
}

FOREIGN Expr *set_expr(types::Type *ty, Expr **args, size_t len)
{
	return new SetExpr(vector<Expr *>(args, args + len), ty);
}

FOREIGN Expr *dict_expr(types::Type *ty, Expr **args, size_t len)
{
	return new DictExpr(vector<Expr *>(args, args + len), ty);
}

FOREIGN Expr *list_comp_expr(types::Type *ty, Expr *val, For *body)
{
	return new ListCompExpr(val, body, ty);
}

FOREIGN Expr *set_comp_expr(types::Type *ty, Expr *val, For *body)
{
	return new SetCompExpr(val, body, ty);
}

FOREIGN Expr *dict_comp_expr(types::Type *ty, Expr *key, Expr *val, For *body)
{
	return new DictCompExpr(key, val, body, ty);
}

FOREIGN Expr *gen_expr(Expr *val, For *body, Var **captures, size_t len)
{
	return new GenExpr(val, body, vector<Var *>(captures, captures + len));
}

/***** Statements *****/

FOREIGN Stmt *pass_stmt()
{
	return nullptr;
}

FOREIGN Stmt *break_stmt()
{
	return new Break();
}

FOREIGN Stmt *continue_stmt()
{
	return new Continue();
}

FOREIGN Stmt *assert_stmt(Expr *e)
{
	return new Assert(e);
}

FOREIGN Stmt *expr_stmt(Expr *e)
{
	return new ExprStmt(e);
}

FOREIGN Stmt *var_stmt(Expr *v)
{
	return new VarStmt(v);
}

FOREIGN Var *var_stmt_var(VarStmt *v)
{
	return v->getVar();
}

FOREIGN Stmt *assign_stmt(Var *v, Expr *rhs)
{
	return new Assign(v, rhs);
}

FOREIGN Stmt *assign_member_stmt(Expr *lh, const char *name, Expr *rh)
{
	auto t = new AssignMember(lh, name, rh);
	free(name);
	return t;
}

FOREIGN Stmt *assign_index_stmt(Expr *lh, Expr *rh, Expr *rhs)
{
	return new AssignIndex(lh, rh, rhs);
}

FOREIGN Stmt *del_index_stmt(Expr *lh, Expr *rh)
{
	return new DelIndex(lh, rh);
}

FOREIGN Stmt *print_stmt(Expr *e)
{
	return new Print(e);
}

FOREIGN Stmt *if_stmt()
{
	return new If();
}

FOREIGN Stmt *trycatch_stmt()
{
	return new TryCatch();
}

FOREIGN Stmt *throw_stmt(Expr *expr)
{
	return new Throw(expr);
}

FOREIGN Stmt *while_stmt(Expr *cond)
{
	return new While(cond);
}

FOREIGN Stmt *for_stmt(Expr *gen)
{
	return new For(gen);
}

FOREIGN Stmt *return_stmt(Expr *e)
{
	return new Return(e);
}

FOREIGN Stmt *yield_stmt(Expr *e)
{
	return new Yield(e);
}

FOREIGN Stmt *func_stmt(Func *f)
{
	return new FuncStmt(f);
}

/***** Functions *****/

FOREIGN Var *get_module_arg(SeqModule *m)
{
	return m->getArgVar();
}

FOREIGN Func *func(const char *name)
{
	auto *f = new Func();
	f->setName(string(name));
	free(name);
	return f;
}

FOREIGN void set_func_out(Func *f, types::Type *typ)
{
	f->setOut(typ);
}

FOREIGN Block *get_func_block(Func *st)
{
	return st->getBlock();
}

FOREIGN Var *get_func_arg(Func *f, const char *arg)
{
	auto t = f->getArgVar(string(arg));
	free(arg);
	return t;
}

FOREIGN void set_func_enclosing(Func *f, Func *outer)
{
	f->setEnclosingFunc(outer);
}

FOREIGN void set_func_return(Func *f, Return *ret)
{
	f->sawReturn(ret);
}

FOREIGN void set_func_yield(Func *f, Yield *ret)
{
	f->sawYield(ret);
}

FOREIGN void set_func_params(Func *f, const char **names, types::Type **types, size_t len)
{
	vector<string> s;
	for (size_t i = 0; i < len; i++) {
		s.emplace_back(string(names[i]));
		free(names[i]);
	}

	f->setIns(vector<types::Type *>(types, types + len));
	f->setArgNames(s);
}

FOREIGN void set_func_generics(Func *fn, int n)
{
	fn->addGenerics(n);
}

FOREIGN types::Type *get_func_generic(Func *fn, int idx)
{
	return fn->getGeneric(idx);
}

FOREIGN void set_func_extern(Func *fn)
{
	fn->setExternal();
}

FOREIGN char *get_expr_name(Expr *ex)
{
	return strdup(ex->getName().c_str());
}

FOREIGN void set_func_generic_name(Func *fn, int idx, const char *name)
{
	fn->getGeneric(idx)->setName(name);
	free(name);
}

FOREIGN void set_ref_generics(types::RefType *fn, int n)
{
	fn->addGenerics(n);
}

FOREIGN types::Type *get_ref_generic(types::RefType *fn, int idx)
{
	return fn->getGeneric(idx);
}

FOREIGN void set_ref_generic_name(types::RefType *fn, int idx, const char *name)
{
	fn->getGeneric(idx)->setName(name);
	free(name);
}

FOREIGN types::Type *realize_type(types::RefType *t, types::Type **types, size_t sz)
{
	if (sz == 0) return t;
	return t->realize(vector<types::Type *>(types, types + sz));
}

FOREIGN BaseFunc *realize_func(FuncExpr *e, types::Type **types, size_t sz)
{
	auto *fn = dynamic_cast<Func *>(e->getFunc());
	if (sz == 0) return fn;
	return fn->realize(vector<types::Type *>(types, types + sz));
}

FOREIGN Stmt *match_stmt(Expr *cond)
{
	auto m = new Match();
	m->setValue(cond);
	return m;
}

FOREIGN Block *add_match_case(Match *m, Pattern *p)
{
	return m->addCase(p);
}

FOREIGN Pattern *bound_pattern(Pattern *p)
{
	return new BoundPattern(p);
}

FOREIGN Pattern *guarded_pattern(Pattern *p, Expr *e)
{
	return new GuardedPattern(p, e);
}

FOREIGN Pattern *star_pattern()
{
	return new StarPattern();
}

FOREIGN Pattern *wildcard_pattern(void)
{
	return new Wildcard();
}

FOREIGN Pattern *int_pattern(int i)
{
	return new IntPattern(i);
}

FOREIGN Pattern *str_pattern(const char *c)
{
	auto t = new StrPattern(string(c));
	free(c);
	return t;
}

FOREIGN Pattern *range_pattern(int a, int b)
{
	return new RangePattern(a, b);
}

FOREIGN Pattern *record_pattern(Pattern **p, size_t size)
{
	return new RecordPattern(vector<Pattern *>(p, p + size));
}

FOREIGN Pattern *or_pattern(Pattern **p, size_t size)
{
	return new OrPattern(vector<Pattern *>(p, p + size));
}

FOREIGN Pattern *array_pattern(Pattern **p, size_t size)
{
	return new ArrayPattern(vector<Pattern *>(p, p + size));
}

FOREIGN Pattern *seq_pattern(const char *c)
{
	auto t = new SeqPattern(string(c));
	free(c);
	return t;
}

FOREIGN Pattern *bool_pattern(bool b)
{
	return new BoolPattern(b);
}

FOREIGN Var *get_bound_pattern_var(BoundPattern *b)
{
	return b->getVar();
}

FOREIGN void set_pattern_trycatch(Pattern *p, TryCatch *tc)
{
	p->setTryCatch(tc);
}

/***** Block utils *****/

FOREIGN Block *get_module_block(SeqModule *st)  { return st->getBlock(); }
FOREIGN Block *get_while_block(While *st)       { return st->getBlock(); }
FOREIGN Block *get_for_block(For *st)           { return st->getBlock(); }
FOREIGN Block *get_else_block(If *st)           { E("else_block"); return st->addElse(); }
FOREIGN Block *get_elif_block(If *st, Expr *ex) { E("cond_block"); return st->addCond(ex); }
FOREIGN Block *get_trycatch_block(TryCatch *tc) { return tc->getBlock(); }

FOREIGN Block *get_trycatch_catch(TryCatch *tc, types::Type *t)
{
	E("catch_block");
	return tc->addCatch(t);
}

FOREIGN Block *get_trycatch_finally(TryCatch *tc)
{
	E("finally_block");
	return tc->getFinally();
}

FOREIGN Var *get_trycatch_var(TryCatch *tc, unsigned idx)
{
	return tc->getVar(idx);
}

FOREIGN void set_enclosing_trycatch(Expr *e, TryCatch *tc)
{
	e->setTryCatch(tc);
}

FOREIGN char type_eq(types::Type *a, types::Type *b)
{
	return types::is(a, b);
}


FOREIGN struct seq_srcinfo {
	char *file;
	int line;
	int col;
	int line;
};

FOREIGN void set_pos(SrcObject *obj, const char *f, int l, int c)
{
	if (!obj) return;
	obj->setSrcInfo(SrcInfo(string(f), l, c));
	free(f);
}

FOREIGN seq_srcinfo get_pos(SrcObject *obj)
{
	if (!obj) {
		return seq_srcinfo { (char *)"", 0, 0, 0 };
	}
	auto info = obj->getSrcInfo();
	return seq_srcinfo { strdup(info.file.c_str()), info.line, info.col, 0 };
}

FOREIGN Var *get_for_var(For *f)
{
	return f->getVar();
}

FOREIGN types::Type *get_type(Expr *e, BaseFunc *base)
{
	auto ret = e->getType();
	return ret;
}

FOREIGN BaseFunc *get_func(FuncExpr *e)
{
	return e->getFunc();
}

FOREIGN types::Type *get_var_type(Var *e)
{
	return e->getType();
}

FOREIGN void set_base(Stmt *st, BaseFunc *base)
{
	if (st) {
		st->setBase(base);
	}
}

FOREIGN void add_stmt(Stmt *st, Block *block)
{
	if (st) {
		block->add(st);
	}
}

FOREIGN void *init_module()
{
	return new SeqModule();
}

FOREIGN bool exec_module(
	SeqModule *sm, char **args, size_t arg_len,
	char debug, char **error, seq_srcinfo **srcInfo)
{
	try {
		vector<string> s;
		for (size_t i = 0; i < arg_len; i++) {
			s.emplace_back(string(args[i]));
			free(args[i]);
		}
		sm->execute(s, {}, debug);
		*error = nullptr;
		*srcInfo = nullptr;
		return true;
	} catch (exc::SeqException &e) {
		string msg = e.what();
		*error = strdup(msg.c_str());
		
		auto info = e.getSrcInfo();
		*srcInfo = new seq_srcinfo;
		(*srcInfo)->line = info.line;
		(*srcInfo)->col = info.col;
		(*srcInfo)->file = strdup(info.file.c_str());
		(*srcInfo)->len = 0;
		
		return false;
	}
}
