#include <iostream>
#include <fstream>
#include "base.h"
#include "num.h"

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

SEQ_FUNC void serializeIntArray(void *x, seq_int_t len, char *filename)
{
	std::ofstream out(filename);
	auto *y = (seq_int_t *)x;

	serializeIntDirect(len, out);
	for (seq_int_t i = 0; i < len; i++)
		serializeIntDirect(y[i], out);

	out.close();
}

SEQ_FUNC void *deserializeIntArray(char *filename, seq_int_t *len)
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

SEQ_FUNC void serializeFloatArray(void *x, seq_int_t len, char *filename)
{
	std::ofstream out(filename);
	auto *y = (double *)x;

	serializeIntDirect(len, out);
	for (seq_int_t i = 0; i < len; i++)
		serializeFloatDirect(y[i], out);

	out.close();
}

SEQ_FUNC void *deserializeFloatArray(char *filename, seq_int_t *len)
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

Type *types::IntType::getLLVMType(LLVMContext& context)
{
	return seqIntLLVM(context);
}

Type *types::FloatType::getLLVMType(LLVMContext& context)
{
	return llvm::Type::getDoubleTy(context);
}

seq_int_t types::IntType::size() const
{
	return sizeof(seq_int_t);
}

seq_int_t types::FloatType::size() const
{
	return sizeof(double);
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
