#pragma once

#include "sir/sir.h"

namespace seq {
namespace ir {
namespace transform {
namespace parallel {

struct OMPSched {
  struct Param {
    bool isLiteral;
    union {
      int intVal;
      Var *varVal;
    } val;

    Param(int intVal) : isLiteral(true), val() { val.intVal = intVal; }
    Param(Var *varVal) : isLiteral(false), val() { val.varVal = varVal; }

    Value *getValue(Module *M) {
      if (isLiteral) {
        return M->getInt(val.intVal);
      } else {
        return M->Nr<VarValue>(val.varVal);
      }
    }
  };

  bool setThreads;
  Param threads;
  bool dynamic;
  Param chunk;
  int code;

  OMPSched();
};

OMPSched getScedule(ForFlow *v, const std::vector<Var *> &vars = {});
OMPSched getScedule(ImperativeForFlow *v, const std::vector<Var *> &vars = {});

} // namespace parallel
} // namespace transform
} // namespace ir
} // namespace seq
