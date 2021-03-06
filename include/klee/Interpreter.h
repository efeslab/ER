//===-- Interpreter.h - Abstract Execution Engine Interface -----*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//===----------------------------------------------------------------------===//

#ifndef KLEE_INTERPRETER_H
#define KLEE_INTERPRETER_H

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <ctime>
#include "klee/Internal/Support/Serialize.h"
struct KTest;

namespace llvm {
class Function;
class LLVMContext;
class Module;
class raw_ostream;
class raw_fd_ostream;
}

namespace klee {
class ExecutionState;
class Interpreter;
class TreeStreamWriter;

class InterpreterHandler {
public:
  InterpreterHandler() {}
  virtual ~InterpreterHandler() {}

  virtual llvm::raw_ostream &getInfoStream() const = 0;

  virtual std::string getOutputFilename(const std::string &filename) = 0;
  virtual std::unique_ptr<llvm::raw_fd_ostream> openOutputFile(const std::string &filename) = 0;

  virtual void incPathsExplored() = 0;

  virtual void processTestCase(const ExecutionState &state,
                               bool getSymbolicSolution,
                               const char *err,
                               const char *suffix) = 0;
  /// Tell the interpreter when the symbolic engine started, excluding bitcode
  /// parsing and other setup process.
  virtual void setStartTime(std::time_t t) = 0;
  virtual void reportInEngineTime() const = 0;
};

class Interpreter {
public:
  /// ModuleOptions - Module level options which can be set when
  /// registering a module with the interpreter.
  struct ModuleOptions {
    std::string LibraryDir;
    std::string EntryPoint;
    bool Optimize;
    bool CheckDivZero;
    bool CheckOvershift;
    /// Monolithic Module?
    /// true if the given module is self-contained, i.e. already linked with all
    /// required module (POSIX, uclibc, kleeIntrinsic, etc.) and preprosessed by
    /// klee
    bool MonolithicModule;

    ModuleOptions(const std::string &_LibraryDir,
                  const std::string &_EntryPoint, bool _Optimize,
                  bool _CheckDivZero, bool _CheckOvershift,
                  bool _MonolithicModule)
        : LibraryDir(_LibraryDir), EntryPoint(_EntryPoint), Optimize(_Optimize),
          CheckDivZero(_CheckDivZero), CheckOvershift(_CheckOvershift),
          MonolithicModule(_MonolithicModule) {}
  };

  enum LogType
  {
	  STP, //.CVC (STP's native language)
	  KQUERY, //.KQUERY files (kQuery native language)
	  SMTLIB2 //.SMT2 files (SMTLIB version 2 files)
  };

  /// InterpreterOptions - Options varying the runtime behavior during
  /// interpretation.
  struct InterpreterOptions {
    /// A frequency at which to make concrete reads return constrained
    /// symbolic values. This is used to test the correctness of the
    /// symbolic execution on concrete programs.
    unsigned MakeConcreteSymbolic;

    InterpreterOptions()
      : MakeConcreteSymbolic(false)
    {}
  };

protected:
  const InterpreterOptions interpreterOpts;

  Interpreter(const InterpreterOptions &_interpreterOpts)
    : interpreterOpts(_interpreterOpts)
  {}

public:
  virtual ~Interpreter() {}

  static Interpreter *create(llvm::LLVMContext &ctx,
                             const InterpreterOptions &_interpreterOpts,
                             InterpreterHandler *ih);

  /// Register the module to be executed.
  /// \param modules A list of modules that should form the final
  ///                module
  /// \return The final module after it has been optimized, checks
  /// inserted, and modified for interpretation.
  virtual llvm::Module *
  setModule(std::vector<std::unique_ptr<llvm::Module>> &modules,
            const ModuleOptions &opts) = 0;

  // supply a tree stream writer which the interpreter will use
  // to record the concrete path (as a stream of '0' and '1' bytes).
  virtual void setPathWriter(TreeStreamWriter *tsw) = 0;
  virtual void setPathDataRecWriter(TreeStreamWriter *tsw) = 0;

  // supply a tree stream writer which the interpreter will use
  // to record the symbolic path (as a stream of '0' and '1' bytes).
  virtual void setSymbolicPathWriter(TreeStreamWriter *tsw) = 0;

  // supply a tree stream writer which the interpreter will use
  // to record the stack path (as a stream of string, each contains a full text-version stack)
  virtual void setStackPathWriter(TreeStreamWriter *tsw) = 0;
  
  // supply a tree stream writer which the interpreter will use
  // to record the cosntraint path (as a stream of string, each contains a full text-version constraint)
  virtual void setConsPathWriter(TreeStreamWriter *tsw) = 0;
  
  // supply a tree stream writer which the interpreter will use
  // to record the statics path
  virtual void setStatsPathWriter(TreeStreamWriter *tsw) = 0;

  // supply a test case to replay from. this can be used to drive the
  // interpretation down a user specified path. use null to reset.
  virtual void setReplayKTest(const struct KTest *out) = 0;

  // supply a list of branch decisions specifying which direction to
  // take on forks. this can be used to drive the interpretation down
  // a user specified path. use null to reset.
  virtual void setReplayPath(const std::vector<PathEntry> *path) = 0;
  virtual void setReplayDataRecEntries(const std::vector<DataRecEntry> *datarec) = 0;

  // supply a set of symbolic bindings that will be used as "seeds"
  // for the search. use null to reset.
  virtual void useSeeds(const std::vector<struct KTest *> *seeds) = 0;

  virtual void runFunctionAsMain(llvm::Function *f,
                                 int argc,
                                 char **argv,
                                 char **envp) = 0;

  /*** Runtime options ***/

  virtual void setHaltExecution(bool value) = 0;

  virtual void setInhibitForking(bool value) = 0;

  virtual void prepareForEarlyExit() = 0;

  /// request the Interpreter to dump information for debug purpose
  /// The information may not be dumpped right after requested.
  /// When will the request be served depends on derived class implementation.
  virtual void requestInfo() = 0;

  /*** State accessor methods ***/

  virtual unsigned getPathStreamID(const ExecutionState &state) = 0;
  virtual unsigned getPathDataRecStreamID(const ExecutionState &state) = 0;

  virtual unsigned getSymbolicPathStreamID(const ExecutionState &state) = 0;

  virtual unsigned getStackPathStreamID(const ExecutionState &state) = 0;

  virtual unsigned getConsPathStreamID(const ExecutionState &state) = 0;

  virtual unsigned getStatsPathStreamID(const ExecutionState &state) = 0;

  virtual void getConstraintLog(const ExecutionState &state,
                                std::string &res,
                                LogType logFormat = STP) = 0;

  virtual bool getSymbolicSolution(const ExecutionState &state,
                                   std::vector<
                                   std::pair<std::string,
                                   std::vector<unsigned char> > >
                                   &res) = 0;

  virtual void getCoveredLines(const ExecutionState &state,
                               std::map<const std::string*, std::set<unsigned> > &res) = 0;
};

} // End klee namespace

#endif /* KLEE_INTERPRETER_H */
