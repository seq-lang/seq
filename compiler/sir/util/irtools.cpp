#include "irtools.h"

namespace seq {
namespace ir {
namespace util {

bool hasAttribute(const Func *func, const std::string &attribute) {
  if (auto *attr = func->getAttribute<KeyValueAttribute>()) {
    return attr->has(attribute);
  }
  return false;
}

bool isStdlibFunc(const Func *func, const std::string &submodule) {
  if (auto *attr = func->getAttribute<KeyValueAttribute>()) {
    std::string module = attr->get(".module");
    return module.rfind("std::" + submodule, 0) == 0;
  }
  return false;
}

CallInstr *call(Func *func, const std::vector<Value *> &args) {
  auto *M = func->getModule();
  return M->Nr<CallInstr>(M->Nr<VarValue>(func), args);
}

Value *makeTuple(const std::vector<Value *> &args, IRModule *M) {
  std::vector<types::Type *> types;
  for (auto *arg : args) {
    types.push_back(arg->getType());
  }
  auto *tupleType = M->getTupleType(types);
  auto *newFunc = M->getOrRealizeMethod(tupleType, "__new__", types);
  assert(newFunc);
  return M->Nr<CallInstr>(M->Nr<VarValue>(newFunc), args);
}

VarValue *makeVar(Value *x, SeriesFlow *flow, BodiedFunc *parent) {
  auto *M = x->getModule();
  auto *v = M->Nr<Var>(x->getType());
  flow->push_back(M->Nr<AssignInstr>(v, x));
  parent->push_back(v);
  return M->Nr<VarValue>(v);
}

Value *alloc(types::Type *type, Value *count) {
  auto *M = type->getModule();
  auto *ptrType = M->getPointerType(type);
  return (*ptrType)(*count);
}

Value *alloc(types::Type *type, int64_t count) {
  auto *M = type->getModule();
  return alloc(type, M->getIntConstant(count));
}

Var *getVar(Value *x) {
  if (auto *v = cast<VarValue>(x)) {
    if (auto *var = cast<Var>(v->getVar())) {
      if (!isA<Func>(var)) {
        return var;
      }
    }
  }
  return nullptr;
}

const Var *getVar(const Value *x) {
  if (auto *v = cast<VarValue>(x)) {
    if (auto *var = cast<Var>(v->getVar())) {
      if (!isA<Func>(var)) {
        return var;
      }
    }
  }
  return nullptr;
}

Func *getFunc(Value *x) {
  if (auto *v = cast<VarValue>(x)) {
    if (auto *func = cast<Func>(v->getVar())) {
      return func;
    }
  }
  return nullptr;
}

const Func *getFunc(const Value *x) {
  if (auto *v = cast<VarValue>(x)) {
    if (auto *func = cast<Func>(v->getVar())) {
      return func;
    }
  }
  return nullptr;
}

BodiedFunc *getStdlibFunc(Value *x, const std::string &name,
                          const std::string &submodule) {
  if (auto *f = getFunc(x)) {
    if (auto *g = cast<BodiedFunc>(f)) {
      if (isStdlibFunc(g, submodule) && g->getUnmangledName() == name) {
        return g;
      }
    }
  }
  return nullptr;
}

const BodiedFunc *getStdlibFunc(const Value *x, const std::string &name,
                                const std::string &submodule) {
  if (auto *f = getFunc(x)) {
    if (auto *g = cast<BodiedFunc>(f)) {
      if (isStdlibFunc(g, submodule) && g->getUnmangledName() == name) {
        return g;
      }
    }
  }
  return nullptr;
}

types::Type *getReturnType(const Func *func) {
  return cast<types::FuncType>(func->getType())->getReturnType();
}

void setReturnType(Func *func, types::Type *rType) {
  auto *M = func->getModule();
  auto *t = cast<types::FuncType>(func->getType());
  assert(t);
  std::vector<types::Type *> argTypes(t->begin(), t->end());
  func->setType(M->getFuncType(rType, argTypes));
}

} // namespace util
} // namespace ir
} // namespace seq
