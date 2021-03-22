#include "dict.h"

#include <algorithm>

#include "sir/util/cloning.h"
#include "sir/util/irtools.h"
#include "sir/util/matching.h"

namespace {

using namespace seq::ir;

/// get or __getitem__ call metadata
struct GetCall {
  /// the function, nullptr if not a get call
  Func *func = nullptr;
  /// the dictionary
  VarValue *dict = nullptr;
  /// the key
  Const *key = nullptr;
  /// the default value, may be null
  Const *dflt = nullptr;
};

/// Identify the call and return its metadata.
/// @param call the call
/// @return the metadata
GetCall analyzeGet(CallInstr *call) {
  // extract the function
  auto *func = util::getFunc(call->getCallee());
  if (!func)
    return {};

  auto unmangled = func->getUnmangledName();

  // canonical get/__getitem__ calls have at least two arguments
  auto it = call->begin();
  auto dist = std::distance(it, call->end());
  if (dist < 2)
    return {};

  // extract the dictionary and keys
  auto *dict = cast<VarValue>(*it++);
  auto *k = cast<Const>(*it++);

  // the dictionary must be a variable and key must be constant
  if (!dict || !k)
    return {};

  // get calls have a default
  if (unmangled == "get" && std::distance(it, call->end()) == 1) {
    auto *dflt = cast<Const>(*it);
    return {func, dict, k, dflt};
  } else if (unmangled == "__getitem__" && std::distance(it, call->end()) == 0) {
    return {func, dict, k, nullptr};
  }

  // call is not correct
  return {};
}

} // namespace

namespace seq {
namespace ir {
namespace transform {
namespace pythonic {

void DictArithmeticOptimization::handle(CallInstr *v) {
  auto *M = v->getModule();

  // get and check the exterior function (should be a __setitem__ with 3 args)
  auto *setFunc = util::getFunc(v->getCallee());
  if (setFunc && setFunc->getUnmangledName() == "__setitem__" &&
      std::distance(v->begin(), v->end()) == 3) {
    auto it = v->begin();

    // extract all the arguments to the function
    // the dictionary must be a variable, the key must be a constant, and the value must
    // be a call
    auto *dictValue = cast<VarValue>(*it++);
    auto *keyValue = cast<Const>(*it++);
    auto *opCall = cast<CallInstr>(*it++);

    // the call must take exactly two arguments
    if (!dictValue || !keyValue || !opCall ||
        std::distance(opCall->begin(), opCall->end()) != 2)
      return;

    // grab the function, which does not necessarily need to be a magic
    auto *opFunc = util::getFunc(opCall->getCallee());
    auto *getCall = cast<CallInstr>(opCall->front());
    if (!opFunc || !getCall)
      return;

    // check the first argument
    auto getAnalysis = analyzeGet(getCall);
    if (!getAnalysis.func)
      return;

    // second argument can be any non-null value
    auto *constant = opCall->back();

    auto *d1 = cast<VarValue>(dictValue);
    auto *d2 = cast<VarValue>(getAnalysis.dict);

    if (!d1 || !d2)
      return;

    // verify that we are dealing with the same dictionary and key
    if (constant && d1->getVar()->getId() == d2->getVar()->getId() &&
        util::match(keyValue, getAnalysis.key)) {
      util::CloneVisitor cv(M);
      Func *replacementFunc;

      // call non-throwing version if we have a default
      if (getAnalysis.dflt) {
        replacementFunc = M->getOrRealizeMethod(
            dictValue->getType(), "__dict_do_op_throws__",
            {dictValue->getType(), keyValue->getType(), constant->getType(),
             getAnalysis.dflt->getType(), opFunc->getType()});
      } else {
        replacementFunc =
            M->getOrRealizeMethod(dictValue->getType(), "__dict_do_op__",
                                  {dictValue->getType(), keyValue->getType(),
                                   constant->getType(), opFunc->getType()});
      }

      if (replacementFunc) {
        std::vector<Value *> args = {cv.clone(dictValue), cv.clone(keyValue),
                                     cv.clone(constant)};
        if (getAnalysis.dflt)
          args.push_back(cv.clone(getAnalysis.dflt));

        // sanity check to make sure function is inlined
        if (std::distance(replacementFunc->arg_begin(), replacementFunc->arg_end()) !=
            std::distance(replacementFunc->arg_begin(), replacementFunc->arg_end()))
          args.push_back(M->N<VarValue>(v, opFunc));

        v->replaceAll(util::call(replacementFunc, args));
      }
    }
  }
}

} // namespace pythonic
} // namespace transform
} // namespace ir
} // namespace seq
