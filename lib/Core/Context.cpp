//===-- Context.cpp -------------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "Context.h"
#include "Memory.h"

#include "klee/Expr.h"

#include "llvm/IR/Type.h"
#include "llvm/IR/DerivedTypes.h"

#include <cassert>

using namespace klee;

static bool Initialized = false;
static Context TheContext;

void Context::initialize(bool IsLittleEndian, Expr::Width PointerWidth) {
  assert(!Initialized && "Duplicate context initialization!");
  TheContext = Context(IsLittleEndian, PointerWidth);
  Initialized = true;
}

const Context &Context::get() {
  assert(Initialized && "Context has not been initialized!");
  return TheContext;
}

// FIXME: This is a total hack, just to avoid a layering issue until this stuff
// moves out of Expr.

ref<Expr> Expr::createSExtToPointerWidth(ref<Expr> e) {
  return SExtExpr::create(e, Context::get().getPointerWidth());
}

ref<Expr> Expr::createZExtToPointerWidth(ref<Expr> e) {
  return ZExtExpr::create(e, Context::get().getPointerWidth());
}

ref<PointerExpr> Expr::createPointer(uint64_t v) {
  MemoryObject *mobj = MemoryObject::getMemoryObjectByAddress(v);
  if (mobj) {
    return PointerExpr::create(v, Context::get().getPointerWidth(), mobj->id);
  } else if (v == 0) {
    return PointerExpr::create(0,
          Context::get().getPointerWidth(), MemoryObject::NULLPTR_MOBJ_HOLDER);
  } else {
    return PointerExpr::create(v,
          Context::get().getPointerWidth(), MemoryObject::UNKNOWN_MOBJ_HOLDER);
  }
}

ref<PointerExpr> Expr::createFunctionPointer(uint64_t v) {
  return PointerExpr::create(v,
        Context::get().getPointerWidth(), MemoryObject::FUNCTION_MOBJ_HOLDER);
}

ref<PointerExpr> PointerExpr::fromConstantExpr(ref<ConstantExpr> &ce) {
  uint64_t value = ce->getZExtValue();
  MemoryObject *mobj = MemoryObject::getMemoryObjectByAddress(value);
  if (mobj) {
    return PointerExpr::create(value, Context::get().getPointerWidth(), mobj->id);
  }
  else if (value == 0) {
    return PointerExpr::create(0,
          Context::get().getPointerWidth(), MemoryObject::NULLPTR_MOBJ_HOLDER);
  } else {
    return PointerExpr::create(value,
          Context::get().getPointerWidth(), MemoryObject::UNKNOWN_MOBJ_HOLDER);
  }
}
