#ifndef SEQ_RECORD_H
#define SEQ_RECORD_H

#include "types.h"
#include <functional>
#include <initializer_list>
#include <vector>

namespace seq {
namespace types {
class RecordType : public Type {
protected:
  std::vector<Type *> types;
  std::vector<std::string> names;
  explicit RecordType(std::vector<Type *> types,
                      std::vector<std::string> names = {},
                      std::string name = "");

public:
  RecordType(RecordType const &) = delete;
  void operator=(RecordType const &) = delete;

  void setContents(std::vector<Type *> types, std::vector<std::string> names);
  bool empty() const;
  std::vector<Type *> getTypes();

  std::string getName() const override;
  llvm::Value *defaultValue(llvm::BasicBlock *block) override;

  void initOps() override;
  void initFields() override;
  bool isAtomic() const override;
  bool is(Type *type) const override;
  unsigned numBaseTypes() const override;
  Type *getBaseType(unsigned idx) const override;
  llvm::Type *getLLVMType(llvm::LLVMContext &context) const override;
  void addLLVMTypesToStruct(llvm::StructType *structType);
  size_t size(llvm::Module *module) const override;

  RecordType *asRec() override;
  RecordType *clone(Generic *ref) override;
  static RecordType *get(std::vector<Type *> types,
                         std::vector<std::string> names = {},
                         std::string name = "");
};

} // namespace types
} // namespace seq

#endif /* SEQ_RECORD_H */
