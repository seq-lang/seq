#ifndef SEQ_VAR_H
#define SEQ_VAR_H

#include <cstdlib>
#include <map>
#include "types.h"
#include "stage.h"
#include "pipeline.h"

namespace seq {
	class BaseFunc;
	class LoadStore;

	class Var {
	protected:
		Stage *stage;
		bool standalone;
		virtual void assign(Pipeline to);
	public:
		explicit Var(bool standalone=false);
		Var(Pipeline pipeline, bool standalone=false);

		virtual types::Type *getType(Stage *caller) const;
		virtual llvm::Value*& result(Stage *caller) const;
		virtual Stage *getStage() const;
		virtual bool isAssigned() const;
		virtual BaseFunc *getBase() const;

		virtual Pipeline operator|(Pipeline to);
		virtual Var& operator=(Pipeline to);

		virtual LoadStore& operator[](Var& idx);
		virtual LoadStore& operator[](seq_int_t idx);

		void ensureConsistentBase(BaseFunc *base);

		virtual Var *clone(types::RefType *ref);
	};

	class Latest : public Var {
	private:
		Latest();
	public:
		Latest(Latest const&)=delete;
		void operator=(Latest const&)=delete;

		types::Type *getType(Stage *caller) const override;
		llvm::Value*& result(Stage *caller) const override;
		Stage *getStage() const override;
		bool isAssigned() const override;
		BaseFunc *getBase() const override;

		static Latest& get();

		Latest *clone(types::RefType *ref) override;
	};

	static Latest& _ = Latest::get();
}

#endif /* SEQ_VAR_H */
