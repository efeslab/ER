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

RmFabsPass::RmFabsPass() : ModulePass(ID) {}

bool RmFabsPass::runOnModule(Module &M) {
  for (Module::iterator f = M.begin(), fe = M.end(); f != fe; ++f) {
    for (Function::iterator b = f->begin(), be = f->end(); b != be; ++b) {

      for (BasicBlock::iterator I = b->begin(), IE = b->end(); I != IE; ++I) {
        if (isa<CallInst>(I)) {
          Function *calledFunc = cast<CallInst>(I)->getCalledFunction();
          if (calledFunc) { // in case it is a indirect call
            StringRef name = calledFunc->getName();
            if (name.compare("llvm.fabs.f64") == 0 ||
                name.compare("llvm.fabs.f32") == 0 ||
                name.compare("llvm.fabs.f80") == 0) {
                  
                // replace instruction calling llvm.fabs.f64 with its argument as value
                Value *arg = cast<CallInst>(I)->getArgOperand(0);
                ReplaceInstWithValue(b->getInstList(), I, arg);
            }
          }
        }
      }
    }
  }
  return true;
}

char RmFabsPass::ID = 0;
