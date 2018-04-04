#include "seq/any.h"
#include "seq/base.h"

using namespace seq;
using namespace llvm;

types::BaseType::BaseType() : Type("Base", AnyType::get())
{
}

types::BaseType *types::BaseType::get()
{
	static types::BaseType instance;
	return &instance;
}
