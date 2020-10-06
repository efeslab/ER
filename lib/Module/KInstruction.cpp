//===-- KInstruction.cpp --------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "klee/Internal/Module/KInstruction.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Metadata.h"

using namespace llvm;
using namespace klee;

/***/

KInstruction::~KInstruction() {
  delete[] operands;
}

std::string KInstruction::getSourceLocation() const {
  if (!info->file.empty())
    return info->file + ":" + std::to_string(info->line) + " " +
           std::to_string(info->column);
  else return "[no debug info]";
}

unsigned int KInstruction::getLoadedFreq() const {
  MDNode *MD = inst->getMetadata("klee.freq");
  if (MD) {
    if (ConstantAsMetadata *CMD = dyn_cast<ConstantAsMetadata>(MD->getOperand(0).get())) {
      if (ConstantInt *CI = dyn_cast<ConstantInt>(CMD->getValue())) {
        return CI->getZExtValue();
      }
    }
  }
  return 0;
}

unsigned int KInstruction::getRecordingCost() const {
  const llvm::Module *m = inst->getModule();
  return m->getDataLayout().getTypeSizeInBits(inst->getType()) * getLoadedFreq();
}
