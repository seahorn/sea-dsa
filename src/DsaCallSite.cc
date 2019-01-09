#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"

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

DsaCallSite::DsaCallSite(const ImmutableCallSite &cs) : m_cs(cs), m_cell(nullptr) {}
DsaCallSite::DsaCallSite(const Instruction &cs) : m_cs(&cs), m_cell(nullptr) {}
DsaCallSite::DsaCallSite(const Value &cs) : m_cs(&cs), m_cell(nullptr) {}

DsaCallSite::DsaCallSite(const llvm::Value &cs, Cell &c): m_cs(&cs), m_cell(&c) {}

const Value *DsaCallSite::getRetVal() const {
  if (const Function *F = getCallee()) {
    const FunctionType *FTy = F->getFunctionType();
    if (!(FTy->getReturnType()->isVoidTy()))
      return getInstruction();
  }
  return nullptr;
}

bool DsaCallSite::hasCell() const { return m_cell; }
Cell &DsaCallSite::getCell() const {return *m_cell; }

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

bool DsaCallSite::isResolved(){
  return hasCell() && !getCell().getNode()->isIncomplete();
}

} // namespace sea_dsa
