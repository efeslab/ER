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
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include "llvm/Support/raw_ostream.h"

#include <fstream>
#include <sstream>
#include <string>

using namespace llvm;
using namespace klee;

char PTWritePass::ID;

PTWritePass::PTWritePass(std::string &cfg) : ModulePass(ID) {
  if (cfg != "") {
    std::ifstream f(cfg);
    while (f.good()) {
      std::string linestr;
      std::getline(f, linestr);

      if (linestr == "") {
        continue;
      }

      dataRecInstSet.insert(linestr);

      std::string prefix = "";
      unsigned i = 0;

      // get the mod
      for (; i < linestr.length(); i++) {
        if (linestr[i] != ':') {
          prefix += linestr[i];
        } else {
          break;
        }
      }

      dataRecFuncSet.insert(prefix);

      prefix += ':';
      i++;
      for (; i < linestr.length(); i++) {
        if (linestr[i] != ':') {
          prefix += linestr[i];
        } else {
          break;
        }
      }

      dataRecBBSet.insert(prefix);
    }
    f.close();
  }
}

bool PTWritePass::runOnModule(Module &M) {
  for (Module::iterator f = M.begin(), fe = M.end(); f != fe; ++f) {
    if (dataRecFuncSet.find(f->getName().str()) == dataRecFuncSet.end()) {
      continue;
    }

    for (Function::iterator b = f->begin(), be = f->end(); b != be; ++b) {
      std::string bname = f->getName().str() + ":" + b->getName().str();
      if (dataRecBBSet.find(bname) == dataRecBBSet.end()) {
        continue;
      }

      for (BasicBlock::iterator i = b->begin(), ie = b->end(); i != ie; ++i) {
        if (i->getType()->isVoidTy()) {
          continue;
        }

        std::string iname = f->getName().str() + ":" + b->getName().str() + ":" + i->getName().str();
        if (dataRecInstSet.find(iname) == dataRecInstSet.end()) {
          continue;
        }

        if (!i->getType()->isIntegerTy()) {
          llvm::errs() << "recorded instruction not integer\n";
          continue;
        }

        Instruction &I = *i;

        std::vector<llvm::Type *> argTypes;
        argTypes.push_back(Type::getInt64Ty(f->getContext()));
        llvm::Type *voidTy = Type::getVoidTy(f->getContext());
        llvm::FunctionType *FTy = FunctionType::get(voidTy, argTypes, false);
        llvm::InlineAsm *IA =
          llvm::InlineAsm::get(FTy, "ptwrite $0", "r,~{dirflag},~{fpsr},~{flags}", true, false);

        std::vector<llvm::Value *> args;
        args.push_back(&I);
        Instruction *CI = llvm::CallInst::Create(IA, args, "");
        b->getInstList().insertAfter(i, CI);
      }
    }
  }

  return true;
}
