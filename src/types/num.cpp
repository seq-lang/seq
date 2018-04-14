#include <iostream>
#include <fstream>
#include "seq/seq.h"
#include "seq/base.h"
#include "seq/num.h"

using namespace seq;
using namespace llvm;

SEQ_FUNC void printInt(seq_int_t x)
{
	std::cout << x << std::endl;
}

static void serializeIntDirect(seq_int_t x, std::ostream& out)
{
	out.write(reinterpret_cast<const char *>(&x), sizeof(x));
}

static seq_int_t deserializeIntDirect(std::istream& in)
{
	char buf[sizeof(seq_int_t)];
	in.read(buf, sizeof(seq_int_t));
	return *reinterpret_cast<seq_int_t *>(buf);
}

SEQ_FUNC void serializeInt(seq_int_t x, char *filename)
{
	std::ofstream out(filename);
	serializeIntDirect(x, out);
}

SEQ_FUNC seq_int_t deserializeInt(char *filename)
{
	std::ifstream in(filename);
	auto x = deserializeIntDirect(in);
	in.close();
	return x;
}

SEQ_FUNC void serializeIntArray(seq_int_t *x, seq_int_t len, char *filename)
{
	std::ofstream out(filename);

	serializeIntDirect(len, out);
	for (seq_int_t i = 0; i < len; i++)
		serializeIntDirect(x[i], out);

	out.close();
}

SEQ_FUNC seq_int_t *deserializeIntArray(char *filename, seq_int_t *len)
{
	std::ifstream in(filename);
	*len = deserializeIntDirect(in);
	auto *array = (seq_int_t *)std::malloc(*len * sizeof(seq_int_t));

	for (seq_int_t i = 0; i < *len; i++) {
		array[i] = deserializeIntDirect(in);
	}

	in.close();
	return array;
}

SEQ_FUNC void printFloat(double n)
{
	std::cout << n << std::endl;
}

void serializeFloatDirect(double x, std::ostream& out)
{
	out.write(reinterpret_cast<const char *>(&x), sizeof(x));
}

double deserializeFloatDirect(std::istream& in)
{
	char buf[sizeof(double)];
	in.read(buf, sizeof(double));
	return *reinterpret_cast<double *>(buf);
}

SEQ_FUNC void serializeFloat(double x, char *filename)
{
	std::ofstream out(filename);
	serializeFloatDirect(x, out);
	out.close();
}

SEQ_FUNC double deserializeFloat(char *filename)
{
	std::ifstream in(filename);
	auto x = deserializeFloatDirect(in);
	in.close();
	return x;
}

SEQ_FUNC void serializeFloatArray(double *x, seq_int_t len, char *filename)
{
	std::ofstream out(filename);

	serializeIntDirect(len, out);
	for (seq_int_t i = 0; i < len; i++)
		serializeFloatDirect(x[i], out);

	out.close();
}

SEQ_FUNC double *deserializeFloatArray(char *filename, seq_int_t *len)
{
	std::ifstream in(filename);
	*len = deserializeIntDirect(in);
	auto *array = (double *)std::malloc(*len * sizeof(double));

	for (seq_int_t i = 0; i < *len; i++) {
		array[i] = deserializeFloatDirect(in);
	}

	in.close();
	return array;
}

SEQ_FUNC void printBool(bool b)
{
	std::cout << (b ? "true" : "false") << std::endl;
}

void serializeBoolDirect(bool x, std::ostream& out)
{
	out.write(reinterpret_cast<const char *>(&x), sizeof(x));
}

bool deserializeBoolDirect(std::istream& in)
{
	char buf[sizeof(bool)];
	in.read(buf, sizeof(bool));
	return *reinterpret_cast<bool *>(buf);
}

SEQ_FUNC void serializeBool(bool x, char *filename)
{
	std::ofstream out(filename);
	serializeBoolDirect(x, out);
	out.close();
}

SEQ_FUNC bool deserializeBool(char *filename)
{
	std::ifstream in(filename);
	auto x = deserializeBoolDirect(in);
	in.close();
	return x;
}

SEQ_FUNC void serializeBoolArray(bool *x, seq_int_t len, char *filename)
{
	std::ofstream out(filename);

	serializeIntDirect(len, out);
	for (seq_int_t i = 0; i < len; i++)
		serializeBoolDirect(x[i], out);

	out.close();
}

SEQ_FUNC bool *deserializeBoolArray(char *filename, seq_int_t *len)
{
	std::ifstream in(filename);
	*len = deserializeIntDirect(in);
	auto *array = (bool *)std::malloc(*len * sizeof(double));

	for (seq_int_t i = 0; i < *len; i++) {
		array[i] = deserializeBoolDirect(in);
	}

	in.close();
	return array;
}

types::NumberType::NumberType() : Type("Num", BaseType::get())
{
}

types::IntType::IntType() : Type("Int", NumberType::get(), SeqData::INT)
{
	vtable.print = (void *)printInt;
	vtable.serialize = (void *)serializeInt;
	vtable.deserialize = (void *)deserializeInt;
	vtable.serializeArray = (void *)serializeIntArray;
	vtable.deserializeArray = (void *)deserializeIntArray;
}

types::FloatType::FloatType() : Type("Float", NumberType::get(), SeqData::FLOAT)
{
	vtable.print = (void *)printFloat;
	vtable.serialize = (void *)serializeFloat;
	vtable.deserialize = (void *)deserializeFloat;
	vtable.serializeArray = (void *)serializeFloatArray;
	vtable.deserializeArray = (void *)deserializeFloatArray;
}

types::BoolType::BoolType() : Type("Bool", NumberType::get(), SeqData::BOOL)
{
	vtable.print = (void *)printBool;
	vtable.serialize = (void *)serializeBool;
	vtable.deserialize = (void *)deserializeBool;
	vtable.serializeArray = (void *)serializeBoolArray;
	vtable.deserializeArray = (void *)deserializeBoolArray;
}

Value *types::IntType::checkEq(BaseFunc *base,
                               ValMap ins1,
                               ValMap ins2,
                               BasicBlock *block)
{
	IRBuilder<> builder(block);
	Value *n1 = builder.CreateLoad(getSafe(ins1, SeqData::INT));
	Value *n2 = builder.CreateLoad(getSafe(ins2, SeqData::INT));

	return builder.CreateICmpEQ(n1, n2);
}

Value *types::FloatType::checkEq(BaseFunc *base,
                                 ValMap ins1,
                                 ValMap ins2,
                                 BasicBlock *block)
{
	IRBuilder<> builder(block);
	Value *f1 = builder.CreateLoad(getSafe(ins1, SeqData::FLOAT));
	Value *f2 = builder.CreateLoad(getSafe(ins2, SeqData::FLOAT));

	return builder.CreateFCmpOEQ(f1, f2);
}

Value *types::BoolType::checkEq(BaseFunc *base,
                                ValMap ins1,
                                ValMap ins2,
                                BasicBlock *block)
{
	IRBuilder<> builder(block);
	Value *b1 = builder.CreateLoad(getSafe(ins1, SeqData::BOOL));
	Value *b2 = builder.CreateLoad(getSafe(ins2, SeqData::BOOL));

	return builder.CreateICmpEQ(b1, b2);
}

Type *types::IntType::getLLVMType(LLVMContext& context) const
{
	return seqIntLLVM(context);
}

Type *types::FloatType::getLLVMType(LLVMContext& context) const
{
	return llvm::Type::getDoubleTy(context);
}

Type *types::BoolType::getLLVMType(LLVMContext& context) const
{
	return IntegerType::getInt8Ty(context);
}

seq_int_t types::IntType::size(Module *module) const
{
	return sizeof(seq_int_t);
}

seq_int_t types::FloatType::size(Module *module) const
{
	return sizeof(double);
}

seq_int_t types::BoolType::size(Module *module) const
{
	return sizeof(bool);
}

types::NumberType *types::NumberType::get()
{
	static NumberType instance;
	return &instance;
}

types::IntType *types::IntType::get()
{
	static IntType instance;
	return &instance;
}

types::FloatType *types::FloatType::get()
{
	static FloatType instance;
	return &instance;
}

types::BoolType *types::BoolType::get()
{
	static BoolType instance;
	return &instance;
}
