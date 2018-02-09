#ifndef SEQ_STAGES_H
#define SEQ_STAGES_H

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <map>

#include "llvm.h"
#include "types.h"

namespace seq {
	class Seq;
	class Pipeline;

	typedef void (*SeqOp)(char *, uint32_t);
	typedef bool (*SeqPred)(char *, uint32_t);

	enum SeqData {
		SEQ,
		LEN,
		QUAL,
		IDENT,
		SEQ_DATA_COUNT
	};

	class Stage {
	private:
		Seq *base;
		bool linked;

		types::Type *in;
		types::Type *out;
	protected:
		Stage *prev;
		std::vector<Stage *> nexts;
	public:
		std::string name;
		llvm::BasicBlock *block;
		llvm::BasicBlock *after;
		std::shared_ptr<std::map<SeqData, llvm::Value *>> outs;

		Stage(std::string name, types::Type *in, types::Type *out);
		explicit Stage(std::string name);

		std::string getName() const;
		Stage *getPrev() const;
		void setPrev(Stage *prev);
		std::vector<Stage *>& getNext();
		void setBase(Seq *base);
		Seq *getBase() const;
		types::Type *getInType() const;
		types::Type *getOutType() const;
		Pipeline& asPipeline();
		virtual void addNext(Stage *next);
		virtual llvm::BasicBlock *getAfter() const;
		virtual void setAfter(llvm::BasicBlock *block);
		bool isLinked() const;
		void setLinked();

		virtual void validate();
		virtual void codegen(llvm::Module *module, llvm::LLVMContext& context);
		virtual void codegenNext(llvm::Module *module, llvm::LLVMContext& context);
		virtual void finalize(llvm::ExecutionEngine *eng);

		Pipeline& operator|(Stage& to);
		Pipeline& operator|(Pipeline& to);
	};
}

std::ostream& operator<<(std::ostream& os, seq::Stage& stage);

#endif /* SEQ_STAGES_H */
