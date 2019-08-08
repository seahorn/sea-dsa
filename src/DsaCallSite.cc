#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"

#include "sea_dsa/AllocSite.hh"
#include "sea_dsa/CallSite.hh"
#include "sea_dsa/Graph.hh"

using namespace llvm;

namespace sea_dsa {

bool DsaCallSite::isPointerTy::operator()(const Value *v) {
  return v->getType()->isPointerTy();
}

bool DsaCallSite::isPointerTy::operator()(const Argument &a) {
  return a.getType()->isPointerTy();
}

DsaCallSite::DsaCallSite(const ImmutableCallSite &cs)
    : m_cs(cs), m_cell(nullptr), m_resolved(getCallee()) {}
DsaCallSite::DsaCallSite(const Instruction &cs)
    : m_cs(&cs), m_cell(nullptr), m_resolved(getCallee()) {}
DsaCallSite::DsaCallSite(const Value &cs)
    : m_cs(&cs), m_cell(nullptr), m_resolved(getCallee()) {}
DsaCallSite::DsaCallSite(const Value &cs, Cell &c)
    : m_cs(&cs), m_cell(&c), m_resolved(getCallee()) {}

bool DsaCallSite::hasCell() const { return m_cell != nullptr; }

const Cell &DsaCallSite::getCell() const {
  assert(hasCell());
  return *m_cell;
}

Cell &DsaCallSite::getCell() {
  assert(hasCell());
  return *m_cell;
}

const llvm::Value &DsaCallSite::getCalledValue() const {
  return *(m_cs.getCalledValue());
}

bool DsaCallSite::isIndirectCall() const {
  if (const CallInst *CI = dyn_cast<const CallInst>(m_cs.getInstruction())) {
    if (CI->isInlineAsm()) {
      return false;
    }
  }
  return (!m_cs.getCalledFunction());
}

bool DsaCallSite::isResolved() const { return m_resolved; }

void DsaCallSite::markResolved(bool v) { m_resolved = v; }

const Value *DsaCallSite::getRetVal() const {
  if (const Function *F = getCallee()) {
    const FunctionType *FTy = F->getFunctionType();
    if (!(FTy->getReturnType()->isVoidTy()))
      return getInstruction();
  }
  return nullptr;
}

const Function *DsaCallSite::getCallee() const {
  return m_cs.getCalledFunction();
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
  // D: direct, I: indirect, U: unknown (e.g., asm)

  if (getCallee()) {
    // o << "D ";
    o << *m_cs.getInstruction();
  } else if (isIndirectCall()) {
    // o << "I ";
    o << *m_cs.getInstruction();
    if (isResolved()) {
      o << " RESOLVED";
    } else {
      o << " UNRESOLVED\n";
    }
    o << "\n";
    if (m_cell)
      o << "Callee cell " << *m_cell << "\n";
  } else {
    // o << "U ";
    o << *m_cs.getInstruction() << "\n";
  }
}

} // namespace sea_dsa
