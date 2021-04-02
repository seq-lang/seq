#include "sequre.h"
#include "sir/util/cloning.h"
#include "sir/util/irtools.h"
#include "sir/util/matching.h"
#include <iterator>

namespace seq {
namespace ir {
namespace transform {
namespace sequre {
/*
 * Binary expression tree
 */
int BET_ADD_OP = 1;
int BET_MUL_OP = 2;
int BET_POW_OP = 3;
int BET_OTHER_OP = 4;

class BETNode {
  public:
    int variableId;
    int op;
    BETNode *leftChild;
    BETNode *rightChild;
    bool expanded;
    int64_t value;
    bool constant;
  
  // public:
    BETNode();
    BETNode(int variableId);
    BETNode(int variableId, int op, bool expanded, int64_t value, bool constant);
    BETNode(int variableId, int op, BETNode *leftChild, BETNode *rightChild, bool expanded, int64_t value, bool constant);
    void setVariableId(int variableId) { this->variableId = variableId; }
    void setOperator(int op) { this->op = op; }
    void setLeftChild(BETNode *leftChild) { this->leftChild = leftChild; }
    void setRightChild(BETNode *rightChild) { this->rightChild = rightChild; }
    int getVariableId() { return variableId; }
    int getOperator() { return op; }
    int getValue() { return value; }
    BETNode* getLeftChild() { return leftChild; }
    BETNode* getRightChild() { return rightChild; }
    bool isExpanded() { return expanded; }
    void setExpanded() { expanded=true; }
    void replace(BETNode *other) {
      op=other->getOperator();
      leftChild=other->getLeftChild();
      rightChild=other->getRightChild();
      expanded=other->isExpanded();
    }
    BETNode* copy();
    bool isLeaf() { return !leftChild && !rightChild; }
    bool isAdd() { return op == BET_ADD_OP; }
    bool isMul() { return op == BET_MUL_OP; }
    bool isPow() { return op == BET_POW_OP; }
    bool isConstant() { return constant; }
    void print();
};

BETNode::BETNode() : variableId(0), op(0), leftChild(nullptr), rightChild(nullptr), expanded(false), value(1), constant(false) {}
BETNode::BETNode(int variableId) : variableId(variableId), op(0), leftChild(nullptr), rightChild(nullptr), expanded(false), value(1), constant(false) {}
BETNode::BETNode(int variableId, int op, bool expanded, int64_t value, bool constant) : variableId(variableId), op(op), leftChild(nullptr), rightChild(nullptr), expanded(expanded), value(value), constant(constant) {}
BETNode::BETNode(int variableId, int op, BETNode *leftChild, BETNode *rightChild, bool expanded, int64_t value, bool constant) : variableId(variableId), op(op), leftChild(leftChild), rightChild(rightChild), expanded(expanded), value(value), constant(constant) {}
BETNode* BETNode::copy() {
  BETNode *newNode = new BETNode(variableId, op, expanded, value, constant);
  BETNode *lc = getLeftChild();
  BETNode *rc = getRightChild();
  if (lc) newNode->setLeftChild(lc->copy());
  if (rc) newNode->setRightChild(rc->copy());
  return newNode;
}
void BETNode::print() {
  std::cout << op << " " << variableId << std::endl;
  if (leftChild) leftChild->print();
  if (rightChild) rightChild->print();
}

class BET {
  public:
    std::unordered_map<int, BETNode*> roots;
    std::vector<int> stopVarIds;
    std::vector<BETNode*> polynomials;
    bool treeAltered;

  // public:
    BET() : treeAltered(false) {}
    void addNode(BETNode *betNode) {
      expandNode(betNode);
      roots[betNode->getVariableId()] = betNode;
    }
    void addStopVar(int varId) { stopVarIds.push_back(varId); }
    void expandNode(BETNode *betNode);
    void formPolynomials();
    void formPolynomial(BETNode *betNode);
    std::vector<int64_t> extractCoefficents(int polyIdx);
    void extractCoefficents(BETNode *betNode, std::vector<int64_t> &coefficients);
    int64_t parseCoefficient(BETNode *betNode);
    std::vector<std::vector<int64_t>> extractExponents(int polyIdx);
    void extractExponents(BETNode *betNode, std::vector<std::vector<int64_t>> &exponents);
    void parseExponents(BETNode *betNode, std::map<int, int64_t> &termExponents);
    BETNode* root() { return roots[stopVarIds.back()]; }
};

void BET::expandNode(BETNode *betNode) {
    if (betNode->isExpanded()) return;

    int op = betNode->getOperator();
    if (!op) { 
      auto search = roots.find(betNode->getVariableId());
      if (search != roots.end()) betNode->replace(search->second);
    } else {
      expandNode(betNode->getLeftChild());
      expandNode(betNode->getRightChild());
    }
    
    betNode->setExpanded();
  }
void BET::formPolynomials() {
  for (int stopVarId : stopVarIds) {
    BETNode *polyRoot = roots[stopVarId]->copy();
    do {
      treeAltered = false;
      formPolynomial(polyRoot);
    } while (treeAltered);
    polynomials.push_back(polyRoot);
  }
}
void BET::formPolynomial(BETNode *betNode) { 
  if (betNode->isLeaf()) return;

  BETNode *lc = betNode->getLeftChild();
  BETNode *rc = betNode->getRightChild();
  
  if (!(betNode->isMul() || betNode->isPow()) ||
      !(lc->isAdd() || rc->isAdd())) {
    formPolynomial(lc);
    formPolynomial(rc);
    return;
  }

  treeAltered = true;
  BETNode *addNode = lc->isAdd() ? lc : rc;
  BETNode *otherNode = lc->isAdd() ? rc : lc;

  if (betNode->isMul()) {
    betNode->setOperator(BET_ADD_OP);
    addNode->setOperator(BET_MUL_OP);
    BETNode *newMulNode = new BETNode(
      0, BET_MUL_OP, addNode->getRightChild(), otherNode, true, 1, false);
    if (lc==otherNode) betNode->setLeftChild(newMulNode);
    if (rc==otherNode) betNode->setRightChild(newMulNode);
    addNode->setRightChild(otherNode->copy());
  }

  if (betNode->isPow()) {
    treeAltered = false;
    throw "Not implemented!";
  }
}
std::vector<int64_t> BET::extractCoefficents(int polyIdx) {
  BETNode *betNode = polynomials[polyIdx];
  std::vector<int64_t> coefficients;
  extractCoefficents(betNode, coefficients);
  return coefficients;
}
void BET::extractCoefficents(BETNode *betNode, std::vector<int64_t> &coefficients) {
  if (!(betNode->isAdd())) {
    coefficients.push_back(parseCoefficient(betNode));
    return;
  }

  BETNode *lc = betNode->getLeftChild();
  BETNode *rc = betNode->getRightChild();
  extractCoefficents(lc, coefficients);
  extractCoefficents(rc, coefficients);
}
int64_t BET::parseCoefficient(BETNode *betNode) {
  if (betNode->isConstant() || betNode->isLeaf()) return betNode->getValue();

  BETNode *lc = betNode->getLeftChild();
  BETNode *rc = betNode->getRightChild();
  return parseCoefficient(lc) * parseCoefficient(rc);
}
std::vector<std::vector<int64_t>> BET::extractExponents(int polyIdx) {
  BETNode *betNode = polynomials[polyIdx];
  std::vector<std::vector<int64_t>> exponents;
  extractExponents(betNode, exponents);
  return exponents;
}
void BET::extractExponents(BETNode *betNode, std::vector<std::vector<int64_t>> &exponents) {
  if (!(betNode->isAdd())) {
    std::map<int, int64_t> termExponents;
    parseExponents(betNode, termExponents);
    std::vector<int64_t> exponentsVec;
    for(auto e : termExponents) exponentsVec.push_back(e.second);
    exponents.push_back(exponentsVec);
    return;
  }

  BETNode *lc = betNode->getLeftChild();
  BETNode *rc = betNode->getRightChild();
  extractExponents(lc, exponents);
  extractExponents(rc, exponents);
}
void BET::parseExponents(BETNode *betNode, std::map<int, int64_t> &termExponents) {
  if (betNode->isConstant()) return;
  if (betNode->isLeaf()) {
    termExponents[betNode->getVariableId()]++;
    return;
  }

  BETNode *lc = betNode->getLeftChild();
  BETNode *rc = betNode->getRightChild();

  if (betNode->isPow()) {
    BETNode *constNode = lc->isConstant() ? lc : rc;
    BETNode *varNode = lc->isConstant() ? rc : lc;
    termExponents[varNode->getVariableId()] += constNode->getValue();
    return;
  }

  parseExponents(lc, termExponents);
  parseExponents(rc, termExponents);
}

/*
 * Substitution optimizations
 */

int getOperator(CallInstr *callInstr) {
  auto *f = util::getFunc(callInstr->getCallee());
  auto instrName = f->getName();
  if (instrName.find("__add__") != std::string::npos) return BET_ADD_OP;
  if (instrName.find("__mul__") != std::string::npos) return BET_MUL_OP;
  if (instrName.find("__pow__") != std::string::npos) return BET_POW_OP;
  return BET_OTHER_OP;
}

BETNode* parseArithmetic(CallInstr *callInstr) {
  // Arithmetics are binary
  BETNode *betNode = new BETNode();
  
  int op = getOperator(callInstr);
  betNode->setOperator(op);
  
  auto *lhs = callInstr->front();
  auto *rhs = callInstr->back();
  auto *lhsInstr = cast<CallInstr>(lhs);
  auto *rhsInstr = cast<CallInstr>(rhs);
  auto *lhsConst = cast<IntConst>(lhs);
  auto *rhsConst = cast<IntConst>(rhs);

  if (lhsConst) betNode->setLeftChild(
    new BETNode(lhs->getId(), 0, true, lhsConst->getVal(), true));
  else if (!lhsInstr) betNode->setLeftChild(
    new BETNode(lhs->getUsedVariables().front()->getId()));
  else betNode->setLeftChild(parseArithmetic(lhsInstr));
  
  if (rhsConst) betNode->setRightChild(
    new BETNode(rhs->getId(), 0, true, rhsConst->getVal(), true));
  else if (!rhsInstr) betNode->setRightChild(
    new BETNode(rhs->getUsedVariables().front()->getId()));
  else betNode->setRightChild(parseArithmetic(rhsInstr));

  return betNode;
}

void parseInstruction(seq::ir::Value *instruction,
                      BET *bet) {
  auto *retIns = cast<ReturnInstr>(instruction);
  if (retIns) {
    auto vars = retIns->getValue()->getUsedVariables();
    bet->addStopVar(vars.front()->getId());
    return;
  }
  
  auto *assIns = cast<AssignInstr>(instruction);
  if (!assIns) return;

  auto *var = assIns->getLhs();
  auto *callInstr = cast<CallInstr>(assIns->getRhs());
  if (!callInstr) return;

  BETNode *betNode = parseArithmetic(callInstr);
  betNode->setVariableId(var->getId());
  bet->addNode(betNode);
}

void ArithmeticsOptimizations::applyPolynomialOptimizations(CallInstr *v) {
    auto *f = util::getFunc(v->getCallee());
    
    if (!f) return;
    if (f->getUnmangledName().find("sequre_") == std::string::npos) return;
    // see(v);
    
    // if (!f) return;
    // auto *pf = getParentFunc();
    // if (!pf || pf->getUnmangledName() != "sequre_arithmetics") return;
    
    // auto *arg = v->front(); // argument of call
    // auto *bar = M->getOrRealizeFunc("bar", {arg->getType()});
    
    // auto *bar = M->getOrRealizeFunc("bar", {});
    // auto *call = util::call(bar, {});
    // insertBefore(call); // call 'bar' before 'foo'
    
    auto *bf = cast<BodiedFunc>(f);
    auto *b = cast<SeriesFlow>(bf->getBody());

    BET *bet = new BET();
    for(auto it = b->begin(); it != b->end(); ++it)
      parseInstruction(*it, bet);
    
    // for (auto& it: bet->roots)
    //   std::cout << it.first << " -> " << it.second << std::endl;
    
    bet->formPolynomials();
    
    std::vector<int64_t> coefs = bet->extractCoefficents(0);
    for(auto e : coefs) std::cout << e << std::endl;
    std::vector<std::vector<int64_t>> exps = bet->extractExponents(0);
    for(auto t : exps) {
      std::cout << std::endl;
      for(auto e : t) std::cout << e << " ";
    }
    
    // auto *evalp = M->getOrRealizeFunc("secure_evalp", {});
    // auto *call = util::call(evalp, {args, coefs, exps});
    // v->replaceAll(call);

    auto *M = v->getModule();
    Value *self = v->front();
    auto *funcType = cast<types::FuncType>(bf->getType());
    auto *returnType = funcType->getReturnType();
    types::Type *selfType = self->getType();
    Func *evalPolyFunc = M->getOrRealizeFunc(
        "secure_evalp",
        {selfType,
         M->getArrayType(returnType),
         M->getArrayType(returnType),
         M->getArrayType(M->getArrayType(returnType)),
         returnType});
    // std::cout << "Method not found!" << std::endl;
    // if (!evalPolyFunc) return;
    // std::cout << "Method found!" << std::endl;

    // Value *evalPolyCall = util::call(evalPolyFunc, {self, coefs, exps});
    // v->replaceAll(evalPolyCall);
}

void ArithmeticsOptimizations::handle(CallInstr *v) {
    applyPolynomialOptimizations(v);
}

} // namespace sequre
} // namespace transform
} // namespace ir
} // namespace seq