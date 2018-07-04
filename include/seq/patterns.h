#ifndef SEQ_PATTERNS_H
#define SEQ_PATTERNS_H

#include <vector>
#include "types.h"
#include "func.h"
#include "var.h"

namespace seq {

	class Pattern {
	private:
		types::Type *type;
	public:
		explicit Pattern(types::Type *type);
		virtual void validate(types::Type *type);
		virtual llvm::Value *codegen(BaseFunc *base,
		                             types::Type *type,
		                             llvm::Value *val,
		                             llvm::BasicBlock*& block)=0;
		virtual Pattern *clone();
	};

	class Wildcard : public Pattern {
	private:
		Var *var;
		llvm::Value *result;
	public:
		Wildcard();
		void validate(types::Type *type) override;
		llvm::Value *codegen(BaseFunc *base,
		                     types::Type *type,
		                     llvm::Value *val,
		                     llvm::BasicBlock*& block) override;
		Wildcard *clone() override;
		Var *getVar();
	};

	class IntPattern : public Pattern {
	private:
		seq_int_t val;
	public:
		explicit IntPattern(seq_int_t val);
		llvm::Value *codegen(BaseFunc *base,
		                     types::Type *type,
		                     llvm::Value *val,
		                     llvm::BasicBlock*& block) override;
	};

	class BoolPattern : public Pattern {
	private:
		bool val;
	public:
		explicit BoolPattern(bool val);
		llvm::Value *codegen(BaseFunc *base,
		                     types::Type *type,
		                     llvm::Value *val,
		                     llvm::BasicBlock*& block) override;
	};

	class RecordPattern : public Pattern {
		std::vector<Pattern *> patterns;
	public:
		explicit RecordPattern(std::vector<Pattern *> patterns);
		void validate(types::Type *type) override;
		llvm::Value *codegen(BaseFunc *base,
		                     types::Type *type,
		                     llvm::Value *val,
		                     llvm::BasicBlock*& block) override;
		RecordPattern *clone() override;
	};

	class RangePattern : public Pattern {
	private:
		seq_int_t a;
		seq_int_t b;
	public:
		RangePattern(seq_int_t a, seq_int_t b);
		llvm::Value *codegen(BaseFunc *base,
		                     types::Type *type,
		                     llvm::Value *val,
		                     llvm::BasicBlock*& block) override;
	};

	class OrPattern : public Pattern {
		std::vector<Pattern *> patterns;
	public:
		explicit OrPattern(std::vector<Pattern *> patterns);
		void validate(types::Type *type) override;
		llvm::Value *codegen(BaseFunc *base,
		                     types::Type *type,
		                     llvm::Value *val,
		                     llvm::BasicBlock*& block) override;
		OrPattern *clone() override;
	};

}

#endif /* SEQ_PATTERNS_H */
