#pragma once

#include "codon/sir/dsl/nodes.h"
#include "codon/sir/sir.h"
#include "codon/sir/transform/pass.h"

namespace seq {

class KmerRevcomp
    : public codon::ir::AcceptorExtend<KmerRevcomp, codon::ir::dsl::CustomInstr> {
private:
  codon::ir::Value *kmer;

public:
  static const char NodeId;

  using AcceptorExtend::AcceptorExtend;

  explicit KmerRevcomp(codon::ir::Value *kmer) : AcceptorExtend(), kmer(kmer) {}

  std::unique_ptr<codon::ir::dsl::codegen::ValueBuilder> getBuilder() const override;
  std::unique_ptr<codon::ir::dsl::codegen::CFBuilder> getCFBuilder() const override;

  bool match(const codon::ir::Value *v) const override;
  codon::ir::Value *doClone(codon::ir::util::CloneVisitor &cv) const override;
  std::ostream &doFormat(std::ostream &os) const override;
};

class KmerRevcompInterceptor : public codon::ir::transform::Pass {
  static const std::string KEY;
  std::string getKey() const override { return KEY; }
  void run(codon::ir::Module *) override;
};

} // namespace seq
