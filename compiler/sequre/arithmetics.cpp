#include "arithmetics.h"
#include "sir/util/cloning.h"
#include "sir/util/irtools.h"
#include "sir/util/matching.h"
#include <iterator>
#include <math.h>

namespace seq {

using namespace ir;

const std::string secureContainerTypeName = "SecureContainer";

/*
 * Binary expression tree
 */
const int BET_ADD_OP = 1;
const int BET_MUL_OP = 2;
const int BET_POW_OP = 3;
const int BET_REVEAL_OP = 4;
const int BET_OTHER_OP = 5;

class BETNode {
  int64_t value;
  int variableId;
  int op;
  BETNode *leftChild;
  BETNode *rightChild;
  bool expanded;
  bool constant;

public:
  BETNode();
  BETNode(int variableId);
  BETNode(int variableId, int op, bool expanded, int64_t value, bool constant);
  BETNode(int variableId, int op, BETNode *leftChild, BETNode *rightChild,
          bool expanded, int64_t value, bool constant);
  ~BETNode() {
    if (leftChild)
      delete leftChild;
    if (rightChild)
      delete rightChild;
  }

  void setVariableId(int variableId) { this->variableId = variableId; }
  void setOperator(int op) { this->op = op; }
  void setLeftChild(BETNode *leftChild) { this->leftChild = leftChild; }
  void setRightChild(BETNode *rightChild) { this->rightChild = rightChild; }
  void setExpanded() { expanded = true; }
  int getVariableId() { return variableId; }
  int getOperator() { return op; }
  int64_t getValue() { return value; }
  BETNode *getLeftChild() { return leftChild; }
  BETNode *getRightChild() { return rightChild; }
  bool isExpanded() { return expanded; }
  bool isLeaf() { return !leftChild && !rightChild; }
  bool isAdd() { return op == BET_ADD_OP; }
  bool isMul() { return op == BET_MUL_OP; }
  bool isPow() { return op == BET_POW_OP; }
  bool isConstant() { return constant; }
  void replace(BETNode *);
  BETNode *copy();
  void print();
};

BETNode::BETNode()
    : value(1), variableId(0), op(0), leftChild(nullptr), rightChild(nullptr),
      expanded(false), constant(false) {}

BETNode::BETNode(int variableId)
    : value(1), variableId(variableId), op(0), leftChild(nullptr), rightChild(nullptr),
      expanded(false), constant(false) {}

BETNode::BETNode(int variableId, int op, bool expanded, int64_t value, bool constant)
    : value(value), variableId(variableId), op(op), leftChild(nullptr),
      rightChild(nullptr), expanded(expanded), constant(constant) {}

BETNode::BETNode(int variableId, int op, BETNode *leftChild, BETNode *rightChild,
                 bool expanded, int64_t value, bool constant)
    : value(value), variableId(variableId), op(op), leftChild(leftChild),
      rightChild(rightChild), expanded(expanded), constant(constant) {}

void BETNode::replace(BETNode *other) {
  op = other->getOperator();
  leftChild = other->getLeftChild();
  rightChild = other->getRightChild();
  expanded = other->isExpanded();
  value = other->getValue();
  constant = other->isConstant();
}

BETNode *BETNode::copy() {
  auto *newNode = new BETNode(variableId, op, expanded, value, constant);
  auto *lc = getLeftChild();
  auto *rc = getRightChild();
  if (lc)
    newNode->setLeftChild(lc->copy());
  if (rc)
    newNode->setRightChild(rc->copy());
  return newNode;
}

void BETNode::print() {
  std::cout << op << " " << variableId
            << (constant ? " Is constant " : " Not constant ") << value << std::endl;
  if (leftChild)
    leftChild->print();
  if (rightChild)
    rightChild->print();
}

class BET {
  std::unordered_map<int, BETNode *> roots;
  std::vector<int> stopVarIds;
  std::set<int> vars;
  std::vector<BETNode *> polynomials;
  std::vector<std::vector<int64_t>> pascalMatrix;
  bool treeAltered;

public:
  BET() : treeAltered(false) {}
  ~BET() {
    auto *root = this->root();
    if (root)
      delete root;
  }

  int getVarsSize() { return vars.size(); }
  void addRoot(BETNode *betNode) { roots[betNode->getVariableId()] = betNode; }
  void addRoot(int, int);
  void addNode(BETNode *);
  void addStopVar(int varId) { stopVarIds.insert(stopVarIds.begin(), varId); }
  void formPolynomials();
  void parseVars(BETNode *);
  BETNode *root();
  BETNode *polyRoot();
  BETNode *getNextPolyNode();
  std::vector<int64_t> extractCoefficents(BETNode *);
  std::vector<int64_t> extractExponents(BETNode *);
  std::set<int> extractVars(BETNode *);
  std::vector<std::vector<int64_t>> getPascalMatrix() { return pascalMatrix; }

private:
  void addVar(int varId) { vars.insert(varId); }
  void expandNode(BETNode *);
  void expandPow(BETNode *);
  void expandMul(BETNode *);
  void formPolynomial(BETNode *);
  void extractCoefficents(BETNode *, std::vector<int64_t> &);
  void extractExponents(BETNode *, std::vector<int64_t> &);
  void extractVars(BETNode *, std::set<int> &);
  void parseExponents(BETNode *, std::map<int, int64_t> &);
  void updatePascalMatrix(int64_t);
  BETNode *getMulTree(BETNode *, BETNode *, int64_t, int64_t);
  BETNode *getPowTree(BETNode *, BETNode *, int64_t, int64_t);
  int64_t parseCoefficient(BETNode *);
  int64_t getBinomialCoefficient(int64_t, int64_t);
  std::vector<int64_t> getPascalRow(int64_t);
};

void BET::expandNode(BETNode *betNode) {
  if (betNode->isExpanded())
    return;

  int op = betNode->getOperator();
  if (!op) {
    auto search = roots.find(betNode->getVariableId());
    if (search != roots.end())
      betNode->replace(search->second);
  } else {
    expandNode(betNode->getLeftChild());
    expandNode(betNode->getRightChild());
  }

  betNode->setExpanded();
}

void BET::expandPow(BETNode *betNode) {
  auto *lc = betNode->getLeftChild();
  auto *rc = betNode->getRightChild();

  assert(rc->isConstant() &&
         "Sequre polynomial optimization expects each exponent to be a constant.");

  if (lc->isMul()) {
    treeAltered = true;
    betNode->setOperator(BET_MUL_OP);
    lc->setOperator(BET_POW_OP);
    auto *newPowNode =
        new BETNode(0, BET_POW_OP, lc->getRightChild(), rc, true, 1, false);
    betNode->setRightChild(newPowNode);
    lc->setRightChild(rc->copy());
    return;
  }

  if (lc->isAdd()) {
    treeAltered = true;
    auto *v1 = lc->getLeftChild();
    auto *v2 = lc->getRightChild();

    auto *powTree = getPowTree(v1, v2, rc->getValue(), 0);

    betNode->setOperator(BET_ADD_OP);
    delete lc;
    betNode->setLeftChild(powTree->getLeftChild());
    delete rc;
    betNode->setRightChild(powTree->getRightChild());
    return;
  }
}

void BET::expandMul(BETNode *betNode) {
  treeAltered = true;

  auto *lc = betNode->getLeftChild();
  auto *rc = betNode->getRightChild();

  auto *addNode = lc->isAdd() ? lc : rc;
  auto *otherNode = lc->isAdd() ? rc : lc;
  betNode->setOperator(BET_ADD_OP);
  addNode->setOperator(BET_MUL_OP);
  auto *newMulNode =
      new BETNode(0, BET_MUL_OP, addNode->getRightChild(), otherNode, true, 1, false);
  if (lc == otherNode)
    betNode->setLeftChild(newMulNode);
  if (rc == otherNode)
    betNode->setRightChild(newMulNode);
  addNode->setRightChild(otherNode->copy());
}

void BET::formPolynomial(BETNode *betNode) {
  if (betNode->isLeaf())
    return;

  if (betNode->isPow()) {
    expandPow(betNode);
    return;
  }

  auto *lc = betNode->getLeftChild();
  auto *rc = betNode->getRightChild();
  if (!betNode->isMul() || !(lc->isAdd() || rc->isAdd())) {
    formPolynomial(lc);
    formPolynomial(rc);
    return;
  }

  expandMul(betNode);
}

void BET::extractCoefficents(BETNode *betNode, std::vector<int64_t> &coefficients) {
  if (!(betNode->isAdd())) {
    coefficients.push_back(parseCoefficient(betNode));
    return;
  }

  auto *lc = betNode->getLeftChild();
  auto *rc = betNode->getRightChild();
  extractCoefficents(lc, coefficients);
  extractCoefficents(rc, coefficients);
}

void BET::extractExponents(BETNode *betNode, std::vector<int64_t> &exponents) {
  if (!(betNode->isAdd())) {
    std::map<int, int64_t> termExponents;
    for (auto varId : vars)
      termExponents[varId] = 0;
    parseExponents(betNode, termExponents);
    for (auto e : termExponents)
      exponents.push_back(e.second);
    return;
  }

  auto *lc = betNode->getLeftChild();
  auto *rc = betNode->getRightChild();
  extractExponents(lc, exponents);
  extractExponents(rc, exponents);
}

void BET::extractVars(BETNode *betNode, std::set<int> &vars) {
  if (betNode->isConstant())
    return;
  if (betNode->isLeaf()) {
    vars.insert(betNode->getVariableId());
    return;
  }
    
  auto *lc = betNode->getLeftChild();
  auto *rc = betNode->getRightChild();
  extractVars(lc, vars);
  extractVars(rc, vars);
}

void BET::parseExponents(BETNode *betNode, std::map<int, int64_t> &termExponents) {
  if (betNode->isConstant())
    return;
  if (betNode->isLeaf()) {
    termExponents[betNode->getVariableId()]++;
    return;
  }

  auto *lc = betNode->getLeftChild();
  auto *rc = betNode->getRightChild();

  if (betNode->isPow() && !lc->isConstant()) {
    assert(rc->isConstant() &&
           "Sequre polynomial optimization expects each exponent to be a constant.");
    termExponents[lc->getVariableId()] += rc->getValue();
    return;
  }

  parseExponents(lc, termExponents);
  parseExponents(rc, termExponents);
}

void BET::updatePascalMatrix(int64_t n) {
  for (auto i = pascalMatrix.size(); i < n + 1; ++i) {
    auto newRow = std::vector<int64_t>(i + 1);
    for (auto j = 0; j < i + 1; ++j)
      newRow[j] = (j == 0 || j == i)
                      ? 1
                      : (pascalMatrix[i - 1][j - 1] + pascalMatrix[i - 1][j]);
    pascalMatrix.push_back(newRow);
  }
}

BETNode *BET::getMulTree(BETNode *v1, BETNode *v2, int64_t constant, int64_t iter) {
  auto *pascalNode =
      new BETNode(0, 0, true, getBinomialCoefficient(constant, iter), true);
  auto *leftConstNode = new BETNode(0, 0, true, constant - iter, true);
  auto *rightConstNode = new BETNode(0, 0, true, iter, true);
  auto *leftPowNode =
      new BETNode(0, BET_POW_OP, v1->copy(), leftConstNode, true, 1, false);
  auto *rightPowNode =
      new BETNode(0, BET_POW_OP, v2->copy(), rightConstNode, true, 1, false);
  auto *rightMulNode =
      new BETNode(0, BET_MUL_OP, leftPowNode, rightPowNode, true, 1, false);

  return new BETNode(0, BET_MUL_OP, pascalNode, rightMulNode, true, 1, false);
}

BETNode *BET::getPowTree(BETNode *v1, BETNode *v2, int64_t constant, int64_t iter) {
  auto *newMulNode = getMulTree(v1, v2, constant, iter);

  if (constant == iter)
    return newMulNode;

  auto *newAddNode = new BETNode(0, BET_ADD_OP, true, 1, false);

  newAddNode->setLeftChild(newMulNode);
  newAddNode->setRightChild(getPowTree(v1, v2, constant, iter + 1));

  return newAddNode;
}

int64_t BET::parseCoefficient(BETNode *betNode) {
  auto *lc = betNode->getLeftChild();
  auto *rc = betNode->getRightChild();

  if (betNode->isPow()) {
    assert(lc->isLeaf() && "Pow expression should be at bottom of the polynomial tree");
    return (lc->isConstant() ? std::pow(lc->getValue(), rc->getValue()) : 1);
  }
  if (betNode->isConstant() || betNode->isLeaf()) {
    return betNode->getValue();
  }

  return parseCoefficient(lc) * parseCoefficient(rc);
}

int64_t BET::getBinomialCoefficient(int64_t n, int64_t k) {
  auto pascalRow = getPascalRow(n);
  return pascalRow[k];
}

std::vector<int64_t> BET::getPascalRow(int64_t n) {
  if (n >= pascalMatrix.size())
    updatePascalMatrix(n);

  return pascalMatrix[n];
}

void BET::addRoot(int newVarId, int oldVarId) {
  auto *oldNode = roots[oldVarId]->copy();
  oldNode->setVariableId(newVarId);
  roots[newVarId] = oldNode;
}

void BET::addNode(BETNode *betNode) {
  expandNode(betNode);
  addRoot(betNode);
}

void BET::formPolynomials() {
  for (int stopVarId : stopVarIds) {
    auto *polyRoot = roots[stopVarId]->copy();
    do {
      treeAltered = false;
      formPolynomial(polyRoot);
    } while (treeAltered);
    polynomials.push_back(polyRoot);
  }
}

void BET::parseVars(BETNode *betNode) {
  if (betNode->isConstant())
    return;
  if (betNode->isLeaf()) {
    addVar(betNode->getVariableId());
    return;
  }

  parseVars(betNode->getLeftChild());
  parseVars(betNode->getRightChild());
}

BETNode *BET::root() {
  if (!stopVarIds.size())
    return nullptr;

  auto stopVarId = stopVarIds.back();
  auto search = roots.find(stopVarId);
  if (search == roots.end())
    return nullptr;

  return roots[stopVarId];
}

BETNode *BET::polyRoot() {
  if (!polynomials.size())
    return nullptr;

  return polynomials.back();
}

BETNode *BET::getNextPolyNode() {
  auto *polyNode = polyRoot();

  if (polyNode) {
    polynomials.pop_back();
    return polyNode;
  }

  return nullptr;
}

std::vector<int64_t> BET::extractCoefficents(BETNode *betNode) {
  std::vector<int64_t> coefficients;
  extractCoefficents(betNode, coefficients);
  return coefficients;
}

std::vector<int64_t> BET::extractExponents(BETNode *betNode) {
  std::vector<int64_t> exponents;
  extractExponents(betNode, exponents);
  return exponents;
}

std::set<int> BET::extractVars(BETNode *betNode) {
  std::set<int> varIds;
  extractVars(betNode, varIds);
  return varIds;
}

bool isArithmetic(int op) { return op && op < 4; }
bool isReveal(int op) { return op == 4; }

/*
 * Substitution optimizations
 */

bool isSequreFunc(Func *f) {
  return bool(f) && util::hasAttribute(f, "std.sequre.attributes.sequre");
}

bool isPolyOptFunc(Func *f) {
  return bool(f) && util::hasAttribute(f, "std.sequre.attributes.sequre_poly");
}

bool isBeaverOptFunc(Func *f) {
  return bool(f) && util::hasAttribute(f, "std.sequre.attributes.sequre_beaver");
}

int getOperator(CallInstr *callInstr) {
  auto *f = util::getFunc(callInstr->getCallee());
  auto instrName = f->getName();
  if (instrName.find("__add__") != std::string::npos)
    return BET_ADD_OP;
  if (instrName.find("__mul__") != std::string::npos)
    return BET_MUL_OP;
  if (instrName.find("__pow__") != std::string::npos)
    return BET_POW_OP;
  if (instrName.find("secure_reveal") != std::string::npos)
    return BET_REVEAL_OP;
  return BET_OTHER_OP;
}

types::Type *getTupleType(int n, types::Type *elemType, Module *M) {
  std::vector<types::Type *> tupleTypes;
  for (int i = 0; i != n; ++i)
    tupleTypes.push_back(elemType);
  return M->getTupleType(tupleTypes);
}

BETNode *parseArithmetic(CallInstr *callInstr) {
  // Arithmetics are binary
  auto *betNode = new BETNode();

  auto op = getOperator(callInstr);
  betNode->setOperator(op);

  auto *lhs = callInstr->front();
  auto *rhs = callInstr->back();
  auto *lhsInstr = cast<CallInstr>(lhs);
  auto *rhsInstr = cast<CallInstr>(rhs);
  auto *lhsConst = cast<IntConst>(lhs);
  auto *rhsConst = cast<IntConst>(rhs);

  if (lhsConst)
    betNode->setLeftChild(new BETNode(lhs->getId(), 0, true, lhsConst->getVal(), true));
  else if (!lhsInstr)
    betNode->setLeftChild(new BETNode(lhs->getUsedVariables().front()->getId()));
  else
    betNode->setLeftChild(parseArithmetic(lhsInstr));

  if (rhsConst)
    betNode->setRightChild(
        new BETNode(rhs->getId(), 0, true, rhsConst->getVal(), true));
  else if (!rhsInstr)
    betNode->setRightChild(new BETNode(rhs->getUsedVariables().front()->getId()));
  else
    betNode->setRightChild(parseArithmetic(rhsInstr));

  return betNode;
}

void parseInstruction(seq::ir::Value *instruction, BET *bet) {
  auto *retIns = cast<ReturnInstr>(instruction);
  if (retIns) {
    auto vars = retIns->getValue()->getUsedVariables();
    bet->addStopVar(vars.front()->getId());
    return;
  }

  auto *assIns = cast<AssignInstr>(instruction);
  if (!assIns)
    return;

  auto *var = assIns->getLhs();
  auto *callInstr = cast<CallInstr>(assIns->getRhs());
  if (!callInstr)
    return;

  auto op = getOperator(callInstr);
  if (isArithmetic(op)) {
    auto *betNode = parseArithmetic(callInstr);
    betNode->setVariableId(var->getId());
    bet->addNode(betNode);
  } else if (isReveal(op)) {
    bet->addRoot(var->getId(), util::getVar(callInstr->back())->getId());
    bet->addStopVar(var->getId());
  }
}

CallInstr *nextPolynomialCall(CallInstr *v, BodiedFunc *bf, BET *bet) {
  auto polyNode = bet->getNextPolyNode();
  auto coefs = bet->extractCoefficents(polyNode);
  auto exps = bet->extractExponents(polyNode);
  auto vars = bet->extractVars(polyNode);

  auto *M = v->getModule();
  auto *self = M->Nr<VarValue>(bf->arg_front());
  auto *selfType = self->getType();
  auto *funcType = cast<types::FuncType>(bf->getType());
  auto *returnType = funcType->getReturnType();
  auto *inputsType = getTupleType(vars.size(), returnType, M);
  auto *coefsType = getTupleType(coefs.size(), M->getIntType(), M);
  auto *expsType = getTupleType(exps.size(), M->getIntType(), M);

  auto *evalPolyFunc = M->getOrRealizeMethod(
      selfType, "secure_evalp", {selfType, inputsType, coefsType, expsType});
  assert(evalPolyFunc && "secure_evalp not found in provided MPC class");

  std::vector<Value *> inputArgs;
  for (auto it = bf->arg_begin(); it != bf->arg_end(); ++it) {
    if (vars.find((*it)->getId()) == vars.end())
      continue;
    auto *arg = M->Nr<VarValue>(*it);
    inputArgs.push_back(arg);
  }
  std::vector<Value *> coefsArgs;
  for (auto e : coefs)
    coefsArgs.push_back(M->getInt(e));
  std::vector<Value *> expsArgs;
  for (auto e : exps)
    expsArgs.push_back(M->getInt(e));

  auto *inputArg = util::makeTuple(inputArgs, M);
  auto *coefsArg = util::makeTuple(coefsArgs, M);
  auto *expsArg = util::makeTuple(expsArgs, M);

  return util::call(evalPolyFunc, {self, inputArg, coefsArg, expsArg});
}

void convertInstructions(CallInstr *v, BodiedFunc *bf, SeriesFlow *series, BET *bet) {
  auto it = series->begin();
  while (it != series->end()) {
    auto *retIns = cast<ReturnInstr>(*it);
    if (retIns) {
      retIns->setValue(nextPolynomialCall(v, bf, bet));
      ++it;
      continue;
    }

    auto *assIns = cast<AssignInstr>(*it);
    if (!assIns) {
      ++it;
      continue;
    }

    auto *callInstr = cast<CallInstr>(assIns->getRhs());
    if (!callInstr) {
      ++it;
      continue;
    }

    auto op = getOperator(callInstr);
    if (isArithmetic(op)) {
      it = series->erase(it);
      continue;
    }

    if (isReveal(op)) {
      callInstr->setArgs({callInstr->front(), nextPolynomialCall(v, bf, bet)});
      ++it;
      continue;
    }
  }
}

void ArithmeticsOptimizations::applyPolynomialOptimizations(CallInstr *v) {
  auto *f = util::getFunc(v->getCallee());
  if (!isPolyOptFunc(f))
    return;

  auto *bf = cast<BodiedFunc>(f);
  auto *series = cast<SeriesFlow>(bf->getBody());

  auto *bet = new BET();
  for (auto it = series->begin(); it != series->end(); ++it)
    parseInstruction(*it, bet);
  bet->parseVars(bet->root());
  bet->formPolynomials();

  convertInstructions(v, bf, series, bet);
}

void ArithmeticsOptimizations::applyBeaverOptimizations(CallInstr *v) {
  auto *pf = getParentFunc();
  if (!isSequreFunc(pf) && !isBeaverOptFunc(pf))
    return;
  auto *f = util::getFunc(v->getCallee());
  if (!f)
    return;
  bool isGt = f->getName().find("__gt__") != std::string::npos;
  bool isLt = f->getName().find("__lt__") != std::string::npos;
  bool isAdd = f->getName().find("__add__") != std::string::npos;
  bool isSub = f->getName().find("__sub__") != std::string::npos;
  bool isMul = f->getName().find("__mul__") != std::string::npos;
  bool isDiv = f->getName().find("__truediv__") != std::string::npos;
  bool isPow = f->getName().find("__pow__") != std::string::npos;
  if (!isGt && !isLt && !isAdd && !isSub && !isMul && !isPow && !isDiv)
    return;

  auto *M = v->getModule();
  auto *self = M->Nr<VarValue>(pf->arg_front());
  auto *selfType = self->getType();
  auto *lhs = v->front();
  auto *rhs = v->back();
  auto *lhsType = lhs->getType();
  auto *rhsType = rhs->getType();

  bool isSqrtInv = false;
  if (isDiv) {  // Special case where 1 / sqrt(x) is called
    auto *sqrtInstr = cast<CallInstr>(rhs);
    if (sqrtInstr) {
      auto *sqrtFunc = util::getFunc(sqrtInstr->getCallee());
      if (sqrtFunc)
        isSqrtInv = sqrtFunc->getName().find("sqrt") != std::string::npos;
    }
  }

  bool lhs_is_secure_container = lhsType->getName().find(secureContainerTypeName) != std::string::npos;
  bool rhs_is_secure_container = rhsType->getName().find(secureContainerTypeName) != std::string::npos;

  if (!lhs_is_secure_container and !rhs_is_secure_container)
    return;

  bool lhs_is_int = lhsType->is(M->getIntType());
  bool rhs_is_int = rhsType->is(M->getIntType());

  if (isMul && lhs_is_int)
    return;
  if (isMul && rhs_is_int)
    return;
  if (isDiv && lhs_is_int && !isSqrtInv)
    return;
  if (isPow && lhs_is_int)
    return;
  if (isPow && !rhs_is_int)
    return;

  std::string methodName =
    isGt      ? "secure_gt"       :
    isLt      ? "secure_lt"       :
    isAdd     ? "secure_add"      :
    isSub     ? "secure_sub"      :
    isMul     ? "secure_mult"     :
    isSqrtInv ? "secure_sqrt_inv" :
    isDiv     ? "secure_div"      :
    isPow     ? "secure_pow"      :
    "invalid_operation";
  if (!isBeaverOptFunc(pf) && (isMul || isPow || isDiv || isSqrtInv))
    methodName += "_no_cache";

  if (isSqrtInv) {
    rhs = cast<CallInstr>(rhs)->back();
    rhsType = rhs->getType();
  }

  auto *sequreInternalType = M->getOrRealizeType("Internal", {}, "std.sequre.stdlib.internal");
  auto *method =
      M->getOrRealizeMethod(sequreInternalType, methodName, {selfType, lhsType, rhsType});
  
  if (!method)
    return;

  auto *func = util::call(method, {self, lhs, rhs});
  v->replaceAll(func);
}

void ArithmeticsOptimizations::applyOptimizations(CallInstr *v) {
  applyPolynomialOptimizations(v);
  applyBeaverOptimizations(v);
}

void ArithmeticsOptimizations::handle(CallInstr *v) { applyOptimizations(v); }

} // namespace seq
