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

#include "lang/seq.h"
#include "parser/cache.h"
#include "parser/ocaml/ocaml.h"
#include "parser/parser.h"
#include "parser/visitors/codegen/codegen.h"
#include "parser/visitors/doc/doc.h"
#include "parser/visitors/format/format.h"
#include "parser/visitors/simplify/simplify.h"
#include "parser/visitors/typecheck/typecheck.h"
#include "sir/sir.h"
#include "util/fmt/format.h"

int _ocaml_time = 0;
int _ll_time = 0;
int _level = 0;
int _dbg_level = 0;
bool _isTest = false;

namespace seq {

std::unique_ptr<ir::IRModule> parse(const string &argv0, const string &file,
                                    const string &code, bool isCode, int isTest,
                                    int startLine) {
  try {
    auto d = getenv("SEQ_DEBUG");
    if (d) {
      auto s = string(d);
      _dbg_level |= s.find('t') != string::npos ? (1 << 0) : 0; // time
      _dbg_level |= s.find('r') != string::npos ? (1 << 2) : 0; // realize
      _dbg_level |= s.find('T') != string::npos ? (1 << 4) : 0; // type-check
      _dbg_level |= s.find('l') != string::npos ? (1 << 5) : 0; // lexer
    }

    char abs[PATH_MAX + 1];
    realpath(file.c_str(), abs);

    ast::StmtPtr codeStmt =
        isCode ? ast::parseCode(abs, code, startLine) : ast::parseFile(abs);

    using namespace std::chrono;

    auto cache = make_shared<ast::Cache>(argv0);
    if (isTest)
      cache->testFlags = isTest;

    auto t = high_resolution_clock::now();
    auto transformed = ast::SimplifyVisitor::apply(cache, move(codeStmt), abs,
                                                   (isTest > 1) /* || true */);
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
    auto typechecked = ast::TypecheckVisitor::apply(cache, move(transformed));
    if (!isTest) {
      LOG_TIME("[T] typecheck = {:.1f}",
               duration_cast<milliseconds>(high_resolution_clock::now() - t).count() /
                   1000.0);
      if (_dbg_level) {
        auto fo = fopen("_dump_typecheck.seq", "w");
        fmt::print(fo, "{}", ast::FormatVisitor::apply(typechecked, cache));
        fclose(fo);
      }
    }

    t = high_resolution_clock::now();
    auto module = ast::CodegenVisitor::apply(cache, move(typechecked));
    module->setSrcInfo({abs, 0, 0, 0, 0});
    if (!isTest)
      LOG_TIME("[T] codegen   = {:.1f}",
               duration_cast<milliseconds>(high_resolution_clock::now() - t).count() /
                   1000.0);
    _isTest = isTest;
    return module;
  } catch (seq::exc::SeqException &e) {
    if (isTest) {
      LOG("ERROR: {}", e.what());
    } else {
      seq::compilationError(e.what(), e.getSrcInfo().file, e.getSrcInfo().line,
                            e.getSrcInfo().col);
    }
    return std::unique_ptr<ir::IRModule>();
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
    return std::unique_ptr<ir::IRModule>();
  }
}

/*
void execute(seq::SeqModule *module, const vector<string> &args,
             const vector<string> &libs, bool debug) {
  config::config().debug = debug;
  try {
    module->execute(args, libs, !_isTest);
  } catch (exc::SeqException &e) {
    compilationError(e.what(), e.getSrcInfo().file, e.getSrcInfo().line,
                     e.getSrcInfo().col);
  }
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
*/

void generateDocstr(const string &argv0) {
  vector<string> files;
  string s;
  while (std::getline(std::cin, s))
    files.push_back(s);
  auto j = ast::DocVisitor::apply(argv0, files);
  fmt::print("{}\n", j.dump());
}

} // namespace seq
