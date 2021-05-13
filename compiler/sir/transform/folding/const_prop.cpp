#include "const_prop.h"

#include "sir/analyze/dataflow/reaching.h"
#include "sir/util/cloning.h"

namespace seq {
namespace ir {
namespace transform {
namespace folding {

void ConstPropPass::handle(VarValue *v) {
  auto *r = getAnalysisResult<analyze::dataflow::RDResult>(reachingDefKey);
  if (!r)
    return;

  auto *c = r->cfgResult;

  auto it = r->results.find(getParentFunc()->getId());
  auto it2 = c->graphs.find(getParentFunc()->getId());
  if (it == r->results.end() || it2 == c->graphs.end())
    return;

  auto *rd = it->second.get();
  auto *cfg = it2->second.get();

  auto reaching = rd->getReachingDefinitions(v->getVar(), v);

  if (reaching.size() != 1)
    return;

  auto def = *reaching.begin();
  if (def == -1)
    return;

  auto *constDef = cast<Const>(cfg->getValue(def));
  if (!constDef || (!isA<IntConst>(constDef) && !isA<FloatConst>(constDef) &&
                    !isA<BoolConst>(constDef)))
    return;

  util::CloneVisitor cv(v->getModule());
  v->replaceAll(cv.clone(constDef));
}

} // namespace folding
} // namespace transform
} // namespace ir
} // namespace seq
