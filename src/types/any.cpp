#include "seq/any.h"

using namespace seq;
using namespace llvm;

types::AnyType::AnyType() : Type("Any", nullptr)
{
}

Type *types::AnyType::getLLVMType(LLVMContext& context)
{
	throw exc::SeqException("cannot instantiate Any class");
}

types::AnyType *types::AnyType::get()
{
	static types::AnyType instance;
	return &instance;
}
