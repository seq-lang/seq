#include "pipeline.h"

namespace seq {
namespace ir {
namespace transform {
namespace pipeline {

bool hasAttribute(const Func *func, const std::string &attribute) {
  if (auto *attr = func->getAttribute<FuncAttribute>()) {
    return attr->has(attribute);
  }
  return false;
}

bool isStdlibFunc(const Func *func) { return hasAttribute(func, ".stdlib"); }

template <typename T> bool isConst(const Value *x) {
  return isA<TemplatedConstant<T>>(x);
}

template <typename T> bool isConst(const Value *x, const T &value) {
  if (auto *c = cast<TemplatedConstant<T>>(x)) {
    return c->getVal() == value;
  }
  return false;
}

template <typename T> T getConst(const Value *x) {
  auto *c = cast<TemplatedConstant<T>>(x);
  assert(c);
  return c->getVal();
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

BodiedFunc *getStdlibFunc(Value *x, const std::string &name) {
  if (auto *f = getFunc(x)) {
    if (auto *g = cast<BodiedFunc>(f)) {
      if (/*isStdlibFunc(g) &&*/ g->getUnmangledName() == name) {
        return g;
      }
    }
  }
  return nullptr;
}

const BodiedFunc *getStdlibFunc(const Value *x, const std::string &name) {
  if (auto *f = getFunc(x)) {
    if (auto *g = cast<BodiedFunc>(f)) {
      if (/*isStdlibFunc(g) &&*/ g->getUnmangledName() == name) {
        return g;
      }
    }
  }
  return nullptr;
}

FlowInstr *convertGetitemForPrefetch(Value *getitem) {
  auto *M = getitem->getModule();
  Value *prefetch = nullptr; // TODO
  auto *yield = M->Nr<YieldInstr>();

  auto *series = M->Nr<SeriesFlow>();
  series->push_back(prefetch);
  series->push_back(yield);

  return M->Nr<FlowInstr>(series, getitem);
}

void applySubstitutionOptimizations(PipelineFlow *p) {
  auto *M = p->getModule();

  PipelineFlow::Stage *prev = nullptr;
  auto it = p->begin();
  while (it != p->end()) {
    if (prev) {
      {
        auto *f1 = getStdlibFunc(prev->getFunc(), "kmers");
        auto *f2 = getStdlibFunc(it->getFunc(), "revcomp");
        if (f1 && f2) {
          auto *funcType = cast<types::FuncType>(f1->getType());
          auto *genType = cast<types::GeneratorType>(funcType->getReturnType());
          auto *seqType = funcType->front();
          auto *kmerType = genType->getBase();
          auto *kmersRevcompFunc = M->getOrRealizeFunc(
              "_kmers_revcomp", genType, {seqType, M->getIntType()}, {kmerType});
          cast<VarValue>(prev->getFunc())->setVar(kmersRevcompFunc);
          it = p->erase(it);
          continue;
        }
      }

      {
        auto *f1 = getStdlibFunc(prev->getFunc(), "kmers_with_pos");
        auto *f2 = getStdlibFunc(it->getFunc(), "revcomp_with_pos");
        if (f1 && f2) {
          auto *funcType = cast<types::FuncType>(f1->getType());
          auto *genType = cast<types::GeneratorType>(funcType->getReturnType());
          auto *seqType = funcType->front();
          auto *kmerType =
              cast<types::MemberedType>(genType->getBase())->back().getType();
          auto *kmersRevcompWithPosFunc =
              M->getOrRealizeFunc("_kmers_revcomp_with_pos", genType,
                                  {seqType, M->getIntType()}, {kmerType});
          cast<VarValue>(prev->getFunc())->setVar(kmersRevcompWithPosFunc);
          it = p->erase(it);
          continue;
        }
      }

      {
        auto *f1 = getStdlibFunc(prev->getFunc(), "kmers");
        auto *f2 = getStdlibFunc(it->getFunc(), "canonical");
        if (f1 && f2 && isConst<int64_t>(prev->back(), 1)) {
          auto *funcType = cast<types::FuncType>(f1->getType());
          auto *genType = cast<types::GeneratorType>(funcType->getReturnType());
          auto *seqType = funcType->front();
          auto *kmerType = genType->getBase();
          auto *kmersCanonicalFunc =
              M->getOrRealizeFunc("_kmers_canonical", genType, {seqType}, {kmerType});
          cast<VarValue>(prev->getFunc())->setVar(kmersCanonicalFunc);
          prev->erase(prev->end() - 1); // remove step argument
          it = p->erase(it);
          continue;
        }
      }

      {
        auto *f1 = getStdlibFunc(prev->getFunc(), "kmers_with_pos");
        auto *f2 = getStdlibFunc(it->getFunc(), "canonical_with_pos");
        if (f1 && f2 && isConst<int64_t>(prev->back(), 1)) {
          auto *funcType = cast<types::FuncType>(f1->getType());
          auto *genType = cast<types::GeneratorType>(funcType->getReturnType());
          auto *seqType = funcType->front();
          auto *kmerType =
              cast<types::MemberedType>(genType->getBase())->back().getType();
          auto *kmersCanonicalWithPosFunc = M->getOrRealizeFunc(
              "_kmers_canonical_with_pos", genType, {seqType}, {kmerType});
          cast<VarValue>(prev->getFunc())->setVar(kmersCanonicalWithPosFunc);
          prev->erase(prev->end() - 1); // remove step argument
          it = p->erase(it);
          continue;
        }
      }
    }
    prev = &*it;
    ++it;
  }
}

class PrefetchFunctionTransformer : public util::LambdaValueVisitor {
  void handle(ReturnInstr *x) override {
    auto *M = x->getModule();
    x->replaceAll(M->Nr<YieldInstr>(x->getValue()));
  }

  void handle(CallInstr *x) override {
    // if (!getitem_call) return
    auto *M = x->getModule();
    Value *prefetch = nullptr; // TODO
    auto *yield = M->Nr<YieldInstr>();

    auto *series = M->Nr<SeriesFlow>();
    series->push_back(prefetch);
    series->push_back(yield);

    x->replaceAll(M->Nr<FlowInstr>(series, x));
  }
};

void PipelineOptimizations::handle(PipelineFlow *x) {
  std::cout << "BEFORE: " << *x << std::endl;
  applySubstitutionOptimizations(x);
  std::cout << "AFTER:  " << *x << std::endl << std::endl;
}

} // namespace pipeline
} // namespace transform
} // namespace ir
} // namespace seq
