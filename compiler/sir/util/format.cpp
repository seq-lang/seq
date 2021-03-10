#include "format.h"

#include <algorithm>
#include <sstream>
#include <unordered_set>

#include "util/fmt/format.h"
#include "util/fmt/ostream.h"
#include "visitor.h"

namespace {
using namespace seq::ir;

struct NodeFormatter {
  const types::Type *type = nullptr;
  const Value *value = nullptr;
  const Var *var = nullptr;
  bool canShowFull = false;

  std::unordered_set<int> &seenNodes;
  std::unordered_set<std::string> &seenTypes;

  NodeFormatter(const types::Type *type, std::unordered_set<int> &seenNodes,
                std::unordered_set<std::string> &seenTypes)
      : type(type), seenNodes(seenNodes), seenTypes(seenTypes) {}

  NodeFormatter(const Value *value, std::unordered_set<int> &seenNodes,
                std::unordered_set<std::string> &seenTypes)
      : value(value), seenNodes(seenNodes), seenTypes(seenTypes) {}
  NodeFormatter(const Var *var, std::unordered_set<int> &seenNodes,
                std::unordered_set<std::string> &seenTypes)
      : var(var), seenNodes(seenNodes), seenTypes(seenTypes) {}

  friend std::ostream &operator<<(std::ostream &os, const NodeFormatter &n);
};

class FormatVisitor : util::ConstIRVisitor {
private:
  std::ostream &os;
  std::unordered_set<int> &seenNodes;
  std::unordered_set<std::string> &seenTypes;

public:
  FormatVisitor(std::ostream &os, std::unordered_set<int> &seenNodes,
                std::unordered_set<std::string> &seenTypes)
      : os(os), seenNodes(seenNodes), seenTypes(seenTypes) {}
  virtual ~FormatVisitor() noexcept = default;

  void visit(const IRModule *v) override {
    auto types = makeFormatters(v->types_begin(), v->types_end(), true);
    auto vars = makeFormatters(v->begin(), v->end(), true);
    fmt::print(
        os, FMT_STRING("(module (types ({}))\n(vars ({}))\n(main {})\n(argv {}))"),
        fmt::join(types.begin(), types.end(), "\n"),
        fmt::join(vars.begin(), vars.end(), "\n"),
        makeFormatter(v->getMainFunc(), true), makeFormatter(v->getArgVar(), true));
  }

  void visit(const Var *v) override {
    fmt::print(os, FMT_STRING("(var (ref_string \"{}\")\n(type {})\n(global {}))"),
               v->referenceString(), makeFormatter(v->getType()), v->isGlobal());
  }

  void visit(const BodiedFunc *v) override {
    auto args = makeFormatters(v->arg_begin(), v->arg_end(), true);
    auto symbols = makeFormatters(v->begin(), v->end(), true);
    fmt::print(os,
               FMT_STRING("(bodied_func (ref_string \"{}\")\n(type {})\n(builtin {})\n"
                          "(args ({}))\n(vars ({}))\n(body {}))"),
               v->referenceString(), makeFormatter(v->getType()), v->isBuiltin(),
               fmt::join(args.begin(), args.end(), "\n"),
               fmt::join(symbols.begin(), symbols.end(), "\n"),
               makeFormatter(v->getBody()));
  }

  void visit(const ExternalFunc *v) override {
    fmt::print(
        os,
        FMT_STRING(
            "(external_func (ref_string \"{}\")\n(type {})\n(unmangled_name \"{}\"))"),
        v->referenceString(), makeFormatter(v->getType()), v->getUnmangledName());
  }
  void visit(const InternalFunc *v) override {
    fmt::print(os, FMT_STRING("(internal_func (ref_string \"{}\")\n(type {}))"),
               v->referenceString(), makeFormatter(v->getType()));
  }
  void visit(const LLVMFunc *v) override {
    std::vector<std::string> literals;

    for (auto it = v->literal_begin(); it != v->literal_end(); ++it) {
      const auto &l = *it;
      if (l.isStatic()) {
        literals.push_back(fmt::format(FMT_STRING("(static {})"), l.getStaticValue()));
      } else {
        literals.push_back(
            fmt::format(FMT_STRING("(type {})"), makeFormatter(l.getTypeValue())));
      }
    }

    auto body = v->getLLVMBody();
    std::replace(body.begin(), body.end(), '\n', ' ');

    fmt::print(os,
               FMT_STRING("(llvm_func (ref_string \"{}\")\n(type {})\n(decls \"{}\")\n"
                          "(body \"{}\")\n(literals ({})))"),
               v->referenceString(), makeFormatter(v->getType()),
               v->getLLVMDeclarations(), body,
               fmt::join(literals.begin(), literals.end(), "\n"));
  }

  void visit(const VarValue *v) override {
    fmt::print(os, FMT_STRING("(var_value (ref_string \"{}\")\n(var {}))"),
               v->referenceString(), makeFormatter(v->getVar()));
  }

  void visit(const PointerValue *v) override {
    fmt::print(os, FMT_STRING("(ptr_value (ref_string \"{}\")\n(var {}))"),
               v->referenceString(), makeFormatter(v->getVar()));
  }

  void visit(const SeriesFlow *v) override {
    auto series = makeFormatters(v->begin(), v->end());
    fmt::print(os, FMT_STRING("(series_flow (ref_string \"{}\")\n(body ({})))"),
               v->referenceString(), fmt::join(series.begin(), series.end(), "\n"));
  }
  void visit(const IfFlow *v) override {
    fmt::print(os,
               FMT_STRING("(if_flow (ref_string \"{}\")\n(cond {})\n(true_branch {})\n"
                          "(false_branch {}))"),
               v->referenceString(), makeFormatter(v->getCond()),
               makeFormatter(v->getTrueBranch()), makeFormatter(v->getFalseBranch()));
  }
  void visit(const WhileFlow *v) override {
    fmt::print(os, FMT_STRING("(while_flow (ref_string \"{}\")\n(cond {})\n(body {}))"),
               v->referenceString(), makeFormatter(v->getCond()),
               makeFormatter(v->getBody()));
  }
  void visit(const ForFlow *v) override {
    fmt::print(
        os,
        FMT_STRING("(for_flow (ref_string \"{}\")\n(iter {})\n(var {})\n(body {}))"),
        v->referenceString(), makeFormatter(v->getIter()), makeFormatter(v->getVar()),
        makeFormatter(v->getBody()));
  }
  void visit(const TryCatchFlow *v) override {
    std::vector<std::string> catches;

    for (auto &c : *v) {
      catches.push_back(
          fmt::format(FMT_STRING("(catch (type {})\n(var {})\n(handler {}))"),
                      makeFormatter(c.getType()), makeFormatter(c.getVar()),
                      makeFormatter(c.getHandler())));
    }

    fmt::print(
        os,
        FMT_STRING("(try_catch_flow (ref_string \"{}\")\n(body {})\n(finally {})\n"
                   "(catches ({})))"),
        v->referenceString(), makeFormatter(v->getBody()),
        makeFormatter(v->getFinally()),
        fmt::join(catches.begin(), catches.end(), "\n"));
  }
  void visit(const PipelineFlow *v) override {
    std::vector<std::string> stages;
    for (const auto &s : *v) {
      auto args = makeFormatters(s.begin(), s.end());
      stages.push_back(fmt::format(
          FMT_STRING("(stage (func {})\n(args ({}))\n(generator {})\n(parallel{}))"),
          makeFormatter(s.getFunc()), fmt::join(args.begin(), args.end(), "\n"),
          s.isGenerator(), s.isParallel()));
    }
    fmt::print(os, FMT_STRING("(pipeline (stages ({})))"),
               fmt::join(stages.begin(), stages.end(), "\n"));
  }
  void visit(const dsl::CustomFlow *v) override { os << *v; }

  void visit(const IntConstant *v) override {
    fmt::print(os, FMT_STRING("(int_constant (ref_string \"{}\")\n(value {}))"),
               v->referenceString(), v->getVal());
  }
  void visit(const FloatConstant *v) override {
    fmt::print(os, FMT_STRING("(float_constant (ref_string \"{}\")\n(value {}))"),
               v->referenceString(), v->getVal());
  }
  void visit(const BoolConstant *v) override {
    fmt::print(os, FMT_STRING("(bool_constant (ref_string \"{}\")\n(value {}))"),
               v->referenceString(), v->getVal());
  }
  void visit(const StringConstant *v) override {
    auto value = v->getVal();
    std::replace(value.begin(), value.end(), '\n', ' ');

    fmt::print(os, FMT_STRING("(string_constant (ref_string \"{}\")\n(value \"{}\"))"),
               v->referenceString(), value);
  }
  void visit(const dsl::CustomConstant *v) override { os << *v; }

  void visit(const AssignInstr *v) override {
    fmt::print(os, FMT_STRING("(assign_instr (ref_string \"{}\")\n(lhs {})\n(rhs {}))"),
               v->referenceString(), makeFormatter(v->getLhs()),
               makeFormatter(v->getRhs()));
  }

  void visit(const ExtractInstr *v) override {
    fmt::print(
        os,
        FMT_STRING("(extract_instr (ref_string \"{}\")\n(value {})\n(field \"{}\"))"),
        v->referenceString(), makeFormatter(v->getVal()), v->getField());
  }

  void visit(const InsertInstr *v) override {
    fmt::print(
        os,
        FMT_STRING(
            "(insert_instr (ref_string \"{}\")\n(lhs {})\n(field \"{}\")\n(rhs {}))"),
        v->referenceString(), makeFormatter(v->getLhs()), v->getField(),
        makeFormatter(v->getRhs()));
  }

  void visit(const CallInstr *v) override {
    auto args = makeFormatters(v->begin(), v->end());
    fmt::print(os,
               FMT_STRING("(call_instr (ref_string \"{}\")\n(func {})\n(args ({})))"),
               v->referenceString(), makeFormatter(v->getFunc()),
               fmt::join(args.begin(), args.end(), "\n"));
  }

  void visit(const StackAllocInstr *v) override {
    fmt::print(
        os,
        FMT_STRING(
            "(stack_alloc_instr (ref_string \"{}\")\n(array_type {})\n(count {}))"),
        v->referenceString(), makeFormatter(v->getArrayType()), v->getCount());
  }

  void visit(const TypePropertyInstr *v) override {
    fmt::print(
        os,
        FMT_STRING("(type_property_instr (ref_string \"{}\")\n(inspect_type {})\n"
                   "(property \"{}\"))"),
        v->referenceString(), makeFormatter(v->getInspectType()),
        v->getProperty() == TypePropertyInstr::Property::IS_ATOMIC ? "is_atomic"
                                                                   : "sizeof");
  }

  void visit(const YieldInInstr *v) override {
    fmt::print(os, FMT_STRING("(yield_in_instr (ref_string \"{}\")\n(type {}))"),
               v->referenceString(), makeFormatter(v->getType()));
  }

  void visit(const TernaryInstr *v) override {
    fmt::print(os,
               FMT_STRING("(ternary_instr (ref_string \"{}\")\n(cond {})\n(true_value "
                          "{})\n(false_value {}))"),
               v->referenceString(), makeFormatter(v->getCond()),
               makeFormatter(v->getTrueValue()), makeFormatter(v->getFalseValue()));
  }

  void visit(const BreakInstr *v) override {
    fmt::print(os, FMT_STRING("(break_instr (ref_string \"{}\"))"),
               v->referenceString());
  }

  void visit(const ContinueInstr *v) override {
    fmt::print(os, FMT_STRING("(continue_instr (ref_string \"{}\"))"),
               v->referenceString());
  }

  void visit(const ReturnInstr *v) override {
    fmt::print(os, FMT_STRING("(return_instr (ref_string \"{}\")\n(value {}))"),
               v->referenceString(), makeFormatter(v->getValue()));
  }

  void visit(const YieldInstr *v) override {
    fmt::print(os, FMT_STRING("(yield_instr (ref_string \"{}\")\n(value {}))"),
               v->referenceString(), makeFormatter(v->getValue()));
  }

  void visit(const ThrowInstr *v) override {
    fmt::print(os, FMT_STRING("(throw_instr (ref_string \"{}\")\n(value {}))"),
               v->referenceString(), makeFormatter(v->getValue()));
  }

  void visit(const FlowInstr *v) override {
    fmt::print(os,
               FMT_STRING("(flow_instr (ref_string \"{}\")\n(flow {})\n(value {}))"),
               v->referenceString(), makeFormatter(v->getFlow()),
               makeFormatter(v->getValue()));
  }

  void visit(const dsl::CustomInstr *v) override { os << *v; }

  void visit(const types::IntType *v) override {
    fmt::print(os, FMT_STRING("(int_type (ref_string \"{}\"))"), v->referenceString());
  }

  void visit(const types::FloatType *v) override {
    fmt::print(os, FMT_STRING("(float_type (ref_string \"{}\"))"),
               v->referenceString());
  }

  void visit(const types::BoolType *v) override {
    fmt::print(os, FMT_STRING("(bool_type (ref_string \"{}\"))"), v->referenceString());
  }

  void visit(const types::ByteType *v) override {
    fmt::print(os, FMT_STRING("(byte_type (ref_string \"{}\"))"), v->referenceString());
  }

  void visit(const types::VoidType *v) override {
    fmt::print(os, FMT_STRING("(void_type (ref_string \"{}\"))"), v->referenceString());
  }

  void visit(const types::RecordType *v) override {
    std::vector<std::string> fields;
    std::vector<NodeFormatter> formatters;
    for (const auto &m : *v) {
      fields.push_back(fmt::format(FMT_STRING("\"{}\""), m.getName()));
      formatters.push_back(makeFormatter(m.getType()));
    }

    fmt::print(os,
               FMT_STRING("(record_type (ref_string \"{}\")\n(field_names "
                          "({}))\n(field_types ({})))"),
               v->referenceString(), fmt::join(fields.begin(), fields.end(), "\n"),
               fmt::join(formatters.begin(), formatters.end(), "\n"));
  }

  void visit(const types::RefType *v) override {
    fmt::print(os, FMT_STRING("(ref_type (ref_string \"{}\")\n(contents {}))"),
               v->referenceString(), makeFormatter(v->getContents()));
  }

  void visit(const types::FuncType *v) override {
    auto args = makeFormatters(v->begin(), v->end());
    fmt::print(
        os,
        FMT_STRING(
            "(func_type (ref_string \"{}\")\n(return_type {})\n(arg_types ({})))"),
        v->referenceString(), makeFormatter(v->getReturnType()),
        fmt::join(args.begin(), args.end(), "\n"));
  }

  void visit(const types::OptionalType *v) override {
    fmt::print(os, FMT_STRING("(optional_type (ref_string \"{}\")\n(base {}))"),
               v->referenceString(), makeFormatter(v->getBase()));
  }

  void visit(const types::PointerType *v) override {
    fmt::print(os, FMT_STRING("(pointer_type (ref_string \"{}\")\n(base {}))"),
               v->referenceString(), makeFormatter(v->getBase()));
  }

  void visit(const types::GeneratorType *v) override {
    fmt::print(os, FMT_STRING("(generator_type (ref_string \"{}\")\n(base {}))"),
               v->referenceString(), makeFormatter(v->getBase()));
  }

  void visit(const types::IntNType *v) override {
    fmt::print(os,
               FMT_STRING("(intn_type (ref_string \"{}\")\n(size {})\n(signed {}))"),
               v->referenceString(), v->getLen(), v->isSigned());
  }

  void visit(const dsl::types::CustomType *v) override { os << *v; }

  void format(const IRNode *n) {
    if (n)
      n->accept(*this);
    else
      os << "()";
  }

  void format(const types::Type *t, bool canShowFull = false) {
    if (t) {
      if (seenTypes.find(t->getName()) != seenTypes.end() || !canShowFull)
        fmt::print(os, FMT_STRING("(type {})"), t->referenceString());
      else {
        seenTypes.insert(t->getName());
        t->accept(*this);
      }
    } else
      os << "()";
  }

  void format(const Value *t) {
    if (t) {
      if (seenNodes.find(t->getId()) != seenNodes.end())
        fmt::print(os, FMT_STRING("(value {})"), t->referenceString());
      else {
        seenNodes.insert(t->getId());
        t->accept(*this);
      }

    } else
      os << "()";
  }

  void format(const Var *t, bool canShowFull = false) {
    if (t) {
      if (seenNodes.find(t->getId()) != seenNodes.end() || !canShowFull)
        fmt::print(os, FMT_STRING("(var {})"), t->referenceString());
      else {
        seenNodes.insert(t->getId());
        t->accept(*this);
      }
    } else
      os << "()";
  }

private:
  NodeFormatter makeFormatter(const types::Type *node, bool canShowFull = false) {
    auto ret = NodeFormatter(node, seenNodes, seenTypes);
    ret.canShowFull = canShowFull;
    return ret;
  }
  NodeFormatter makeFormatter(const Value *node) {
    return NodeFormatter(node, seenNodes, seenTypes);
  }
  NodeFormatter makeFormatter(const Var *node, bool canShowFull = false) {
    auto ret = NodeFormatter(node, seenNodes, seenTypes);
    ret.canShowFull = canShowFull;
    return ret;
  }

  template <typename It> std::vector<NodeFormatter> makeFormatters(It begin, It end) {
    std::vector<NodeFormatter> ret;
    while (begin != end) {
      ret.push_back(makeFormatter(*begin));
      ++begin;
    }
    return ret;
  }
  template <typename It>
  std::vector<NodeFormatter> makeFormatters(It begin, It end, bool canShowFull) {
    std::vector<NodeFormatter> ret;
    while (begin != end) {
      ret.push_back(makeFormatter(*begin, canShowFull));
      ++begin;
    }
    return ret;
  }
};

std::ostream &operator<<(std::ostream &os, const NodeFormatter &n) {
  FormatVisitor fv(os, n.seenNodes, n.seenTypes);
  if (n.type)
    fv.format(n.type, n.canShowFull);
  else if (n.value)
    fv.format(n.value);
  else
    fv.format(n.var, n.canShowFull);
  return os;
}
} // namespace

// See https://github.com/fmtlib/fmt/issues/1283.
namespace fmt {
template <typename Char>
struct formatter<NodeFormatter, Char>
    : fmt::v6::internal::fallback_formatter<NodeFormatter, Char> {};
} // namespace fmt

namespace seq {
namespace ir {
namespace util {

std::string format(const IRNode *node) {
  std::stringstream ss;
  format(ss, node);
  return ss.str();
}

std::ostream &format(std::ostream &os, const IRNode *node) {
  std::unordered_set<int> seenNodes;
  std::unordered_set<std::string> seenTypes;

  FormatVisitor fv(os, seenNodes, seenTypes);
  fv.format(node);

  return os;
}

} // namespace util
} // namespace ir
} // namespace seq
