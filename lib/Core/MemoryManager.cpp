//===-- MemoryManager.cpp -------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "MemoryManager.h"

#include "CoreStats.h"
#include "Memory.h"

#include "klee/Expr/Expr.h"
#include "klee/Internal/Support/ErrorHandling.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/MathExtras.h"

#include <inttypes.h>
#include <sys/mman.h>

using namespace klee;

namespace {

llvm::cl::OptionCategory MemoryCat("Memory management options",
                                   "These options control memory management.");

llvm::cl::opt<bool> DeterministicAllocation(
    "allocate-determ",
    llvm::cl::desc("Allocate memory deterministically (default=false)"),
    llvm::cl::init(false), llvm::cl::cat(MemoryCat));

llvm::cl::opt<bool> NullOnZeroMalloc(
    "return-null-on-zero-malloc",
    llvm::cl::desc("Returns NULL if malloc(0) is called (default=false)"),
    llvm::cl::init(false), llvm::cl::cat(MemoryCat));

llvm::cl::opt<unsigned> RedzoneSize(
    "redzone-size",
    llvm::cl::desc("Set the size of the redzones to be added after each "
                   "allocation (in bytes). This is important to detect "
                   "out-of-bounds accesses (default=10)"),
    llvm::cl::init(10), llvm::cl::cat(MemoryCat));

llvm::cl::opt<unsigned long long> DeterministicStartAddress(
    "allocate-determ-start-address",
    llvm::cl::desc("Start address for deterministic allocation. Has to be page "
                   "aligned (default=0x7ff30000000)"),
    llvm::cl::init(0x7ff30000000), llvm::cl::cat(MemoryCat));

llvm::cl::opt<unsigned long long> UnDeterministicStartAddress(
    "allocate-undeterm-start-address",
    llvm::cl::desc("Start address for undeterministic allocation. Has to be page "
                   "aligned (default=0x80f30000000)"),
    llvm::cl::init(0x80f30000000), llvm::cl::cat(MemoryCat));
} // namespace

/***/
MemoryManager::MemoryManager(ArrayCache *_arrayCache)
    : arrayCache(_arrayCache) {
  if (DeterministicAllocation) {
    // Page boundary
    void *determ_expectedAddress = (void *)DeterministicStartAddress.getValue();
    void *undeterm_expectedAddress = (void *)UnDeterministicStartAddress.getValue();
    determ_msp = create_mspace(0, 1, determ_expectedAddress);
    undeterm_msp = create_mspace(0, 1, undeterm_expectedAddress);
  }
  else {
    determ_msp = NULL;
    undeterm_msp = NULL;
  }
}

MemoryManager::~MemoryManager() {
  while (!objects.empty()) {
    MemoryObject *mo = *objects.begin();
    if (!mo->isFixed && !DeterministicAllocation)
      free((void *)mo->address);
    objects.erase(mo);
    delete mo;
  }

  if (DeterministicAllocation) {
    destroy_mspace(determ_msp);
    destroy_mspace(undeterm_msp);
    determ_msp = NULL;
    undeterm_msp = NULL;
  }
}
// Note: I think isLocal == true means this allocation is on stack (e.g. alloca instruction)
//   isLocal == false means this is not on stack (on heap, e.g. calling malloc)
// However I am not sure what does isGlobal mean. It seems like isGlobal == true
//   means allocation unrelated to application itself.
// To make allocations inside application as determistic as possible,
//   memory allocations inside POSIX are also considered as stack allocation.
// Param isDeterm denotes if this allocation should be deterministic
// Note that allocations inside POSIX are undeterministic since extra POSIX code
// is responsible to simulate symbolic behaviours.
MemoryObject *MemoryManager::allocate(uint64_t size, bool isLocal,
                                      bool isGlobal,
                                      const llvm::Value *allocSite,
                                      size_t alignment, bool isDeterm) {
  if (size > 10 * 1024 * 1024)
    klee_warning_once(0, "Large alloc: %" PRIu64
                         " bytes.  KLEE may run out of memory.",
                      size);

  // Return NULL if size is zero, this is equal to error during allocation
  if (NullOnZeroMalloc && size == 0)
    return 0;

  if (!llvm::isPowerOf2_64(alignment)) {
    klee_warning("Only alignment of power of two is supported");
    return 0;
  }

  uint64_t address = 0;
  if (DeterministicAllocation) {
    // Handle the case of 0-sized allocations as 1-byte allocations.
    // This way, we make sure we have this allocation between its own red zones
    size_t alloc_size = std::max(size, (uint64_t)1);
    // use dlmalloc to allocate in preserved virtual address space.
    int ret;
    if (!isDeterm) {
      ret = mspace_posix_memalign(undeterm_msp, (void **)&address, alignment, alloc_size);
    }
    else {
      ret = mspace_posix_memalign(determ_msp, (void **)&address, alignment, alloc_size);
    }
    if (ret) { // non-zero means error appears, let us check error value
      if (ret == EINVAL) {
        klee_warning("Could not allocate %lu bytes with alignment %lu: EINVAL", size, alignment);
      }
      else if (ret == ENOMEM) {
        klee_warning("Could not allocate %lu bytes with alignment %lu: ENOMEN", size, alignment);
      }
      address = 0;
    }
  } else {
    // Use malloc for the standard case
    if (alignment <= 8) {
      address = (uint64_t)malloc(size);
    }
    else {
      int res = posix_memalign((void **)&address, alignment, size);
      if (res < 0) {
        klee_warning("Allocating aligned memory failed.");
        address = 0;
      }
    }
  }

  if (!address) {
    return 0;
  }

  ++stats::allocations;
  MemoryObject *res = new MemoryObject(address, size, isLocal, isGlobal,
                        /*isFixed*/false, isDeterm, allocSite, this);
  objects.insert(res);
  return res;
}

MemoryObject *MemoryManager::allocateFixed(uint64_t address, uint64_t size,
                                           const llvm::Value *allocSite) {
#ifndef NDEBUG
  for (objects_ty::iterator it = objects.begin(), ie = objects.end(); it != ie;
       ++it) {
    MemoryObject *mo = *it;
    if (address + size > mo->address && address < mo->address + mo->size)
      klee_error("Trying to allocate an overlapping object");
  }
#endif

  ++stats::allocations;
  MemoryObject *res =
      new MemoryObject(address, size, /*isLocal*/false, /*isGlobal*/true,
            /*isFixed*/true, /*isDeterm*/false, allocSite, this);
  objects.insert(res);
  return res;
}

void MemoryManager::deallocate(const MemoryObject *mo) { assert(0); }

void MemoryManager::markFreed(MemoryObject *mo) {
  if (objects.find(mo) != objects.end()) {
    if (!mo->isFixed) {
      if (DeterministicAllocation) {
        // managed by dlmalloc
        if (mo->isDeterm) {
          // was allocated deterministically (e.g. stack/heap inside application)
          mspace_free(determ_msp, (void *)mo->address);
        }
        else {
          // was allocated undeterministically (e.g. stack/heap inside POSIX, isGlobal)
          mspace_free(undeterm_msp, (void *)mo->address);
        }
      }
      else {
        // managed by system malloc
        if (mo->address >= UnDeterministicStartAddress && (mo->address < (UnDeterministicStartAddress + (1L << 36)))) {
          klee_warning("Free Undeterm dlmalloc with tcmalloc");
        }
        free((void *)mo->address);
      }
    }
    objects.erase(mo);
  }
}

size_t MemoryManager::getUsedDeterministicSize() {
  if (DeterministicAllocation) {
    struct mallinfo determ_mi = mspace_mallinfo(determ_msp);
    struct mallinfo undeterm_mi = mspace_mallinfo(undeterm_msp);
    return (determ_mi.uordblks + determ_mi.hblkhd) +
           (undeterm_mi.uordblks + undeterm_mi.hblkhd);
  }
  else {
    return 0;
  }
}
