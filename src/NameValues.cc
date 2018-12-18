#include "sea_dsa/support/NameValues.hh"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/tokenizer.hpp>

using namespace llvm;

namespace sea_dsa {

char NameValues::ID = 0;

bool NameValues::runOnModule(Module &M) {
  bool change = false;
  for (Module::iterator FI = M.begin(), E = M.end(); FI != E; ++FI)
    change |= runOnFunction(*FI);
  return change;
}

bool NameValues::runOnFunction(Function &F) {
  bool change = false;

  // -- print to string
  std::string funcAsm;
  raw_string_ostream out(funcAsm);
  out << F;
  out.flush();

  typedef boost::tokenizer<boost::char_separator<char>> tokenizer;
  boost::char_separator<char> nl_sep("\n");
  boost::char_separator<char> sp_sep(" :\t%@");

  tokenizer lines(funcAsm, nl_sep);
  tokenizer::iterator line_iter = lines.begin();

  // -- skip function attributes
  if (boost::starts_with(*line_iter, "; Function Attrs:"))
    ++line_iter;

  unsigned ArgIdx = 0;
  for (Argument &arg : F.args()) {
    ++ArgIdx;
    if (!arg.hasName())
      arg.setName("_arg" + std::to_string(ArgIdx));
  }

  // -- skip function definition line
  ++line_iter;

  for (Function::iterator BI = F.begin(), BE = F.end();
       BI != BE && line_iter != lines.end(); ++BI) {
    BasicBlock &BB = *BI;

    if (!BB.hasName()) {
      std::string bb_line = *line_iter;
      tokenizer names(bb_line, sp_sep);
      std::string bb_name = *names.begin();
      if (bb_name == ";")
        bb_name = "bb";
      BB.setName("_" + bb_name);
      change = true;
    }
    ++line_iter;

    for (BasicBlock::iterator II = BB.begin(), IE = BB.end();
         II != IE && line_iter != lines.end(); ++II) {
      Instruction &I = *II;
      if (!I.hasName() && !(I.getType()->isVoidTy())) {
        std::string inst_line = *line_iter;
        tokenizer names(inst_line, sp_sep);
        std::string inst_name = *names.begin();
        I.setName("_" + inst_name);
        change = true;
      }
      ++line_iter;
    }
  }
  return change;
}
} // namespace sea_dsa

static llvm::RegisterPass<sea_dsa::NameValues> X("seadsa-name-values",
                                                 "Names all unnamed values");
