/// 786

#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

#include <caml/alloc.h>
#include <caml/mlvalues.h>
#include <caml/fail.h>

#include <seq/seq.h>

using namespace std;
using namespace seq;

unordered_map<string, Var*> variables;
vector<vector<string>> current_vars;

Expr *descent_expr(value ast);
Expr *descent_vararg(value ast)
{
   switch (Tag_val(ast)) {
   case 0: { // | PlainArg of expr
      return descent_expr(Field(ast, 0));
   }
   default: throw "not implemented";
   }
}

Expr *descent_expr(value ast)
{
   if (Is_long(ast)) {
      throw "ellipsis not yet supported";
   } else switch (Tag_val(ast)) {
      case 0: { // | Bool of bool
         return new BoolExpr(Int_val(Field(ast, 0)));
      }
      case 1: { // | Int of int
         return new IntExpr(Int_val(Field(ast, 0)));
      }
      case 2: { // | Float of float
         return new FloatExpr(Double_val(Field(ast, 0)));
      }
      case 3: { // | String of string
         string v = String_val(Field(ast, 0));
         return new StrExpr(v);
      }
      case 5: { // | Id of string
         string v = String_val(Field(ast, 0));
         auto it = variables.find(v);
         if (it == variables.end()) {
            throw "variable " + v + " not found";
         } else {
            return new VarExpr(it->second);
         }
      }
      case 16: { // | IfExpr of expr * expr * expr
         auto cond = descent_expr(Field(ast, 0));
         auto ift = descent_expr(Field(ast, 1));
         auto iff = descent_expr(Field(ast, 2));
         return new CondExpr(cond, ift, iff);
      }
      case 19:   // | Cond of expr * string * expr
      case 21: { // | Binary of expr * string * expr
         auto lhs = descent_expr(Field(ast, 0));
         auto op = String_val(Field(ast, 1));
         auto rhs = descent_expr(Field(ast, 2));
         return new BOpExpr(bop(op), lhs, rhs);
      }
      case 24: { // | Call of expr * vararg list
         auto fn = descent_expr(Field(ast, 0));
         vector<Expr*> args;
         for (value head = Field(ast, 1); head != Val_emptylist; head = Field(head, 1)) {
            args.push_back(descent_vararg(Field(head, 0)));
         }
         return new CallExpr(fn, args);
      }
      case 23: { // | Dot of expr * string
         auto lhs = descent_expr(Field(ast, 0));
         string rhs = String_val(Field(ast, 1));
         return new GetElemExpr(lhs, rhs);
      }
      case 22: { // | Index of expr * expr list
         auto lhs = descent_expr(Field(ast, 0));
         auto rhs = descent_expr(Field(ast, 1));
         return new ArrayLookupExpr(lhs, rhs);
      }
      case 27: { // | SliceIndex of expr * expr option * expr option * expr option
         throw "not yet implemented"; 
            // Needs to be merged into the case 22
         auto arr = descent_expr(Field(ast, 0));
         auto stc = Field(ast, 1);
         Expr *st = stc == Val_int(0) ? NULL : descent_expr(Field(stc, 0));
         auto edc = Field(ast, 2);
         Expr *ed = edc == Val_int(0) ? NULL : descent_expr(Field(edc, 0));
         return new ArraySliceExpr(arr, st, ed);
      }
      case 8: { // | Tuple of expr list
         vector<Expr*> exprs;
         for (value head = Field(ast, 0); head != Val_emptylist; head = Field(head, 1)) {
            exprs.push_back(descent_expr(Field(head, 0)));
         }
         return new RecordExpr(exprs, vector<string>(exprs.size(), ""));
      }
      default: throw "wrong item";
   }
   throw "shouldn't be here";
   return NULL;
}

Stmt *descent_statement(value ast, Block *curBlock, BaseFunc *base, bool isFunc = false) 
{
   if (Is_long(ast)) switch (Int_val(ast)) {
      case 0: // | Pass
         return NULL;
      case 1: { // | Break
         auto s = new Break();
         s->setBase(base);
         return s;
      }
      case 2: { // | Continue
         auto s = new Continue();
         s->setBase(base);
         return s;
      }
      default:
         throw "whoops invalid is_long in statement";
   } else switch (Tag_val(ast)) {
      case 0: { // | Statements of statement list
         for (value head = Field(ast, 0); head != Val_emptylist; head = Field(head, 1)) {
            curBlock->add(descent_statement(Field(head, 0), curBlock, base));
         }
      }
      case 1: { // | Exprs of expr list -- single only
         auto s = new ExprStmt(descent_expr(Field(ast, 0)));
         s->setBase(base);
         return s;
      }
      case 2: { // | Assign of expr list * expr list
         value ex = Field(ast, 0);
         auto rhs = descent_expr(Field(ast, 1));
         switch (Tag_val(ex)) {
            case 5: { // | Id of string
               string v = String_val(Field(ex, 0));
               auto it = variables.find(v);
               if (it == variables.end()) {
                  auto s = new VarStmt(rhs);
                  s->setBase(base);
                  variables[v] = s->getVar();
                  current_vars.back().push_back(v);
                  return s;
               } else {
                  auto s = new Assign(it->second, rhs);
                  s->setBase(base);
                  return s;
               }
            }
            case 22: { // idx
               auto lh = descent_expr(Field(ex, 0));
               auto rh = descent_expr(Field(ex, 1));
               auto s = new AssignIndex(lh, rh, rhs);
               s->setBase(base);
               return s;
            }
            default: throw "Assignment that is not yet implemented!";
         }
         throw "Shouldn't be here honestly...!";
      }
      case 4: { // | Print of expr list -- single only
         auto s = new Print(descent_expr(Field(ast, 0)));
         s->setBase(base);
         return s;
      }
      case 12: { // | If of (expr option * statement list) list
         auto ifs = new If();
         for (value head = Field(ast, 0); head != Val_emptylist; head = Field(head, 1)) {
            auto item = Field(head, 0); // expr option * statement list
            Block *block;
            if (Field(item, 0) == Val_int(0)) {
               block = ifs->addElse();
            } else {
               auto expr = descent_expr(Field(Field(item, 0), 0));
               block = ifs->addCond(expr);
            }
            current_vars.push_back(vector<string>());
            for (value h2 = Field(item, 1); h2 != Val_emptylist; h2 = Field(h2, 1)) {
               block->add(descent_statement(Field(h2, 0), block, base));
            }
            for (auto v: current_vars.back())
               variables.erase(v);
            current_vars.pop_back();
         }
         ifs->setBase(base);
         return ifs;
      }
      case 10: { // | While of expr * statement list
         auto cond = descent_expr(Field(ast, 0));
         auto whs = new While(cond);
         current_vars.push_back(vector<string>());
         for (value h2 = Field(ast, 1); h2 != Val_emptylist; h2 = Field(h2, 1)) {
            whs->getBlock()->add(descent_statement(Field(h2, 0), whs->getBlock(), base));
         }
         for (auto v: current_vars.back())
            variables.erase(v);
         current_vars.pop_back();
         whs->setBase(base);
         return whs;
      }
      case 11: { // | For of expr list * expr list * statement list
         auto gen = descent_expr(Field(ast, 1));
         auto frs = new For(gen);

         string var;
         value ex = Field(ast, 0);
         switch (Tag_val(ex)) {
            case 5: { // | Id of string
               var = String_val(Field(ex, 0));
            }
            default: throw "invalid for variable";
         }
         if (variables.find(var) != variables.end()) {
            throw "for variable " + var + " already declared";
         }

         current_vars.push_back(vector<string>());
         current_vars.back().push_back(var);
         variables[var] = frs->getVar();
         for (value h2 = Field(ast, 1); h2 != Val_emptylist; h2 = Field(h2, 1)) {
            frs->getBlock()->add(descent_statement(Field(h2, 0), frs->getBlock(), base));
         }
         for (auto v: current_vars.back())
            variables.erase(v);
         current_vars.pop_back();
         frs->setBase(base);
         return frs;
      }
      case 5: { // | Return of expr list
         auto s = new Return(descent_expr(Field(ast, 0)));
         s->setBase(base);
         return s;
      }
      case 6: { // | Yield of expr list
         auto s = new Yield(descent_expr(Field(ast, 0)));
         if (isFunc) {
            ((Func*)base)->setGen();
         }
         s->setBase(base);
         return s;
      }
      // case 14: { // | Function of vararg * vararg list * statement list
      // }
      default: throw "not yet implemented statement";
   }
   throw "shouldn't be here";
   return NULL;
}

SeqModule *descent_module(value ast)
{
   auto mod = new SeqModule();
   current_vars.push_back(vector<string>());
   for (value head = Field(ast, 0); head != Val_emptylist; head = Field(head, 1)) {
      mod->getBlock()->add(descent_statement(Field(head, 0), 
         mod->getBlock(), 
         mod));
   }
   throw "vars?";
   return mod;
}

/** Entry point **/

extern "C" 
CAMLprim value caml_compile(value ast) 
{
   try {
      auto module = descent_module(ast);
      module->execute(vector<string>(), true);
   } catch (string s) {
      s = "C++ exception: " + s;
      caml_failwith(s.c_str());
   }

   return Val_unit;
}

