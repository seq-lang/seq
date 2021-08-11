#include "schedule.h"

#include <cctype>
#include <sstream>

namespace seq {
namespace ir {
namespace transform {
namespace parallel {
namespace {
bool isValidSchedule(const std::string &schedule) {
  return schedule == "static" || schedule == "dynamic" || schedule == "guided" ||
         schedule == "runtime" || schedule == "auto";
}

bool isScheduleChunkable(const std::string &schedule) {
  return schedule == "static" || schedule == "dynamic" || schedule == "guided";
}

bool isScheduleDynamic(const std::string &schedule) { return schedule != "static"; }

int getScheduleCode(const std::string &schedule = "static", bool chunked = false,
                    bool monotonic = false) {
  // codes from "enum sched_type" at
  // https://github.com/llvm/llvm-project/blob/main/openmp/runtime/src/kmp.h
  int modifier = monotonic ? (1 << 29) : (1 << 30);
  if (schedule == "static") {
    if (chunked)
      return 33;
    else
      return 34;
  } else if (schedule == "dynamic") {
    return 35 | modifier;
  } else if (schedule == "guided") {
    return 36 | modifier;
  } else if (schedule == "runtime") {
    return 37 | modifier;
  } else if (schedule == "auto") {
    return 38 | modifier;
  }
  return getScheduleCode(); // default
}

struct SimpleParser {
  std::string str;
  int where;

  SimpleParser(const std::string &str) : str(str), where(0) {}

  bool more() { return where < str.size(); }

  char curr() { return str[where]; }

  void adv() { ++where; }

  void skip() {
    while (more() && std::isspace(curr()))
      adv();
  }

  std::string nextNum() {
    skip();
    std::stringstream result;
    while (more() && std::isdigit(curr())) {
      result << curr();
      adv();
    }
    return result.str();
  }

  std::string nextIdent() {
    skip();
    std::stringstream result;

    if (more() && std::isalpha(curr())) {
      result << curr();
      adv();
    } else {
      return "";
    }

    while (more() && (curr() == '_' || std::isalnum(curr()))) {
      result << curr();
      adv();
    }

    return result.str();
  }

  bool nextChar(char c) {
    skip();
    if (curr() == c) {
      adv();
      return true;
    } else {
      return false;
    }
  }
};

struct ParseItem {
  std::string name;
  std::vector<std::string> args;
};

bool isIdentifier(const std::string &arg) { return !std::isdigit(arg[0]); }

void warn(const std::string &str, SrcInfo &src) {
  compilationWarning(str, src.file, src.line, src.col);
}

void invalidPragmaString(const std::string &str, SrcInfo &src) {
  warn("ignoring malformed OpenMP pragma: " + str, src);
}

std::vector<ParseItem> parsePragmaString(const std::string &str, SrcInfo &src) {
  std::vector<ParseItem> result;
  SimpleParser parser(str);
  parser.skip();
  while (parser.more()) {
    auto name = parser.nextIdent();
    if (name.empty() || !parser.nextChar('(')) {
      invalidPragmaString(str, src);
      return {};
    }

    std::vector<std::string> args;
    while (parser.more()) {
      auto arg = parser.nextIdent();
      if (arg.empty()) {
        arg = parser.nextNum();
        if (arg.empty()) {
          invalidPragmaString(str, src);
          return {};
        }
      }

      args.push_back(arg);
      if (!parser.nextChar(','))
        break;
    }

    if (!parser.nextChar(')')) {
      invalidPragmaString(str, src);
      return {};
    }

    result.push_back({name, args});
    parser.skip();
  }
  return result;
}

Var *findVar(const std::string name, const std::vector<Var *> &vars, SrcInfo &src) {
  for (auto *var : vars) {
    auto varName = var->getName();
    if (varName.substr(0, varName.find('.')) == name)
      return var;
  }
  warn("could not find variable in OpenMP pragma: " + name, src);
  return nullptr;
}

OMPSched::Param makeParam(const std::string &arg, int ifNotFound,
                          const std::vector<Var *> &vars, SrcInfo &src) {
  if (isIdentifier(arg)) {
    if (auto *var = findVar(arg, vars, src)) {
      if (var->getType()->is(var->getModule()->getIntType()))
        return {var};
      warn("expected int type for variable: " + arg, src);
    }
    return {ifNotFound};
  } else {
    return {std::stoi(arg)};
  }
}
} // namespace

OMPSched::OMPSched() : threads(0), dynamic(false), chunk(1), code(getScheduleCode()) {}

OMPSched getScedule(ImperativeForFlow *v, const std::vector<Var *> &vars) {
  auto src = v->getSrcInfo();
  auto str = v->getSchedule();
  if (str.empty())
    return {};

  auto items = parsePragmaString(str, src);
  if (items.empty())
    return {};

  OMPSched result;
  for (auto &item : items) {
    if (item.name == "schedule") {
      if (item.args.size() != 1 && item.args.size() != 2) {
        warn("OpenMP 'schedule' parameter requires 1 or 2 arguments", src);
        continue;
      }

      auto kind = item.args[0];
      if (!isValidSchedule(kind)) {
        warn("not a valid schedule: " + kind, src);
        continue;
      }

      const bool dynamic = isScheduleDynamic(kind);
      const bool chunked = (item.args.size() > 1);
      const int code = getScheduleCode(kind, chunked);
      OMPSched::Param chunk(1);

      if (chunked) {
        if (!isScheduleChunkable(kind)) {
          warn("schedule does not support chunk size: " + kind, src);
        } else {
          chunk = makeParam(item.args[1], 1, vars, src);
        }
      }

      result.dynamic = dynamic;
      result.chunk = chunk;
      result.code = code;
    } else if (item.name == "num_threads") {
      if (item.args.size() != 1) {
        warn("OpenMP 'num_threads' parameter requires 1 argument", src);
        continue;
      }
      result.threads = makeParam(item.args[0], 0, vars, src);
    } else {
      warn("ignoring unknown or unsupported OpenMP pragma specifier: " + item.name,
           src);
    }
  }
  return result;
}

} // namespace parallel
} // namespace transform
} // namespace ir
} // namespace seq
