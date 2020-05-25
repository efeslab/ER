//===-- Passes.h ------------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_PASSES_H
#define KLEE_PASSES_H

#include "klee/Config/Version.h"

#include "llvm/ADT/Triple.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/CodeGen/IntrinsicLowering.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include <unordered_set>
#include <string>
#include <map>
#include <set>

namespace llvm {
class Function;
class Instruction;
class Module;
class DataLayout;
class TargetLowering;
class Type;
} // namespace llvm

namespace klee {

/// RaiseAsmPass - This pass raises some common occurences of inline
/// asm which are used by glibc into normal LLVM IR.
class RaiseAsmPass : public llvm::ModulePass {
  static char ID;

  const llvm::TargetLowering *TLI;

  llvm::Triple triple;

  llvm::Function *getIntrinsic(llvm::Module &M, unsigned IID, llvm::Type **Tys,
                               unsigned NumTys);
  llvm::Function *getIntrinsic(llvm::Module &M, unsigned IID, llvm::Type *Ty0) {
    return getIntrinsic(M, IID, &Ty0, 1);
  }

  bool runOnInstruction(llvm::Module &M, llvm::Instruction *I);

public:
  RaiseAsmPass() : llvm::ModulePass(ID), TLI(0) {}

  bool runOnModule(llvm::Module &M) override;
};

// This is a module pass because it can add and delete module
// variables (via intrinsic lowering).
class IntrinsicCleanerPass : public llvm::ModulePass {
  static char ID;
  const llvm::DataLayout &DataLayout;
  llvm::IntrinsicLowering *IL;

  bool runOnBasicBlock(llvm::BasicBlock &b, llvm::Module &M);

public:
  IntrinsicCleanerPass(const llvm::DataLayout &TD)
      : llvm::ModulePass(ID), DataLayout(TD),
        IL(new llvm::IntrinsicLowering(TD)) {}
  ~IntrinsicCleanerPass() { delete IL; }

  bool runOnModule(llvm::Module &M) override;
};

class DebugPass : public llvm::ModulePass {
  static char ID;
  int nxtLine;
  bool runOnBasicBlock(llvm::BasicBlock &b, llvm::Module &M);

public:
  DebugPass() : ModulePass(ID), nxtLine(0) {}
  bool runOnModule(llvm::Module &M) override;
};



// performs two transformations which make interpretation
// easier and faster.
//
// 1) Ensure that all the PHI nodes in a basic block have
//    the incoming block list in the same order. Thus the
//    incoming block index only needs to be computed once
//    for each transfer.
//
// 2) Ensure that no PHI node result is used as an argument to
//    a subsequent PHI node in the same basic block. This allows
//    the transfer to execute the instructions in order instead
//    of in two passes.
class PhiCleanerPass : public llvm::FunctionPass {
  static char ID;

public:
  PhiCleanerPass() : llvm::FunctionPass(ID) {}

  bool runOnFunction(llvm::Function &f) override;
};

class DivCheckPass : public llvm::ModulePass {
  static char ID;

public:
  DivCheckPass() : ModulePass(ID) {}
  bool runOnModule(llvm::Module &M) override;
};

/// This pass injects checks to check for overshifting.
///
/// Overshifting is where a Shl, LShr or AShr is performed
/// where the shift amount is greater than width of the bitvector
/// being shifted.
/// In LLVM (and in C/C++) this undefined behaviour!
///
/// Example:
/// \code
///     unsigned char x=15;
///     x << 4 ; // Defined behaviour
///     x << 8 ; // Undefined behaviour
///     x << 255 ; // Undefined behaviour
/// \endcode
class OvershiftCheckPass : public llvm::ModulePass {
  static char ID;

public:
  OvershiftCheckPass() : ModulePass(ID) {}
  bool runOnModule(llvm::Module &M) override;
};

/// LowerSwitchPass - Replace all SwitchInst instructions with chained branch
/// instructions.  Note that this cannot be a BasicBlock pass because it
/// modifies the CFG!
class LowerSwitchPass : public llvm::FunctionPass {
public:
  static char ID; // Pass identification, replacement for typeid
  LowerSwitchPass() : FunctionPass(ID) {}

  bool runOnFunction(llvm::Function &F) override;

  struct SwitchCase {
    llvm ::Constant *value;
    llvm::BasicBlock *block;

    SwitchCase() : value(0), block(0) {}
    SwitchCase(llvm::Constant *v, llvm::BasicBlock *b) : value(v), block(b) {}
  };

  typedef std::vector<SwitchCase> CaseVector;
  typedef std::vector<SwitchCase>::iterator CaseItr;

private:
  void processSwitchInst(llvm::SwitchInst *SI);
  void switchConvert(CaseItr begin, CaseItr end, llvm::Value *value,
                     llvm::BasicBlock *origBlock,
                     llvm::BasicBlock *defaultBlock);
};

/// InstructionOperandTypeCheckPass - Type checks the types of instruction
/// operands to check that they conform to invariants expected by the Executor.
///
/// This is a ModulePass because other pass types are not meant to maintain
/// state between calls.
class InstructionOperandTypeCheckPass : public llvm::ModulePass {
private:
  bool instructionOperandsConform;

public:
  static char ID;
  InstructionOperandTypeCheckPass()
      : llvm::ModulePass(ID), instructionOperandsConform(true) {}
  bool runOnModule(llvm::Module &M) override;
  bool checkPassed() const { return instructionOperandsConform; }
};

/// FunctionAliasPass - Enables a user of KLEE to specify aliases to functions
/// using -function-alias=<name|pattern>:<replacement> which are injected as
/// GlobalAliases into the module. The replaced function is removed.
class FunctionAliasPass : public llvm::ModulePass {

public:
  static char ID;
  FunctionAliasPass() : llvm::ModulePass(ID) {}
  bool runOnModule(llvm::Module &M) override;

private:
  static const llvm::FunctionType *getFunctionType(const llvm::GlobalValue *gv);
  static bool checkType(const llvm::GlobalValue *match, const llvm::GlobalValue *replacement);
  static bool tryToReplace(llvm::GlobalValue *match, llvm::GlobalValue *replacement);
  static bool isFunctionOrGlobalFunctionAlias(const llvm::GlobalValue *gv);

};

#ifdef USE_WORKAROUND_LLVM_PR39177
/// WorkaroundLLVMPR39177Pass - Workaround for LLVM PR39177 within KLEE repo.
/// For more information on this, please refer to the comments in
/// cmake/workaround_llvm_pr39177.cmake
class WorkaroundLLVMPR39177Pass : public llvm::ModulePass {
public:
  static char ID;
  WorkaroundLLVMPR39177Pass() : llvm::ModulePass(ID) {}
  bool runOnModule(llvm::Module &M) override;
};
#endif

/// Instruments every function that contains a KLEE function call as nonopt
class OptNonePass : public llvm::ModulePass {
public:
  static char ID;
  OptNonePass() : llvm::ModulePass(ID) {}
  bool runOnModule(llvm::Module &M) override;
};

class RmFabsPass : public llvm::ModulePass {
public:
  static char ID;
  RmFabsPass();
  bool runOnModule(llvm::Module &M) override;
};

class AssignIDPass : public llvm::ModulePass {
private:
  std::string prefix;
public:
  static char ID;
  AssignIDPass(std::string &_prefix);
  bool runOnModule(llvm::Module &M) override;
};

class RmIDPass : public llvm::ModulePass {
public:
  static char ID;
  RmIDPass();
  bool runOnModule(llvm::Module &M) override;
};

class PTWritePass : public llvm::ModulePass {
private:
  std::unordered_set<std::string> dataRecFuncSet;
  std::unordered_set<std::string> dataRecBBSet;
  std::unordered_set<std::string> dataRecInstSet;
public:
  static char ID;
  static const std::string castPrefix;
  PTWritePass(std::string &cfg);
  bool runOnModule(llvm::Module &M) override;
};

class SelectRandomPass : public llvm::ModulePass {
private:
  std::map<std::string, unsigned int> inst2freq;
  std::vector<std::string> insts;
  unsigned int target;

public:
  static char ID;
  SelectRandomPass(unsigned target);
  bool runOnModule(llvm::Module &M) override;
};

class TagPass : public llvm::ModulePass {
private:
  std::unordered_set<std::string> tagFuncSet;
  std::unordered_set<std::string> tagBBSet;
  std::unordered_set<std::string> tagInstSet;

  bool useDbgInfo;
  std::unordered_set<std::string> fileSet;
  std::set<std::pair<std::string, int> > locSet;
  bool runOnModuleByInst(llvm::Module &M);
  bool runOnModuleByLoc(llvm::Module &M);
public:
  static char ID;
  TagPass(std::string &cfg, bool _useDbgInfo);
  bool runOnModule(llvm::Module &M) override;
};

class DebugIR : public llvm::ModulePass {
  /// If true, write a source file to disk.
  bool WriteSourceToDisk;

  /// Hide certain (non-essential) debug information (only relevant if
  /// createSource is true.
  bool HideDebugIntrinsics;
  bool HideDebugMetadata;

  /// The location of the source file.
  std::string Directory;
  std::string Filename;

  /// True if a temporary file name was generated.
  bool GeneratedPath;

  /// True if the file name was read from the Module.
  bool ParsedPath;

public:
  static char ID;

  llvm::StringRef getPassName() const override { return "DebugIR"; }

  /// Generate a file on disk to be displayed in a debugger. If Filename and
  /// Directory are empty, a temporary path will be generated.
  DebugIR(bool HideDebugIntrinsics, bool HideDebugMetadata,
          llvm::StringRef Directory, llvm::StringRef Filename);
//      : ModulePass(ID), WriteSourceToDisk(true),
//        HideDebugIntrinsics(HideDebugIntrinsics),
//        HideDebugMetadata(HideDebugMetadata), Directory(Directory),
//        Filename(Filename), GeneratedPath(false), ParsedPath(false) {}

  /// Modify input in-place; do not generate additional files, and do not hide
  /// any debug intrinsics/metadata that might be present.
  DebugIR();
      //: ModulePass(ID), WriteSourceToDisk(false), HideDebugIntrinsics(false),
      //  HideDebugMetadata(false), GeneratedPath(false), ParsedPath(false) {}

  /// Run pass on M and set Path to the source file path in the output module.
  bool runOnModule(llvm::Module &M, std::string &Path);
  bool runOnModule(llvm::Module &M) override;

private:

  /// Returns the concatenated Directory + Filename, without error checking
  std::string getPath();

  /// Attempts to read source information from debug information in M, and if
  /// that fails, from M's identifier. Returns true on success, false otherwise.
  bool getSourceInfo(const llvm::Module &M);

  /// Replace the extension of Filename with NewExtension, and return true if
  /// successful. Return false if extension could not be found or Filename is
  /// empty.
  bool updateExtension(llvm::StringRef NewExtension);

  /// Generate a temporary filename and open an fd
  void generateFilename(std::unique_ptr<int> &fd);

  /// Creates DWARF CU/Subroutine metadata
  void createDebugInfo(llvm::Module &M,
                       std::unique_ptr<llvm::Module> &DisplayM);

  /// Returns true if either Directory or Filename is missing, false otherwise.
  bool isMissingPath();

  /// Write M to disk, optionally passing in an fd to an open file which is
  /// closed by this function after writing. If no fd is specified, a new file
  /// is opened, written, and closed.
  void writeDebugBitcode(const llvm::Module *M, int *fd = nullptr);
};


} // namespace klee

#endif /* KLEE_PASSES_H */
