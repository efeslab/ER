//===-- Expr.cpp ----------------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "klee/Expr.h"

#include "klee/Config/Version.h"
// FIXME: We shouldn't need this once fast constant support moves into
// Core. If we need to do arithmetic, we probably want to use APInt.
#include "klee/Internal/Support/IntEvaluation.h"
#include "klee/OptionCategories.h"
#include "klee/util/ExprPPrinter.h"

#include "llvm/ADT/Hashing.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include <sstream>

using namespace klee;
using namespace llvm;

namespace klee {
llvm::cl::OptionCategory
    ExprCat("Expression building and printing options",
            "These options impact the way expressions are build and printed.");
}

namespace {
cl::opt<bool> ConstArrayOpt(
    "const-array-opt", cl::init(false),
    cl::desc(
        "Enable an optimization involving all-constant arrays (default=false)"),
    cl::cat(klee::ExprCat));
}

/***/

unsigned Expr::count = 0;

ref<Expr> Expr::createTempRead(const Array *array, Expr::Width w,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  UpdateList ul(array, 0);

  switch (w) {
  default: assert(0 && "invalid width");
  case Expr::Bool: 
    return ZExtExpr::create(ReadExpr::create(ul, 
                                 ConstantExpr::alloc(0, Expr::Int32), creator, kinst),
                            Expr::Bool, creator, kinst);
  case Expr::Int8: 
    return ReadExpr::create(ul, 
                            ConstantExpr::alloc(0,Expr::Int32), creator, kinst);
  case Expr::Int16: 
    return ConcatExpr::create(ReadExpr::create(ul, 
                ConstantExpr::alloc(1,Expr::Int32), creator, kinst),
                ReadExpr::create(ul, 
                ConstantExpr::alloc(0,Expr::Int32), creator, kinst), creator, kinst);
  case Expr::Int32: 
    return ConcatExpr::create4(
                ReadExpr::create(ul, ConstantExpr::alloc(3,Expr::Int32), creator, kinst),
                ReadExpr::create(ul, ConstantExpr::alloc(2,Expr::Int32), creator, kinst),
                ReadExpr::create(ul, ConstantExpr::alloc(1,Expr::Int32), creator, kinst),
                ReadExpr::create(ul, ConstantExpr::alloc(0,Expr::Int32), creator, kinst),
                creator, kinst);
  case Expr::Int64: 
    return ConcatExpr::create8(
                ReadExpr::create(ul, ConstantExpr::alloc(7,Expr::Int32), creator, kinst),
                ReadExpr::create(ul, ConstantExpr::alloc(6,Expr::Int32), creator, kinst),
                ReadExpr::create(ul, ConstantExpr::alloc(5,Expr::Int32), creator, kinst),
                ReadExpr::create(ul, ConstantExpr::alloc(4,Expr::Int32), creator, kinst),
                ReadExpr::create(ul, ConstantExpr::alloc(3,Expr::Int32), creator, kinst),
                ReadExpr::create(ul, ConstantExpr::alloc(2,Expr::Int32), creator, kinst),
                ReadExpr::create(ul, ConstantExpr::alloc(1,Expr::Int32), creator, kinst),
                ReadExpr::create(ul, ConstantExpr::alloc(0,Expr::Int32), creator, kinst),
                creator, kinst);
  }
}

int Expr::compare(const Expr &b) const {
  static ExprEquivSet equivs;
  int r = compare(b, equivs);
  equivs.clear();
  return r;
}

// returns 0 if b is structurally equal to *this
int Expr::compare(const Expr &b, ExprEquivSet &equivs) const {
  if (this == &b) return 0;

  const Expr *ap, *bp;
  if (this < &b) {
    ap = this; bp = &b;
  } else {
    ap = &b; bp = this;
  }

  if (equivs.count(std::make_pair(ap, bp)))
    return 0;

  Kind ak = getKind(), bk = b.getKind();
  if (ak!=bk)
    return (ak < bk) ? -1 : 1;

  if (hashValue != b.hashValue) 
    return (hashValue < b.hashValue) ? -1 : 1;

  if (int res = compareContents(b)) 
    return res;

  unsigned aN = getNumKids();
  for (unsigned i=0; i<aN; i++)
    if (int res = getKid(i)->compare(*b.getKid(i), equivs))
      return res;

  equivs.insert(std::make_pair(ap, bp));
  return 0;
}

void Expr::printKind(llvm::raw_ostream &os, Kind k) {
  switch(k) {
#define X(C) case C: os << #C; break
    X(Constant);
    X(NotOptimized);
    X(Read);
    X(Select);
    X(Concat);
    X(Extract);
    X(ZExt);
    X(SExt);
    X(Add);
    X(Sub);
    X(Mul);
    X(UDiv);
    X(SDiv);
    X(URem);
    X(SRem);
    X(Not);
    X(And);
    X(Or);
    X(Xor);
    X(Shl);
    X(LShr);
    X(AShr);
    X(Eq);
    X(Ne);
    X(Ult);
    X(Ule);
    X(Ugt);
    X(Uge);
    X(Slt);
    X(Sle);
    X(Sgt);
    X(Sge);
#undef X
  default:
    assert(0 && "invalid kind");
    }
}

////////
//
// Simple hash functions for various kinds of Exprs
//
///////

unsigned Expr::computeHash() {
  unsigned res = getKind() * Expr::MAGIC_HASH_CONSTANT;

  int n = getNumKids();
  for (int i = 0; i < n; i++) {
    res <<= 1;
    res ^= getKid(i)->hash() * Expr::MAGIC_HASH_CONSTANT;
  }
  
  hashValue = res;
  return hashValue;
}

unsigned ConstantExpr::computeHash() {
  Expr::Width w = getWidth();
  if (w <= 64)
    hashValue = value.getLimitedValue() ^ (w * MAGIC_HASH_CONSTANT);
  else
    hashValue = hash_value(value) ^ (w * MAGIC_HASH_CONSTANT);

  return hashValue;
}

unsigned CastExpr::computeHash() {
  unsigned res = getWidth() * Expr::MAGIC_HASH_CONSTANT;
  hashValue = res ^ src->hash() * Expr::MAGIC_HASH_CONSTANT;
  return hashValue;
}

unsigned ExtractExpr::computeHash() {
  unsigned res = offset * Expr::MAGIC_HASH_CONSTANT;
  res ^= getWidth() * Expr::MAGIC_HASH_CONSTANT;
  hashValue = res ^ expr->hash() * Expr::MAGIC_HASH_CONSTANT;
  return hashValue;
}

unsigned ReadExpr::computeHash() {
  unsigned res = index->hash() * Expr::MAGIC_HASH_CONSTANT;
  res ^= updates.hash();
  hashValue = res;
  return hashValue;
}

unsigned NotExpr::computeHash() {
  hashValue = expr->hash() * Expr::MAGIC_HASH_CONSTANT * Expr::Not;
  return hashValue;
}

ref<Expr> Expr::createFromKind(Kind k, std::vector<CreateArg> args) {
  unsigned numArgs = args.size();
  (void) numArgs;

  switch(k) {
    case Constant:
    case Extract:
    case Read:
    default:
      assert(0 && "invalid kind");

    case NotOptimized:
      assert(numArgs == 1 && args[0].isExpr() &&
             "invalid args array for given opcode");
      return NotOptimizedExpr::create(args[0].expr);
      
    case Select:
      assert(numArgs == 3 && args[0].isExpr() &&
             args[1].isExpr() && args[2].isExpr() &&
             "invalid args array for Select opcode");
      return SelectExpr::create(args[0].expr,
                                args[1].expr,
                                args[2].expr);

    case Concat: {
      assert(numArgs == 2 && args[0].isExpr() && args[1].isExpr() && 
             "invalid args array for Concat opcode");
      
      return ConcatExpr::create(args[0].expr, args[1].expr);
    }
      
#define CAST_EXPR_CASE(T)                                    \
      case T:                                                \
        assert(numArgs == 2 &&				     \
               args[0].isExpr() && args[1].isWidth() &&      \
               "invalid args array for given opcode");       \
      return T ## Expr::create(args[0].expr, args[1].width); \
      
#define BINARY_EXPR_CASE(T)                                 \
      case T:                                               \
        assert(numArgs == 2 &&                              \
               args[0].isExpr() && args[1].isExpr() &&      \
               "invalid args array for given opcode");      \
      return T ## Expr::create(args[0].expr, args[1].expr); \

      CAST_EXPR_CASE(ZExt);
      CAST_EXPR_CASE(SExt);
      
      BINARY_EXPR_CASE(Add);
      BINARY_EXPR_CASE(Sub);
      BINARY_EXPR_CASE(Mul);
      BINARY_EXPR_CASE(UDiv);
      BINARY_EXPR_CASE(SDiv);
      BINARY_EXPR_CASE(URem);
      BINARY_EXPR_CASE(SRem);
      BINARY_EXPR_CASE(And);
      BINARY_EXPR_CASE(Or);
      BINARY_EXPR_CASE(Xor);
      BINARY_EXPR_CASE(Shl);
      BINARY_EXPR_CASE(LShr);
      BINARY_EXPR_CASE(AShr);
      
      BINARY_EXPR_CASE(Eq);
      BINARY_EXPR_CASE(Ne);
      BINARY_EXPR_CASE(Ult);
      BINARY_EXPR_CASE(Ule);
      BINARY_EXPR_CASE(Ugt);
      BINARY_EXPR_CASE(Uge);
      BINARY_EXPR_CASE(Slt);
      BINARY_EXPR_CASE(Sle);
      BINARY_EXPR_CASE(Sgt);
      BINARY_EXPR_CASE(Sge);
  }
}


void Expr::printWidth(llvm::raw_ostream &os, Width width) {
  switch(width) {
  case Expr::Bool: os << "Expr::Bool"; break;
  case Expr::Int8: os << "Expr::Int8"; break;
  case Expr::Int16: os << "Expr::Int16"; break;
  case Expr::Int32: os << "Expr::Int32"; break;
  case Expr::Int64: os << "Expr::Int64"; break;
  case Expr::Fl80: os << "Expr::Fl80"; break;
  default: os << "<invalid type: " << (unsigned) width << ">";
  }
}

ref<Expr> Expr::createImplies(ref<Expr> hyp, ref<Expr> conc,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  return OrExpr::create(
          Expr::createIsZero(hyp, creator, kinst), conc, creator, kinst);
}

ref<Expr> Expr::createIsZero(ref<Expr> e,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  return EqExpr::create(e,
            ConstantExpr::create(0, e->getWidth()), creator, kinst);
}

void Expr::print(llvm::raw_ostream &os) const {
  ExprPPrinter::printSingleExpr(os, const_cast<Expr*>(this));
}

void Expr::dump() const {
  this->print(errs());
  errs() << "\n";
}

/***/

ref<Expr> ConstantExpr::fromMemory(void *address, Width width) {
  switch (width) {
  case  Expr::Bool: return ConstantExpr::create(*(( uint8_t*) address), width);
  case  Expr::Int8: return ConstantExpr::create(*(( uint8_t*) address), width);
  case Expr::Int16: return ConstantExpr::create(*((uint16_t*) address), width);
  case Expr::Int32: return ConstantExpr::create(*((uint32_t*) address), width);
  case Expr::Int64: return ConstantExpr::create(*((uint64_t*) address), width);
  // FIXME: what about machines without x87 support?
  default:
    return ConstantExpr::alloc(llvm::APInt(width,
#if LLVM_VERSION_CODE >= LLVM_VERSION(5, 0)
      (width+llvm::APFloatBase::integerPartWidth-1)/llvm::APFloatBase::integerPartWidth,
#else
      (width+llvm::integerPartWidth-1)/llvm::integerPartWidth,
#endif
      (const uint64_t*)address));
  }
}

void ConstantExpr::toMemory(void *address) {
  switch (getWidth()) {
  default: assert(0 && "invalid type");
  case  Expr::Bool: *(( uint8_t*) address) = getZExtValue(1); break;
  case  Expr::Int8: *(( uint8_t*) address) = getZExtValue(8); break;
  case Expr::Int16: *((uint16_t*) address) = getZExtValue(16); break;
  case Expr::Int32: *((uint32_t*) address) = getZExtValue(32); break;
  case Expr::Int64: *((uint64_t*) address) = getZExtValue(64); break;
  // FIXME: what about machines without x87 support?
  case Expr::Fl80:
    *((long double*) address) = *(const long double*) value.getRawData();
    break;
  }
}

void ConstantExpr::toString(std::string &Res, unsigned radix) const {
  Res = value.toString(radix, false);
}

ref<ConstantExpr> ConstantExpr::Concat(const ref<ConstantExpr> &RHS) {
  Expr::Width W = getWidth() + RHS->getWidth();
  APInt Tmp(value);
  Tmp=Tmp.zext(W);
  Tmp <<= RHS->getWidth();
  Tmp |= APInt(RHS->value).zext(W);

  return ConstantExpr::alloc(Tmp,
      ConcatExpr::alloc(ref<Expr>(this), ref<Expr>(RHS))
  );
}

ref<ConstantExpr> ConstantExpr::Extract(unsigned Offset, Width W) {
  return ConstantExpr::alloc(APInt(value.ashr(Offset)).zextOrTrunc(W),
      ExtractExpr::alloc(ref<Expr>(this), Offset, W)
  );
}

ref<ConstantExpr> ConstantExpr::ZExt(Width W) {
  return ConstantExpr::alloc(APInt(value).zextOrTrunc(W),
      ZExtExpr::alloc(ref<Expr>(this), W)
  );
}

ref<ConstantExpr> ConstantExpr::SExt(Width W) {
  return ConstantExpr::alloc(APInt(value).sextOrTrunc(W),
      SExtExpr::alloc(ref<Expr>(this), W)
  );
}

ref<ConstantExpr> ConstantExpr::Add(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value + RHS->value,
      AddExpr::alloc(ref<Expr>(this), ref<Expr>(RHS))
  );
}

// FIXME: make sure no symbolic expression for Neg is fine
ref<ConstantExpr> ConstantExpr::Neg() {
  return ConstantExpr::alloc(-value);
}

ref<ConstantExpr> ConstantExpr::Sub(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value - RHS->value,
      SubExpr::alloc(ref<Expr>(this), ref<Expr>(RHS))
  );
}

ref<ConstantExpr> ConstantExpr::Mul(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value * RHS->value,
      MulExpr::alloc(ref<Expr>(this), ref<Expr>(RHS))
  );
}

ref<ConstantExpr> ConstantExpr::UDiv(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value.udiv(RHS->value),
      UDivExpr::alloc(ref<Expr>(this), ref<Expr>(RHS))
  );
}

ref<ConstantExpr> ConstantExpr::SDiv(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value.sdiv(RHS->value),
      SDivExpr::alloc(ref<Expr>(this), ref<Expr>(RHS))
  );
}

ref<ConstantExpr> ConstantExpr::URem(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value.urem(RHS->value),
      URemExpr::alloc(ref<Expr>(this), ref<Expr>(RHS))
  );
}

ref<ConstantExpr> ConstantExpr::SRem(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value.srem(RHS->value),
      SRemExpr::alloc(ref<Expr>(this), ref<Expr>(RHS))
  );
}

ref<ConstantExpr> ConstantExpr::And(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value & RHS->value,
      AddExpr::alloc(ref<Expr>(this), ref<Expr>(RHS))
  );
}

ref<ConstantExpr> ConstantExpr::Or(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value | RHS->value,
      OrExpr::alloc(ref<Expr>(this), ref<Expr>(RHS))
  );
}

ref<ConstantExpr> ConstantExpr::Xor(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value ^ RHS->value,
      XorExpr::alloc(ref<Expr>(this), ref<Expr>(RHS))
  );
}

ref<ConstantExpr> ConstantExpr::Shl(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value.shl(RHS->value),
      ShlExpr::alloc(ref<Expr>(this), ref<Expr>(RHS))
  );
}

ref<ConstantExpr> ConstantExpr::LShr(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value.lshr(RHS->value),
      LShrExpr::alloc(ref<Expr>(this), ref<Expr>(RHS))
  );
}

ref<ConstantExpr> ConstantExpr::AShr(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value.ashr(RHS->value),
      AShrExpr::alloc(ref<Expr>(this), ref<Expr>(RHS))
  );
}

ref<ConstantExpr> ConstantExpr::Not() {
  return ConstantExpr::alloc(~value,
      NotExpr::alloc(ref<Expr>(this))
  );
}

ref<ConstantExpr> ConstantExpr::Eq(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value == RHS->value, Expr::Bool,
      EqExpr::alloc(ref<Expr>(this), ref<Expr>(RHS))
  );
}

ref<ConstantExpr> ConstantExpr::Ne(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value != RHS->value, Expr::Bool,
      NeExpr::alloc(ref<Expr>(this), ref<Expr>(RHS))
  );
}

ref<ConstantExpr> ConstantExpr::Ult(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value.ult(RHS->value), Expr::Bool,
      UltExpr::alloc(ref<Expr>(this), ref<Expr>(RHS))
  );
}

ref<ConstantExpr> ConstantExpr::Ule(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value.ule(RHS->value), Expr::Bool,
      UleExpr::alloc(ref<Expr>(this), ref<Expr>(RHS))
  );
}

ref<ConstantExpr> ConstantExpr::Ugt(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value.ugt(RHS->value), Expr::Bool,
      UgtExpr::alloc(ref<Expr>(this), ref<Expr>(RHS))
  );
}

ref<ConstantExpr> ConstantExpr::Uge(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value.uge(RHS->value), Expr::Bool,
      UgeExpr::alloc(ref<Expr>(this), ref<Expr>(RHS))
  );
}

ref<ConstantExpr> ConstantExpr::Slt(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value.slt(RHS->value), Expr::Bool,
      SltExpr::alloc(ref<Expr>(this), ref<Expr>(RHS))
  );
}

ref<ConstantExpr> ConstantExpr::Sle(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value.sle(RHS->value), Expr::Bool,
      SleExpr::alloc(ref<Expr>(this), ref<Expr>(RHS))
  );
}

ref<ConstantExpr> ConstantExpr::Sgt(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value.sgt(RHS->value), Expr::Bool,
      SgtExpr::alloc(ref<Expr>(this), ref<Expr>(RHS))
  );
}

ref<ConstantExpr> ConstantExpr::Sge(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value.sge(RHS->value), Expr::Bool,
      SgeExpr::alloc(ref<Expr>(this), ref<Expr>(RHS))
  );
}

/***/

ref<Expr>  NotOptimizedExpr::create(ref<Expr> src,
            Expr::CreatorKind creator, KInstruction *kinst) {
  return NotOptimizedExpr::alloc(src, creator, kinst);
}

/***/

Array::Array(const std::string &_name, uint64_t _size,
             const ref<ConstantExpr> *constantValuesBegin,
             const ref<ConstantExpr> *constantValuesEnd, Expr::Width _domain,
             Expr::Width _range)
    : name(_name), size(_size), domain(_domain), range(_range),
      constantValues(constantValuesBegin, constantValuesEnd) {

  assert((isSymbolicArray() || constantValues.size() == size) &&
         "Invalid size for constant array!");
  computeHash();
#ifndef NDEBUG
  for (const ref<ConstantExpr> *it = constantValuesBegin;
       it != constantValuesEnd; ++it)
    assert((*it)->getWidth() == getRange() &&
           "Invalid initial constant value!");
#endif // NDEBUG
}

Array::~Array() {
}

unsigned Array::computeHash() {
  unsigned res = 0;
  for (unsigned i = 0, e = name.size(); i != e; ++i)
    res = (res * Expr::MAGIC_HASH_CONSTANT) + name[i];
  res = (res * Expr::MAGIC_HASH_CONSTANT) + size;
  hashValue = res;
  return hashValue; 
}
/***/

ref<Expr> ReadExpr::create(const UpdateList &ul, ref<Expr> index,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  // rollback update nodes if possible

  // Iterate through the update list from the most recent to the
  // least recent to find a potential written value for a concrete index;
  // stop if an update with symbolic has been found as we don't know which
  // array element has been updated
  const UpdateNode *un = ul.head;
  bool updateListHasSymbolicWrites = false;
  for (; un; un=un->next) {
    // Check if we have an equivalent concrete index
    ref<Expr> cond = EqExpr::create(index, un->index, Expr::Internal, nullptr);
    if (ConstantExpr *CE = dyn_cast<ConstantExpr>(cond)) {
      if (CE->isTrue())
        // Return the found value
        return un->value;
    } else {
      // Found write with symbolic index
      updateListHasSymbolicWrites = true;
      break;
    }
  }

  if (ul.root->isConstantArray() && !updateListHasSymbolicWrites) {
    // No updates with symbolic index to a constant array have been found
    if (ConstantExpr *CE = dyn_cast<ConstantExpr>(index)) {
      assert(CE->getWidth() <= 64 && "Index too large");
      uint64_t concreteIndex = CE->getZExtValue();
      uint64_t size = ul.root->size;
      if (concreteIndex < size) {
        return ul.root->constantValues[concreteIndex];
      }
    }
  }

  // Now, no update with this concrete index exists
  // Try to remove any most recent but unimportant updates
  if (ConstantExpr *CE = dyn_cast<ConstantExpr>(index)) {
    assert(CE->getWidth() <= 64 && "Index too large");
    uint64_t concreteIndex = CE->getZExtValue();
    uint64_t size = ul.root->size;
    if (concreteIndex < size) {
      // Create shortened update list
      UpdateList newUpdateList(ul.root, un);
      return ReadExpr::alloc(newUpdateList, index, creator, kinst);
    }
  }

  return ReadExpr::alloc(ul, index, creator, kinst);
}

int ReadExpr::compareContents(const Expr &b) const { 
  return updates.compare(static_cast<const ReadExpr&>(b).updates);
}

ref<Expr> SelectExpr::create(ref<Expr> c, ref<Expr> t, ref<Expr> f,
                Expr::CreatorKind creator,
                KInstruction *kinst) {
  Expr::Width kt = t->getWidth();

  assert(c->getWidth()==Bool && "type mismatch");
  assert(kt==f->getWidth() && "type mismatch");

  if (ConstantExpr *CE = dyn_cast<ConstantExpr>(c)) {
    return CE->isTrue() ? t : f;
  } else if (t==f) {
    return t;
  } else if (kt==Expr::Bool) { // c ? t : f  <=> (c and t) or (not c and f)
    if (ConstantExpr *CE = dyn_cast<ConstantExpr>(t)) {      
      if (CE->isTrue()) {
        return OrExpr::create(c, f, creator, kinst);
      } else {
        return AndExpr::create(
                    Expr::createIsZero(c, creator, kinst), f, creator, kinst);
      }
    } else if (ConstantExpr *CE = dyn_cast<ConstantExpr>(f)) {
      if (CE->isTrue()) {
        return OrExpr::create(
                    Expr::createIsZero(c, creator, kinst), t, creator, kinst);
      } else {
        return AndExpr::create(c, t, creator, kinst);
      }
    }
  }
  
  return SelectExpr::alloc(c, t, f, creator, kinst);
}

/***/

ref<Expr> ConcatExpr::create(const ref<Expr> &l, const ref<Expr> &r,
                Expr::CreatorKind creator,
                KInstruction *kinst) {
  Expr::Width w = l->getWidth() + r->getWidth();
  
  // Fold concatenation of constants.
  //
  if (ConstantExpr *lCE = dyn_cast<ConstantExpr>(l)) {
    if (ConstantExpr *rCE = dyn_cast<ConstantExpr>(r)) {
      return lCE->Concat(rCE);
    }
    else if (lCE->isZero()) {
      if (r->getWidth() < w) {
        // concat 0 x -> zext x
        return ZExtExpr::create(r, w, creator, kinst);
      }
    }
  }

  // Merge contiguous Extracts
  if (ExtractExpr *ee_left = dyn_cast<ExtractExpr>(l)) {
    if (ExtractExpr *ee_right = dyn_cast<ExtractExpr>(r)) {
      if (ee_left->expr == ee_right->expr &&
          ee_right->offset + ee_right->width == ee_left->offset) {
        return ExtractExpr::create(ee_left->expr,
                        ee_right->offset, w, creator, kinst);
      }
    }
  }

  return ConcatExpr::alloc(l, r, creator, kinst);
}

/// Shortcut to concat N kids.  The chain returned is unbalanced to the right
ref<Expr> ConcatExpr::createN(unsigned n_kids, const ref<Expr> kids[],
                Expr::CreatorKind creator,
                KInstruction *kinst) {
  assert(n_kids > 0);
  if (n_kids == 1)
    return kids[0];
  
  ref<Expr> r = ConcatExpr::create(kids[n_kids-2], kids[n_kids-1], creator, kinst);
  for (int i=n_kids-3; i>=0; i--)
    r = ConcatExpr::create(kids[i], r, creator, kinst);
  return r;
}

/// Shortcut to concat 4 kids.  The chain returned is unbalanced to the right
ref<Expr> ConcatExpr::create4(const ref<Expr> &kid1, const ref<Expr> &kid2,
                              const ref<Expr> &kid3, const ref<Expr> &kid4,
                              Expr::CreatorKind creator,
                              KInstruction *kinst) {
  return ConcatExpr::create(kid1, 
                ConcatExpr::create(kid2, 
                    ConcatExpr::create(kid3, kid4, creator, kinst),
                creator, kinst),
            creator, kinst);
}

/// Shortcut to concat 8 kids.  The chain returned is unbalanced to the right
ref<Expr> ConcatExpr::create8(const ref<Expr> &kid1, const ref<Expr> &kid2,
			      const ref<Expr> &kid3, const ref<Expr> &kid4,
			      const ref<Expr> &kid5, const ref<Expr> &kid6,
			      const ref<Expr> &kid7, const ref<Expr> &kid8,
                  Expr::CreatorKind creator,
                  KInstruction *kinst) {
  return ConcatExpr::create(kid1,
                ConcatExpr::create(kid2,
                    ConcatExpr::create(kid3, 
			            ConcatExpr::create(kid4,
                            ConcatExpr::create4(kid5, kid6, kid7, kid8, creator, kinst),
                        creator, kinst),
                    creator, kinst),
                creator, kinst),
            creator, kinst);
}

/***/

ref<Expr> ExtractExpr::create(ref<Expr> expr, unsigned off, Width w,
                  Expr::CreatorKind creator,
                  KInstruction *kinst) {
  unsigned kw = expr->getWidth();
  assert(w > 0 && off + w <= kw && "invalid extract");
  
  if (w == kw) {
    return expr;
  } else if (ConstantExpr *CE = dyn_cast<ConstantExpr>(expr)) {
    return CE->Extract(off, w);
  } else if (ConcatExpr *ce = dyn_cast<ConcatExpr>(expr)) { // Extract(Concat)
    // if the extract skips the right side of the concat
    if (off >= ce->getRight()->getWidth())
      return ExtractExpr::create(ce->getLeft(),
                off - ce->getRight()->getWidth(), w, creator, kinst);

    // if the extract skips the left side of the concat
    if (off + w <= ce->getRight()->getWidth())
      return ExtractExpr::create(ce->getRight(), off, w, creator, kinst);

    // E(C(x,y)) = C(E(x), E(y))
    return ConcatExpr::create(
            ExtractExpr::create(ce->getKid(0), 0,
                w - ce->getKid(1)->getWidth() + off, creator, kinst),
            ExtractExpr::create(ce->getKid(1), off,
                    ce->getKid(1)->getWidth() - off, creator, kinst), creator, kinst);
  }
  else if (CastExpr *cast = dyn_cast<CastExpr>(expr)) {
    assert(cast->getWidth() > cast->src->getWidth() && "CastExpr not longer than CastExpr->src");
    if (off + w <= cast->src->getWidth()) { // The data we want to Extract already known before Z/SExt
      if (off == 0 && (w == cast->src->getWidth())) { // we can remove both Extract and Z/SExt
        // Extract(w8, 0, Z/SExt(w32, Read(w8, idx, array))) => Read(w8, idx, array)
        return cast->src;
      }
      else {
        // Extract(w8, 2, Z/SExt(w32, Read(w8, idx, array))) => Extract(w8, 2, Read(w8, idx, array))
        return ExtractExpr::create(cast->src, off, w, creator, kinst);
      }
    }
  }
  
  return ExtractExpr::alloc(expr, off, w, creator, kinst);
}

/***/

ref<Expr> NotExpr::create(const ref<Expr> &e,
                  Expr::CreatorKind creator,
                  KInstruction *kinst) {
  if (ConstantExpr *CE = dyn_cast<ConstantExpr>(e))
    return CE->Not();
  
  return NotExpr::alloc(e, creator, kinst);
}


/***/

ref<Expr> ZExtExpr::create(const ref<Expr> &e, Width w,
                  Expr::CreatorKind creator,
                  KInstruction *kinst) {
  unsigned kBits = e->getWidth();
  if (w == kBits) {
    return e;
  } else if (w < kBits) { // trunc
    return ExtractExpr::create(e, 0, w, creator, kinst);
  } else if (ConstantExpr *CE = dyn_cast<ConstantExpr>(e)) {
    return CE->ZExt(w);
  } else {
    return ZExtExpr::alloc(e, w, creator, kinst);
  }
}

ref<Expr> SExtExpr::create(const ref<Expr> &e, Width w,
                  Expr::CreatorKind creator,
                  KInstruction *kinst) {
  unsigned kBits = e->getWidth();
  if (w == kBits) {
    return e;
  } else if (w < kBits) { // trunc
    return ExtractExpr::create(e, 0, w, creator, kinst);
  } else if (ConstantExpr *CE = dyn_cast<ConstantExpr>(e)) {
    return CE->SExt(w);
  } else {    
    return SExtExpr::alloc(e, w, creator, kinst);
  }
}

/***/

static ref<Expr> AndExpr_create(Expr *l, Expr *r,
            Expr::CreatorKind creator, KInstruction *kinst);
static ref<Expr> XorExpr_create(Expr *l, Expr *r,
            Expr::CreatorKind creator, KInstruction *kinst);

static ref<Expr> EqExpr_createPartial(Expr *l, const ref<ConstantExpr> &cr,
            Expr::CreatorKind creator, KInstruction *kinst);
static ref<Expr> AndExpr_createPartialR(const ref<ConstantExpr> &cl, Expr *r,
            Expr::CreatorKind creator, KInstruction *kinst);
static ref<Expr> SubExpr_createPartialR(const ref<ConstantExpr> &cl, Expr *r,
            Expr::CreatorKind creator, KInstruction *kinst);
static ref<Expr> XorExpr_createPartialR(const ref<ConstantExpr> &cl, Expr *r,
            Expr::CreatorKind creator, KInstruction *kinst);

static ref<Expr> AddExpr_createPartialR(const ref<ConstantExpr> &cl, Expr *r,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  Expr::Width type = cl->getWidth();

  if (type==Expr::Bool) {
    return XorExpr_createPartialR(cl, r, creator, kinst);
  } else if (cl->isZero()) {
    return r;
  } else {
    Expr::Kind rk = r->getKind();
    if (rk==Expr::Add && isa<ConstantExpr>(r->getKid(0))) { // A + (B+c) == (A+B) + c
      return AddExpr::create(AddExpr::create(cl, r->getKid(0)),
                             r->getKid(1), creator, kinst);
    } else if (rk==Expr::Sub && isa<ConstantExpr>(r->getKid(0))) { // A + (B-c) == (A+B) - c
      return SubExpr::create(AddExpr::create(cl, r->getKid(0)),
                             r->getKid(1), creator, kinst);
    } else {
      return AddExpr::alloc(cl, r, creator, kinst);
    }
  }
}
static ref<Expr> AddExpr_createPartial(Expr *l, const ref<ConstantExpr> &cr,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  return AddExpr_createPartialR(cr, l, creator, kinst);
}
static ref<Expr> AddExpr_create(Expr *l, Expr *r,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  Expr::Width type = l->getWidth();

  if (type == Expr::Bool) {
    return XorExpr_create(l, r, creator, kinst);
  } else {
    Expr::Kind lk = l->getKind(), rk = r->getKind();
    if (lk==Expr::Add && isa<ConstantExpr>(l->getKid(0))) { // (k+a)+b = k+(a+b)
      return AddExpr::create(l->getKid(0),
                 AddExpr::create(l->getKid(1), r, creator, kinst), creator, kinst);
    } else if (lk==Expr::Sub && isa<ConstantExpr>(l->getKid(0))) { // (k-a)+b = k+(b-a)
      return AddExpr::create(l->getKid(0),
                 SubExpr::create(r, l->getKid(1), creator, kinst), creator, kinst);
    } else if (rk==Expr::Add && isa<ConstantExpr>(r->getKid(0))) { // a + (k+b) = k+(a+b)
      return AddExpr::create(r->getKid(0),
                 AddExpr::create(l, r->getKid(1), creator, kinst), creator, kinst);
    } else if (rk==Expr::Sub && isa<ConstantExpr>(r->getKid(0))) { // a + (k-b) = k+(a-b)
      return AddExpr::create(r->getKid(0),
                 SubExpr::create(l, r->getKid(1), creator, kinst), creator, kinst);
    } else {
      return AddExpr::alloc(l, r, creator, kinst);
    }
  }  
}

static ref<Expr> SubExpr_createPartialR(const ref<ConstantExpr> &cl, Expr *r,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  Expr::Width type = cl->getWidth();

  if (type==Expr::Bool) {
    return XorExpr_createPartialR(cl, r, creator, kinst);
  } else {
    Expr::Kind rk = r->getKind();
    if (rk==Expr::Add && isa<ConstantExpr>(r->getKid(0))) { // A - (B+c) == (A-B) - c
      return SubExpr::create(SubExpr::create(cl, r->getKid(0), creator, kinst),
                             r->getKid(1), creator, kinst);
    } else if (rk==Expr::Sub && isa<ConstantExpr>(r->getKid(0))) { // A - (B-c) == (A-B) + c
      return AddExpr::create(SubExpr::create(cl, r->getKid(0), creator, kinst),
                             r->getKid(1), creator, kinst);
    } else {
      return SubExpr::alloc(cl, r, creator, kinst);
    }
  }
}
static ref<Expr> SubExpr_createPartial(Expr *l, const ref<ConstantExpr> &cr,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  // l - c => l + (-c)
  return AddExpr_createPartial(l, 
               ConstantExpr::alloc(0, cr->getWidth())->Sub(cr), creator, kinst);
}
static ref<Expr> SubExpr_create(Expr *l, Expr *r,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  Expr::Width type = l->getWidth();

  if (type == Expr::Bool) {
    return XorExpr_create(l, r, creator, kinst);
  } else if (*l==*r) {
    return ConstantExpr::alloc(0, type);
  } else {
    Expr::Kind lk = l->getKind(), rk = r->getKind();
    if (lk==Expr::Add && isa<ConstantExpr>(l->getKid(0))) { // (k+a)-b = k+(a-b)
      return AddExpr::create(l->getKid(0),
                     SubExpr::create(l->getKid(1), r, creator, kinst), creator, kinst);
    } else if (lk==Expr::Sub && isa<ConstantExpr>(l->getKid(0))) { // (k-a)-b = k-(a+b)
      return SubExpr::create(l->getKid(0),
                     AddExpr::create(l->getKid(1), r, creator, kinst), creator, kinst);
    } else if (rk==Expr::Add && isa<ConstantExpr>(r->getKid(0))) { // a - (k+b) = (a-c) - k
      return SubExpr::create(SubExpr::create(l, r->getKid(1), creator, kinst),
                             r->getKid(0), creator, kinst);
    } else if (rk==Expr::Sub && isa<ConstantExpr>(r->getKid(0))) { // a - (k-b) = (a+b) - k
      return SubExpr::create(AddExpr::create(l, r->getKid(1), creator, kinst),
                             r->getKid(0), creator, kinst);
    } else {
      return SubExpr::alloc(l, r, creator, kinst);
    }
  }  
}

static ref<Expr> MulExpr_createPartialR(const ref<ConstantExpr> &cl, Expr *r,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  Expr::Width type = cl->getWidth();

  if (type == Expr::Bool) {
    return AndExpr_createPartialR(cl, r, creator, kinst);
  } else if (cl->isOne()) {
    return r;
  } else if (cl->isZero()) {
    return cl;
  } else {
    return MulExpr::alloc(cl, r, creator, kinst);
  }
}
static ref<Expr> MulExpr_createPartial(Expr *l, const ref<ConstantExpr> &cr,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  return MulExpr_createPartialR(cr, l, creator, kinst);
}
static ref<Expr> MulExpr_create(Expr *l, Expr *r,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  Expr::Width type = l->getWidth();
  
  if (type == Expr::Bool) {
    return AndExpr::alloc(l, r, creator, kinst);
  } else {
    return MulExpr::alloc(l, r, creator, kinst);
  }
}

static ref<Expr> AndExpr_createPartial(Expr *l, const ref<ConstantExpr> &cr,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  if (cr->isAllOnes()) {
    return l;
  } else if (cr->isZero()) {
    return cr;
  } else {
    return AndExpr::alloc(l, cr, creator, kinst);
  }
}
static ref<Expr> AndExpr_createPartialR(const ref<ConstantExpr> &cl, Expr *r,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  return AndExpr_createPartial(r, cl, creator, kinst);
}
static ref<Expr> AndExpr_create(Expr *l, Expr *r,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  return AndExpr::alloc(l, r, creator, kinst);
}

static ref<Expr> OrExpr_createPartial(Expr *l, const ref<ConstantExpr> &cr,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  if (cr->isAllOnes()) {
    return cr;
  } else if (cr->isZero()) {
    return l;
  } else {
    return OrExpr::alloc(l, cr, creator, kinst);
  }
}
static ref<Expr> OrExpr_createPartialR(const ref<ConstantExpr> &cl, Expr *r,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  return OrExpr_createPartial(r, cl, creator, kinst);
}
static ref<Expr> OrExpr_create(Expr *l, Expr *r,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  return OrExpr::alloc(l, r, creator, kinst);
}

static ref<Expr> XorExpr_createPartialR(const ref<ConstantExpr> &cl, Expr *r,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  if (cl->isZero()) {
    return r;
  } else if (cl->getWidth() == Expr::Bool) {
    return EqExpr_createPartial(r, ConstantExpr::create(0, Expr::Bool), creator, kinst);
  } else {
    return XorExpr::alloc(cl, r, creator, kinst);
  }
}

static ref<Expr> XorExpr_createPartial(Expr *l, const ref<ConstantExpr> &cr,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  return XorExpr_createPartialR(cr, l, creator, kinst);
}
static ref<Expr> XorExpr_create(Expr *l, Expr *r,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  return XorExpr::alloc(l, r, creator, kinst);
}

static ref<Expr> UDivExpr_create(const ref<Expr> &l, const ref<Expr> &r,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  if (l->getWidth() == Expr::Bool) { // r must be 1
    return l;
  } else{
    return UDivExpr::alloc(l, r, creator, kinst);
  }
}

static ref<Expr> SDivExpr_create(const ref<Expr> &l, const ref<Expr> &r,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  if (l->getWidth() == Expr::Bool) { // r must be 1
    return l;
  } else{
    return SDivExpr::alloc(l, r, creator, kinst);
  }
}

static ref<Expr> URemExpr_create(const ref<Expr> &l, const ref<Expr> &r,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  if (l->getWidth() == Expr::Bool) { // r must be 1
    return ConstantExpr::create(0, Expr::Bool);
  } else{
    return URemExpr::alloc(l, r, creator, kinst);
  }
}

static ref<Expr> SRemExpr_create(const ref<Expr> &l, const ref<Expr> &r,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  if (l->getWidth() == Expr::Bool) { // r must be 1
    return ConstantExpr::create(0, Expr::Bool);
  } else{
    return SRemExpr::alloc(l, r, creator, kinst);
  }
}

static ref<Expr> ShlExpr_create(const ref<Expr> &l, const ref<Expr> &r,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  if (l->getWidth() == Expr::Bool) { // l & !r
    return AndExpr::create(l,
            Expr::createIsZero(r, creator, kinst), creator, kinst);
  } else{
    return ShlExpr::alloc(l, r, creator, kinst);
  }
}

static ref<Expr> LShrExpr_create(const ref<Expr> &l, const ref<Expr> &r,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  if (l->getWidth() == Expr::Bool) { // l & !r
    return AndExpr::create(l, 
            Expr::createIsZero(r, creator, kinst), creator, kinst);
  } else{
    return LShrExpr::alloc(l, r, creator, kinst);
  }
}

static ref<Expr> AShrExpr_create(const ref<Expr> &l, const ref<Expr> &r,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  if (l->getWidth() == Expr::Bool) { // l
    return l;
  } else{
    return AShrExpr::alloc(l, r, creator, kinst);
  }
}

#define BCREATE_R(_e_op, _op, partialL, partialR) \
ref<Expr>  _e_op ::create(const ref<Expr> &l, const ref<Expr> &r,       \
            Expr::CreatorKind creator,                  \
            KInstruction *kinst) {                            \
  assert(l->getWidth()==r->getWidth() && "type mismatch");              \
  if (ConstantExpr *cl = dyn_cast<ConstantExpr>(l)) {                   \
    if (ConstantExpr *cr = dyn_cast<ConstantExpr>(r))                   \
      return cl->_op(cr);                                               \
    return _e_op ## _createPartialR(cl, r.get(), creator, kinst);       \
  } else if (ConstantExpr *cr = dyn_cast<ConstantExpr>(r)) {            \
    return _e_op ## _createPartial(l.get(), cr, creator, kinst);        \
  }                                                                     \
  return _e_op ## _create(l.get(), r.get(), creator, kinst);            \
}

#define BCREATE(_e_op, _op) \
ref<Expr>  _e_op ::create(const ref<Expr> &l, const ref<Expr> &r,   \
            Expr::CreatorKind creator,              \
            KInstruction *kinst) {                        \
  assert(l->getWidth()==r->getWidth() && "type mismatch");          \
  if (ConstantExpr *cl = dyn_cast<ConstantExpr>(l))                 \
    if (ConstantExpr *cr = dyn_cast<ConstantExpr>(r))               \
      return cl->_op(cr);                                           \
  return _e_op ## _create(l, r, creator, kinst);                    \
}

BCREATE_R(AddExpr, Add, AddExpr_createPartial, AddExpr_createPartialR)
BCREATE_R(SubExpr, Sub, SubExpr_createPartial, SubExpr_createPartialR)
BCREATE_R(MulExpr, Mul, MulExpr_createPartial, MulExpr_createPartialR)
BCREATE_R(AndExpr, And, AndExpr_createPartial, AndExpr_createPartialR)
BCREATE_R(OrExpr, Or, OrExpr_createPartial, OrExpr_createPartialR)
BCREATE_R(XorExpr, Xor, XorExpr_createPartial, XorExpr_createPartialR)
BCREATE(UDivExpr, UDiv)
BCREATE(SDivExpr, SDiv)
BCREATE(URemExpr, URem)
BCREATE(SRemExpr, SRem)
BCREATE(ShlExpr, Shl)
BCREATE(LShrExpr, LShr)
BCREATE(AShrExpr, AShr)

#define CMPCREATE(_e_op, _op) \
ref<Expr>  _e_op ::create(const ref<Expr> &l, const ref<Expr> &r,       \
            Expr::CreatorKind creator,                        \
            KInstruction *kinst) {                            \
  assert(l->getWidth()==r->getWidth() && "type mismatch");              \
  if (ConstantExpr *cl = dyn_cast<ConstantExpr>(l))                     \
    if (ConstantExpr *cr = dyn_cast<ConstantExpr>(r))                   \
      return cl->_op(cr);                                               \
  return _e_op ## _create(l, r, creator, kinst);                        \
}

#define CMPCREATE_T(_e_op, _op, _reflexive_e_op, partialL, partialR) \
ref<Expr>  _e_op ::create(const ref<Expr> &l, const ref<Expr> &r,   \
            Expr::CreatorKind creator,                    \
            KInstruction *kinst) {                        \
  assert(l->getWidth()==r->getWidth() && "type mismatch");             \
  if (ConstantExpr *cl = dyn_cast<ConstantExpr>(l)) {                  \
    if (ConstantExpr *cr = dyn_cast<ConstantExpr>(r))                  \
      return cl->_op(cr);                                              \
    return partialR(cl, r.get(), creator, kinst);                      \
  } else if (ConstantExpr *cr = dyn_cast<ConstantExpr>(r)) {           \
    return partialL(l.get(), cr, creator, kinst);                      \
  } else {                                                             \
    return _e_op ## _create(l.get(), r.get(), creator, kinst);         \
  }                                                                    \
}
  

static ref<Expr> EqExpr_create(const ref<Expr> &l, const ref<Expr> &r,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  if (l == r) {
    return ConstantExpr::alloc(1, Expr::Bool);
  } else {
    return EqExpr::alloc(l, r, creator, kinst);
  }
}


/// Tries to optimize EqExpr cl == rd, where cl is a ConstantExpr and
/// rd a ReadExpr.  If rd is a read into an all-constant array,
/// returns a disjunction of equalities on the index.  Otherwise,
/// returns the initial equality expression. 
static ref<Expr> TryConstArrayOpt(const ref<ConstantExpr> &cl, 
				  ReadExpr *rd, Expr::CreatorKind creator = Expr::Unknown,
                  KInstruction *kinst = nullptr) {
  if (rd->updates.root->isSymbolicArray() || rd->updates.getSize())
    return EqExpr_create(cl, rd, creator, kinst);

  // Number of positions in the array that contain value ct.
  unsigned numMatches = 0;

  // for now, just assume standard "flushing" of a concrete array,
  // where the concrete array has one update for each index, in order
  ref<Expr> res = ConstantExpr::alloc(0, Expr::Bool);
  for (unsigned i = 0, e = rd->updates.root->size; i != e; ++i) {
    if (cl == rd->updates.root->constantValues[i]) {
      // Arbitrary maximum on the size of disjunction.
      if (++numMatches > 100)
        return EqExpr_create(cl, rd, creator, kinst);
      
      ref<Expr> mayBe = 
        EqExpr::create(rd->index, ConstantExpr::alloc(i, 
                              rd->index->getWidth()), creator, kinst);
      res = OrExpr::create(res, mayBe, creator, kinst);
    }
  }

  return res;
}

static ref<Expr> EqExpr_createPartialR(const ref<ConstantExpr> &cl, Expr *r,
            Expr::CreatorKind creator,
            KInstruction *kinst) {  
  Expr::Width width = cl->getWidth();

  Expr::Kind rk = r->getKind();
  if (width == Expr::Bool) {
    if (cl->isTrue()) {
      return r;
    } else {
      // 0 == ...
      
      if (rk == Expr::Eq) {
        const EqExpr *ree = cast<EqExpr>(r);

        // eliminate double negation
        if (ConstantExpr *CE = dyn_cast<ConstantExpr>(ree->left)) {
          // 0 == (0 == A) => A
          if (CE->getWidth() == Expr::Bool &&
              CE->isFalse())
            return ree->right;
        }
      } else if (rk == Expr::Or) {
        const OrExpr *roe = cast<OrExpr>(r);

        // transform not(or(a,b)) to and(not a, not b)
        return AndExpr::create(Expr::createIsZero(roe->left, creator, kinst),
                               Expr::createIsZero(roe->right, creator, kinst),
                               creator, kinst);
      }
    }
  } else if (rk == Expr::SExt) {
    // (sext(a,T)==c) == (a==c)
    const SExtExpr *see = cast<SExtExpr>(r);
    Expr::Width fromBits = see->src->getWidth();
    ref<ConstantExpr> trunc = cl->ZExt(fromBits);

    // pathological check, make sure it is possible to
    // sext to this value *from any value*
    if (cl == trunc->SExt(width)) {
      return EqExpr::create(see->src, trunc, creator, kinst);
    } else {
      return ConstantExpr::create(0, Expr::Bool);
    }
  } else if (rk == Expr::ZExt) {
    // (zext(a,T)==c) == (a==c)
    const ZExtExpr *zee = cast<ZExtExpr>(r);
    Expr::Width fromBits = zee->src->getWidth();
    ref<ConstantExpr> trunc = cl->ZExt(fromBits);
    
    // pathological check, make sure it is possible to
    // zext to this value *from any value*
    if (cl == trunc->ZExt(width)) {
      return EqExpr::create(zee->src, trunc, creator, kinst);
    } else {
      return ConstantExpr::create(0, Expr::Bool);
    }
  } else if (rk==Expr::Add) {
    const AddExpr *ae = cast<AddExpr>(r);
    if (isa<ConstantExpr>(ae->left)) {
      // c0 = c1 + b => c0 - c1 = b
      return EqExpr_createPartialR(
                cast<ConstantExpr>(SubExpr::create(cl, ae->left, creator, kinst)),
                                   ae->right.get(), creator, kinst);
    }
  } else if (rk==Expr::Sub) {
    const SubExpr *se = cast<SubExpr>(r);
    if (isa<ConstantExpr>(se->left)) {
      // c0 = c1 - b => c1 - c0 = b
      return EqExpr_createPartialR(
                cast<ConstantExpr>(SubExpr::create(se->left, cl, creator, kinst)),
                                   se->right.get(), creator, kinst);
    }
  } else if (rk == Expr::Read && ConstArrayOpt) {
    return TryConstArrayOpt(cl, static_cast<ReadExpr*>(r));
  }
    
  return EqExpr_create(cl, r, creator, kinst);
}

static ref<Expr> EqExpr_createPartial(Expr *l, const ref<ConstantExpr> &cr,
            Expr::CreatorKind creator,
            KInstruction *kinst) {  
  return EqExpr_createPartialR(cr, l, creator, kinst);
}
  
ref<Expr> NeExpr::create(const ref<Expr> &l, const ref<Expr> &r,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  return EqExpr::create(ConstantExpr::create(0, Expr::Bool),
                        EqExpr::create(l, r), creator, kinst);
}

ref<Expr> UgtExpr::create(const ref<Expr> &l, const ref<Expr> &r,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  return UltExpr::create(r, l, creator, kinst);
}
ref<Expr> UgeExpr::create(const ref<Expr> &l, const ref<Expr> &r,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  return UleExpr::create(r, l, creator, kinst);
}

ref<Expr> SgtExpr::create(const ref<Expr> &l, const ref<Expr> &r,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  return SltExpr::create(r, l, creator, kinst);
}
ref<Expr> SgeExpr::create(const ref<Expr> &l, const ref<Expr> &r,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  return SleExpr::create(r, l, creator, kinst);
}

static ref<Expr> UltExpr_create(const ref<Expr> &l, const ref<Expr> &r,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  Expr::Width t = l->getWidth();
  if (t == Expr::Bool) { // !l && r
    return AndExpr::create(
                Expr::createIsZero(l, creator, kinst), r, creator, kinst);
  } else {
    return UltExpr::alloc(l, r, creator, kinst);
  }
}

static ref<Expr> UleExpr_create(const ref<Expr> &l, const ref<Expr> &r,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  if (l->getWidth() == Expr::Bool) { // !(l && !r)
    return OrExpr::create(
            Expr::createIsZero(l, creator, kinst), r, creator, kinst);
  } else {
    return UleExpr::alloc(l, r, creator, kinst);
  }
}

static ref<Expr> SltExpr_create(const ref<Expr> &l, const ref<Expr> &r,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  if (l->getWidth() == Expr::Bool) { // l && !r
    return AndExpr::create(l, 
            Expr::createIsZero(r, creator, kinst), creator, kinst);
  } else {
    return SltExpr::alloc(l, r, creator, kinst);
  }
}

static ref<Expr> SleExpr_create(const ref<Expr> &l, const ref<Expr> &r,
            Expr::CreatorKind creator,
            KInstruction *kinst) {
  if (l->getWidth() == Expr::Bool) { // !(!l && r)
    return OrExpr::create(l,
            Expr::createIsZero(r, creator, kinst), creator, kinst);
  } else {
    return SleExpr::alloc(l, r, creator, kinst);
  }
}

CMPCREATE_T(EqExpr, Eq, EqExpr, EqExpr_createPartial, EqExpr_createPartialR)
CMPCREATE(UltExpr, Ult)
CMPCREATE(UleExpr, Ule)
CMPCREATE(SltExpr, Slt)
CMPCREATE(SleExpr, Sle)

ConstantExpr* debug_to_ConstantExpr(Expr *e) {
  return dyn_cast<ConstantExpr>(e);
}
