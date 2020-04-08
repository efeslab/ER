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

      if (linestr.empty() || linestr[0] == '#') {
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
        llvm::Type *itype = i->getType();
        if (itype->isVoidTy()) {
          continue;
        }

        std::string iname = f->getName().str() + ":" + b->getName().str() + ":" + i->getName().str();
        if (dataRecInstSet.find(iname) == dataRecInstSet.end()) {
          continue;
        }
        if (!itype->isIntOrPtrTy()) {
          llvm::errs() << "recorded instruction not integer\n";
          continue;
        }

        llvm::errs() << "Instruction " << iname << " matched\n";
        // to assign each additional CastInst a unique ID
        static unsigned int ptwrite_cnt = 0;

        Instruction &I = *i;

        std::vector<llvm::Type *> argTypes;
        llvm::LLVMContext &C = f->getContext();
        llvm::Type *TyInt64 = Type::getInt64Ty(C);
        argTypes.push_back(TyInt64);
        llvm::Type *voidTy = Type::getVoidTy(C);
        llvm::FunctionType *FTy = FunctionType::get(voidTy, argTypes, false);
        llvm::InlineAsm *IA =
          llvm::InlineAsm::get(FTy, "ptwrite $0", "r,~{dirflag},~{fpsr},~{flags}", true, false);

        std::vector<llvm::Value *> args;
        Instruction *insertAfterI = &I;
        if (itype->isPointerTy()) {
          // need special pointer to int cast
          llvm::errs() << "Warning: pointer recording at " << iname <<
            " may not work due to undeterministic malloc\n";
          Twine castname = Twine("ptwriteptrcast") + Twine(ptwrite_cnt++);
          CastInst *castI = CastInst::CreatePointerCast(&I, TyInt64, castname);
          castI->insertAfter(&I);
          args.push_back(castI);
          insertAfterI = castI;
        }
        else if (itype != TyInt64) {
          // need type cast
          Twine castname = Twine("ptwritecast") + Twine(ptwrite_cnt++);
          CastInst *castI = CastInst::CreateZExtOrBitCast(&I, TyInt64, castname);
          castI->insertAfter(&I);
          args.push_back(castI);
          insertAfterI = castI;
        }
        else {
          args.push_back(&I);
        }
        Instruction *CI = llvm::CallInst::Create(IA, args, "");
        CI->insertAfter(insertAfterI);
      }
    }
  }

  return true;
}
