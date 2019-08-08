#ifndef __DSA_CALLSITE_HH_
#define __DSA_CALLSITE_HH_

#include "llvm/IR/CallSite.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include "boost/iterator/filter_iterator.hpp"
#include <memory>
#include <vector>

namespace llvm {
class Value;
class Function;
class Instruction;
class raw_ostream;
} // namespace llvm

namespace sea_dsa {

class Cell;

class DsaCallSite {

  struct isPointerTy {
    bool operator()(const llvm::Value *v);
    bool operator()(const llvm::Argument &a);
  };

  // -- The callsite
  const llvm::ImmutableCallSite m_cs;
  // -- The cell to which the callee function points to.  It can be
  //    null if the callsite is direct.
  Cell *m_cell;
  // -- If true then the indirect call is resolved.
  bool m_resolved;

public:
  using const_formal_iterator =
      boost::filter_iterator<isPointerTy,
                             typename llvm::Function::const_arg_iterator>;
  using const_actual_iterator =
      boost::filter_iterator<isPointerTy,
                             typename llvm::ImmutableCallSite::arg_iterator>;

  DsaCallSite(const llvm::ImmutableCallSite &cs);
  DsaCallSite(const llvm::Instruction &cs);
  DsaCallSite(const llvm::Value &cs);
  DsaCallSite(const llvm::Value &cs, Cell &c);

  bool operator==(const DsaCallSite &o) const {
    return getInstruction() == o.getInstruction();
  }

  bool hasCell() const;
  const Cell &getCell() const;
  Cell &getCell();

  const llvm::Value &getCalledValue() const;

  bool isIndirectCall() const;

  bool isResolved() const;

  void markResolved(bool v = true);

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
