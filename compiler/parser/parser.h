#pragma once

#include <string>
#include <vector>

#include "lang/seq.h"

namespace seq {

SeqModule *parse(const std::string &argv0, const std::string &file, bool isCode = false,
                 bool isTest = false);
void execute(seq::SeqModule *module, std::vector<std::string> args = {},
             std::vector<std::string> libs = {}, bool debug = false);
void compile(seq::SeqModule *module, const std::string &out, bool debug = false);
void generateDocstr(const std::string &argv0);

} // namespace seq
