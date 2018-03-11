#ifndef SEQ_STAGES_H
#define SEQ_STAGES_H

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <map>

#include "llvm.h"
#include "types.h"
#include "seqdata.h"

namespace seq {
	class Seq;
	class Pipeline;

	typedef void (*SeqOp)(char *, uint32_t);
	typedef bool (*SeqPred)(char *, uint32_t);
	typedef uint32_t (*SeqHash)(char *, uint32_t);

	class Stage {
	private:
		Seq *base;
		bool added;
	protected:
		types::Type *in;
		types::Type *out;
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
		virtual void setPrev(Stage *prev);
		std::vector<Stage *>& getNext();
		Seq *getBase() const;
		void setBase(Seq *base);
		types::Type *getInType() const;
		types::Type *getOutType() const;
		virtual void addNext(Stage *next);
		virtual llvm::BasicBlock *getAfter() const;
		virtual void setAfter(llvm::BasicBlock *block);
		bool isAdded() const;
		void setAdded();

		virtual void validate();
		virtual void ensurePrev();
		virtual void codegen(llvm::Module *module, llvm::LLVMContext& context);
		virtual void codegenNext(llvm::Module *module, llvm::LLVMContext& context);
		virtual void finalize(llvm::ExecutionEngine *eng);

		virtual Pipeline operator|(Pipeline to);
		operator Pipeline();
	};
}

std::ostream& operator<<(std::ostream& os, seq::Stage& stage);

#endif /* SEQ_STAGES_H */
