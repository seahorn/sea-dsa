#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"

#include "sea_dsa/CallSite.hh"

using namespace llvm;

namespace sea_dsa {

bool DsaCallSite::isPointerTy::operator()(const Value *v) {
  return v->getType()->isPointerTy();
}

bool DsaCallSite::isPointerTy::operator()(const Argument &a) {
  return a.getType()->isPointerTy();
}

static const Function*
getCalledFunctionThroughAliasesAndCasts(ImmutableCallSite &CS) {
  const Value* CalledV = CS.getCalledValue();
  CalledV = CalledV->stripPointerCasts(); 
  
  if (const Function* F = dyn_cast<const Function>(CalledV)) {
    return F;
  }

  if (const GlobalAlias *GA = dyn_cast<const GlobalAlias>(CalledV)) {
    if (const Function* F =
	dyn_cast<const Function>(GA->getAliasee()->stripPointerCasts())) {
      return F;
    }
  }
  
  return nullptr;
}
  
DsaCallSite::DsaCallSite(const ImmutableCallSite &cs)
  : m_cs(cs)
  , m_cell(None)
  , m_cloned(false)
  , m_callee(getCalledFunctionThroughAliasesAndCasts(m_cs)) {}
DsaCallSite::DsaCallSite(const Instruction &cs)
  : m_cs(&cs)
  , m_cell(None)
  , m_cloned(false)
  , m_callee(getCalledFunctionThroughAliasesAndCasts(m_cs)) {}
DsaCallSite::DsaCallSite(const Instruction &cs, Cell c)
  : m_cs(&cs)
  , m_cell(c)
  , m_cloned(false)
  , m_callee(getCalledFunctionThroughAliasesAndCasts(m_cs)) {
  m_cell.getValue().getNode();
}
DsaCallSite::DsaCallSite(const Instruction &cs, const Function &callee)
  : m_cs(&cs)
  , m_cell(None)
  , m_cloned(false)
  , m_callee(&callee) {
  assert(isIndirectCall() ||
	 getCalledFunctionThroughAliasesAndCasts(m_cs) == &callee);
}

bool DsaCallSite::hasCell() const { return m_cell.hasValue(); }

const Cell &DsaCallSite::getCell() const {
  assert(hasCell());
  return m_cell.getValue();
}

Cell &DsaCallSite::getCell() {
  assert(hasCell());
  return m_cell.getValue();
}

const llvm::Value &DsaCallSite::getCalledValue() const {
  return *(m_cs.getCalledValue());
}

bool DsaCallSite::isIndirectCall() const {
  // XXX: this does not compile
  // return m_cs.isIndirectCall();

  const Value *V = m_cs.getCalledValue();
  if (!V)
    return false;
  if (isa<const Function>(V) || isa<const Constant>(V))
    return false;
  if (const CallInst *CI = dyn_cast<const CallInst>(getInstruction())) {
    if (CI->isInlineAsm())
      return false;
  }
  return true;
}

bool DsaCallSite::isInlineAsm() const {
  return m_cs.isInlineAsm();
}
bool DsaCallSite::isCloned() const { return m_cloned; }

void DsaCallSite::markCloned(bool v) { m_cloned = v; }


const Value *DsaCallSite::getRetVal() const {
  if (const Function *F = getCallee()) {
    const FunctionType *FTy = F->getFunctionType();
    if (!(FTy->getReturnType()->isVoidTy()))
      return getInstruction();
  }
  return nullptr;
}

const Function *DsaCallSite::getCallee() const {
  return m_callee;
}

const Function *DsaCallSite::getCaller() const { return m_cs.getCaller(); }

const Instruction *DsaCallSite::getInstruction() const {
  return m_cs.getInstruction();
}

DsaCallSite::const_formal_iterator DsaCallSite::formal_begin() const {
  isPointerTy p;
  assert(getCallee());
  return boost::make_filter_iterator(p, getCallee()->arg_begin(),
                                     getCallee()->arg_end());
}

DsaCallSite::const_formal_iterator DsaCallSite::formal_end() const {
  isPointerTy p;
  assert(getCallee());
  return boost::make_filter_iterator(p, getCallee()->arg_end(),
                                     getCallee()->arg_end());
}

DsaCallSite::const_actual_iterator DsaCallSite::actual_begin() const {
  isPointerTy p;
  return boost::make_filter_iterator(p, m_cs.arg_begin(), m_cs.arg_end());
}

DsaCallSite::const_actual_iterator DsaCallSite::actual_end() const {
  isPointerTy p;
  return boost::make_filter_iterator(p, m_cs.arg_end(), m_cs.arg_end());
}

void DsaCallSite::write(raw_ostream &o) const {
  o << *m_cs.getInstruction();
  if (isIndirectCall() && hasCell()) {
    o << "\nCallee cell " << m_cell.getValue();
  }
}

} // namespace sea_dsa
