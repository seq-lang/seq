/*
 * parser.cpp --- Seq AST parser.
 *
 * (c) Seq project. All rights reserved.
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "parser/cache.h"
#include "parser/ocaml/ocaml.h"
#include "parser/parser.h"
#include "parser/visitors/doc/doc.h"
#include "parser/visitors/format/format.h"
#include "parser/visitors/simplify/simplify.h"
#include "parser/visitors/translate/translate.h"
#include "parser/visitors/typecheck/typecheck.h"
#include "sir/sir.h"
#include "util/fmt/format.h"

#include "sir/util/format.h"
#include <fstream>

int _ocaml_time = 0;
int _ll_time = 0;
int _level = 0;
int _dbg_level = 0;
bool _isTest = false;

namespace seq {

ir::Module *parse(const string &argv0, const string &file, const string &code,
                  bool isCode, int isTest, int startLine,
                  const std::unordered_map<std::string, std::string> &defines) {
  try {
    auto d = getenv("SEQ_DEBUG");
    if (d) {
      auto s = string(d);
      _dbg_level |= s.find('t') != string::npos ? (1 << 0) : 0; // time
      _dbg_level |= s.find('r') != string::npos ? (1 << 2) : 0; // realize
      _dbg_level |= s.find('T') != string::npos ? (1 << 4) : 0; // type-check
      _dbg_level |= s.find('l') != string::npos ? (1 << 5) : 0; // lexer
    }

    char abs[PATH_MAX + 1] = {'-', 0};
    if (file != "-")
      realpath(file.c_str(), abs);

    ast::StmtPtr codeStmt =
        isCode ? ast::parseCode(abs, code, startLine) : ast::parseFile(abs);

    using namespace std::chrono;

    auto cache = make_shared<ast::Cache>(argv0);
    cache->module0 = file;
    if (isTest)
      cache->testFlags = isTest;

    auto t = high_resolution_clock::now();

    unordered_map<string, pair<string, int64_t>> newDefines;
    for (auto &d : defines)
      newDefines[d.first] = {d.second, 0};
    auto transformed = ast::SimplifyVisitor::apply(cache, move(codeStmt), abs,
                                                   newDefines, (isTest > 1));
    if (!isTest) {
      LOG_TIME("[T] ocaml = {:.1f}", _ocaml_time / 1000.0);
      LOG_TIME("[T] simplify = {:.1f}",
               (duration_cast<milliseconds>(high_resolution_clock::now() - t).count() -
                _ocaml_time) /
                   1000.0);
      if (_dbg_level) {
        auto fo = fopen("_dump_simplify.seq", "w");
        fmt::print(fo, "{}", ast::FormatVisitor::apply(transformed, cache));
        fclose(fo);
        fo = fopen("_dump_simplify.sexp", "w");
        fmt::print(fo, "{}\n", transformed->toString());
        fclose(fo);
      }
    }

    t = high_resolution_clock::now();
    auto typechecked =
        ast::TypecheckVisitor::apply(cache, move(transformed), newDefines);
    if (!isTest) {
      LOG_TIME("[T] typecheck = {:.1f}",
               duration_cast<milliseconds>(high_resolution_clock::now() - t).count() /
                   1000.0);
      if (_dbg_level) {
        auto fo = fopen("_dump_typecheck.seq", "w");
        fmt::print(fo, "{}", ast::FormatVisitor::apply(typechecked, cache));
        fclose(fo);
        fo = fopen("_dump_typecheck.sexp_", "w");
        fmt::print(fo, "{}\n", typechecked->toString());
        fclose(fo);
      }
    }

    t = high_resolution_clock::now();
    auto *module = ast::TranslateVisitor::apply(cache, move(typechecked));
    module->setSrcInfo({abs, 0, 0, 0, 0});

    if (!isTest)
      LOG_TIME("[T] translate   = {:.1f}",
               duration_cast<milliseconds>(high_resolution_clock::now() - t).count() /
                   1000.0);
    if (_dbg_level) {
      auto out = seq::ir::util::format(module);
      std::ofstream os("_dump_sir.lisp");
      os << out;
      os.close();
    }

    _isTest = isTest;
    return module;
  } catch (exc::SeqException &e) {
    if (isTest) {
      LOG("ERROR: {}", e.what());
    } else {
      compilationError(e.what(), e.getSrcInfo().file, e.getSrcInfo().line,
                       e.getSrcInfo().col);
    }
    return nullptr;
  } catch (exc::ParserException &e) {
    for (int i = 0; i < e.messages.size(); i++) {
      if (isTest) {
        LOG("ERROR: {}", e.messages[i]);
      } else {
        compilationError(e.messages[i], e.locations[i].file, e.locations[i].line,
                         e.locations[i].col, /*terminate=*/false);
      }
    }
    return nullptr;
  }
}

void generateDocstr(const string &argv0) {
  vector<string> files;
  string s;
  while (std::getline(std::cin, s))
    files.push_back(s);
  auto j = ast::DocVisitor::apply(argv0, files);
  fmt::print("{}\n", j.dump());
}

} // namespace seq
