//===-- Executor.h ----------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Class to perform actual execution, hides implementation details from external
// interpreter.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_EXECUTOR_H
#define KLEE_EXECUTOR_H

#include "klee/ExecutionState.h"
#include "klee/Expr/ArrayCache.h"
#include "klee/Internal/Module/Cell.h"
#include "klee/Internal/Module/KInstruction.h"
#include "klee/Internal/Module/KModule.h"
#include "klee/Internal/System/Time.h"
#include "klee/Interpreter.h"
#include "klee/util/OracleEvaluator.h"

//#include "../Solver/QueryLoggingSolver.h"

#include "llvm/ADT/Twine.h"
#include "llvm/Support/raw_ostream.h"

#include "../Expr/ArrayExprOptimizer.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

struct KTest;

namespace llvm {
  class BasicBlock;
  class BranchInst;
  class CallInst;
  class Constant;
  class ConstantExpr;
  class Function;
  class GlobalValue;
  class Instruction;
  class LLVMContext;
  class DataLayout;
  class Twine;
  class Value;
}

namespace klee {
  class Array;
  struct Cell;
  class ExecutionState;
  class ExternalDispatcher;
  class Expr;
  class InstructionInfoTable;
  struct KFunction;
  struct KInstruction;
  class KInstIterator;
  class KModule;
  class MemoryManager;
  class MemoryObject;
  class ObjectState;
  class PTree;
  class Searcher;
  class SeedInfo;
  class SpecialFunctionHandler;
  struct StackFrame;
  class StatsTracker;
  class TimingSolver;
  class TreeStreamWriter;
  class MergeHandler;
  class MergingSearcher;
  template<class T> class ref;



  /// \todo Add a context object to keep track of data only live
  /// during an instruction step. Should contain addedStates,
  /// removedStates, and haltExecution, among others.

class Executor : public Interpreter {
  friend class RandomPathSearcher;
  friend class OwningSearcher;
  friend class WeightedRandomSearcher;
  friend class SpecialFunctionHandler;
  friend class StatsTracker;
  friend class MergeHandler;

public:
  typedef std::pair<ExecutionState*,ExecutionState*> StatePair;

  enum TerminateReason {
    Abort,
    Assert,
    BadVectorAccess,
    Exec,
    External,
    Free,
    Model,
    Overflow,
    Ptr,
    ReadOnly,
    ReportError,
    User,
    Unhandled,
    ReplayPath,
    Timeout,
  };

private:
  static const char *TerminateReasonNames[];

  class TimerInfo;

  std::unique_ptr<KModule> kmodule;
  InterpreterHandler *interpreterHandler;
  Searcher *searcher;

  ExternalDispatcher *externalDispatcher;
  TimingSolver *solver;
  MemoryManager *memory;
  std::set<ExecutionState*> states;
  StatsTracker *statsTracker;
  TreeStreamWriter *pathWriter, *pathDataRecWriter, *symPathWriter;
  TreeStreamWriter *stackPathWriter, *consPathWriter, *statsPathWriter;
  SpecialFunctionHandler *specialFunctionHandler;
  TimerGroup timers;
  std::unique_ptr<PTree> processTree;

  /// Used to track states that have been added during the current
  /// instructions step.
  /// \invariant \ref addedStates is a subset of \ref states.
  /// \invariant \ref addedStates and \ref removedStates are disjoint.
  std::vector<ExecutionState *> addedStates;
  /// Used to track states that have been removed during the current
  /// instructions step.
  /// \invariant \ref removedStates is a subset of \ref states.
  /// \invariant \ref addedStates and \ref removedStates are disjoint.
  std::vector<ExecutionState *> removedStates;

  /// When non-empty the Executor is running in "seed" mode. The
  /// states in this map will be executed in an arbitrary order
  /// (outside the normal search interface) until they terminate. When
  /// the states reach a symbolic branch then either direction that
  /// satisfies one or more seeds will be added to this map. What
  /// happens with other states (that don't satisfy the seeds) depends
  /// on as-yet-to-be-determined flags.
  std::map<ExecutionState*, std::vector<SeedInfo> > seedMap;

  /// Map of globals to their representative memory object.
  std::map<const llvm::GlobalValue*, MemoryObject*> globalObjects;

  /// Map of globals to their bound address. This also includes
  /// globals that have no representative object (i.e. functions).
  std::map<const llvm::GlobalValue*, ref<ConstantExpr> > globalAddresses;

  /// The set of legal function addresses, used to validate function
  /// pointers. We use the actual Function* address as the function address.
  std::set<uint64_t> legalFunctions;

  /// When non-null the bindings that will be used for calls to
  /// klee_make_symbolic in order replay.
  const struct KTest *replayKTest;

  /// When non-null, this evaluator knows all inputs of symbolic objects
  OracleEvaluator *oracle_eval;

  /// When non-null a list of branch decisions to be used for replay.
  const std::vector<PathEntry> *replayPath;
  const std::vector<DataRecEntry> *replayDataRecEntries;

  /// The index into the current \ref replayKTest or \ref replayPath
  /// object. (moved inside ExecutionState, since we might replay multiple states at the same time)
  /// unsigned replayPosition;

  /// When non-null a list of "seed" inputs which will be used to
  /// drive execution.
  const std::vector<struct KTest *> *usingSeeds;

  /// Disables forking, instead a random path is chosen. Enabled as
  /// needed to control memory usage. \see fork()
  bool atMemoryLimit;

  /// Disables forking, set by client. \see setInhibitForking()
  bool inhibitForking;

  /// Signals the executor to halt execution at the next instruction
  /// step.
  bool haltExecution;

  /// Whether implied-value concretization is enabled. Currently
  /// false, it is buggy (it needs to validate its writes).
  bool ivcEnabled;

  /// The maximum time to allow for a single core solver query.
  /// (e.g. for a single STP query)
  time::Span coreSolverTimeout;

  /// Maximum time to allow for a single instruction.
  time::Span maxInstructionTime;

  /// Assumes ownership of the created array objects
  ArrayCache arrayCache;

  /// File to print executed instructions to
  std::unique_ptr<llvm::raw_ostream> debugInstFile;

  // @brief Buffer used by logBuffer
  std::string debugBufferString;

  // @brief buffer to store logs before flushing to file
  llvm::raw_string_ostream debugLogBuffer;

  // @brief if printInfo is requested
  bool info_requested;

  /// Optimizes expressions
  ExprOptimizer optimizer;

  /// Points to the merging searcher of the searcher chain,
  /// `nullptr` if merging is disabled
  MergingSearcher *mergingSearcher = nullptr;

  llvm::Function* getTargetFunction(llvm::Value *calledVal,
                                    ExecutionState &state);

  void executeInstruction(ExecutionState &state, KInstruction *ki);

  void run(ExecutionState &initialState);

  // Given a concrete object in our [klee's] address space, add it to
  // objects checked code can reference.
  MemoryObject *addExternalObject(ExecutionState &state, void *addr,
                                  unsigned size, bool isReadOnly);

  void initializeGlobalObject(ExecutionState &state, ObjectState *os,
			      const llvm::Constant *c,
			      unsigned offset);
  void initializeGlobals(ExecutionState &state);

  void stepInstruction(ExecutionState &state);
  void updateStates(ExecutionState *current);
  void transferToBasicBlock(const llvm::BasicBlock *dst,
			    llvm::BasicBlock *src,
			    ExecutionState &state);

  void callExternalFunction(ExecutionState &state,
                            KInstruction *target,
                            llvm::Function *function,
                            std::vector< ref<Expr> > &arguments);

  ObjectState *bindObjectInState(ExecutionState &state, const MemoryObject *mo,
                                 bool isLocal, const Array *array = 0);

  /// Resolve a pointer to the memory objects it could point to the
  /// start of, forking execution when necessary and generating errors
  /// for pointers to invalid locations (either out of bounds or
  /// address inside the middle of objects).
  ///
  /// \param results[out] A list of ((MemoryObject,ObjectState),
  /// state) pairs for each object the given address can point to the
  /// beginning of.
  typedef std::vector< std::pair<std::pair<const MemoryObject*, const ObjectState*>,
                                 ExecutionState*> > ExactResolutionList;
  void resolveExact(ExecutionState &state,
                    ref<Expr> p,
                    ExactResolutionList &results,
                    const std::string &name);

  /// Allocate and bind a new object in a particular state. NOTE: This
  /// function may fork.
  ///
  /// \param isLocal Flag to indicate if the object should be
  /// automatically deallocated on function return (this also makes it
  /// illegal to free directly).
  ///
  /// \param target Value at which to bind the base address of the new
  /// object.
  ///
  /// \param reallocFrom If non-zero and the allocation succeeds,
  /// initialize the new object from the given one and unbind it when
  /// done (realloc semantics). The initialized bytes will be the
  /// minimum of the size of the old and new objects, with remaining
  /// bytes initialized as specified by zeroMemory.
  ///
  /// \param allocationAlignment If non-zero, the given alignment is
  /// used. Otherwise, the alignment is deduced via
  /// Executor::getAllocationAlignment
  void executeAlloc(ExecutionState &state,
                    ref<Expr> size,
                    bool isLocal,
                    KInstruction *target,
                    bool zeroMemory=false,
                    const ObjectState *reallocFrom=0,
                    size_t allocationAlignment=0);

  /// Force return the requested allocation size.
  /// No matter it was deterministically allocated or not.
  void executeMallocUsableSize(ExecutionState &state,
                               ref<Expr> address,
                               KInstruction *target);

  /// Free the given address with checking for errors. If target is
  /// given it will be bound to 0 in the resulting states (this is a
  /// convenience for realloc). Note that this function can cause the
  /// state to fork and that \ref state cannot be safely accessed
  /// afterwards.
  void executeFree(ExecutionState &state,
                   ref<Expr> address,
                   KInstruction *target = 0);

  /// NOTE: ki could be null if this function call is "pthread_exit" created by
  /// the Instruction::Ret of a thread
  void executeCall(ExecutionState &state,
                   KInstruction *ki,
                   llvm::Function *f,
                   std::vector< ref<Expr> > &arguments);

  // do address resolution / object binding / out of bounds checking
  // and perform the operation
  // \param[in] force: whether we should force overwritting a read only object.
  // This is only meant to use when loading data from the trace
  void
  executeMemoryOperation(ExecutionState &state, bool isWrite, ref<Expr> address,
                         ref<Expr> value /* undef if read */,
                         KInstruction *target /* undef if write */,
                         bool force = false /* overwrite read only object */);

  void executeMakeSymbolic(ExecutionState &state, const MemoryObject *mo,
                           const std::string &name);

  /// Create a new state where each input condition has been added as
  /// a constraint and return the results. The input state is included
  /// as one of the results. Note that the output vector may included
  /// NULL pointers for states which were unable to be created.
  void branch(ExecutionState &state,
              const std::vector< ref<Expr> > &conditions,
              std::vector<ExecutionState*> &result);

  // Fork current and return states in which condition holds / does
  // not hold, respectively. One of the states is necessarily the
  // current state, and one of the states may be null.
  StatePair fork(ExecutionState &current, ref<Expr> condition, bool isInternal);

  /// Add the given (boolean) condition as a constraint on state. This
  /// function is a wrapper around the state's addConstraint function
  /// which also manages propagation of implied values,
  /// validity checks, and seed patching.
  /// return: true if condition could be True, otherwise false
  bool addConstraint(ExecutionState &state, ref<Expr> condition);

  // Called on [for now] concrete reads, replaces constant with a symbolic
  // Used for testing.
  ref<Expr> replaceReadWithSymbolic(ExecutionState &state, ref<Expr> e);

  const Cell& eval(KInstruction *ki, unsigned index,
                   ExecutionState &state) const;

  Cell& getArgumentCell(ExecutionState &state,
                        KFunction *kf,
                        unsigned index) {
    return state.stack().back().locals[kf->getArgRegister(index)];
  }

  Cell& getArgumentCell(StackFrame &sf, KFunction *kf, unsigned index) {
    return sf.locals[kf->getArgRegister(index)];
  }

  Cell& getDestCell(ExecutionState &state,
                    KInstruction *target) {
    return state.stack().back().locals[target->dest];
  }

  void bindLocal(KInstruction *target,
                 ExecutionState &state,
                 ref<Expr> value);
  void bindArgument(KFunction *kf,
                    unsigned index,
                    ExecutionState &state,
                    ref<Expr> value);

  /// Evaluates an LLVM constant expression.  The optional argument ki
  /// is the instruction where this constant was encountered, or NULL
  /// if not applicable/unavailable.
  ref<klee::ConstantExpr> evalConstantExpr(const llvm::ConstantExpr *c,
					   const KInstruction *ki = NULL);

  /// Evaluates an LLVM constant.  The optional argument ki is the
  /// instruction where this constant was encountered, or NULL if
  /// not applicable/unavailable.
  ref<klee::ConstantExpr> evalConstant(const llvm::Constant *c,
				       const KInstruction *ki = NULL);

  /// Return a unique constant value for the given expression in the
  /// given state, if it has one (i.e. it provably only has a single
  /// value). Otherwise return the original expression.
  ref<Expr> toUnique(const ExecutionState &state, ref<Expr> &e);

  /// Return a constant value for the given expression, forcing it to
  /// be constant in the given state by adding a constraint if
  /// necessary. Note that this function breaks completeness and
  /// should generally be avoided.
  ///
  /// \param purpose An identify string to printed in case of concretization.
  ref<klee::ConstantExpr> toConstant(ExecutionState &state, ref<Expr> e,
                                     const char *purpose);

  /// Bind a constant value for e to the given target. NOTE: This
  /// function may fork state if the state has multiple seeds.
  void executeGetValue(ExecutionState &state, ref<Expr> e, KInstruction *target);

  /// Get textual information regarding a memory address.
  std::string getAddressInfo(ExecutionState &state, ref<Expr> address) const;

  // Determines the \param lastInstruction of the \param state which is not KLEE
  // internal and returns its InstructionInfo
  const InstructionInfo & getLastNonKleeInternalInstruction(const ExecutionState &state,
      llvm::Instruction** lastInstruction);

  bool shouldExitOn(enum TerminateReason termReason);

  // remove state from queue and delete
  void terminateState(ExecutionState &state);
  // call exit handler and terminate state
  void terminateStateEarly(ExecutionState &state, const llvm::Twine &message);
  // call exit handler and terminate state
  void terminateStateOnExit(ExecutionState &state);
  // call error handler and terminate state
  void terminateStateOnError(ExecutionState &state, const llvm::Twine &message,
                             enum TerminateReason termReason,
                             const char *suffix = NULL,
                             const llvm::Twine &longMessage = "");

  // call error handler and terminate state, for execution errors
  // (things that should not be possible, like illegal instruction or
  // unlowered instrinsic, or are unsupported, like inline assembly)
  void terminateStateOnExecError(ExecutionState &state,
                                 const llvm::Twine &message,
                                 const llvm::Twine &info="") {
    terminateStateOnError(state, message, Exec, NULL, info);
  }
  void exitOnSolverTimeout(ExecutionState &state, const llvm::Twine &message) {
    terminateStateOnError(state, message, Timeout);
    interpreterHandler->reportInEngineTime();
    std::exit(0);
  }

  /// bindModuleConstants - Initialize the module constant table.
  void bindModuleConstants();

  template <typename TypeIt>
  void computeOffsets(KGEPInstruction *kgepi, TypeIt ib, TypeIt ie);

  /// bindInstructionConstants - Initialize any necessary per instruction
  /// constant values.
  void bindInstructionConstants(KInstruction *KI);

  void doImpliedValueConcretization(ExecutionState &state,
                                    ref<Expr> e,
                                    ref<ConstantExpr> value);

  void checkMemoryUsage();
  void printDebugInstructions(ExecutionState &state);
  void doDumpStates();
  void dumpStateAtBranch(ExecutionState &state, PathEntry pe, ref<Expr> new_constraint);
  void dumpStateAtFork(ExecutionState &current, ref<Expr> condition);
  /// Record a single bit based on Solver Validity result.
  ///
  /// That bit represents a branch decision (true if taken, false if not taken)
  /// \param[in] current Record to the pathOS of this ExecutionState
  /// \param[in] solvalid The solver validity result (means "must be" True or False) returned from a solver query
  void record1BitAtFork(ExecutionState &current, Solver::Validity solvalid);

  static inline void getConstraintFromBool(ref<Expr> condition, ref<Expr> &new_constraint, Solver::Validity &res, bool br) {
    if (br) {
      res = Solver::True;
      new_constraint = condition;
    }
    else {
      res = Solver::False;
      new_constraint = Expr::createIsZero(condition);
    }
  }

  void printInfo(llvm::raw_ostream &os);

  /// Only for debug purposes; enable via debugger or klee-control
  void dumpStates();
  void dumpPTree();

  /* Multi-threading related function */
  // Pthread Create needs to specify a new StackFrame instead of just using the
  // current thread's stack
  void bindArgumentToPthreadCreate(KFunction *kf, unsigned index,
                                   StackFrame &sf, ref<Expr> value);
  // Schedule what threads to execute next in the given ExecutionState. There
  // are 3 sceharios to consider:
  // 1. One thread is terminated (enabled=false, yield=false)
  // 2. One thread is not terminated but proactively yield (enabled=true,
  // yield=true)
  // 3. One thread is not terminated nor yielding, but preempted.
  // @return if schedule successfully
  // NOTE: For unknown reason, the cloud9 code base allow you just
  // schedule one possible next thread in case 1 and 2. But you have to
  // fork to iterate all possible schedules in case 3.
  // NOTE: I have not implemented forking upon schedule, I assume you can
  // only choose one possible next thread for case 1,2. And do nothing for case
  // 3.
  // NOTE: I have not implemented schedule recording, which may be required
  // during replay later.
  bool schedule(ExecutionState &state, bool yield);
  void executeThreadCreate(ExecutionState &state, thread_id_t tid,
                           ref<Expr> start_function, ref<Expr> arg);
  void executeThreadExit(ExecutionState &state);

public:

  Executor(llvm::LLVMContext &ctx, const InterpreterOptions &opts,
      InterpreterHandler *ie);
  virtual ~Executor();

  const InterpreterHandler& getHandler() {
    return *interpreterHandler;
  }

  void setPathWriter(TreeStreamWriter *tsw) override { pathWriter = tsw; }
  void setPathDataRecWriter(TreeStreamWriter *tsw) override { pathDataRecWriter = tsw; }

  void setStackPathWriter(TreeStreamWriter *tsw) override { stackPathWriter = tsw; }

  void setSymbolicPathWriter(TreeStreamWriter *tsw) override {
    symPathWriter = tsw;
  }
  
  void setConsPathWriter(TreeStreamWriter *tsw) override { 
    consPathWriter = tsw; 
  }
  
  void setStatsPathWriter(TreeStreamWriter *tsw) override { 
    statsPathWriter = tsw; 
  }

  void setReplayKTest(const struct KTest *out) override {
    assert(!replayPath && "cannot replay both buffer and path");
    replayKTest = out;
  }

  void setReplayPath(const std::vector<PathEntry> *path) override {
    assert(!replayKTest && "cannot replay both buffer and path");
    replayPath = path;
  }

  void setReplayDataRecEntries(const std::vector<DataRecEntry> *datarec) override {
    assert(!replayKTest && "cannot replay both buffer and path");
    replayDataRecEntries = datarec;
  }

  /// Try load the value of a given KInstuction from recorded path file
  /// \param[out] true if given KInst is loaded successfully
  bool tryLoadDataRecording(ExecutionState &state, KInstruction *KI);
  /// use a given constant value to concretize the result of a given
  /// KInstruction.
  void concretizeKInst(ExecutionState &state, KInstruction *KI,
      ref<ConstantExpr> loadedValue, bool writeMem);
  /// Record given KInstruction if it is selected to do so.
  /// \param[out] true if given KInst is recorded successfully
  bool tryStoreDataRecording(ExecutionState &state, KInstruction *KI);

  // Read next PathEntry using (and advancing) the cursor in state
  void getNextPathEntry(ExecutionState &state, PathEntry &pe) {
    assert(replayPath && "Trying to get next PathEntry without a valud replayPath");
    assert(state.replayPosition < replayPath->size() && "replayPath exhausts too early");
    pe = (*replayPath)[state.replayPosition++];
  }

  void getNextDataRecEntry(ExecutionState &state, DataRecEntry &dre) {
    assert(replayDataRecEntries && "Trying to get next DataRecEntry without a valid replayDataRecEntries");
    assert(state.replayDataRecEntriesPosition < replayDataRecEntries->size() && "replayDataRecEntries exhausts too early");
    dre = (*replayDataRecEntries)[state.replayDataRecEntriesPosition++];
  }

  llvm::Module *setModule(std::vector<std::unique_ptr<llvm::Module>> &modules,
                          const ModuleOptions &opts) override;

  void useSeeds(const std::vector<struct KTest *> *seeds) override {
    usingSeeds = seeds;
  }

  void runFunctionAsMain(llvm::Function *f, int argc, char **argv,
                         char **envp) override;

  /*** Runtime options ***/

  void setHaltExecution(bool value) override { haltExecution = value; }

  void setInhibitForking(bool value) override { inhibitForking = value; }

  void prepareForEarlyExit() override;

  /// called outside, request the Interpreter to dump information
  /// the request will be served after current instruction executed.
  ///   (before executing next instruction)
  void requestInfo() override {
    info_requested = true;
  }

  /*** State accessor methods ***/

  unsigned getPathStreamID(const ExecutionState &state) override;
  unsigned getPathDataRecStreamID(const ExecutionState &state) override;

  unsigned getSymbolicPathStreamID(const ExecutionState &state) override;

  unsigned getStackPathStreamID(const ExecutionState &state) override;
  
  unsigned getConsPathStreamID(const ExecutionState &state) override;
  
  unsigned getStatsPathStreamID(const ExecutionState &state) override;

  void getConstraintLog(const ExecutionState &state, std::string &res,
                        Interpreter::LogType logFormat =
                            Interpreter::STP) override;

  bool getSymbolicSolution(
      const ExecutionState &state,
      std::vector<std::pair<std::string, std::vector<unsigned char>>> &res)
      override;

  void getCoveredLines(const ExecutionState &state,
                       std::map<const std::string *, std::set<unsigned>> &res)
      override;

  Expr::Width getWidthForLLVMType(llvm::Type *type) const;
  size_t getAllocationAlignment(const llvm::Value *allocSite) const;

  /// Returns the errno location in memory of the state
  int *getErrnoLocation(const ExecutionState &state) const;

  // void writeStackKQueries(std::string& buf);
  /*** Path Recording methods ***/
  /// Assert next recorded branch is taken or not based on symbolic constraints
  ///
  /// \param[in] state The ExecutionState where next branch locates
  /// \param[out] br The branch decision determined by symbolic constraints
  void AssertNextBranchTaken(ExecutionState &state, bool br);

  /// Get constraints enforced by next recorded branch.
  ///
  /// Used when you don't or can't determine this branch via symbolic approach
  /// This function handled two type of record:
  ///   1. Only 1 bit is recorded: will call getConstraintFromBool
  ///   2. Additional bits are recorded: will generate:
  ///       (true && [ (K == C) && ...
  ///       for K, C in all (sym child expr, recorded concrete expr)])
  /// \param[in] state The ExecutionState where next branch locates
  /// \param[in] condition The symbolic expression bound to this branch
  /// \param[out] new_constraint A single expression consists of conjunctions
  /// \param[out] res Represent if next branch is taken (Solver::True) or not
  void getNextBranchConstraint(ExecutionState &state, ref<Expr> condition,
      ref<Expr> &new_constraint, Solver::Validity &res);

  MergingSearcher *getMergingSearcher() const { return mergingSearcher; };
  void setMergingSearcher(MergingSearcher *ms) { mergingSearcher = ms; };
};

} // End klee namespace

#endif /* KLEE_EXECUTOR_H */
