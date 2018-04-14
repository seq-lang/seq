#include "seq/any.h"

using namespace seq;
using namespace llvm;

types::AnyType::AnyType() : Type("Any", nullptr)
{
}

types::AnyType *types::AnyType::get()
{
	static types::AnyType instance;
	return &instance;
}
