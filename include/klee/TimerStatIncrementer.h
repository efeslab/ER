//===-- TimerStatIncrementer.h ----------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_TIMERSTATINCREMENTER_H
#define KLEE_TIMERSTATINCREMENTER_H

#include "klee/Statistics.h"
#include "klee/Internal/Support/Timer.h"
#include <cassert>

namespace klee {

  /**
   * A TimerStatIncrementer adds its lifetime to a specified Statistic.
   */
  class TimerStatIncrementer {
  private:
    const WallTimer timer;
    Statistic &statistic;
    bool checked;

  public:
    explicit TimerStatIncrementer(Statistic &_statistic) : statistic(_statistic), checked(false) {}
    ~TimerStatIncrementer() {
      if (!checked) {
        check();
      }
    }

    time::Span check() {
      assert(!checked && "TimerStatIncrementer should not be checked twice");
      // record microseconds
      time::Span t = delta();
      statistic += t.toMicroseconds();
      checked = true;
      return t;
    }

    time::Span delta() const { return timer.delta(); }
  };
}

#endif /* KLEE_TIMERSTATINCREMENTER_H */
