//===-- IntrinsicCleaner.cpp ----------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "klee/Internal/Module/Passes.h"

#include "klee/Config/Version.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include "llvm/Support/raw_ostream.h"

#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <set>

#include <ctime>
#include <cstdlib>

using namespace llvm;
using namespace klee;

char SelectRandomPass::ID;

SelectRandomPass::SelectRandomPass(unsigned int _target)
    : llvm::ModulePass(ID), target(_target) {}

bool SelectRandomPass::runOnModule(Module &M) {
  srand(time(NULL));
  const llvm::DataLayout &DL = M.getDataLayout();
  for (Module::iterator f = M.begin(), fe = M.end(); f != fe; ++f) {
    for (Function::iterator b = f->begin(), be = f->end(); b != be; ++b) {
      std::string bname = b->getName().str();
      if (bname.substr(0, 4) == "POST")
        continue;
      for (BasicBlock::iterator i = b->begin(), ie = b->end(); i != ie; ++i) {
        std::string iname = i->getName().str();
        if (iname.substr(0, 4) == "POST")
          continue;
        llvm::Type *itype = i->getType();
        if (itype->isVoidTy())
          continue;
        if (itype->isPointerTy())
          continue;
        unsigned int type_width = DL.getTypeSizeInBits(itype);
        if (type_width > 64)
          continue;
        MDNode *MD = i->getMetadata("klee.freq");
        if (MD) {
          if (ConstantAsMetadata *CMD =
                dyn_cast<ConstantAsMetadata>(MD->getOperand(0).get())) {
            if (ConstantInt *CI = dyn_cast<ConstantInt>(CMD->getValue())) {
              unsigned int freq = CI->getZExtValue();
              if (freq * type_width < target && freq > 0) {
                std::string name = f->getName().str()+":"+b->getName().str()+":"+i->getName().str();
                insts.push_back(name);
                inst2freq.insert(std::make_pair(name, freq * type_width/8));
                //llvm::errs() << "%" << name << "\t" << freq *type_width/8 <<
                //'\n';
              }
            }
          }
        }
      }
    }
  }

  llvm::errs() << "#Total instructions in pool: " << insts.size() << '\n';
  std::set<int> selectedindex;
  unsigned int cnt = 0;
  int j = 0;
  while (cnt < target && j < 5000) {
    j += 1;
    int r = rand() % insts.size();
    if (selectedindex.find(r) == selectedindex.end()) {
      selectedindex.insert(r);
      int freq = inst2freq[insts[r]];
      cnt += freq;
      llvm::errs() << insts[r] << '\n';
      llvm::errs() << '#' << insts[r] << "\t\t\tfreq " << freq << " cnt/target: " << cnt
                   << '/' << target << "\n";
    }
  }

  llvm::errs() << "#total: " << cnt << "\n";

  return true;
}

