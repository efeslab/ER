//===-- KInstruction.h ------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_KINSTRUCTION_H
#define KLEE_KINSTRUCTION_H

#include "klee/Config/Version.h"
#include "klee/Internal/Module/InstructionInfoTable.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Support/DataTypes.h"
#include "llvm/Support/raw_ostream.h"

#include <vector>

namespace llvm {
  class Instruction;
}

namespace klee {
  class Executor;
  struct InstructionInfo;
  class KModule;


  /// KInstruction - Intermediate instruction representation used
  /// during execution.
  struct KInstruction {
    llvm::Instruction *inst;    
    const InstructionInfo *info;

    /// Value numbers for each operand. -1 is an invalid value,
    /// otherwise negative numbers are indices (negated and offset by
    /// 2) into the module constant table and positive numbers are
    /// register indices.
    int *operands;
    /// Destination register index.
    unsigned dest;
    /// How many times this Instruction has been executed
    /// Maintained at Executor::executeInstruction
    unsigned int frequency = 0;

  public:
    virtual ~KInstruction();
    std::string getSourceLocation() const;
    static unsigned int getLoadedFreq(llvm::Instruction *inst);
    unsigned int getLoadedFreq() const { return getLoadedFreq(inst); }
    // recording cost == inst freq * inst width (bits)
    unsigned int getRecordingCost() const;
    // note that UniqueID depends on prepass to assign human-readable id
    std::string getUniqueID() const { return getUniqueID(inst); }
    static std::string getUniqueID(const llvm::Instruction *inst) {
      const llvm::BasicBlock *bb = inst->getParent();
      return getUniqueID(bb) + ':' + inst->getName().str();
    }
    static std::string getUniqueID(const llvm::BasicBlock *bb) {
      const llvm::Function *f = bb->getParent();
      return getUniqueID(f) + ':' + bb->getName().str();
    }
    static const std::string getUniqueID(const llvm::Function *f) {
      return f->getName().str();
    }

  };

  struct KGEPInstruction : KInstruction {
    /// indices - The list of variable sized adjustments to add to the pointer
    /// operand to execute the instruction. The first element is the operand
    /// index into the GetElementPtr instruction, and the second element is the
    /// element size to multiple that index by.
    std::vector< std::pair<unsigned, uint64_t> > indices;

    /// offset - A constant offset to add to the pointer operand to execute the
    /// instruction.
    uint64_t offset;
  };
}

#endif /* KLEE_KINSTRUCTION_H */
