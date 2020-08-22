#include "lang/seq.h"

using namespace seq;
using namespace llvm;

Block::Block(Stmt *parent) : parent(parent), stmts() {}

void Block::add(Stmt *stmt) {
  stmts.push_back(stmt);
  stmt->setParent(this);
}

void Block::codegen(BasicBlock *&block) {
  for (auto *stmt : stmts)
    stmt->codegen(block);
}

Stmt::Stmt(std::string name)
    : SrcObject(), name(std::move(name)), base(nullptr), breaks(), continues(),
      parent(nullptr), loop(false), tc(nullptr) {}

std::string Stmt::getName() const { return name; }

Stmt *Stmt::getPrev() const {
  if (!parent)
    return nullptr;

  return parent->parent;
}

Block *Stmt::getParent() { return parent; }

void Stmt::setParent(Block *parent) {
  assert(!this->parent);
  this->parent = parent;
}

BaseFunc *Stmt::getBase() const { return base; }

void Stmt::setBase(BaseFunc *base) {
  if (!base)
    return;

  this->base = base;
}

Stmt *Stmt::findEnclosingLoop() {
  Stmt *stmt = this;
  Stmt *orig = stmt;

  while (stmt) {
    if (stmt->isLoop())
      return stmt;
    stmt = stmt->getPrev();
  }

  throw exc::SeqException("break or continue outside of loop", orig->getSrcInfo());
}

void Stmt::addBreakToEnclosingLoop(BranchInst *inst) {
  findEnclosingLoop()->addBreak(inst);
}

void Stmt::addContinueToEnclosingLoop(BranchInst *inst) {
  findEnclosingLoop()->addContinue(inst);
}

void Stmt::setTryCatch(TryCatch *tc) { this->tc = tc; }

TryCatch *Stmt::getTryCatch() {
  if (tc)
    return tc;

  Stmt *stmt = getPrev();
  Stmt *last = this;

  while (stmt) {
    if (auto *s = dynamic_cast<TryCatch *>(stmt)) {
      // make sure we're not enclosed by except or finally
      if (last->parent == s->getBlock())
        return s;
    }

    last = stmt;
    stmt = stmt->getPrev();
  }

  return nullptr;
}

bool Stmt::isLoop() { return loop; }

void Stmt::ensureLoop() { assert(isLoop()); }

void Stmt::addBreak(BranchInst *inst) {
  ensureLoop();
  breaks.push_back(inst);
}

void Stmt::addContinue(BranchInst *inst) {
  ensureLoop();
  continues.push_back(inst);
}

void Stmt::setBreaks(BasicBlock *block) {
  ensureLoop();
  for (auto *inst : breaks)
    inst->setSuccessor(0, block);
  breaks.clear();
}

void Stmt::setContinues(BasicBlock *block) {
  ensureLoop();
  for (auto *inst : continues)
    inst->setSuccessor(0, block);
  continues.clear();
}

void Stmt::codegen(BasicBlock *&block) {
  try {
    return codegen0(block);
  } catch (exc::SeqException &e) {
    if (e.getSrcInfo().line <= 0)
      e.setSrcInfo(getSrcInfo());
    throw e;
  }
}

std::ostream &operator<<(std::ostream &os, Stmt &stmt) { return os << stmt.getName(); }
