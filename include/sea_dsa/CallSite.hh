#ifndef __DSA_CALLSITE_HH_
#define __DSA_CALLSITE_HH_

#include "llvm/ADT/Optional.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

// XXX: included for Cell. 
#include "sea_dsa/Graph.hh" 

#include "boost/iterator/filter_iterator.hpp"

namespace llvm {
class Value;
class Function;
class Instruction;
} // namespace llvm

namespace sea_dsa {

class DsaCallSite {

  struct isPointerTy {
    bool operator()(const llvm::Value *v);
    bool operator()(const llvm::Argument &a);
  };

  // -- the call site
  llvm::ImmutableCallSite m_cs;
  // -- the cell to which the indirect callee function points to.
  //    it has no value if the callsite is direct.
  llvm::Optional<Cell> m_cell;
  // -- whether the callsite has been cloned: used only internally during
  // -- bottom-up pass.
  bool m_cloned;
  // -- class invariant: m_callee != nullptr
  const llvm::Function* m_callee;
  
public:
  using const_formal_iterator =
      boost::filter_iterator<isPointerTy,
                             typename llvm::Function::const_arg_iterator>;
  using const_actual_iterator =
      boost::filter_iterator<isPointerTy,
                             typename llvm::ImmutableCallSite::arg_iterator>;
  
  DsaCallSite(const llvm::ImmutableCallSite &cs);
  DsaCallSite(const llvm::Instruction &cs);
  DsaCallSite(const llvm::Instruction &cs, Cell c);
  DsaCallSite(const llvm::Instruction &cs, const llvm::Function &callee);
  
  bool operator==(const DsaCallSite &o) const {
    return (getInstruction() == o.getInstruction() &&
	    getCallee() == o.getCallee());
  }

  bool operator<(const DsaCallSite &o) const {
    if (getInstruction() < o.getInstruction()) {
      return true;
    } else if (getInstruction() == o.getInstruction()) {
      return getCallee() < o.getCallee();
    } else {
      return false;
    }
  }
  
  bool hasCell() const;
  const Cell &getCell() const;
  Cell &getCell();

  const llvm::Value &getCalledValue() const;

  bool isIndirectCall() const;
  bool isInlineAsm() const;
  bool isCloned() const;

  void markCloned(bool v = true);
  
  const llvm::Value *getRetVal() const;

  const llvm::Function *getCallee() const;
  const llvm::Function *getCaller() const;

  const llvm::Instruction *getInstruction() const;

  const llvm::ImmutableCallSite &getCallSite() const { return m_cs; }

  const_formal_iterator formal_begin() const;
  const_formal_iterator formal_end() const;
  llvm::iterator_range<const_formal_iterator> formals() const {
    return llvm::make_range(formal_begin(), formal_end());
  }

  const_actual_iterator actual_begin() const;
  const_actual_iterator actual_end() const;
  llvm::iterator_range<const_actual_iterator> actuals() const {
    return llvm::make_range(actual_begin(), actual_end());
  }
  
  void write(llvm::raw_ostream &o) const;  
};
} // namespace sea_dsa
#endif
