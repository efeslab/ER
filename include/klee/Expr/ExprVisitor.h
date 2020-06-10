//===-- ExprVisitor.h -------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_EXPRVISITOR_H
#define KLEE_EXPRVISITOR_H

#include "ExprHashMap.h"

namespace klee {
  class ExprVisitor {
  protected:
    // typed variant, but non-virtual for efficiency
    class Action {
    public:
      enum Kind { SkipChildren, DoChildren, ChangeTo };

    private:
      //      Action() {}
      Action(Kind _kind) 
        : kind(_kind), argument(nullptr) {}
      Action(Kind _kind, const ref<Expr> &_argument) 
        : kind(_kind), argument(_argument) {}

      friend class ExprVisitor;

    public:
      Kind kind;
      ref<Expr> argument;

      static Action changeTo(const ref<Expr> &expr) { 
        return Action(ChangeTo,expr); 
      }
      static Action doChildren() { return Action(DoChildren); }
      static Action skipChildren() { return Action(SkipChildren); }
    };

  protected:
    explicit
    ExprVisitor(bool _recursive=false) : recursive(_recursive) {}
    virtual ~ExprVisitor() {}

    // visitExpr is called before you visit the underlying dervied Expr type and
    // before you visit (visitExpr or visit derived Expr type) any of its
    // children
    virtual Action visitExpr(const Expr&);
    // visitExprPost is called
    // 1) after you visit an Expr, visit and visitExprPost all of its children
    // 2) after you rebuilt an Expr, perhaps rebuilt multiple times in recursive
    // mode
    virtual Action visitExprPost(const Expr&);
    virtual ref<UpdateNode> visitUpdateNode(const ref<UpdateNode> &un);

    // visit derived Expr type is called after visitExpr but before
    // visitExprPost
    virtual Action visitNotOptimized(const NotOptimizedExpr&);
    virtual Action visitRead(const ReadExpr&);
    virtual Action visitSelect(const SelectExpr&);
    virtual Action visitConcat(const ConcatExpr&);
    virtual Action visitExtract(const ExtractExpr&);
    virtual Action visitZExt(const ZExtExpr&);
    virtual Action visitSExt(const SExtExpr&);
    virtual Action visitAdd(const AddExpr&);
    virtual Action visitSub(const SubExpr&);
    virtual Action visitMul(const MulExpr&);
    virtual Action visitUDiv(const UDivExpr&);
    virtual Action visitSDiv(const SDivExpr&);
    virtual Action visitURem(const URemExpr&);
    virtual Action visitSRem(const SRemExpr&);
    virtual Action visitNot(const NotExpr&);
    virtual Action visitAnd(const AndExpr&);
    virtual Action visitOr(const OrExpr&);
    virtual Action visitXor(const XorExpr&);
    virtual Action visitShl(const ShlExpr&);
    virtual Action visitLShr(const LShrExpr&);
    virtual Action visitAShr(const AShrExpr&);
    virtual Action visitEq(const EqExpr&);
    virtual Action visitNe(const NeExpr&);
    virtual Action visitUlt(const UltExpr&);
    virtual Action visitUle(const UleExpr&);
    virtual Action visitUgt(const UgtExpr&);
    virtual Action visitUge(const UgeExpr&);
    virtual Action visitSlt(const SltExpr&);
    virtual Action visitSle(const SleExpr&);
    virtual Action visitSgt(const SgtExpr&);
    virtual Action visitSge(const SgeExpr&);

  private:
    typedef ExprHashMap< ref<Expr> > visited_ty;
    visited_ty visited;
    // you need a recursive visitor if you want to keep visit each node until
    // you did not make any change in one pass
    // E.g. ExprReplaceVisitor2, which can find and replace multiple expressions,
    // has to be recursive because one replacement may create opportunities for
    // other replacement.
    bool recursive;

    ref<Expr> visitActual(const ref<Expr> &e);
    
  public:
    // apply the visitor to the expression and return a possibly
    // modified new expression.
    ref<Expr> visit(const ref<Expr> &e);
    void resetVisited() { visited.clear(); }
  };

}

#endif /* KLEE_EXPRVISITOR_H */
