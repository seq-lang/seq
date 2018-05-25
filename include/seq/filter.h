#ifndef SEQ_FILTER_H
#define SEQ_FILTER_H

#include <string>
#include "expr.h"
#include "stage.h"

namespace seq {
	class Filter : public Stage {
	private:
		Expr *key;
	public:
		explicit Filter(Expr *key);
		explicit Filter(Func *key);
		void validate() override;
		void codegen(llvm::Module *module) override;
		static Filter& make(Expr *key);
		static Filter& make(Func& key);
	};
}

#endif /* SEQ_FILTER_H */
