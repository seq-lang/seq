#include "flow.h"

#include "util/fmt/ostream.h"
#include "util/visitor.h"

#include "instr.h"
#include "operand.h"
#include "var.h"

namespace {
void dedupeFlows(seq::ir::AttributeHolder *parent, std::vector<seq::ir::Flow *> flows) {
  std::unordered_set<std::string> names;
  for (auto &f : flows) {
    if (f) {
      if (names.find(f->name) != names.end()) {
        f->name = fmt::format(FMT_STRING("{}.{}"), f->name, names.size());
      }
      names.insert(f->name);
      f->parent = parent;
    }
  }
}
} // namespace

namespace seq {
namespace ir {

void Flow::accept(util::SIRVisitor &v) { v.visit(this); }

std::string Flow::referenceString() const {
  return fmt::format(FMT_STRING("{}.{}"), parent ? parent->referenceString() : "",
                     name);
}

void BlockFlow::accept(util::SIRVisitor &v) { v.visit(this); }

std::ostream &BlockFlow::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: {{\n"), name);
  for (auto &instr : instructions) {
    fmt::print(os, FMT_STRING("{}\n"), *instr);
  }
  os << '}';
  return os;
}

void SeriesFlow::accept(util::SIRVisitor &v) { v.visit(this); }

std::ostream &SeriesFlow::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: {{\n"), name);
  for (auto &child : series) {
    fmt::print(os, FMT_STRING("{}\n"), *child);
  }
  os << '}';
  return os;
}

WhileFlow::WhileFlow(std::string name, OperandPtr cond, FlowPtr check, FlowPtr body)
    : Flow(std::move(name)), check(std::move(check)), body(std::move(body)),
      cond(std::move(cond)) {
  dedupeFlows(this, {this->check.get(), this->body.get()});
  this->cond->parent = this;
}

void WhileFlow::accept(util::SIRVisitor &v) { v.visit(this); }

std::vector<Flow *> WhileFlow::getChildren() const {
  std::vector<Flow *> ret;
  if (check)
    ret.push_back(check.get());
  if (body)
    ret.push_back(body.get());

  return ret;
}

std::ostream &WhileFlow::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: while ("), name);
  if (check)
    fmt::print(os, FMT_STRING("({}) "), *check);
  fmt::print(os, FMT_STRING("{}) {{\n{}\n}}"), *cond, *body);
  return os;
}

ForFlow::ForFlow(std::string name, FlowPtr setup, OperandPtr cond, FlowPtr check,
                 FlowPtr body, FlowPtr update)
    : Flow(std::move(name)), setup(std::move(setup)), check(std::move(check)),
      body(std::move(body)), update(std::move(update)), cond(std::move(cond)) {
  dedupeFlows(this, {this->setup.get(), this->check.get(), this->body.get(), this->update.get()});
  this->cond->parent = this;
}

void ForFlow::accept(util::SIRVisitor &v) { v.visit(this); }

std::vector<Flow *> ForFlow::getChildren() const {
  std::vector<Flow *> ret;
  if (setup)
    ret.push_back(setup.get());
  if (check)
    ret.push_back(check.get());
  if (body)
    ret.push_back(body.get());
  if (update)
    ret.push_back(update.get());

  return ret;
}

std::ostream &ForFlow::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: for ("), name);
  if (setup)
    fmt::print(os, FMT_STRING("({})"), *setup);
  os << "; ";
  if (check)
    fmt::print(os, FMT_STRING("({}) "), *check);
  fmt::print(os, FMT_STRING("{}; "), *cond);
  if (update)
    fmt::print(os, FMT_STRING("({})"), *update);
  fmt::print(os, FMT_STRING(") {{\n{}\n}}"), *body);
  return os;
}

IfFlow::IfFlow(std::string name, OperandPtr cond, FlowPtr check, FlowPtr trueBranch,
               FlowPtr falseBranch)
    : Flow(std::move(name)), check(std::move(check)), trueBranch(std::move(trueBranch)),
      falseBranch(std::move(falseBranch)), cond(std::move(cond)) {
  dedupeFlows(this, {this->check.get(), this->trueBranch.get(), this->falseBranch.get()});
  this->cond->parent = this;
}

void IfFlow::accept(util::SIRVisitor &v) { v.visit(this); }

std::vector<Flow *> IfFlow::getChildren() const {
  std::vector<Flow *> ret;
  if (check)
    ret.push_back(check.get());
  if (trueBranch)
    ret.push_back(trueBranch.get());
  if (falseBranch)
    ret.push_back(falseBranch.get());

  return ret;
}

std::ostream &IfFlow::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: if ("), name);
  if (check)
    fmt::print(os, FMT_STRING("({}) "), *check);
  fmt::print(os, FMT_STRING("{}) {{\n{}\n}}"), *cond, *trueBranch);
  if (falseBranch)
    fmt::print(os, FMT_STRING("else {{\n{}\n}}"), *falseBranch);
  return os;
}

void TryCatchFlow::accept(util::SIRVisitor &v) { v.visit(this); }

std::vector<Flow *> TryCatchFlow::getChildren() const {
  std::vector<Flow *> ret;
  for (auto &c : catches) {
    ret.push_back(c->handler.get());
  }
  return ret;
}

std::ostream &TryCatchFlow::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: try {{\n{}\n}}"), name, *body);
  for (auto &c : catches) {
    fmt::print(os, FMT_STRING("catch ({}{}{}) {{\n{}\n}}"), *c->type,
               c->catchVar ? " -> " : "",
               c->catchVar ? c->catchVar->referenceString() : "", *c->handler);
  }
  if (finally)
    fmt::print(os, FMT_STRING("finally {{\n{}\n}}"), *finally);
  return os;
}

} // namespace ir
} // namespace seq