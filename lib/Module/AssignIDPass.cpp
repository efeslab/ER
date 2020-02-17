//===-- IntrinsicCleaner.cpp ----------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "Passes.h"

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
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include "llvm/Support/raw_ostream.h"

#include <sstream>
#include <string>

using namespace llvm;

using namespace klee;

char AssignIDPass::ID;

AssignIDPass::AssignIDPass(std::string &_prefix)
    : llvm::ModulePass(ID), prefix(_prefix) {}

bool AssignIDPass::runOnModule(Module &M) {
  for (Module::iterator f = M.begin(), fe = M.end(); f != fe; ++f) {
    unsigned bcnt = 0;
    for (Function::iterator b = f->begin(), be = f->end(); b != be; ++b) {
      std::string bname = prefix + "B" + std::to_string(bcnt++);
      if (b->getName() == "") {
        b->setName(bname);
      }
      unsigned icnt = 0;
      for (BasicBlock::iterator i = b->begin(), ie = b->end(); i != ie; ++i) {
        if (!i->getType()->isVoidTy()) {
          if (i->getName() == "") {
            std::string iname = bname + "I" + std::to_string(icnt++);
            i->setName(iname);
          }
        }
      }
    }
  }

  return true;
}
