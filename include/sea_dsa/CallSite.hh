#ifndef __DSA_CALLSITE_HH_
#define __DSA_CALLSITE_HH_

#include "llvm/IR/CallSite.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include "boost/iterator/filter_iterator.hpp"
#include "llvm/ADT/Optional.h"

namespace llvm {
class Value;
class Function;
class Instruction;
} // namespace llvm

namespace sea_dsa {
class Cell;

class DsaCallSite {

  struct isPointerTy {
    bool operator()(const llvm::Value *v);
    bool operator()(const llvm::Argument &a);
  };

  const llvm::ImmutableCallSite m_cs;
  sea_dsa::Cell *m_cell;

public:
  typedef boost::filter_iterator<isPointerTy,
                                 typename llvm::Function::const_arg_iterator>
      const_formal_iterator;
  typedef boost::filter_iterator<isPointerTy,
                                 typename llvm::ImmutableCallSite::arg_iterator>
      const_actual_iterator;

  DsaCallSite(const llvm::ImmutableCallSite &cs);
  DsaCallSite(const llvm::Instruction &cs);
  DsaCallSite(const llvm::Value &cs);

  DsaCallSite(const llvm::Value &cs, sea_dsa::Cell &c);

  bool operator==(const DsaCallSite &o) const {
    return getInstruction() == o.getInstruction();
  }

  const llvm::Value *getRetVal() const;

  const llvm::Function *getCallee() const;
  const llvm::Function *getCaller() const;

  const llvm::Instruction *getInstruction() const;

  const llvm::ImmutableCallSite &getCallSite() const { return m_cs; }

  const_formal_iterator formal_begin() const;
  const_formal_iterator formal_end() const;

  const_actual_iterator actual_begin() const;
  const_actual_iterator actual_end() const;

  bool hasCell() const;
  sea_dsa::Cell &getCell() const;

  bool isResolved();

  bool isIndirectCall(){
    const llvm::Value *V = m_cs.getCalledValue();
    if (!V)
      return false;
    if (llvm::isa<const llvm::Function>(V) || llvm::isa<llvm::Constant>(V))
      return false;
    if (const llvm::CallInst *CI = llvm::dyn_cast<llvm::CallInst>(m_cs.getInstruction())) {
      if (CI->isInlineAsm())
          return false;
    }
    return true;
  }


};
} // namespace sea_dsa
#endif
