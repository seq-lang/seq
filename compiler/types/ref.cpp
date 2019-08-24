#include "seq/seq.h"
#include <cassert>

using namespace seq;
using namespace llvm;

types::RefType::RefType(std::string name)
    : Type(std::move(name), BaseType::get(), false, true), Generic(),
      done(false), root(this), llvmCache(), realizationCache(),
      contents(nullptr), membNamesDeduced(), membExprsDeduced() {}

void types::RefType::setDone() {
  assert(this == root && !done);

  if (!contents) {
    for (auto &magic : vtable.overloads)
      magic.func->resolveTypes();

    for (auto &e : vtable.methods)
      e.second->resolveTypes();

    try {
      setDeducedContents();
    } catch (exc::SeqException &) {
      // this can fail if e.g. types are not known due to generics
      // in this case, hope that types get resolved later...
    }
  }

  done = true;
}

void types::RefType::setContents(types::RecordType *contents) {
  this->contents = contents;
}

void types::RefType::setDeducedContents() {
  assert(membExprsDeduced.size() == membNamesDeduced.size());
  std::vector<types::Type *> membTypesDeduced;
  for (unsigned i = 0; i < membExprsDeduced.size(); i++) {
    Expr *expr = membExprsDeduced[i];
    expr->resolveTypes();
    membTypesDeduced.push_back(expr->getType());
    if (membTypesDeduced.back()->is(Void)) {
      contents = types::RecordType::get({});
      throw exc::SeqException("member '" + membNamesDeduced[i] +
                              "' of class '" + getName() + "' is void");
    }
  }
  contents = types::RecordType::get(membTypesDeduced, membNamesDeduced);
}

void types::RefType::addMember(std::string name, Expr *expr) {
  if (done || contents ||
      std::find(membNamesDeduced.begin(), membNamesDeduced.end(), name) !=
          membNamesDeduced.end())
    return;

  membNamesDeduced.push_back(name);
  membExprsDeduced.push_back(expr);
}

std::string types::RefType::getName() const {
  if (numGenerics() == 0)
    return name;

  std::string name = this->name + "[";

  for (unsigned i = 0; i < numGenerics(); i++) {
    name += getGeneric(i)->getName();
    if (i < numGenerics() - 1)
      name += ",";
  }

  name += "]";
  return name;
}

std::string types::RefType::genericName() { return getName(); }

types::Type *types::RefType::realize(std::vector<types::Type *> types) {
  if (this != root)
    return root->realize(types);

  types::Type *cached = root->realizationCache.find(types);

  if (cached)
    return cached;

  if (!done) {
    types::GenericType *pending = types::GenericType::get(this, types);
    return pending;
  }

  Generic *x = realizeGeneric(types);
  auto *ref = dynamic_cast<types::RefType *>(x);
  assert(ref);

  if (!ref->contents || !ref->membNamesDeduced.empty())
    ref->setDeducedContents();

  addCachedRealized(types, ref);
  return ref;
}

std::vector<types::Type *>
types::RefType::deduceTypesFromArgTypes(std::vector<types::Type *> argTypes) {
  // deal with custom __init__s:
  bool foundInit = false;
  for (auto &magic : vtable.overloads) {
    if (magic.name != "__init__")
      continue;

    foundInit = true;
    magic.func->resolveTypes();
    types::FuncType *funcType = magic.func->getFuncType();

    if (funcType->numBaseTypes() - 2 != argTypes.size())
      continue;

    std::vector<types::Type *> types;
    // start loop from 2 since 0th base type is return type and 1st is self
    for (unsigned i = 2; i < funcType->numBaseTypes(); i++)
      types.push_back(funcType->getBaseType(i));

    try {
      return Generic::deduceTypesFromArgTypes(types, argTypes);
    } catch (exc::SeqException &) {
    }
  }

  if (foundInit)
    throw exc::SeqException("could not deduce type parameters of class '" +
                            name + "'");

  return Generic::deduceTypesFromArgTypes(contents->getTypes(), argTypes);
}

Value *types::RefType::memb(Value *self, const std::string &name,
                            BasicBlock *block) {
  initFields();
  initOps();

  // Defer to contained tuple, unless this is a method reference.
  if (!contents || contents->hasMethod(name) || Type::hasMethod(name))
    return Type::memb(self, name, block);

  LLVMContext &context = block->getContext();
  IRBuilder<> builder(block);
  self = builder.CreateBitCast(self, getStructPointerType(context));
  Value *x = builder.CreateLoad(self);

  try {
    return contents->memb(x, name, block);
  } catch (exc::SeqException &) {
    throw exc::SeqException("type '" + getName() + "' has no member '" + name +
                            "'");
  }
}

types::Type *types::RefType::membType(const std::string &name) {
  initFields();
  initOps();

  try {
    return Type::membType(name);
  } catch (exc::SeqException &) {
  }

  // Defer to contained tuple, unless this is a method reference.
  if (contents && !contents->hasMethod(name)) {
    try {
      return contents->membType(name);
    } catch (exc::SeqException &) {
    }
  }

  throw exc::SeqException("type '" + getName() + "' has no member '" + name +
                          "'");
}

Value *types::RefType::setMemb(Value *self, const std::string &name, Value *val,
                               BasicBlock *block) {
  initFields();
  LLVMContext &context = block->getContext();
  IRBuilder<> builder(block);
  self = builder.CreateBitCast(self, getStructPointerType(context));
  Value *x = builder.CreateLoad(self);
  x = contents->setMemb(x, name, val, block);
  builder.CreateStore(x, self);
  return self;
}

Value *types::RefType::defaultValue(BasicBlock *block) {
  return ConstantPointerNull::get(
      cast<PointerType>(getLLVMType(block->getContext())));
}

void types::RefType::initOps() {
  if (!vtable.magic.empty())
    return;

  vtable.magic = {
      {"__new__",
       {},
       this,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         assert(contents);
         self = contents->alloc(nullptr, b.GetInsertBlock());
         self = b.CreateBitCast(self, getLLVMType(b.getContext()));
         return self;
       },
       false},

      {"__bool__",
       {},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateZExt(b.CreateIsNotNull(self),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__raw__",
       {},
       PtrType::get(Byte),
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return self;
       },
       false},
  };

  if (contents) {
    vtable.magic.push_back(
        {"__init__", contents->getTypes(), this,
         [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
           self = b.CreateBitCast(self, getStructPointerType(b.getContext()));
           for (unsigned i = 0; i < args.size(); i++)
             self = setMemb(self, std::to_string(i + 1), args[i],
                            b.GetInsertBlock());
           self = b.CreateBitCast(self, getLLVMType(b.getContext()));
           return self;
         }});
  }
}

void types::RefType::initFields() {
  if (contents)
    contents->initFields();
}

bool types::RefType::isAtomic() const { return false; }

bool types::RefType::is(types::Type *type) const {
  types::RefType *ref = type->asRef();
  return ref && (ref == none() || this == none() ||
                 (name == ref->name && Generic::is(ref)));
}

unsigned types::RefType::numBaseTypes() const {
  if (numGenerics() > 0)
    return numGenerics();
  return contents ? contents->numBaseTypes() : 0;
}

types::Type *types::RefType::getBaseType(unsigned idx) const {
  if (numGenerics() > 0)
    return getGeneric(idx);
  return contents ? contents->getBaseType(idx) : nullptr;
}

Type *types::RefType::getStructPointerType(LLVMContext &context) const {
  std::vector<types::Type *> types = getRealizedTypes();
  StructType *structType = root->llvmCache.find(types);

  if (structType)
    return PointerType::get(structType, 0);

  assert(contents);
  structType = StructType::create(context, name);
  root->llvmCache.add(types, structType);
  contents->addLLVMTypesToStruct(structType);
  return PointerType::get(structType, 0);
}

Type *types::RefType::getLLVMType(LLVMContext &context) const {
  return IntegerType::getInt8PtrTy(context);
}

size_t types::RefType::size(Module *module) const { return sizeof(void *); }

types::RefType *types::RefType::asRef() { return this; }

Value *types::RefType::make(BasicBlock *block, std::vector<Value *> vals) {
  assert(contents);
  LLVMContext &context = block->getContext();
  Value *val = contents->defaultValue(block);
  Value *ref = contents->alloc(nullptr, block);
  IRBuilder<> builder(block);
  llvm::Type *type = getStructPointerType(context);
  ref = builder.CreateBitCast(ref, type);
  val = builder.CreateBitCast(val, cast<PointerType>(type)->getElementType());
  builder.CreateStore(val, ref);

  for (unsigned i = 0; i < vals.size(); i++)
    ref = setMemb(ref, std::to_string(i + 1), vals[i], block);

  ref = builder.CreateBitCast(ref, getLLVMType(context));
  return ref;
}

types::RefType *types::RefType::get(std::string name) {
  return new RefType(std::move(name));
}

types::RefType *types::RefType::clone(Generic *ref) {
  assert(done && contents);

  if (ref->seenClone(this))
    return (types::RefType *)ref->getClone(this);

  types::RefType *x = types::RefType::get(name);
  ref->addClone(this, x);
  setCloneBase(x, ref);

  x->contents = contents->clone(ref);

  std::vector<MagicOverload> overloadsCloned;
  for (auto &magic : getVTable().overloads)
    overloadsCloned.push_back({magic.name, magic.func->clone(ref)});

  std::map<std::string, BaseFunc *> methodsCloned;
  for (auto &method : getVTable().methods)
    methodsCloned.insert({method.first, method.second->clone(ref)});

  x->getVTable().overloads = overloadsCloned;
  x->getVTable().methods = methodsCloned;

  std::vector<Expr *> membExprsDeducedCloned;
  for (auto *expr : membExprsDeduced)
    membExprsDeducedCloned.push_back(expr->clone(ref));

  x->membNamesDeduced = membNamesDeduced;
  x->membExprsDeduced = membExprsDeducedCloned;

  x->root = root;
  x->done = true;
  return x;
}

void types::RefType::addCachedRealized(std::vector<types::Type *> types,
                                       Generic *x) {
  root->realizationCache.add(types, dynamic_cast<types::Type *>(x));
}

types::RefType *types::RefType::none() {
  static RefType *noneRef = RefType::get("<None>");
  return noneRef;
}

types::MethodType::MethodType(types::Type *self, FuncType *func)
    : RecordType({self, func}, {"self", "func"}), self(self), func(func) {}

Value *types::MethodType::call(BaseFunc *base, Value *self,
                               const std::vector<Value *> &args,
                               BasicBlock *block, BasicBlock *normal,
                               BasicBlock *unwind) {
  Value *x = memb(self, "self", block);
  Value *f = memb(self, "func", block);
  std::vector<Value *> argsFull(args);
  argsFull.insert(argsFull.begin(), x);
  return func->call(base, f, argsFull, block, normal, unwind);
}

bool types::MethodType::is(types::Type *type) const {
  return isGeneric(type) && types::is(getBaseType(0), type->getBaseType(0)) &&
         types::is(getBaseType(1), type->getBaseType(1));
}

unsigned types::MethodType::numBaseTypes() const { return 2; }

types::Type *types::MethodType::getBaseType(unsigned idx) const {
  return idx ? self : func;
}

types::Type *
types::MethodType::getCallType(const std::vector<Type *> &inTypes) {
  std::vector<Type *> inTypesFull(inTypes);
  inTypesFull.insert(inTypesFull.begin(), self);
  return func->getCallType(inTypesFull);
}

Value *types::MethodType::make(Value *self, Value *func, BasicBlock *block) {
  LLVMContext &context = self->getContext();
  Value *method = UndefValue::get(getLLVMType(context));
  method = setMemb(method, "self", self, block);
  method = setMemb(method, "func", func, block);
  return method;
}

types::MethodType *types::MethodType::get(types::Type *self,
                                          types::FuncType *func) {
  return new MethodType(self, func);
}

types::MethodType *types::MethodType::clone(Generic *ref) {
  return MethodType::get(self->clone(ref), func->clone(ref));
}
