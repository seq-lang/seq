#pragma once

#include <string>
#include <vector>

#include "seq/seq.h"

SEQ_FUNC seq::SeqModule *parse(const char *argv0, const char *file,
                               bool isCode = false, bool isTest = false);

namespace seq {
void execute(seq::SeqModule *module, std::vector<std::string> args = {},
             std::vector<std::string> libs = {}, bool debug = false);
void compile(seq::SeqModule *module, const std::string &out,
             bool debug = false);
} // namespace seq
