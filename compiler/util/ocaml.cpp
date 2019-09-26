#include "seq/seq.h"

using namespace std;
using namespace seq;

#define FOREIGN extern "C"

#include <caml/fail.h>

/***** Types *****/

FOREIGN char *strxdup(const char *c, size_t s) {
  auto *d = (char *)malloc(s + 1);
  memcpy(d, c, s);
  d[s] = '\0';
  return d;
}

FOREIGN types::Type *void_type() { return types::Void; }
FOREIGN types::Type *bool_type() { return types::Bool; }
FOREIGN types::Type *byte_type() { return types::Byte; }
FOREIGN types::Type *int_type() { return types::Int; }
FOREIGN types::Type *float_type() { return types::Float; }
FOREIGN types::Type *str_type() { return types::Str; }
FOREIGN types::Type *str_seq_type() { return types::Seq; }
FOREIGN types::Type *generic_type() { return types::GenericType::get(); }

FOREIGN types::Type *iN_type(unsigned len) {
  return types::IntNType::get(len, true);
}

FOREIGN types::Type *uN_type(unsigned len) {
  return types::IntNType::get(len, false);
}

FOREIGN types::Type *kmer_type(unsigned k) { return types::KMer::get(k); }

FOREIGN types::Type *array_type(types::Type *base) {
  return types::ArrayType::get(base);
}

FOREIGN char *get_type_name(types::Type *ex) {
  return strdup(ex->getName().c_str());
}

FOREIGN types::Type *record_type_named(char **names, types::Type **ty,
                                       size_t sz, char *name) {
  vector<string> s;
  for (size_t i = 0; i < sz; i++) {
    s.emplace_back(names[i]);
    free(names[i]);
  }
  auto t = types::RecordType::get(vector<types::Type *>(ty, ty + sz), s,
                                  string(name));
  free(name);
  return t;
}

FOREIGN void set_record_names(types::RecordType *t, char **names,
                              types::Type **ty, size_t sz) {
  vector<string> s;
  for (size_t i = 0; i < sz; i++) {
    s.emplace_back(names[i]);
    free(names[i]);
  }
  t->setContents(vector<types::Type *>(ty, ty + sz), s);
}

FOREIGN types::Type *func_type(types::Type *ret, types::Type **ty, size_t sz) {
  return types::FuncType::get(vector<types::Type *>(ty, ty + sz), ret);
}

FOREIGN types::Type *generator_type(types::Type *ret) {
  return types::GenType::get(ret);
}

FOREIGN types::Type *ref_type(char *name) {
  auto t = types::RefType::get(name);
  free(name);
  return t;
}

FOREIGN types::Type *ptr_type(types::Type *base) {
  return types::PtrType::get(base);
}

FOREIGN void set_ref_record(types::RefType *f, types::RecordType *rec) {
  f->setContents(rec);
}

FOREIGN void add_ref_method(types::Type *ref, char *name, Func *fn) {
  ref->addMethod(name, fn, false);
  free(name);
}

FOREIGN void set_ref_done(types::RefType *ref) { ref->setDone(); }

FOREIGN types::Type *type_of_expr(Expr *expr) {
  return types::GenericType::get(expr);
}

/***** Expressions *****/

FOREIGN Expr *none_expr() { return new NoneExpr(); }

FOREIGN Expr *bool_expr(char b) { return new BoolExpr(b); }

FOREIGN Expr *int_expr(int64_t i) { return new IntExpr(i); }

FOREIGN Expr *bigint_expr(char *i) {
  throw exc::SeqException("not yet supported");
  free(i);
}

FOREIGN Expr *float_expr(double f) { return new FloatExpr(f); }

FOREIGN Expr *str_expr(char *s, size_t len) {
  Expr *t = new StrExpr(string(s, len));
  free(s);
  return t;
}

FOREIGN Expr *str_seq_expr(char *s, size_t len) {
  Expr *t = new SeqExpr(string(s, len));
  free(s);
  return t;
}

FOREIGN Expr *func_expr(Func *f) { return new FuncExpr(f); }

FOREIGN Expr *var_expr(Var *v) { return new VarExpr(v); }

FOREIGN void var_expr_set_atomic(VarExpr *ve) { ve->setAtomic(); }

FOREIGN Expr *type_expr(types::Type *t) { return new TypeExpr(t); }

FOREIGN Expr *is_expr(Expr *lhs, Expr *rhs) { return new IsExpr(lhs, rhs); }

FOREIGN Expr *is_not_expr(Expr *lhs, Expr *rhs) {
  return new UOpExpr(uop("!"), new IsExpr(lhs, rhs));
}

FOREIGN Expr *cond_expr(Expr *cond, Expr *ift, Expr *iff) {
  return new CondExpr(cond, ift, iff);
}

FOREIGN Expr *uop_expr(char *op, Expr *lhs) {
  auto t = new UOpExpr(uop(string(op)), lhs);
  free(op);
  return t;
}

FOREIGN Expr *bop_expr(char *op, Expr *lhs, Expr *rhs) {
  auto t = new BOpExpr(bop(string(op)), lhs, rhs);
  free(op);
  return t;
}

FOREIGN Expr *bop_expr_in_place(char *op, Expr *lhs, Expr *rhs) {
  auto t = new BOpExpr(bop(string(op)), lhs, rhs, true);
  free(op);
  return t;
}

FOREIGN Expr *call_expr(Expr *fn, Expr **args, size_t size) {
  return new CallExpr(fn, vector<Expr *>(args, args + size));
}

FOREIGN Expr *partial_expr(Expr *fn, Expr **args, size_t size) {
  return new PartialCallExpr(fn, vector<Expr *>(args, args + size));
}

FOREIGN Expr *pipe_expr(Expr **args, size_t size) {
  return new PipeExpr(vector<Expr *>(args, args + size));
}

FOREIGN void pipe_expr_set_parallel(PipeExpr *pipe, unsigned which) {
  pipe->setParallel(which);
}

FOREIGN Expr *get_elem_expr(Expr *lhs, char *rhs) {
  auto t = new GetElemExpr(lhs, string(rhs));
  free(rhs);
  return t;
}

FOREIGN Expr *array_expr(types::Type *ty, Expr *cnt) {
  return new ArrayExpr(ty, cnt);
}

FOREIGN Expr *array_expr_alloca(types::Type *ty, Expr *cnt) {
  return new ArrayExpr(ty, cnt, true);
}

FOREIGN Expr *construct_expr(types::Type *ty, Expr **args, size_t len) {
  return new ConstructExpr(ty, vector<Expr *>(args, args + len));
}

FOREIGN Expr *array_lookup_expr(Expr *lhs, Expr *rhs) {
  return new ArrayLookupExpr(lhs, rhs);
}

FOREIGN Expr *array_slice_expr(Expr *arr, Expr *st, Expr *ed) {
  return new ArraySliceExpr(arr, st, ed);
}

FOREIGN Expr *in_expr(Expr *lhs, Expr *rhs) {
  return new ArrayContainsExpr(lhs, rhs);
}

FOREIGN Expr *not_in_expr(Expr *lhs, Expr *rhs) {
  return new UOpExpr(uop("!"), new ArrayContainsExpr(lhs, rhs));
}

FOREIGN Expr *record_expr(Expr **args, size_t size) {
  return new RecordExpr(vector<Expr *>(args, args + size),
                        vector<string>(size, " "));
}

FOREIGN Expr *static_expr(types::Type *ty, char *name) {
  auto t = new GetStaticElemExpr(ty, string(name));
  free(name);
  return t;
}

FOREIGN Expr *list_expr(types::Type *ty, Expr **args, size_t len) {
  return new ListExpr(vector<Expr *>(args, args + len), ty);
}

FOREIGN Expr *set_expr(types::Type *ty, Expr **args, size_t len) {
  return new SetExpr(vector<Expr *>(args, args + len), ty);
}

FOREIGN Expr *dict_expr(types::Type *ty, Expr **args, size_t len) {
  return new DictExpr(vector<Expr *>(args, args + len), ty);
}

FOREIGN Expr *list_comp_expr(types::Type *ty, Expr *val) {
  return new ListCompExpr(val, nullptr, ty);
}

FOREIGN Expr *set_comp_expr(types::Type *ty, Expr *val) {
  return new SetCompExpr(val, nullptr, ty);
}

FOREIGN Expr *dict_comp_expr(types::Type *ty, Expr *key, Expr *val) {
  return new DictCompExpr(key, val, nullptr, ty);
}

FOREIGN void set_list_comp_body(ListCompExpr *e, For *body) {
  e->setBody(body);
}

FOREIGN void set_set_comp_body(SetCompExpr *e, For *body) { e->setBody(body); }

FOREIGN void set_dict_comp_body(DictCompExpr *e, For *body) {
  e->setBody(body);
}

FOREIGN void set_gen_comp_body(GenExpr *e, For *body) { e->setBody(body); }

FOREIGN Expr *gen_comp_expr(Expr *val, Var **captures, size_t len) {
  return new GenExpr(val, nullptr, vector<Var *>(captures, captures + len));
}

FOREIGN Expr *method_expr(Expr *self, Func *func) {
  return new MethodExpr(self, func);
}

FOREIGN Expr *typeof_expr(Expr *val) { return new TypeOfExpr(val); }

FOREIGN Expr *ptr_expr(Var *val) { return new VarPtrExpr(val); }

FOREIGN Expr *atomic_xchg_expr(Var *lhs, Expr *rhs) {
  return new AtomicExpr(AtomicExpr::Op::XCHG, lhs, rhs);
}
FOREIGN Expr *atomic_add_expr(Var *lhs, Expr *rhs) {
  return new AtomicExpr(AtomicExpr::Op::ADD, lhs, rhs);
}
FOREIGN Expr *atomic_sub_expr(Var *lhs, Expr *rhs) {
  return new AtomicExpr(AtomicExpr::Op::SUB, lhs, rhs);
}
FOREIGN Expr *atomic_and_expr(Var *lhs, Expr *rhs) {
  return new AtomicExpr(AtomicExpr::Op::AND, lhs, rhs);
}
FOREIGN Expr *atomic_nand_expr(Var *lhs, Expr *rhs) {
  return new AtomicExpr(AtomicExpr::Op::NAND, lhs, rhs);
}
FOREIGN Expr *atomic_or_expr(Var *lhs, Expr *rhs) {
  return new AtomicExpr(AtomicExpr::Op::OR, lhs, rhs);
}
FOREIGN Expr *atomic_xor_expr(Var *lhs, Expr *rhs) {
  return new AtomicExpr(AtomicExpr::Op::XOR, lhs, rhs);
}
FOREIGN Expr *atomic_max_expr(Var *lhs, Expr *rhs) {
  return new AtomicExpr(AtomicExpr::Op::MAX, lhs, rhs);
}
FOREIGN Expr *atomic_min_expr(Var *lhs, Expr *rhs) {
  return new AtomicExpr(AtomicExpr::Op::MIN, lhs, rhs);
}

/***** Statements *****/

FOREIGN Stmt *pass_stmt() { return nullptr; }

FOREIGN Stmt *break_stmt() { return new Break(); }

FOREIGN Stmt *continue_stmt() { return new Continue(); }

FOREIGN Stmt *assert_stmt(Expr *e) { return new Assert(e); }

FOREIGN Stmt *expr_stmt(Expr *e) { return new ExprStmt(e); }

FOREIGN Stmt *var_stmt(Expr *v, types::Type *t) { return new VarStmt(v, t); }

FOREIGN Var *var_stmt_var(VarStmt *v) { return v->getVar(); }

FOREIGN Stmt *assign_stmt(Var *v, Expr *rhs) { return new Assign(v, rhs); }

FOREIGN void assign_stmt_set_atomic(Assign *a) { a->setAtomic(); }

FOREIGN Stmt *assign_member_stmt(Expr *lh, char *name, Expr *rh) {
  auto t = new AssignMember(lh, name, rh);
  free(name);
  return t;
}

FOREIGN Stmt *assign_index_stmt(Expr *lh, Expr *rh, Expr *rhs) {
  return new AssignIndex(lh, rh, rhs);
}

FOREIGN Stmt *del_stmt(Var *v) { return new Del(v); }

FOREIGN Stmt *del_index_stmt(Expr *lh, Expr *rh) {
  return new DelIndex(lh, rh);
}

FOREIGN Stmt *print_stmt(Expr *e) { return new Print(e); }

FOREIGN Stmt *print_stmt_repl(Expr *e) { return new Print(e, true); }

FOREIGN Stmt *if_stmt() { return new If(); }

FOREIGN Stmt *trycatch_stmt() { return new TryCatch(); }

FOREIGN Stmt *throw_stmt(Expr *expr) { return new Throw(expr); }

FOREIGN Stmt *while_stmt(Expr *cond) { return new While(cond); }

FOREIGN Stmt *for_stmt(Expr *gen) { return new For(gen); }

FOREIGN Stmt *return_stmt(Expr *e) { return new Return(e); }

FOREIGN Stmt *yield_stmt(Expr *e) { return new Yield(e); }

FOREIGN Stmt *prefetch_stmt(Expr **where, Expr **keys, size_t len) {
  return new Prefetch(vector<Expr *>(keys, keys + len),
                      vector<Expr *>(where, where + len));
}

FOREIGN Stmt *func_stmt(Func *f) { return new FuncStmt(f); }

/***** Functions *****/

FOREIGN Var *get_module_arg(SeqModule *m) { return m->getArgVar(); }

FOREIGN Func *func(char *name) {
  auto *f = new Func();
  f->setName(string(name));
  free(name);
  return f;
}

FOREIGN void set_func_out(Func *f, types::Type *typ) { f->setOut(typ); }

FOREIGN Block *get_func_block(Func *st) { return st->getBlock(); }

FOREIGN Var *get_func_arg(Func *f, char *arg) {
  auto t = f->getArgVar(string(arg));
  free(arg);
  return t;
}

FOREIGN char *get_func_names(Expr *e) {
  auto *fn = dynamic_cast<FuncExpr *>(e);
  if (!fn)
    return (char *)"";

  auto f = dynamic_cast<Func *>(fn->getFunc());
  if (!f)
    return (char *)"";
  auto an = f->getArgNames();
  string r;
  for (const auto &s : an)
    r += s + "\b";
  char *res = strdup(r.c_str());
  return res;
}

FOREIGN void set_func_enclosing(Func *f, Func *outer) {
  f->setEnclosingFunc(outer);
}

FOREIGN void set_func_return(Func *f, Return *ret) { f->sawReturn(ret); }

FOREIGN void set_func_yield(Func *f, Yield *yield) { f->sawYield(yield); }

FOREIGN void set_func_prefetch(Func *f, Prefetch *pre) { f->sawPrefetch(pre); }

FOREIGN void resolve_types(Stmt *s) { s->resolveTypes(); }

FOREIGN void set_func_params(Func *f, char **names, types::Type **types,
                             size_t len) {
  vector<string> s;
  for (size_t i = 0; i < len; i++) {
    s.emplace_back(names[i]);
    free(names[i]);
  }
  f->setIns(vector<types::Type *>(types, types + len));
  f->setArgNames(s);
}

FOREIGN void set_func_generics(Func *fn, int n) { fn->addGenerics(n); }

FOREIGN types::Type *get_func_generic(Func *fn, int idx) {
  return fn->getGeneric(idx);
}

FOREIGN void set_func_extern(Func *fn) { fn->setExternal(); }

FOREIGN char *get_expr_name(Expr *ex) { return strdup(ex->getName().c_str()); }

FOREIGN void set_func_generic_name(Func *fn, int idx, char *name) {
  fn->getGeneric(idx)->setName(name);
  free(name);
}

FOREIGN void set_ref_generics(types::RefType *fn, int n) { fn->addGenerics(n); }

FOREIGN types::Type *get_ref_generic(types::RefType *fn, int idx) {
  return fn->getGeneric(idx);
}

FOREIGN void set_ref_generic_name(types::RefType *fn, int idx, char *name) {
  fn->getGeneric(idx)->setName(name);
  free(name);
}

FOREIGN Stmt *match_stmt(Expr *cond) {
  auto m = new Match();
  m->setValue(cond);
  return m;
}

FOREIGN Block *add_match_case(Match *m, Pattern *p) { return m->addCase(p); }

FOREIGN Pattern *bound_pattern(Pattern *p) { return new BoundPattern(p); }

FOREIGN Pattern *guarded_pattern(Pattern *p, Expr *e) {
  return new GuardedPattern(p, e);
}

FOREIGN Pattern *star_pattern() { return new StarPattern(); }

FOREIGN Pattern *wildcard_pattern(void) { return new Wildcard(); }

FOREIGN Pattern *int_pattern(int i) { return new IntPattern(i); }

FOREIGN Pattern *str_pattern(char *c, size_t len) {
  auto t = new StrPattern(string(c, len));
  free(c);
  return t;
}

FOREIGN Pattern *range_pattern(int a, int b) { return new RangePattern(a, b); }

FOREIGN Pattern *record_pattern(Pattern **p, size_t size) {
  return new RecordPattern(vector<Pattern *>(p, p + size));
}

FOREIGN Pattern *or_pattern(Pattern **p, size_t size) {
  return new OrPattern(vector<Pattern *>(p, p + size));
}

FOREIGN Pattern *array_pattern(Pattern **p, size_t size) {
  return new ArrayPattern(vector<Pattern *>(p, p + size));
}

FOREIGN Pattern *seq_pattern(char *c, size_t len) {
  auto t = new SeqPattern(string(c, len));
  free(c);
  return t;
}

FOREIGN Pattern *bool_pattern(bool b) { return new BoolPattern(b); }

FOREIGN Var *get_bound_pattern_var(BoundPattern *b) { return b->getVar(); }

FOREIGN void set_pattern_trycatch(Pattern *p, TryCatch *tc) {
  p->setTryCatch(tc);
}

/***** Block utils *****/

FOREIGN Block *get_module_block(SeqModule *st) { return st->getBlock(); }

FOREIGN Block *get_while_block(While *st) { return st->getBlock(); }

FOREIGN Block *get_for_block(For *st) { return st->getBlock(); }

FOREIGN Block *get_else_block(If *st) { return st->addElse(); }

FOREIGN Block *get_elif_block(If *st, Expr *ex) { return st->addCond(ex); }

FOREIGN Block *get_trycatch_block(TryCatch *tc) { return tc->getBlock(); }

FOREIGN Block *get_trycatch_catch(TryCatch *tc, types::Type *t) {
  return tc->addCatch(t);
}

FOREIGN Block *get_trycatch_finally(TryCatch *tc) { return tc->getFinally(); }

FOREIGN Var *get_trycatch_var(TryCatch *tc, unsigned idx) {
  return tc->getVar(idx);
}

FOREIGN void set_enclosing_trycatch(Expr *e, TryCatch *tc) {
  if (tc)
    e->setTryCatch(tc);
}

FOREIGN void set_global(Var *v) { v->setGlobal(); }

FOREIGN void set_thread_local(Var *v) { v->setThreadLocal(); }

FOREIGN char type_eq(types::Type *a, types::Type *b) { return types::is(a, b); }

FOREIGN char *get_func_attrs(Func *f) {
  string r;
  for (auto s : f->getAttributes())
    r += s + "\b";
  return strdup(r.substr(0, r.size() - 1).c_str());
}

FOREIGN void set_func_attr(Func *f, char *c) {
  f->addAttribute(string(c));
  free(c);
}

FOREIGN struct seq_srcinfo {
  char *file;
  int line;
  int col;
  int len;
};

FOREIGN void set_pos(SrcObject *obj, char *f, int l, int c, int len) {
  if (!obj)
    return;
  obj->setSrcInfo(SrcInfo(string(f), l, c, len));
  free(f);
}

FOREIGN seq_srcinfo get_pos(SrcObject *obj) {
  if (!obj) {
    return seq_srcinfo{(char *)"", 0, 0, 0};
  }
  auto info = obj->getSrcInfo();
  return seq_srcinfo{strdup(info.file.c_str()), info.line, info.col, info.len};
}

FOREIGN Var *get_for_var(For *f) { return f->getVar(); }

FOREIGN types::Type *get_type(Expr *e) {
  auto ret = e->getType();
  return ret;
}

FOREIGN BaseFunc *get_func(FuncExpr *e) { return e->getFunc(); }

FOREIGN types::Type *get_var_type(Var *e) { return e->getType(); }

FOREIGN void set_base(Stmt *st, BaseFunc *base) {
  if (st) {
    st->setBase(base);
  }
}

FOREIGN void add_stmt(Stmt *st, Block *block) {
  if (st) {
    block->add(st);
  }
}

FOREIGN void *init_module() { return new SeqModule(); }

FOREIGN void set_func_realize_types(FuncExpr *e, types::Type **types,
                                    size_t sz) {
  e->setRealizeTypes(vector<types::Type *>(types, types + sz));
}

FOREIGN void set_elem_realize_types(GetElemExpr *e, types::Type **types,
                                    size_t sz) {
  e->setRealizeTypes(vector<types::Type *>(types, types + sz));
}

FOREIGN void set_static_realize_types(GetStaticElemExpr *e, types::Type **types,
                                      size_t sz) {
  e->setRealizeTypes(vector<types::Type *>(types, types + sz));
}

FOREIGN types::Type *realize_type(types::RefType *t, types::Type **types,
                                  size_t sz, char **error) {
  return types::GenericType::get(t, vector<types::Type *>(types, types + sz));
}

/// Anything below throws exceptions

// Yes, separator character here is \b
// and yes, it will fail if \b is within filename or message
//          (although one might wonder what's \b doing in a filename?)
// and yes, it needs proper escaping.
#define CATCH(er)                                                              \
  auto info = e.getSrcInfo();                                                  \
  asprintf(er, "%s\b%s\b%d\b%d\b%d", e.what(), info.file.c_str(), info.line,   \
           info.col, info.len);

FOREIGN void exec_module(SeqModule *sm, char **args, size_t alen, char debug,
                         char **error) {
  try {
    vector<string> s;
    for (size_t i = 0; i < alen; i++) {
      s.emplace_back(string(args[i]));
      free(args[i]);
    }
    sm->execute(s, {}, debug);
  } catch (exc::SeqException &e) {
    CATCH(error);
  }
}

#if LLVM_VERSION_MAJOR == 6
FOREIGN SeqJIT *jit_init() {
  SeqJIT::init();
  return new SeqJIT();
}

FOREIGN Var *jit_var(SeqJIT *jit, Expr *expr) { return jit->addVar(expr); }

FOREIGN void jit_func(SeqJIT *jit, Func *func, char **error) {
  try {
    jit->addFunc(func);
    fflush(stdout);
  } catch (exc::SeqException &e) {
    CATCH(error);
  }
}
#else
FOREIGN void *jit_init() { return 0; }
FOREIGN Var *jit_var(void *jit, Expr *expr) { return 0; }
FOREIGN void jit_func(void *jit, Func *func, char **error) {
  *error = strdup("JIT is not supported without LLVM 6");
}
#endif

FOREIGN void caml_error_callback(char *msg, int line, int col, char *file) {
  seq::compilationError(std::string(msg), std::string(file), line, col);
}

FOREIGN void caml_warning_callback(char *msg, int line, int col, char *file) {
  seq::compilationWarning(std::string(msg), std::string(file), line, col);
}
