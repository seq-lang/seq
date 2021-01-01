/*
 * parser.h --- Seq AST parser.
 *
 * (c) Seq project. All rights reserved.
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#pragma once

#include <string>
#include <vector>

#include "lang/seq.h"
#include "sir/sir.h"

namespace seq {

std::unique_ptr<ir::IRModule> parse(const std::string &argv0, const std::string &file,
                                    const std::string &code = "", bool isCode = false,
                                    int isTest = 0, int startLine = 0);
/*
void execute(seq::SeqModule *module, const std::vector<std::string> &args = {},
             const std::vector<std::string> &libs = {}, bool debug = false);
void compile(seq::SeqModule *module, const std::string &out, bool debug = false);
*/
void generateDocstr(const std::string &argv0);

} // namespace seq
