//===-- MemoryManager.h -----------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_MEMORYMANAGER_H
#define KLEE_MEMORYMANAGER_H

#include <cstddef>
#include <set>
#include <cstdint>
#include <malloc.h>

#include "dlmalloc.h"

namespace llvm {
class Value;
}

namespace klee {
class MemoryObject;
class ArrayCache;

class MemoryManager {
private:
  typedef std::set<MemoryObject *> objects_ty;
  objects_ty objects;
  ArrayCache *const arrayCache;

  // When enabling deterministic allocation,
  // "determistic" not only means
  //   1) a certain alloc request with given size should be assigned to the
  //   same address across record and replay
  // but also means:
  //   2) that alloc request is guaranteed to happen in both record and replay
  //   with the same size.
  // "determ_msp" is the allocator for requests satisfing 2). (e.g. allocs in application)
  // "undeterm_msp" is the allocator for other requests. (e.g. allocs in POSIX)
  // I distinguish these allocations to make allocs (either stack or heap)
  //   inside applications as deterministic as possible. At the same time, there
  //   are always differences between record and replay (e.g.  extra POSIX code
  //   is responsible to simulate symbolic behaviour), which are impossible to
  //   be deterministic.
  mspace determ_msp;
  mspace undeterm_msp;

public:
  MemoryManager(ArrayCache *arrayCache);
  ~MemoryManager();

  /**
   * Returns memory object which contains a handle to real virtual process
   * memory.
   */
  MemoryObject *allocate(uint64_t size, bool isLocal, bool isGlobal,
                         const llvm::Value *allocSite, size_t alignment,
                         bool isInPOSIX);
  MemoryObject *allocateFixed(uint64_t address, uint64_t size,
                              const llvm::Value *allocSite);
  void deallocate(const MemoryObject *mo);
  void markFreed(MemoryObject *mo);
  ArrayCache *getArrayCache() const { return arrayCache; }

  /*
   * Returns the size used by deterministic allocation in bytes
   */
  size_t getUsedDeterministicSize();
};

} // End klee namespace

#endif /* KLEE_MEMORYMANAGER_H */
