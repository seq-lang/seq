#include "seq/seq.h"

using namespace seq;
using namespace llvm;

types::AnyType::AnyType() : Type("Any", nullptr, true)
{
}

types::AnyType *types::AnyType::get() noexcept
{
	static types::AnyType instance;
	return &instance;
}
