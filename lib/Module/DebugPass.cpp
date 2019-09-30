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

using namespace llvm;

using namespace klee;

char DebugPass::ID;

// nxtLine = 0 at initialization

bool DebugPass::runOnModule(Module &M) {
  bool dirty = false;
  for (Module::iterator f = M.begin(), fe = M.end(); f != fe; ++f)
    for (Function::iterator b = f->begin(), be = f->end(); b != be; ++b)
      dirty |= runOnBasicBlock(*b, M);

  return dirty;
}



bool DebugPass::runOnBasicBlock(BasicBlock &b, Module &M) {
  bool dirty = false;
  LLVMContext &ctx = M.getContext();

  for (BasicBlock::iterator i = b.begin(), ie = b.end(); i != ie; ++i) {

    // see if the memcpy shit exists...
    CallInst *ci = dyn_cast<CallInst>(&*i);
    if (ci) {
      Function *called = ci->getCalledFunction();
      Function *func = ci->getFunction();

      // If we have an inlinable functions w/out debugLoc, I bet we
      // messed it up. SO, we just create a fake debugLocation for the
      // new call (this usually comes from lowering intrinsics)

      if (func->getSubprogram() &&
          called && called->getSubprogram() &&
          !ci->getDebugLoc())
      {
        ci->setDebugLoc(DILocation::get(ctx,
                                        nxtLine,
                                        1,
                                        func->getSubprogram()));
        assert (ci->getDebugLoc());

        dirty = true;
      }
    }
  }

  return dirty;
}
