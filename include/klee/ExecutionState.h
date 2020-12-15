//===-- ExecutionState.h ----------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_EXECUTIONSTATE_H
#define KLEE_EXECUTIONSTATE_H

#include "klee/Expr/Constraints.h"
#include "klee/Expr/Expr.h"
#include "klee/Internal/ADT/TreeStream.h"
#include "klee/Internal/System/Time.h"
#include "klee/MergeHandler.h"
#include "klee/Threading.h"

// FIXME: We do not want to be exposing these? :(
#include "../../lib/Core/AddressSpace.h"
#include "klee/Internal/Module/KInstIterator.h"

#include <map>
#include <set>
#include <vector>
#include <string>

namespace klee {
class Array;
class PTreeNode;
struct InstructionInfo;

llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const MemoryMap &mm);

/// @brief ExecutionState representing a path under exploration
class ExecutionState {
public:
  typedef std::map<thread_uid_t, Thread> threads_ty;
  typedef std::map<wlist_id_t, std::set<thread_uid_t>> wlists_ty;
  typedef Thread::stack_ty stack_ty;

private:
  // unsupported, use copy constructor
  ExecutionState &operator=(const ExecutionState &);
  // for initialization
  void setupMain(KFunction *kf);
  void setupTime();

public:
  // Execution - Control Flow specific (include multi-threading)
  // FIXME: should I make them private?
  threads_ty threads;
  wlists_ty waitingLists;
  // used to allocate new wlist_id
  wlist_id_t wlistCounter;
  // logical timestamp, each instruction takes one unit time
  uint64_t stateTime;
  threads_ty::iterator crtThreadIt;

  // Overall state of the state - Data specific

  /// @brief Address space used by this state (e.g. Global and Heap)
  AddressSpace addressSpace;

  /// @brief Constraints collected so far
  ConstraintManager constraints;

  /// Statistics and information

  /// @brief Costs for all queries issued for this state, in seconds
  mutable time::Span queryCost;
  mutable time::Span fork_queryCost;
  mutable time::Span prev_fork_queryCost;
  mutable time::Span prev_fork_queryCost_single;

  /// Whether Executor executes into __user_main
  /// It influences whether we should record/replay execution path
  /// This is a process level state (not thread level)
  bool isInUserMain;

  /// @brief Exploration depth, i.e., number of times KLEE branched for this state
  unsigned depth;

  /// @brief History of complete path: represents branches taken to
  /// reach/create this state (both concrete and symbolic)
  TreeOStream pathOS;
  TreeOStream pathDataRecOS;

  /// @brief History of stack for each branch decision: recording entire stack
  //  when each brach decision (both concrete and symbolic) is made
  TreeOStream stackPathOS;
  
  /// @brief History of constriant for each branch decision
  //  when each brach decision (both concrete and symbolic) is made
  TreeOStream consPathOS;

  /// @brief History of symbolic path: represents symbolic branches
  /// taken to reach/create this state
  TreeOStream symPathOS;
  
  /// @brief History of stats for each branch decision
  //  when each brach decision (both concrete and symbolic) is made
  TreeOStream statsPathOS;

  /// @brief Counts how many instructions were executed since the last new
  /// instruction was covered.
  unsigned instsSinceCovNew;

  /// @brief Whether a new instruction was covered in this state
  bool coveredNew;

  /// @brief Disables forking for this state. Set by user code
  bool forkDisabled;

  /// The index into the current \ref replayKTest or \ref replayPath
  /// object.
  unsigned replayPosition;
  unsigned replayDataRecEntriesPosition;
  /// The number of branches recorded
  /// regardless of fork or switch or indirectbr, symbolic or concrete
  ///   should record or not (isInPosix, isInUserMain)
  unsigned nbranches_rec;

  /// @brief Set containing which lines in which files are covered by this state
  std::map<const std::string *, std::set<unsigned> > coveredLines;

  /// @brief Pointer to the process tree of the current state
  PTreeNode *ptreeNode;

  /// @brief Ordered list of symbolics: used to generate test cases.
  //
  // FIXME: Move to a shared list structure (not critical).
  std::vector<std::pair<ref<const MemoryObject>, const Array *>> symbolics;

  /// @brief Set of used array names for this state.  Used to avoid collisions.
  std::set<std::string> arrayNames;

  // The objects handling the klee_open_merge calls this state ran through
  std::vector<ref<MergeHandler> > openMergeStack;

  // The numbers of times this state has run through Executor::stepInstruction
  std::uint64_t steppedInstructions;

private:
  ExecutionState() : replayPosition(0), replayDataRecEntriesPosition(0), nbranches_rec(0), ptreeNode(0) {}

public:
  ExecutionState(KFunction *kf);

  // XXX total hack, just used to make a state so solver can
  // use on structure
  ExecutionState(const Constraints_ty &assumptions);

  // constructor implemention
  ExecutionState(const ExecutionState &state);

  ~ExecutionState();

  ExecutionState *branch();

  void addSymbolic(const MemoryObject *mo, const Array *array);
  /// return true if e could be True
  bool addConstraint(ref<Expr> e) { return constraints.addConstraint(e); }

  bool merge(const ExecutionState &b);
  void pushFrame(KInstIterator caller, KFunction *kf) {
    pushFrame(crtThread(), caller, kf);
  }
  void pushFrame(Thread &t, KInstIterator caller, KFunction *kf);
  void popFrame() { popFrame(crtThread()); }
  void popFrame(Thread &t);

  /* Multi-threading related function */

  Thread &createThread(thread_id_t tid, KFunction *kf);
  void terminateThread(threads_ty::iterator it);
  threads_ty::iterator nextThread(threads_ty::iterator it) {
    if (it != threads.end())
      ++it;
    if (it == threads.end())
      it = threads.begin();
    return it;
  }
  void scheduleNext(threads_ty::iterator it) {
    assert(it != threads.end());
    crtThreadIt = it;
  }
  wlist_id_t getWaitingList() { return wlistCounter++; }
  void sleepThread(wlist_id_t wlist);
  void notifyOne(wlist_id_t wlist, thread_uid_t tid);
  void notifyAll(wlist_id_t wlist);

  /* Debugging helper */
  void dumpConstraints(llvm::raw_ostream &out) const;
  void dumpConstraints() const;
  void dumpStack(llvm::raw_ostream &out) const;
  void dumpStack() const;
  void dumpStackPathOS();
  void dumpStatsPathOS();
  void dumpConsPathOS(const std::string &cons);

  /* Shortcut methods */
  Thread &crtThread() { return crtThreadIt->second; }
  const Thread &crtThread() const { return crtThreadIt->second; }
  KInstIterator &pc() { return crtThread().pc; }
  const KInstIterator &pc() const { return crtThread().pc; }
  KInstIterator &prevPC() { return crtThread().prevPC; }
  const KInstIterator &prevPC() const { return crtThread().prevPC; }
  stack_ty &stack() { return crtThread().stack; }
  const stack_ty &stack() const { return crtThread().stack; }
  bool isInPOSIX() const { return crtThread().isInPOSIX; }
  bool isInLIBC() const { return crtThread().isInLIBC; }
  unsigned &incomingBBIndex() { return crtThread().incomingBBIndex; }
  unsigned incomingBBIndex() const { return crtThread().incomingBBIndex; }
  inline bool shouldRecord() const {
    return isInUserMain && !isInPOSIX();
  }
  inline bool isInTargetProgram() const {
    return isInUserMain && !isInPOSIX() && !isInLIBC();
  }
};
}

#endif /* KLEE_EXECUTIONSTATE_H */
