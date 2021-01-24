//===-- KModule.h -----------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_KMODULE_H
#define KLEE_KMODULE_H

#include "klee/Config/Version.h"
#include "klee/Interpreter.h"

#include "llvm/ADT/ArrayRef.h"

#include <map>
#include <memory>
#include <set>
#include <vector>

namespace llvm {
  class BasicBlock;
  class Constant;
  class Function;
  class Instruction;
  class Module;
  class DataLayout;
}

namespace klee {
  struct Cell;
  class Executor;
  class Expr;
  class InterpreterHandler;
  class InstructionInfoTable;
  struct KInstruction;
  class KModule;
  template<class T> class ref;

  struct KFunction {
    llvm::Function *function;

    unsigned numArgs, numRegisters;

    unsigned numInstructions;
    KInstruction **instructions;

    std::map<const llvm::BasicBlock*, unsigned> basicBlockEntry;

    /// Whether instructions in this function should count as
    /// "coverable" for statistics and search heuristics.
    bool trackCoverage;
    /// How many times this Function has been called.
    /// Maintained at ExecutionState::pushFrame
    unsigned int frequency = 0;

  public:
    explicit KFunction(llvm::Function*, KModule *);
    KFunction(const KFunction &) = delete;
    KFunction &operator=(const KFunction &) = delete;

    ~KFunction();

    unsigned getArgRegister(unsigned index) { return index; }
    KInstruction *getKInstruction(llvm::Instruction *inst);
  };


  class KConstant {
  public:
    /// Actual LLVM constant this represents.
    llvm::Constant* ct;

    /// The constant ID.
    unsigned id;

    /// First instruction where this constant was encountered, or NULL
    /// if not applicable/unavailable.
    KInstruction *ki;

    KConstant(llvm::Constant*, unsigned, KInstruction*);
  };


  class KModule {
  public:
    std::unique_ptr<llvm::Module> module;
    std::unique_ptr<llvm::DataLayout> targetData;

    // Our shadow versions of LLVM structures.
    std::vector<std::unique_ptr<KFunction>> functions;
    std::map<llvm::Function*, KFunction*> functionMap;

    // Functions which escape (may be called indirectly)
    // XXX change to KFunction
    std::set<llvm::Function*> escapingFunctions;

    std::unique_ptr<InstructionInfoTable> infos;

    std::vector<llvm::Constant*> constants;
    std::map<const llvm::Constant *, std::unique_ptr<KConstant>> constantMap;
    KConstant* getKConstant(const llvm::Constant *c);

    std::unique_ptr<Cell[]> constantTable;

    // Functions which are part of KLEE runtime
    std::set<const llvm::Function*> internalFunctions;

  private:
    // Mark function with functionName as part of the KLEE runtime
    void addInternalFunction(const char* functionName);

    std::map<llvm::Instruction*, KInstruction*> instructionMapCache;

  public:
    KModule() = default;

    /// Optimise and prepare module such that KLEE can execute it
    //
    void optimiseAndPrepare(const Interpreter::ModuleOptions &opts,
                            llvm::ArrayRef<const char *>);

    /// Manifest the generated module (e.g. assembly.ll, output.bc) and
    /// prepares KModule
    ///
    /// @param ih
    /// @param forceSourceOutput true if assembly.ll should be created
    ///
    // FIXME: ihandler should not be here
    void manifest(InterpreterHandler *ih, bool forceSourceOutput);

    /// Link the provided modules together as one KLEE module.
    ///
    /// If the entry point is empty, all modules are linked together.
    /// If the entry point is not empty, all modules are linked which resolve
    /// the dependencies of the module containing entryPoint
    ///
    /// @param modules list of modules to be linked together
    /// @param entryPoint name of the function which acts as the program's entry
    /// point
    /// @return true if at least one module has been linked in, false if nothing
    /// changed
    bool link(std::vector<std::unique_ptr<llvm::Module>> &modules,
              const std::string &entryPoint);

    void instrument(const Interpreter::ModuleOptions &opts);

    /// Return an id for the given constant, creating a new one if necessary.
    unsigned getConstantID(llvm::Constant *c, KInstruction* ki);

    /// Run passes that check if module is valid LLVM IR and if invariants
    /// expected by KLEE's Executor hold.
    void checkModule();

    /// Get the corresponding KInstruction of a llvm::Instruction.
    KInstruction *getKInstruction(llvm::Instruction *inst);

    /// Remove floating point instructions bacause Klee does not support it
    static void removeFabs(llvm::Module *M);

    /// Assign a unique ID for each instruction and basic block. The unique ID will
    /// be used in recording.
    static void assignID(llvm::Module *M, std::string &prefix);

    /// randomely select instructions
    static void selectRandInst(llvm::Module *M, unsigned int target);


    /// Add PTWrite instruction after specified instructions or inside specific
    /// functions
    static void addPTWrite(llvm::Module *M, const std::string &instcfg,
                           const std::string &funccfg);

    /// Add Tag fake instruction after sepecified instructions
    static void addTag(llvm::Module *M, std::string &cfg, bool useDbgInfo);

    /// Use IR to replace debug info
    static void assignDebugIR(llvm::Module *M, std::string &directory, std::string &filename);

    /// Remove the human readable ID from each instruction.
    static void removeID(llvm::Module *M);

    /// Save Instruction and Function frequency to LLVM IR Module as metadata (MDNode).
    /// Those frequencies were maintained inside KInstruction/KFunction until
    /// this function is called.
    /// MDNode can be exported to LLVM bitcode and still accessible when the
    /// bitcode is loaded again.
    void saveCntToMDNode();
  };
} // End klee namespace

#endif /* KLEE_KMODULE_H */
