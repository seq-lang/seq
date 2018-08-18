/// 786

#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

#include <caml/fail.h>
#include <caml/alloc.h>
#include <caml/memory.h>
#include <caml/mlvalues.h>

#include <seq/seq.h>

#include "format.h"

using namespace std;
using namespace seq;

unordered_map<string, Var*> variables;
vector<vector<string>> current_vars;

#define PE(l, s, ...)   fmt::print(stderr, "{}" s, string(l, ' '), ##__VA_ARGS__)
#define E(...)          fmt::print(stderr, __VA_ARGS__)
#define S(...)          fmt::format(__VA_ARGS__)

Expr *descent_expr(value ast);
Expr *descent_vararg(value ast)
{
   CAMLparam1(ast);
   CAMLlocal1(a);
   Expr *e;

   switch (Tag_val(ast)) {
      case 0: { // | PlainArg of expr
         a = Field(ast, 0);
         e = descent_expr(a);
         break;
      }
      default: S("vararg {} not implemented", Tag_val(ast));
   }
   return e;
}

#define CAMLiter(list, head, pff) \
   while (list != Val_emptylist) { \
      head = Field(list, 0); \
      pff; \
      list = Field(list, 1); \
   }


Expr *descent_expr(value ast)
{
   CAMLparam1(ast);
   CAMLlocal5(head, head2, a, b, c);
   Expr *e = NULL;

   if (Is_long(ast)) {
      throw S("ellipsis not yet supported");
   } else switch (Tag_val(ast)) {
      case 0: { // | Bool of bool
         bool b = Int_val(Field(ast, 0));
         E("Bool({})", b);
         e = new BoolExpr(b);
         break;
      }
      case 1: { // | Int of int
         int i = Int_val(Field(ast, 0));
         E("Int({})", i);
         e = new IntExpr(i);
         break;
      }
      case 2: { // | Float of float
         double f = Double_val(Field(ast, 0));
         E("Float({})", f);
         e = new FloatExpr(f);
         break;
      }
      case 3: { // | String of string
         string v = String_val(Field(ast, 0));
         E("String({})", v);
         e = new StrExpr(v);
         break;
      }
      case 5: { // | Id of string
         string v = String_val(Field(ast, 0));
         auto it = variables.find(v);
         if (it == variables.end()) {
            throw S("variable {} not found", v);
         } else {
            E("Var({})", v);
            e = new VarExpr(it->second);
            break;
         }
      }
      case 16: { // | IfExpr of expr * expr * expr
         E("Cond[");
         a = Field(ast, 0);
         auto cond = descent_expr(a);
         E("; ");
         a = Field(ast, 1);
         auto ift = descent_expr(a);
         E("; ");
         a = Field(ast, 2);
         auto iff = descent_expr(a);
         E("]");
         e = new CondExpr(cond, ift, iff);
         break;
      }
      case 19:   // | Cond of expr * string * expr
      case 21: { // | Binary of expr * string * expr
         auto op = String_val(Field(ast, 1));
         E("{}[", op);
         a = Field(ast, 0);
         auto lhs = descent_expr(a);
         E(", ");
         a = Field(ast, 2);
         auto rhs = descent_expr(a);
         E("]");
         e = new BOpExpr(bop(op), lhs, rhs);
         break;
      }
      case 24: { // | Call of expr * vararg list
         E("Call[");
         a = Field(ast, 0);
         auto fn = descent_expr(a);
         vector<Expr*> args;
         a = Field(ast, 1);
         CAMLiter(a, head, {
            E(", ");
            args.push_back(descent_vararg(head));
         });
         E("]");
         e = new CallExpr(fn, args);
         break;
      }
      case 23: { // | Dot of expr * string
         E("Dot[");
         a = Field(ast, 0);
         auto lhs = descent_expr(a);
         string rhs = String_val(Field(ast, 1));
         E(", {}]", rhs);
         e = new GetElemExpr(lhs, rhs);
         break;
      }
      case 22: { // | Index of expr * expr list
         E("Index[");
         a = Field(ast, 0);
         auto lhs = descent_expr(a);
         E(", ");
         a = Field(ast, 1);
         head = Field(a, 0); // first element
         auto rhs = descent_expr(head);
         E("]");
         e = new ArrayLookupExpr(lhs, rhs);
         break;
      }
      // case 27: { // | SliceIndex of expr * expr option * expr option * expr option
      //    throw S("slicing not yet implemented"); 
      //       // Needs to be merged into the case 22
      //    auto arr = descent_expr(Field(ast, 0));
      //    auto stc = Field(ast, 1);
      //    Expr *st = stc == Val_int(0) ? NULL : descent_expr(Field(stc, 0));
      //    auto edc = Field(ast, 2);
      //    Expr *ed = edc == Val_int(0) ? NULL : descent_expr(Field(edc, 0));
      //    e = new ArraySliceExpr(arr, st, ed);
      //    break;
      // }
      case 8: { // | Tuple of expr list
         E("Tuple[");
         vector<Expr*> exprs;
         int cnt = 0;
         a = Field(ast, 0);
         CAMLiter(a, head, {
            if (cnt++) E(", ");
            exprs.push_back(descent_expr(head));
         });
         E("]");
         e = new RecordExpr(exprs, vector<string>(exprs.size(), ""));
         break;
      }
      default: throw S("expression {} not implemented", Tag_val(ast));
   }
   return e;
}

Stmt *descent_statement(value ast, Block *curBlock, BaseFunc *base, int level = 0, bool isFunc = false) 
{
   CAMLparam1(ast);
   CAMLlocal5(head, head2, a, b, c);
   Stmt *s = NULL;

   // E("here-- {} {}\n", Is_long(ast), Is_long(ast)?Int_val(ast):Tag_val(ast));

   if (Is_long(ast)) switch (Int_val(ast)) {
      case 0: // | Pass
         PE(level, "Pass\n");
         s = NULL;
         break;
      case 1: { // | Break
         PE(level, "Break\n");
         s = new Break();
         break;
      }
      case 2: { // | Continue
         PE(level, "Continue\n");
         s = new Continue();
         break;
      }
      default: throw S("unknown simple statement {}", Int_val(ast));
   } else switch (Tag_val(ast)) {
      case 0: { // | Statements of statement list
         a = Field(ast, 0);
         CAMLiter(a, head, {
            auto st = descent_statement(head, curBlock, base, level, isFunc);
            if (st) curBlock->add(st);
         });
         break;
      }
      case 1: { // | Exprs of expr list -- single only
         PE(level, "");
         head = Field(ast, 0);
         s = new ExprStmt(descent_expr(head));
         E("\n");
         break;
      }
      case 2: { // | Assign of expr * expr list
         b = Field(ast, 1);
         head2 = Field(b, 0); // get first

         head = Field(ast, 0);
         switch (Tag_val(head)) {
            case 5: { // | Id of string
               head = Field(head, 0);
               string v = String_val(head);
               auto it = variables.find(v);
               if (it == variables.end()) {
                  PE(level, "Var[{} := ", v);
                  auto rhs = descent_expr(head2);
                  E("]\n");
                  s = new VarStmt(rhs);
                  variables[v] = ((VarStmt*)s)->getVar();
                  current_vars.back().push_back(v);
               } else {
                  PE(level, "Assign[{} := ", v);
                  auto rhs = descent_expr(head2);
                  E("]\n");
                  s = new Assign(it->second, rhs);
               }
               break;
            }
            case 22: { // idx
               PE(level, "AssignIndex[");
               a = Field(head, 0);
               auto lh = descent_expr(a);
               E("; ");
               a = Field(head, 1);
               auto rh = descent_expr(a);
               E(" := ");
               auto rhs = descent_expr(head2);
               E("]\n");
               s = new AssignIndex(lh, rh, rhs);
               break;
            }
            default: throw S("Assignment that is not yet implemented!");
         }
         break;
      }
      case 4: { // | Print of expr list -- single only
         PE(level, "Print[");
         a = Field(ast, 0);
         head = Field(a, 0); // only first
         s = new Print(descent_expr(head));
         E("]\n");
         break;
      }
      case 12: { // | If of (expr option * statement list) list
         s = new If();
         PE(level, "If[\n");
         a = Field(ast, 0);
         CAMLiter(a, head, { // head = expr option * statement list
            b = Field(head, 0); 
            Block *block;
            if (b == Val_int(0)) {
               PE(level + 1, "_ -> [\n");
               block = ((If*)s)->addElse();
            } else {
               PE(level + 1, "");
               c = Field(b, 0);
               auto expr = descent_expr(c);
               E(" -> [\n");
               block = ((If*)s)->addCond(expr);
            }

            current_vars.push_back(vector<string>());
            c = Field(head, 1);
            CAMLiter(c, head2, {
               auto ss = descent_statement(head2, block, base, level + 2, isFunc);
               if (ss) block->add(ss);
            });
            for (auto v: current_vars.back())
               variables.erase(v);
            current_vars.pop_back();
            PE(level + 1, "]\n");
         });
         PE(level, "]\n");
         break;
      }
      case 10: { // | While of expr * statement list
         PE(level, "While[");
         a = Field(ast, 0);
         auto cond = descent_expr(a);
         s = new While(cond);
         E("; \n");
         current_vars.push_back(vector<string>());
         b = Field(ast, 1);
         CAMLiter(b, head, {
            auto ss = descent_statement(head, ((While*)s)->getBlock(), base, level + 1, isFunc);
            if (ss) ((While*)s)->getBlock()->add(ss);
         });
         for (auto v: current_vars.back())
            variables.erase(v);
         current_vars.pop_back();
         PE(level, "]\n");
         break;
      }
      case 11: { // | For of expr * expr * statement list
         PE(level, "For[");
         a = Field(ast, 1);
         auto gen = descent_expr(a);
         s = new For(gen);

         string var;
         a = Field(ast, 0);
         switch (Tag_val(a)) {
            case 5: { // | Id of string
               var = String_val(Field(a, 0));
            }
            default: throw S("invalid tag {} for for-variable", Tag_val(a));
         }
         if (variables.find(var) != variables.end()) {
            throw S("for-variable '{}' already declared", var);
         }
         E(" AS Var[{}];\n", var);

         current_vars.push_back(vector<string>());
         current_vars.back().push_back(var);
         variables[var] = ((For*)s)->getVar();
         a = Field(ast, 2);
         CAMLiter(a, head, {
            auto ss = descent_statement(head, ((For*)s)->getBlock(), base, level + 1, isFunc);
            if (ss) ((For*)s)->getBlock()->add(ss);
         });
         for (auto v: current_vars.back())
            variables.erase(v);
         current_vars.pop_back();
         PE(level, "]\n");
         break;
      }
      case 5: { // | Return of expr list
         PE(level, "Return[");
         a = Field(ast, 0);
         s = new Return(descent_expr(a));
         E("]\n");
         break;
      }
      case 6: { // | Yield of expr list
         PE(level, "Yield[");
         a = Field(ast, 0);
         s = new Yield(descent_expr(a));
         if (isFunc) {
            E("; fn := {}", ((Func*)base)->genericName());
            ((Func*)base)->setGen();
         } else {
            throw S("Yield in SeqModule");
         }
         E("]\n");
         break;
      }
      // case 14: { // | Function of vararg * vararg list * statement list
      // }
      default: throw S("statement {} not yet implemented", Tag_val(ast));
   }
   if (s) s->setBase(base);
   return s;
}

SeqModule *descent_module(value ast)
{
   CAMLparam1(ast);
   CAMLlocal2(head, list);

   E("Module Start\n");
   auto mod = new SeqModule();
   current_vars.push_back(vector<string>());
   
   list = Field(ast, 0);
   CAMLiter(list, head, {
      auto ss = descent_statement(head, mod->getBlock(), mod, 0, false);
      if (ss) mod->getBlock()->add(ss);
   });
   E("Module End\n");
   E("|> IR ==>\n");
   return mod;
}

/** Entry point **/

extern "C" 
CAMLprim value caml_compile(value ast) 
{
   CAMLparam1(ast);

   E("C++ parser is about to kick ass\n");
   try {
      auto module = descent_module(ast);
      module->execute(vector<string>(), true);
   } catch (string &s) {
      caml_failwith(s.c_str());
   } catch (exc::SeqException &e) {
      caml_failwith(e.what());
   } catch (...) {
      caml_failwith("N/A");
   }

   return Val_unit;
}

