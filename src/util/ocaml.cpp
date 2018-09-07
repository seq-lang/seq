#include "seq/seq.h"

using namespace std;
using namespace seq;

#define FOREIGN extern "C"

/***** Types *****/

FOREIGN types::Type *void_type()        { return types::VoidType::get(); }
FOREIGN types::Type *bool_type()        { return types::BoolType::get(); }
FOREIGN types::Type *int_type()         { return types::IntType::get(); }
FOREIGN types::Type *float_type()       { return types::FloatType::get(); }
FOREIGN types::Type *str_type()         { return types::StrType::get(); }
FOREIGN types::Type *seq_type()         { return types::SeqType::get(); }
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

FOREIGN types::Type *ref_type(const char *name)
{
	auto f = types::RefType::get(name);
	return f;
}

FOREIGN void set_ref_record(types::RefType *f, types::RecordType *rec) 
{
   f->setContents(rec);
}

FOREIGN void add_ref_method(types::Type *ref, const char *name, Func *fn)
{
	ref->addMethod(name, fn);
}

FOREIGN void set_ref_done(types::RefType *ref)
{
	ref->setDone();
}

/***** Expressions *****/

FOREIGN Expr *bool_expr(char b)       { return new BoolExpr(b); }
FOREIGN Expr *int_expr(int i)         { return new IntExpr(i); }
FOREIGN Expr *float_expr(double f)    { return new FloatExpr(f); }
FOREIGN Expr *str_expr(const char *s) { return new StrExpr(string(s)); }
FOREIGN Expr *func_expr(Func *f)      { return new FuncExpr(f); }
FOREIGN Expr *var_expr(Var *v)        { return new VarExpr(v); }

FOREIGN Expr *cond_expr(Expr *cond, Expr *ift, Expr *iff)
{
	return new CondExpr(cond, ift, iff);
}

FOREIGN Expr *uop_expr(const char *op, Expr *lhs)
{
	return new UOpExpr(uop(string(op)), lhs);
}

FOREIGN Expr *bop_expr(const char *op, Expr *lhs, Expr *rhs)
{
	return new BOpExpr(bop(string(op)), lhs, rhs);
}

FOREIGN Expr *call_expr(Expr *fn, Expr **args, size_t size)
{
	return new CallExpr(fn, vector<Expr*>(args, args + size));
}

FOREIGN Expr *get_elem_expr(Expr *lhs, const char *rhs)
{
	return new GetElemExpr(lhs, string(rhs));
}

FOREIGN Expr *array_expr(types::Type *ty, Expr *cnt)
{
	return new ArrayExpr(ty, cnt);
}

FOREIGN Expr *construct_expr(types::Type *ty, Expr **args, size_t len)
{
	return new ConstructExpr(ty, vector<Expr*>(args, args + len));
}

FOREIGN Expr *array_lookup_expr(Expr *lhs, Expr *rhs)
{
	return new ArrayLookupExpr(lhs, rhs);
}

FOREIGN Expr *array_slice_expr(Expr *arr, Expr *st, Expr *ed)
{
	return new ArraySliceExpr(arr, st, ed);
}

FOREIGN Expr *record_expr(Expr **args, size_t size)
{
	return new RecordExpr(
	             vector<Expr*>(args, args + size),
	             vector<string>(size, ""));
}

FOREIGN Expr *static_expr(types::Type *ty, const char *name)
{
	return new GetStaticElemExpr(ty, string(name));
}

FOREIGN Expr *method_expr(Expr *expr, Func *method)
{
	throw exc::SeqException("wrooong!");
	// return new MethodExpr(expr, method);
}

/***** Statements *****/

FOREIGN Stmt *pass_stmt()  // Needed at all?
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
	return new AssignMember(lh, name, rh);
}

FOREIGN Stmt *assign_index_stmt(Expr *lh, Expr *rh, Expr *rhs)
{
	return new AssignIndex(lh, rh, rhs);
}

FOREIGN Stmt *print_stmt(Expr *e)
{
	return new Print(e);
}

FOREIGN Stmt *if_stmt()
{
	return new If();
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

FOREIGN Func *func(const char *name) {
	auto *f = new Func();
	f->setName(string(name));
	// f->setOut(out);
	return f;
}

FOREIGN Block *get_func_block(Func *st)
{
	return st->getBlock();
}

FOREIGN Var *get_func_arg(Func *f, const char *arg)
{
	return f->getArgVar(string(arg));
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
	f->setIns(vector<types::Type*>(types, types + len));
	f->setArgNames(vector<string>(names, names + len));
}

FOREIGN void set_func_generics(Func *fn, int n)
{
   fn->addGenerics(n);
}

FOREIGN types::Type *get_func_generic(Func *fn, int idx)
{
   return fn->getGeneric(idx);
}

FOREIGN void set_func_generic_name(Func *fn, int idx, const char *name)
{
   fn->getGeneric(idx)->setName(name);
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
	return new StrPattern(string(c));
}

FOREIGN Pattern *bool_pattern(bool b)
{
	return new BoolPattern(b);
}

FOREIGN Var *get_bound_pattern_var(BoundPattern *b)
{
	return b->getVar();
}

/***** Block utils *****/

FOREIGN Block *get_module_block(SeqModule *st)  { return st->getBlock(); }
FOREIGN Block *get_while_block(While *st)       { return st->getBlock(); }
FOREIGN Block *get_for_block(For *st)           { return st->getBlock(); }
FOREIGN Block *get_else_block(If *st)           { return st->addElse(); }
FOREIGN Block *get_elif_block(If *st, Expr *ex) { return st->addCond(ex); }

FOREIGN struct seq_srcinfo {
   char *file;
   int line;
   int col;
};

FOREIGN void set_pos (SrcObject *obj, const char *f, int l, int c) 
{
   if (obj) {
      // int type = dynamic_cast<Expr*>(obj) ? 1 : (dynamic_cast<Stmt*>(obj) ? 2 : 0);
      obj->setSrcInfo(SrcInfo(string(f), l, c));
   }
}

FOREIGN seq_srcinfo get_pos (SrcObject *obj) 
{
   if (!obj) {
      return seq_srcinfo { (char*)"", 0, 0 };
   }
   auto info = obj->getSrcInfo();
   char *c = new char[info.file.size() + 1];
   strncpy(c, info.file.c_str(), info.file.size());
   c[info.file.size()] = 0;
   return seq_srcinfo { c, info.line, info.col };
}

FOREIGN Var *get_for_var(For *f)
{
	return f->getVar();
}

FOREIGN void set_base(Stmt *st, BaseFunc *base)
{
	if (st) st->setBase(base);
}

FOREIGN void add_stmt(Stmt *st, Block *block)
{
	if (st) block->add(st);
}

FOREIGN void *init_module()
{
	return new SeqModule();
}

FOREIGN bool exec_module(SeqModule *sm, char debug, char **error, seq_srcinfo **srcInfo)
{
	try {
		sm->execute(vector<string>(), debug);
      *error = NULL;
      *srcInfo = NULL;
      return true;
	} catch (exc::SeqException &e) {
      string msg = e.what();
      *error = new char[msg.size() + 1];
      strncpy(*error, msg.c_str(), msg.size());

      auto info = e.getSrcInfo();
      *srcInfo = new seq_srcinfo;
      (*srcInfo)->line = info.line;
      (*srcInfo)->col = info.col;
      (*srcInfo)->file = new char[info.file.size() + 1];
      strncpy((*srcInfo)->file, info.file.c_str(), info.file.size());

      return false;
	}
}
