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

char TagPass::ID;

TagPass::TagPass(std::string &cfg, bool _useDbgInfo)
                  : ModulePass(ID), useDbgInfo(_useDbgInfo) {
  if (cfg != "") {
    if (!useDbgInfo) {
      std::ifstream f(cfg);
      while (f.good()) {
        std::string linestr;
        std::getline(f, linestr);

        if (linestr.empty() || linestr[0] == '#') {
          continue;
        }

        tagInstSet.insert(linestr);

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

        tagFuncSet.insert(prefix);

        prefix += ':';
        i++;
        for (; i < linestr.length(); i++) {
          if (linestr[i] != ':') {
            prefix += linestr[i];
          } else {
            break;
          }
        }

        tagBBSet.insert(prefix);
      }
      f.close();
    }
    else {
      std::ifstream f(cfg);
      while (f.good()) {
        std::string linestr;
        std::getline(f, linestr);
        if (linestr.empty() || linestr[0] == '#') {
          continue;
        }

        for (unsigned i = 0; i < linestr.length(); i++) {
          if (linestr[i] == ':') {
            linestr[i] = ' ';
          }
        }

        std::string filename;
        int line;
        std::stringstream ss;
        ss.str(linestr);

        ss >> filename;
        ss >> line;
        fileSet.insert(filename);
        locSet.insert(std::make_pair(filename, line));
      }
    }
  }
}

bool TagPass::runOnModuleByInst(Module &M) {
  for (Module::iterator f = M.begin(), fe = M.end(); f != fe; ++f) {
    if (tagFuncSet.find(f->getName().str()) == tagFuncSet.end()) {
      continue;
    }

    for (Function::iterator b = f->begin(), be = f->end(); b != be; ++b) {
      std::string bname = f->getName().str() + ":" + b->getName().str();
      if (tagBBSet.find(bname) == tagBBSet.end()) {
        continue;
      }

      for (BasicBlock::iterator i = b->begin(), ie = b->end(); i != ie; ++i) {
        llvm::Type *itype = i->getType();
        if (itype->isVoidTy()) {
          continue;
        }

        std::string iname = f->getName().str() + ":" + b->getName().str() + ":" + i->getName().str();
        if (tagInstSet.find(iname) == tagInstSet.end()) {
          continue;
        }
        if (!itype->isIntOrPtrTy()) {
          llvm::errs() << "recorded instruction not integer\n";
          continue;
        }

        llvm::errs() << "Instruction " << iname << " matched\n";
        Instruction &I = *i;

        std::vector<llvm::Type *> argTypes;
        llvm::LLVMContext &C = f->getContext();
        llvm::Type *voidTy = Type::getVoidTy(C);
        llvm::FunctionType *FTy = FunctionType::get(voidTy, argTypes, false);
        llvm::InlineAsm *IA =
          llvm::InlineAsm::get(FTy, "tag", "r,~{dirflag},~{fpsr},~{flags}", true, false);

        std::vector<llvm::Value *> args;
        Instruction *insertBeforeI = &I;
        Instruction *CI = llvm::CallInst::Create(IA, args, "");
        CI->insertBefore(insertBeforeI);
        CI->setDebugLoc(i->getDebugLoc());
      }
    }
  }

  return true;
}

bool TagPass::runOnModuleByLoc(Module &M) {
  for (Module::iterator f = M.begin(), fe = M.end(); f != fe; ++f) {
    bool funcNotMatched = false;
    for (Function::iterator b = f->begin(), be = f->end(); b != be; ++b) {
      bool prevMatched = false;
      for (BasicBlock::iterator i = b->begin(), ie = b->end(); i != ie; ++i) {
        const DebugLoc &loc = i->getDebugLoc();
        if (!loc)
          continue;

        auto *Scope = cast<DIScope>(loc.getScope());
        std::string filename = Scope->getFilename();

        if (fileSet.find(filename) == fileSet.end()) {
          funcNotMatched = true;
          break;
        }

        // only match the first instruction of in a contiguous range
        bool currMatched = locSet.find(std::make_pair(filename, loc.getLine())) != locSet.end();
        if (currMatched && !prevMatched) {
          llvm::errs() << filename << ":" << loc.getLine() << " matched\n";
          Instruction &I = *i;

          std::vector<llvm::Type *> argTypes;
          llvm::LLVMContext &C = f->getContext();
          llvm::Type *voidTy = Type::getVoidTy(C);
          llvm::FunctionType *FTy = FunctionType::get(voidTy, argTypes, false);
          llvm::InlineAsm *IA =
            llvm::InlineAsm::get(FTy, "tag", "r,~{dirflag},~{fpsr},~{flags}", true, false);

          std::vector<llvm::Value *> args;
          Instruction *insertBeforeI = &I;
          Instruction *CI = llvm::CallInst::Create(IA, args, "");
          CI->insertBefore(insertBeforeI);
          CI->setDebugLoc(i->getDebugLoc());
        }
        prevMatched = currMatched;
      }
      if (funcNotMatched) {
        break;
      }
    }
  }
  return true;
}

bool TagPass::runOnModule(Module &M) {
  if (useDbgInfo)
    return runOnModuleByLoc(M);
  else
    return runOnModuleByInst(M);
}

