#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "lang/seq.h"
#include "parser/ast/cache.h"
#include "parser/ast/codegen/codegen.h"
#include "parser/ast/doc/doc.h"
#include "parser/ast/format/format.h"
#include "parser/ast/transform/transform.h"
#include "parser/ast/typecheck/typecheck.h"
#include "parser/ocaml.h"
#include "parser/parser.h"
#include "util/fmt/format.h"

int __ocaml_time__ = 0;
int __level__ = 0;
int __dbg_level__ = 0;
bool __isTest = false;

namespace seq {

void generateDocstr(const string &argv0) {
  vector<string> files;
  string s;
  while (std::getline(std::cin, s))
    files.push_back(s);
  auto j = ast::DocVisitor::apply(argv0, files);
  fmt::print("{}\n", j.dump());
}

seq::SeqModule *parse(const string &argv0, const string &file, const string &code,
                      bool isCode, bool isTest, int startLine) {
  try {
    auto d = getenv("SEQ_DEBUG");
    if (d) {
      auto s = string(d);
      __dbg_level__ |= s.find('t') != string::npos ? (1 << 0) : 0; // time
      __dbg_level__ |= s.find('r') != string::npos ? (1 << 2) : 0; // realize
      __dbg_level__ |= s.find('T') != string::npos ? (1 << 4) : 0; // typecheck
      __dbg_level__ |= s.find('l') != string::npos ? (1 << 5) : 0; // lexer
    }

    char abs[PATH_MAX + 1];
    realpath(file.c_str(), abs);

    ast::StmtPtr codeStmt =
        isCode ? ast::parseCode(abs, code, startLine) : ast::parseFile(abs);

    using namespace std::chrono;

    auto cache = make_shared<ast::Cache>(argv0);

    auto t = high_resolution_clock::now();
    auto transformed = ast::TransformVisitor::apply(cache, move(codeStmt), abs);
    if (!isTest) {
      LOG_TIME("[T] ocaml = {:.1f}\n", __ocaml_time__ / 1000.0);
      LOG_TIME("[T] transform = {:.1f}\n",
               (duration_cast<milliseconds>(high_resolution_clock::now() - t).count() -
                __ocaml_time__) /
                   1000.0);
      if (__dbg_level__) {
        auto fo = fopen("_dump_transform.seq", "w");
        fmt::print(fo, "{}", ast::FormatVisitor::apply(transformed));
        fclose(fo);
      }
    }

    t = high_resolution_clock::now();
    auto typechecked = ast::TypecheckVisitor::apply(cache, move(transformed));
    if (!isTest) {
      LOG_TIME("[T] typecheck = {:.1f}\n",
               duration_cast<milliseconds>(high_resolution_clock::now() - t).count() /
                   1000.0);
      if (__dbg_level__) {
        auto fo = fopen("_dump_typecheck.seq", "w");
        fmt::print(fo, "{}", ast::FormatVisitor::apply(typechecked, cache));
        fclose(fo);
      }
    }

    t = high_resolution_clock::now();
    auto module = ast::CodegenVisitor::apply(cache, move(typechecked));
    __isTest = isTest;
    return module;
  } catch (seq::exc::SeqException &e) {
    if (isTest) {
      LOG("ERROR: {}", e.what());
    } else {
      seq::compilationError(e.what(), e.getSrcInfo().file, e.getSrcInfo().line,
                            e.getSrcInfo().col);
    }
    exit(EXIT_FAILURE);
    return nullptr;
  } catch (seq::exc::ParserException &e) {
    for (int i = 0; i < e.messages.size(); i++) {
      if (isTest) {
        LOG("ERROR: {}", e.messages[i]);
      } else {
        compilationMessage("\033[1;31merror:\033[0m", e.messages[i],
                           e.locations[i].file, e.locations[i].line,
                           e.locations[i].col);
      }
    }
    exit(EXIT_FAILURE);
    return nullptr;
  }
}

void execute(seq::SeqModule *module, vector<string> args, vector<string> libs,
             bool debug) {
  config::config().debug = debug;
  // try {
  module->execute(args, libs, !__isTest);
  // } catch (exc::SeqException &e) {
  // compilationError(e.what(), e.getSrcInfo().file, e.getSrcInfo().line,
  //  e.getSrcInfo().col);
  // }
}

void compile(seq::SeqModule *module, const string &out, bool debug) {
  config::config().debug = debug;
  try {
    module->compile(out);
  } catch (exc::SeqException &e) {
    compilationError(e.what(), e.getSrcInfo().file, e.getSrcInfo().line,
                     e.getSrcInfo().col);
  }
}

} // namespace seq
