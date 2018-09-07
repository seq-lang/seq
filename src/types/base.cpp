#include "seq/seq.h"

using namespace seq;
using namespace llvm;

types::BaseType::BaseType() : Type("Base", AnyType::get())
{
}

types::BaseType *types::BaseType::get() noexcept
{
	static types::BaseType instance;
	return &instance;
}
