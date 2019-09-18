#include "seq/seq.h"

using namespace seq;
using namespace llvm;

types::GenericType::GenericType(types::RefType *pending,
                                std::vector<types::Type *> types, Expr *expr)
    : Type("generic", types::BaseType::get()), genericName(), type(nullptr),
      pending(pending), types(std::move(types)), extensions(), expr(expr) {
  assert(!(pending && expr));
}

types::GenericType::GenericType() : GenericType(nullptr, {}, nullptr) {}

void types::GenericType::setName(std::string name) {
  genericName = std::move(name);
}

void types::GenericType::realize(types::Type *type) {
  assert(!this->type && !pending && !expr);
  this->type = type;
}

void types::GenericType::realize() const {
  assert(!(pending && expr));
  if (!type) {
    if (pending) {
      type = pending->realize(types);
    } else if (expr) {
      expr->resolveTypes();
      type = expr->getType();
    }

    if (type) {
      for (auto &e : extensions)
        type->addMethod(e.name, e.func, e.force);
    }
  }
}

bool types::GenericType::realized() const { return type != nullptr; }

void types::GenericType::ensure() const {
  realize();
  if (!realized())
    throw exc::SeqException("generic type '" + genericName +
                            "' not yet realized");
}

types::Type *types::GenericType::getType() const {
  ensure();
  auto *genType = dynamic_cast<types::GenericType *>(type);
  return genType ? genType->getType() : type;
}

int types::GenericType::getID() const {
  ensure();
  return type->getID();
}

std::string types::GenericType::getName() const {
  realize();
  if (!type)
    return genericName.empty() ? "<Generic>" : genericName;
  return type->getName();
}

types::Type *types::GenericType::getParent() const {
  ensure();
  return type->getParent();
}

bool types::GenericType::isAbstract() const {
  ensure();
  return type->isAbstract();
}

types::VTable &types::GenericType::getVTable() {
  ensure();
  return type->getVTable();
}

Value *types::GenericType::alloc(Value *count, BasicBlock *block) {
  ensure();
  return type->alloc(count, block);
}

Value *types::GenericType::call(BaseFunc *base, Value *self,
                                const std::vector<Value *> &args,
                                BasicBlock *block, BasicBlock *normal,
                                BasicBlock *unwind) {
  ensure();
  return type->call(base, self, args, block, normal, unwind);
}

Value *types::GenericType::memb(Value *self, const std::string &name,
                                BasicBlock *block) {
  ensure();
  return type->memb(self, name, block);
}

types::Type *types::GenericType::membType(const std::string &name) {
  ensure();
  return type->membType(name);
}

Value *types::GenericType::setMemb(Value *self, const std::string &name,
                                   Value *val, BasicBlock *block) {
  ensure();
  return type->setMemb(self, name, val, block);
}

bool types::GenericType::hasMethod(const std::string &name) {
  ensure();
  return type->hasMethod(name);
}

void types::GenericType::addMethod(std::string name, BaseFunc *func,
                                   bool force) {
  if (type)
    type->addMethod(name, func, force);
  else
    extensions.push_back({name, func, force});
}

BaseFunc *types::GenericType::getMethod(const std::string &name) {
  ensure();
  return type->getMethod(name);
}

Value *types::GenericType::staticMemb(const std::string &name,
                                      BasicBlock *block) {
  ensure();
  return type->staticMemb(name, block);
}

types::Type *types::GenericType::staticMembType(const std::string &name) {
  ensure();
  return type->staticMembType(name);
}

Value *types::GenericType::defaultValue(BasicBlock *block) {
  ensure();
  return type->defaultValue(block);
}

Value *types::GenericType::boolValue(Value *self, BasicBlock *&block,
                                     TryCatch *tc) {
  ensure();
  return type->boolValue(self, block, tc);
}

Value *types::GenericType::strValue(Value *self, BasicBlock *&block,
                                    TryCatch *tc) {
  ensure();
  return type->strValue(self, block, tc);
}

Value *types::GenericType::lenValue(Value *self, BasicBlock *&block,
                                    TryCatch *tc) {
  ensure();
  return type->lenValue(self, block, tc);
}

void types::GenericType::initOps() {
  ensure();
  type->initOps();
}

void types::GenericType::initFields() {
  ensure();
  type->initFields();
}

types::Type *types::GenericType::magicOut(const std::string &name,
                                          std::vector<types::Type *> args) {
  ensure();
  return type->magicOut(name, args);
}

Value *types::GenericType::callMagic(const std::string &name,
                                     std::vector<types::Type *> argTypes,
                                     Value *self, std::vector<Value *> args,
                                     BasicBlock *&block, TryCatch *tc) {
  ensure();
  return type->callMagic(name, argTypes, self, args, block, tc);
}


std::vector<std::pair<std::string, BaseFunc *> > types::GenericType::methods()
{
  ensure();
  return type->methods();
}

bool types::GenericType::isAtomic() const {
  ensure();
  return type->isAtomic();
}

bool types::GenericType::is(types::Type *type) const {
  realize();
  if (!realized())
    return this == type;

  // reduce argument to realized type
  types::GenericType *g1 = nullptr;
  while ((g1 = dynamic_cast<types::GenericType *>(type)) && g1->realized())
    type = g1->type;

  // reduce ourselves to realized type
  types::GenericType *g2 = nullptr;
  types::Type *self = this->type;
  while ((g2 = dynamic_cast<types::GenericType *>(self)) && g2->realized())
    self = g2->type;

  return self->is(type);
}

bool types::GenericType::isGeneric(types::Type *type) const {
  ensure();
  return this->type->isGeneric(type);
}

unsigned types::GenericType::numBaseTypes() const {
  realize();
  return realized() ? type->numBaseTypes() : 0;
}

types::Type *types::GenericType::getBaseType(unsigned idx) const {
  ensure();
  return type->getBaseType(idx);
}

types::Type *
types::GenericType::getCallType(const std::vector<Type *> &inTypes) {
  ensure();
  return type->getCallType(inTypes);
}

Type *types::GenericType::getLLVMType(LLVMContext &context) const {
  ensure();
  return type->getLLVMType(context);
}

size_t types::GenericType::size(Module *module) const {
  ensure();
  return type->size(module);
}

types::RecordType *types::GenericType::asRec() {
  realize();
  return type ? type->asRec() : nullptr;
}

types::RefType *types::GenericType::asRef() {
  realize();
  return type ? type->asRef() : nullptr;
}

types::GenType *types::GenericType::asGen() {
  realize();
  return type ? type->asGen() : nullptr;
}

types::OptionalType *types::GenericType::asOpt() {
  realize();
  return type ? type->asOpt() : nullptr;
}

types::KMer *types::GenericType::asKMer() {
  realize();
  return type ? type->asKMer() : nullptr;
}

types::GenericType *types::GenericType::get() { return new GenericType(); }

types::GenericType *types::GenericType::get(types::RefType *pending,
                                            std::vector<types::Type *> types) {
  return new GenericType(pending, std::move(types), nullptr);
}

types::GenericType *types::GenericType::get(Expr *expr) {
  return new GenericType(nullptr, {}, expr);
}

types::GenericType *types::GenericType::clone(Generic *ref) {
  if (ref->seenClone(this))
    return (types::GenericType *)ref->getClone(this);

  auto *x = types::GenericType::get();
  ref->addClone(this, x);
  x->setName(genericName);
  x->extensions = extensions; // no need to clone these

  if (type) {
    x->realize(type->clone(ref));
  } else if (pending) {
    std::vector<types::Type *> typesCloned;
    for (auto *type : types)
      typesCloned.push_back(type->clone(ref));

    x->pending = pending;
    x->types = typesCloned;
  } else if (expr) {
    x->expr = expr->clone(ref);
  }

  return x;
}

static bool findInTypeHelper(types::GenericType *gen, types::Type *type,
                             std::vector<unsigned> &path,
                             std::vector<types::Type *> &seen) {
  if (type->is(gen))
    return true;

  for (auto *saw : seen) {
    if (saw == type)
      return false;
  }

  const unsigned numBases = type->numBaseTypes();
  for (unsigned i = 0; i < numBases; i++) {
    path.push_back(i);
    seen.push_back(type);
    if (findInTypeHelper(gen, type->getBaseType(i), path, seen))
      return true;
    path.pop_back();
    seen.pop_back();
  }

  return false;
}

bool types::GenericType::findInType(types::Type *type,
                                    std::vector<unsigned> &path) {
  std::vector<types::Type *> seen;
  return findInTypeHelper(this, type, path, seen);
}

Generic::Generic() : generics(), cloneCache() {}

bool Generic::realized() {
  for (auto *generic : generics) {
    if (!generic->realized())
      return false;
  }
  return true;
}

std::vector<types::Type *> Generic::getRealizedTypes() const {
  std::vector<types::Type *> types;
  for (auto *generic : generics)
    types.push_back(generic->getType());
  return types;
}

bool Generic::is(Generic *other) const {
  return typeMatch<types::GenericType>(generics, other->generics);
}

void Generic::addCachedRealized(std::vector<types::Type *> types, Generic *x) {}

void Generic::setCloneBase(Generic *x, Generic *ref) {
  std::vector<types::GenericType *> genericsCloned;
  for (auto *generic : generics)
    genericsCloned.push_back(generic->clone(ref));

  x->generics = genericsCloned;
}

void Generic::addGenerics(int count) {
  assert(generics.empty());

  for (int i = 0; i < count; i++)
    generics.push_back(types::GenericType::get());

  std::vector<types::Type *> types;
  for (auto *generic : generics)
    types.push_back(generic);

  addCachedRealized(types, this);
}

unsigned Generic::numGenerics() const { return (unsigned)generics.size(); }

types::GenericType *Generic::getGeneric(int idx) const {
  assert((unsigned)idx < generics.size());
  return generics[idx];
}

bool Generic::seenClone(void *p) {
  return cloneCache.find(p) != cloneCache.end();
}

void *Generic::getClone(void *p) {
  auto iter = cloneCache.find(p);
  assert(iter != cloneCache.end());
  return iter->second;
}

void Generic::addClone(void *p, void *clone) {
  assert(!seenClone(p));
  cloneCache.insert({p, clone});
}

Generic *Generic::realizeGeneric(std::vector<types::Type *> types) {
  if (types.size() != generics.size())
    throw exc::SeqException("expected " + std::to_string(generics.size()) +
                            " type parameters, but got " +
                            std::to_string(types.size()));

  auto old = cloneCache;
  cloneCache.clear();
  Generic *x = clone(this);

  for (unsigned i = 0; i < types.size(); i++)
    x->generics[i]->realize(types[i]);

  cloneCache = old;
  return x;
}

std::vector<types::Type *>
Generic::deduceTypesFromArgTypes(const std::vector<types::Type *> &inTypes,
                                 const std::vector<types::Type *> &argTypes) {
  assert(numGenerics() > 0 && !realized());

  if (argTypes.size() != inTypes.size())
    throw exc::SeqException("expected " + std::to_string(inTypes.size()) +
                            " arguments, " + "but got " +
                            std::to_string(argTypes.size()));

  std::vector<types::Type *> types(numGenerics(), nullptr);
  std::vector<unsigned> path;

  for (unsigned i = 0; i < types.size(); i++) {
    for (unsigned j = 0; j < argTypes.size(); j++) {
      if (!argTypes[j])
        continue;

      path.clear();
      types::Type *inType = inTypes[j];

      if (getGeneric(i)->findInType(inType, path)) {
        types::Type *argType = argTypes[j];

        /*
         * OK, we found the generic type nested in `inType`;
         * now extract the corresponding type from `argType`.
         */
        bool match = true;
        for (unsigned k : path) {
          if (argType->numBaseTypes() <= k) {
            match = false;
            break;
          }

          inType = inType->getBaseType(k);
          argType = argType->getBaseType(k);
        }

        if (match) {
          types[i] = argType;
          break;
        }
      }
    }
  }

  for (auto *type : types) {
    if (!type)
      throw exc::SeqException(
          "cannot deduce all type parameters for generic '" + genericName() +
          "'");
  }

  return types;
}
