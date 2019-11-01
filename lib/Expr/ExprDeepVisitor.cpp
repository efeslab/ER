#include "klee/Common.h"
#include "klee/Config/Version.h"
#include "klee/Expr.h"
#include "klee/Internal/Module/KInstruction.h"
#include "klee/Internal/Support/ErrorHandling.h"
#include "klee/util/ExprPPrinter.h"
#include "klee/util/ExprUtil.h"
#include "klee/util/ExprVisitor.h"
#include "klee/util/ExprDeepVisitor.h"

#include "llvm/IR/Instructions.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Format.h"

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cxxabi.h>
#include <fstream>
#include <iomanip>
#include <iosfwd>
#include <sstream>
#include <string>
#include <sys/mman.h>
#include <vector>

using namespace llvm;
using namespace klee;

void ExprDeepVisitor::cleanUp() {
  old2new.clear();
  farthestUpdates.clear();
}

ExprVisitor::Action ExprDeepVisitor::visitRead
                            (const ReadExpr &re) {
  std::vector<const UpdateNode *> updateNodeArray;
  const UpdateNode *originHead = nullptr;
  for (const UpdateNode *un = re.updates.head; un; un = un->next) {
    auto it = old2new.find(un);
    if (it == old2new.end()) {
      updateNodeArray.push_back(un);
    }
    else {
      originHead = it->second;
      break;
    }
  }

  auto farthest = farthestUpdates.find(re.updates.root->name);
  UpdateList *ul = nullptr;
  if (farthest == farthestUpdates.end()) {
    assert(originHead == nullptr);
    ul = new UpdateList(re.updates.root, nullptr);
    farthestUpdates.insert({re.updates.root->name, ul});
  }
  else {
    ul = farthest->second;
    // FIXME: on going debate.
    // Note that there should be no divergence in any updatelist.
    // So either:
    //   1. originHead is the farthest and we are extending the farthest (updateNodeArray non-empty)
    //   2. originHead is not the farthest. we meet a ReadExpr referencing a shorter updateList (an older version of memory state of the root array)
    if (ul->head != originHead && !updateNodeArray.empty()) {
      klee_warning("farthest != originHead but updateNodeArray is not empty. violating case 2");
    }
  }

  for (auto it = updateNodeArray.rbegin(); it != updateNodeArray.rend(); ++it) {
    const UpdateNode *un = *it;
    updateNodeArray.pop_back();
    auto idx = visit(un->index);
    auto val = visit(un->value);
    // FIXME: on going debate
    // Due to recursion, the updateNodeArray we are processing here might already be processed inside recursion.
    // We currently just check if such repeated updates happen to old2new map.
    assert(old2new.find(un) == old2new.end());
    ul->extend(idx, val, Expr::FLAG_INTERNAL, un->kinst);
    assert(old2new.find(un) == old2new.end());
#if 0
    llvm::errs() << format("%#lx", (uint64_t)un)
            <<  ":{" << format("%#lx", (uint64_t)un->index.get())
            << ", " << format("%#lx", (uint64_t)un->value.get()) << "}"
            << " ==> " << format("%#lx", (uint64_t)ul->head)
            << ":{" << format("%#lx", (uint64_t)ul->head->index.get())
            << ", " << format("%#lx", (uint64_t)ul->head->value.get()) << "}\n";
#endif
    old2new.insert({un, ul->head});
  }

  ref<Expr> idx = visit(re.index);

  if (klee::ConstantExpr *CE = dyn_cast<klee::ConstantExpr>(idx)) {
    uint64_t index = CE->getZExtValue();

    /* Iterate on the update list and search for a match,
     * if there's no match we return a read instead */
    for (const UpdateNode *un = ul->head; un; un=un->next) {
      if (ConstantExpr *ci = dyn_cast<ConstantExpr>(un->index)) {
        if (ci->getZExtValue() == index)
          return Action::changeTo(un->value);
      }
      else {
        // we meet a symbolic index in the update list, stop matching concrete update nodes
        // return a ReadExpr from this symbolic update node instead.
        return Action::changeTo(ReadExpr::create(UpdateList(ul->root, un),
                        ConstantExpr::alloc(index, ul->root->getDomain())));
      }
    }

    // we can not find a matching concrete update, but all updates are concrete
    // let us match the root Array behind the update list for a concrete match.
    if (ul->root->isConstantArray()) {
      if (index < ul->root->size) {
        return Action::changeTo(ul->root->constantValues[index]);
      }
      else {
        klee_warning("out of bound access (index %lu) to constant Array %s (size %u)", index, ul->root->name.c_str(), ul->root->size);
      }
    }
  }
  return Action::changeTo(ReadExpr::create(UpdateList(ul->root, ul->head), idx));
}

