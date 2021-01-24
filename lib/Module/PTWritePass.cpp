//===-- IntrinsicCleaner.cpp ----------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "klee/Internal/Module/KInstruction.h"
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

#include <algorithm>
#include <fstream>
#include <sstream>
#include <unordered_set>

using namespace llvm;
using namespace klee;

char PTWritePass::ID;
const std::string PTWritePass::castPrefix("ptwritecast");

PTWritePass::PTWritePass(const std::string &instcfg, const std::string &funccfg)
    : ModulePass(ID) {
      setupInstCFG(instcfg);
      setupFuncCFG(funccfg);
}

void PTWritePass::setupInstCFG(const std::string &instcfg) {
  if (instcfg != "") {
    std::ifstream f(instcfg);
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

void PTWritePass::setupFuncCFG(const std::string &funccfg) {
  if (funccfg != "") {
    std::ifstream f(funccfg);
    while (f.good()) {
      std::string linestr;
      std::getline(f, linestr);
      if (linestr.empty() || linestr[0] == '#') {
        continue;
      }
      dataRecWholeFuncSet.insert(linestr);
    }
  }
}

/*
 * InstrumentationManager handles ptwrite instrumentation as well as statistics
 * (report recording overhead).
 * There exists multiple ways to identify a single instrution for
 * instrumentation. They use InstrumentationManager to do the real work.
 */
class InstrumentationManager {
public:
  typedef std::unordered_set<llvm::Instruction *> TyInstSet;

private:
  llvm::LLVMContext &C;
  const llvm::DataLayout &DL;
  // `iasm` represents the type of inline assembly function. It will be
  // constructed at the beginning of runOnModule, according to the module
  // context. It will be consumed during ptwrite instrumentation, where CallInst
  // will be created to call `iasm`
  llvm::InlineAsm *iasm;
  llvm::Type *TyInt64;
  Twine getCastName() {
    // to assign each additional CastInst a unique ID
    static unsigned int ptwrite_cnt = 0;
    return PTWritePass::castPrefix + Twine(ptwrite_cnt++);
  }
  // Statistics
  TyInstSet instrumented_insts;

public:
  InstrumentationManager(llvm::Module &M)
      : C(M.getContext()), DL(M.getDataLayout()) {
    // init inline asm function type
    std::vector<llvm::Type *> argTypes;
    TyInt64 = Type::getInt64Ty(C);
    argTypes.push_back(TyInt64);
    llvm::Type *voidTy = Type::getVoidTy(C);
    llvm::FunctionType *FTy = FunctionType::get(voidTy, argTypes, false);
    iasm = llvm::InlineAsm::get(FTy, "ptwrite $0",
                                "r,~{dirflag},~{fpsr},~{flags}", true, false);
  }
  void InstrumentPTWrite(llvm::Instruction *inst);
  const TyInstSet &getAllInstrumentedInsts() const {
    return instrumented_insts;
  }
};

void InstrumentationManager::InstrumentPTWrite(llvm::Instruction *inst) {
  llvm::Type *itype = inst->getType();
  llvm::Type *TyDouble = Type::getDoubleTy(C);
  std::vector<llvm::Value *> args;
  Instruction *insertAfterI = inst;
  if (itype->isPointerTy()) {
    // need special pointer to int cast
    CastInst *castI = CastInst::CreatePointerCast(inst, TyInt64, getCastName());
    castI->insertAfter(inst);
    insertAfterI = castI;
  } else if (itype->isIntegerTy()) {
    unsigned w = itype->getIntegerBitWidth();
    if (w < 64) {
      // need type cast
      CastInst *castI =
          CastInst::CreateZExtOrBitCast(inst, TyInt64, getCastName());
      castI->insertAfter(inst);
      insertAfterI = castI;
    } else if (w > 64) {
      // TODO handle large interger, need split then multiple ptwrites
      llvm::errs() << "ptwrite for large integers is not implemented\n";
      llvm::errs() << "Inst: " << *inst;
    } // else Int64, no special care is needed
  } else if (itype->isDoubleTy()) {
    CastInst *castI = CastInst::CreateFPCast(inst, TyInt64, getCastName());
    castI->insertAfter(inst);
    insertAfterI = castI;
  } else if (itype->isFloatTy()) {
    CastInst *castI1 = CastInst::CreateFPCast(inst, TyDouble, getCastName());
    castI1->insertAfter(inst);
    CastInst *castI2 = CastInst::CreateFPCast(castI1, TyInt64, getCastName());
    castI2->insertAfter(castI1);
    insertAfterI = castI2;
  } else {
    // TODO handle Vector of Integer type
    llvm::errs() << "The instruction you want to recorded is not an integer\n";
    llvm::errs() << "Inst: " << *inst << '\n';
    return;
  }
  args.push_back(insertAfterI);
  Instruction *CI = llvm::CallInst::Create(iasm, args, "");
  CI->insertAfter(insertAfterI);
  auto ret = instrumented_insts.insert(inst);
  assert(ret.second && "Should not instrument the same inst twice");
}

bool PTWritePass::runOnModule(Module &M) {
  const llvm::DataLayout &DL = M.getDataLayout();
  InstrumentationManager mgr(M);

  for (Module::iterator f = M.begin(), fe = M.end(); f != fe; ++f) {
    const std::string &fname = f->getName();
    if (dataRecWholeFuncSet.find(fname) != dataRecWholeFuncSet.end()) {
      // First match whole function instrumentation
      // For now, I instrument all LoadInst
      for (auto &b : *f) {
        for (auto &I : b) {
          llvm::Instruction *inst = &I;
          if (isa<llvm::LoadInst>(inst)) {
            // TODO InstrumentPTWrite will modify the same BB, should do a two
            // pass and decouple the modification from iteration
            mgr.InstrumentPTWrite(inst);
          }
        }
      }
    } else if (dataRecFuncSet.find(fname) != dataRecFuncSet.end()) {
      // Then match specific instruction identified by the combination of
      // (function name, BB ID, inst ID), note that the id of BB and inst are
      // assigned by the "assign-id" pass from tool/prepass
      for (Function::iterator b = f->begin(), be = f->end(); b != be; ++b) {
        std::string bname = f->getName().str() + ":" + b->getName().str();
        if (dataRecBBSet.find(bname) == dataRecBBSet.end()) {
          continue;
        }

        for (BasicBlock::iterator i = b->begin(), ie = b->end(); i != ie; ++i) {
          std::string iname = f->getName().str() + ":" + b->getName().str() +
                              ":" + i->getName().str();
          if (dataRecInstSet.find(iname) == dataRecInstSet.end()) {
            continue;
          }
          llvm::Instruction *inst = &(*i);
          llvm::Type *itype = inst->getType();
          unsigned int type_width = DL.getTypeSizeInBits(itype);
          unsigned int freq = KInstruction::getLoadedFreq(inst);
          llvm::errs() << "Instruction " << iname << " (w" << type_width << ") "
                       << " freq " << freq << " matched\n";
          if (itype->isPointerTy()) {
            llvm::errs() << "Warning: pointer recording at " << iname
                         << " may not work due to undeterministic malloc\n";
          }
          mgr.InstrumentPTWrite(inst);
        }
      }
    }
  }

  unsigned int actual_bytes = 0;
  unsigned int ptwrite_freq = 0;
  InstrumentationManager::TyInstSet insts = mgr.getAllInstrumentedInsts();
  struct InstInfo {
    const llvm::Instruction *I;
    unsigned freq;
  };
  std::vector<InstInfo> instinfo;
  instinfo.reserve(insts.size());
  for (llvm::Instruction *inst : insts) {
    llvm::Type *itype = inst->getType();
    unsigned int type_width = DL.getTypeSizeInBits(itype);
    unsigned int freq = KInstruction::getLoadedFreq(inst);
    ptwrite_freq += freq;
    actual_bytes += freq * type_width / 8;
    instinfo.push_back({inst, freq});
  }
  llvm::errs() << "Total " << insts.size()
               << " instructions are instrumented\n";
  llvm::errs() << "Actual Bytes to record: " << actual_bytes << '\n';
  llvm::errs() << "PTWrite executed: " << ptwrite_freq << '\n';
  llvm::errs() << "PTWrite Recorded: "
               << ptwrite_freq * DL.getPointerSizeInBits() / 8 << '\n';
  if (ptwrite_freq) {
    // report verbose recording cost information if frequency info is available
    std::sort(instinfo.begin(), instinfo.end(),
              [](auto &a, auto &b) { return a.freq >= b.freq; });
    for (InstInfo &ii : instinfo) {
      llvm::errs() << KInstruction::getInstUniqueID(ii.I) << " freq " << ii.freq
                   << '\n';
    }
  }
  return true;
}
