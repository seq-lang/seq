#ifndef SEQ_SERIALIZE_H
#define SEQ_SERIALIZE_H

#include <string>
#include "stage.h"
#include "exc.h"

namespace seq {

	class Serialize : public Stage {
	private:
		std::string filename;
	public:
		explicit Serialize(std::string filename);
		void codegen(llvm::Module *module) override;
		void finalize(llvm::ExecutionEngine *eng) override;
		static Serialize& make(std::string filename);
	};

	class Deserialize : public Stage {
	private:
		types::Type *type;
		std::string filename;
	public:
		Deserialize(types::Type *type, std::string filename);
		void codegen(llvm::Module *module) override;
		void finalize(llvm::ExecutionEngine *eng) override;
		static Deserialize& make(types::Type *type, std::string filename);
	};

}

#endif /* SEQ_SERIALIZE_H */
